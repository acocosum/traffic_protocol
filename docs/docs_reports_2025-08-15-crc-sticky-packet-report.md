# 交通协议 CRC 校验失败与 TCP 粘包问题分析报告

- 日期：2025-08-15
- 仓库：acocosum/traffic_protocol
- 作者：acocosum
- 相关PR：
  - 调试日志增强：[PR #1](https://github.com/acocosum/traffic_protocol/pull/1)
  - 粘包修复：[PR #2](https://github.com/acocosum/traffic_protocol/pull/2)

## 摘要

在联调过程中，服务端间歇性出现“Failed to decode frame ... error: 3 (CRC)”的错误。通过增强日志与数据抓取分析，定位为典型的 TCP 粘包问题：客户端在极短时间内连续发送“心跳响应”和“实时数据”，TCP 将它们合并为一次传输；服务端以单次 `recv()` 结果作为完整帧去解析，导致把两帧当成一帧，进而产生 CRC 校验失败。

问题通过两阶段修复：
- 阶段一（PR #1）：增加 CRC 失败时的详细日志与数据十六进制 dump，确认粘包事实。
- 阶段二（PR #2）：为每个客户端引入接收缓冲区与帧提取逻辑，支持多帧解析与跨包缓存，彻底解决粘包/拆包导致的 CRC 失败。

## 影响范围与严重性

- 影响组件：服务端协议解析（`decode_frame` 的调用侧）、客户端与服务端之间的 TCP 消息交互
- 严重性：中等（偶发 CRC 错误导致单次消息丢弃；有重试/重发策略时影响可缓解）
- 触发条件：客户端在极短间隔连续发送多条消息（心跳响应 + 实时数据）

## 事件时间线（UTC+8）

- 17:21:50 服务端日志：
  - `[ERROR] CRC verification failed: received=0xA23A, calculated=0x6EED, data_len=127`
  - `[DEBUG] Raw received data: C0 ... 69 C0 C0 00 00 14 AE ... (双 C0 相邻)`
  - `[DEBUG] Unescaped data: ...`
  - `[WARN ] Failed to decode frame from client 0, error: 3 (CRC), data_len: 131`
- 同时客户端日志：
  - 收到心跳查询 -> 发送心跳响应（24 字节）
  - 发送实时数据（107 字节）
  - 随后收到 25 字节服务端消息，且解析为对象 ID 0x0000（提示上次服务端解析异常）

通过日志可见：单次 `recv()` 获得 131 字节（24+107），且在原始数据中存在相邻的 `C0 C0`，印证粘包（两帧反复紧邻）。

## 证据与关键线索

- 原始数据片段中出现 `C0 ... C0 C0 ... C0`，说明连续两帧被粘连。
- `decode_frame` 以 `buffer[0]==FRAME_START && buffer[end]==FRAME_END` 判定完整帧，传入的是合并帧，导致：
  - CRC 取的是第二帧的末尾两字节
  - 计算 CRC 的数据却是两帧的拼接体
  -> 比对必然失败（received ≠ calculated），返回 `PROTOCOL_ERROR_CRC (3)`。

## 根因分析

- 技术根因：服务端将“单次 `recv()` = 单帧”的假设用于解析，缺少对 TCP 流的帧边界管理。TCP 是字节流协议，应用层需自行实现边界恢复（分帧）。
- 触发因素：
  - 客户端在极短时间连续发送多条消息
  - 服务端未维护 per-connection 的接收缓冲区与帧提取逻辑
  - 未处理“多帧合并、跨包不完整帧、无效噪声”等情况

## 修复方案

### 阶段一：增强可观测性（PR #1）

目标：在不改变行为的前提下，提供足够的现场证据定位问题。

变更要点：
- 在 `src/common/protocol.c::decode_frame` 中增加：
  - 收到 CRC 与计算 CRC 的详细对比日志
  - 数据长度与关键字节长度输出
  - 可选的十六进制 dump（受日志等级控制）
- 在 `src/server/signal_controller.c::handle_client_message` 中增加：
  - 客户端信息（client_idx、IP）、收到字节数
  - CRC 失败场景的附加上下文

结果：通过日志确认粘包现象与错误根因。

### 阶段二：协议帧流式解析（PR #2）

目标：从根本上解决粘包/拆包问题，确保每次传入 `decode_frame` 的都是“正好一帧”。

主要改动：
- 为每个客户端维护接收缓冲区（ring/linear buffer）：
  - `client_info_t` 新增：
    - `uint8_t rx_buf[RECV_BUFFER_SIZE];`
    - `size_t rx_len;`
- 接收流程调整：
  1. 将 `recv()` 得到的字节追加到 `rx_buf`
  2. 在 `rx_buf` 中执行“帧提取”循环：
     - 扫描 `FRAME_START (0xC0)`，找到起始索引 `s`
     - 从 `s+1` 开始寻找下一枚 `FRAME_END (0xC0)`，得到终止索引 `e`
     - 对 `[s, e]` 切片作为一帧调用 `decode_frame`
     - 成功则处理该帧并从缓冲中移除已消费的区间，继续循环
     - 若找不到 `FRAME_END`，保留 `[s, rx_len)` 等待下次 `recv()`
  3. 防御性处理：
     - 若缓冲区中存在长时间找不到 `FRAME_START` 的前缀噪声，丢弃噪声
     - 超过最大帧上限或异常数据，丢弃至下一个 `FRAME_START`
- 异常与健壮性：
  - 单帧错误不影响后续帧（逐帧隔离处理）
  - 对“连续多帧”“跨包不完整帧”均能正确处理
  - 保持对 `escape/unescape` 的兼容（分帧在转义前的原始流上，以 `0xC0` 作为可靠边界）

伪代码（逻辑示例）：
```c
append(rx_buf, recv_bytes);

while (true) {
    int s = find_byte(rx_buf, 0, rx_len, FRAME_START);
    if (s < 0) { rx_len = 0; break; }        // 无起始符，清空噪声
    int e = find_byte(rx_buf, s + 1, rx_len, FRAME_END);
    if (e < 0) {                              // 没有结束符，等待更多数据
        if (s > 0) compact_to_front(rx_buf, s, rx_len);
        rx_len -= s;
        break;
    }

    size_t frame_len = e - s + 1;             // [s..e] 含首尾 C0
    protocol_result_t r = decode_frame(rx_buf + s, frame_len, &frame);
    if (r == PROTOCOL_SUCCESS) {
        handle_frame(&frame);
        free_frame(&frame);
    } else {
        log_warn("drop bad frame r=%d", r);
    }

    // 移除已消费的 [0..e]
    remove_prefix(rx_buf, &rx_len, e + 1);
    // 继续循环，处理可能的下一帧
}
```

性能考量：
- 线性扫描 O(n)，RECV_BUFFER_SIZE 有界
- 仅在必要时移动/压缩缓冲区
- 日志在非 DEBUG 级别下开销极低

协议层变更：
- 无需修改帧格式与 CRC 算法
- 仅引入流式分帧能力，提升健壮性

## 复现与验证

复现步骤：
1. 启动服务端与客户端
2. 客户端在收到心跳后立即发送心跳响应与实时数据（间隔 < 10ms）
3. 旧实现中，服务端偶发出现 `PROTOCOL_ERROR_CRC (3)`；新实现应稳定通过

验证用例：
- 单帧正常传输
- 双帧紧邻粘连（心跳响应 + 实时数据）
- 跨包不完整帧（半帧本次接收，半帧下次接收）
- 含无效前缀噪声/垃圾字节
- 超长帧/异常帧丢弃与恢复

期望结果：
- 所有合法帧均能被正确解析与处理
- 非法帧仅影响当前帧，不影响后续帧
- CRC 错误因粘包导致的告警消除

## 风险与回滚

- 风险：新的缓冲与分帧逻辑若有边界条件缺陷，可能导致丢帧或卡死
- 缓解：
  - 限制缓冲区长度与逐帧超时
  - 保留充足日志（DEBUG）便于定位
- 回滚方案：若出现不可接受问题，可快速回滚至 PR #2 之前的版本；保留 PR #1 的增强日志以继续排障

## 后续改进建议

- 增加“帧长度字段”，进一步提升分帧确定性与效率（避免双向扫描）
- 引入成熟编码（SLIP/COBS）的一致实现与测试用例集合
- 建立端到端压力测试与 fuzz 测试（随机插入噪声/乱序/截断）
- 客户端发送策略优化：允许在高负载时对非关键消息做轻微随机抖动，降低粘包概率（非必须）

## 变更摘要

- 新增（或修改）的核心文件：
  - `src/server/signal_controller.c`：引入 per-client 接收缓冲区与多帧提取处理
  - `src/common/protocol.c`：增强 CRC 失败的日志与数据 dump（仅 DEBUG 级开启）
  - 如需：`include/...` 或 `src/common/...` 中定义 `RECV_BUFFER_SIZE` 与辅助工具函数

- 协议与对外行为：
  - 协议格式未变
  - 行为更健壮：支持 TCP 粘包/拆包情况下的稳定解析

## 附录：关键日志样例

服务端（问题现场）：
```
[2025-08-15 17:21:50] [ERROR] CRC verification failed: received=0xA23A, calculated=0x6EED, data_len=127
[2025-08-15 17:21:50] [DEBUG] Raw received data: C0 ... 69 C0 C0 00 00 14 AE ... (后续略)
[2025-08-15 17:21:50] [WARN ] Failed to decode frame from client 0, error: 3 (CRC), data_len: 131
```

客户端（同一时刻）：
```
[2025-08-15 17:21:50] [DEBUG] Received heartbeat query from server
[2025-08-15 17:21:50] [DEBUG] Sending heartbeat response to server (24 bytes)
[2025-08-15 17:21:50] [DEBUG] Sending realtime traffic data (107 bytes)
```

定位依据：相邻 `C0 C0` 显示为“帧首 + 帧尾 + 帧首”紧邻，说明两帧贴合传输，被错误地当成一帧解析。
