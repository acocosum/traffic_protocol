# GB/T 43229-2023 交通信号控制机与车辆检测器通信协议实现

本项目实现了中国国家标准 GB/T 43229-2023《交通信号控制机与车辆检测器间通信协议》的完整C语言版本，包含服务端（交通信号控制机）和客户端（车辆检测器）的模拟实现。

## 项目特性

- ✅ **完整协议实现**: 支持所有标准定义的消息类型和通信规程
- ✅ **CRC16校验**: 实现标准要求的数据完整性校验
- ✅ **数据转义**: 完整的帧转义编解码机制
- ✅ **心跳机制**: 自动连接管理和故障检测
- ✅ **多客户端**: 服务端支持多个检测器同时连接
- ✅ **数据模拟**: 真实的交通检测数据模拟
- ✅ **日志系统**: 完整的调试和运行日志
- ✅ **跨平台**: 支持 Linux/Unix 系统

## 项目结构

```
traffic_protocol/
├── src/
│   ├── common/           # 协议核心实现
│   │   ├── protocol.h    # 协议定义和数据结构
│   │   ├── protocol.c    # 协议编解码实现
│   │   ├── crc16.h       # CRC16校验头文件
│   │   └── crc16.c       # CRC16校验实现
│   ├── server/           # 信号机（服务端）
│   │   ├── signal_controller.h
│   │   └── signal_controller.c
│   ├── client/           # 检测器（客户端）
│   │   ├── vehicle_detector.h
│   │   └── vehicle_detector.c
│   └── utils/            # 工具模块
│       ├── logger.h      # 日志系统
│       ├── logger.c
│       ├── socket_utils.h # Socket工具
│       └── socket_utils.c
├── examples/             # 示例程序
│   ├── server_demo.c     # 服务端演示
│   └── client_demo.c     # 客户端演示
├── bin/                  # 可执行文件目录
├── build/                # 编译输出目录
├── docs/                 # 文档目录
├── Makefile              # 构建脚本
└── README.md             # 项目说明
```

## 快速开始

### 系统要求

- **操作系统**: Linux/Unix (推荐 Ubuntu 18.04+, CentOS 7+)
- **编译器**: GCC 4.8+ 或兼容的C编译器
- **依赖**: 无外部依赖，只使用标准C库

### 编译项目

```bash
# 克隆或下载项目代码
cd traffic_protocol

# 编译所有组件
make all

# 查看编译结果
ls bin/
```

编译成功后，会在 `bin/` 目录下生成两个可执行文件：
- `server_demo` - 交通信号控制机演示程序
- `client_demo` - 车辆检测器演示程序

### 基本使用

#### 1. 启动服务端（交通信号控制机）

```bash
# 使用默认参数启动（端口40000）
./bin/server_demo

# 或指定自定义参数
./bin/server_demo -p 40000 -a 110100 -i 1 -l 1
```

**服务端参数说明：**
- `-p <port>`: 监听端口（默认: 40000）
- `-a <admin>`: 行政区划代码（默认: 110100，北京市东城区）
- `-i <id>`: 设备编号（默认: 1）
- `-l <level>`: 日志级别（0=DEBUG, 1=INFO, 2=WARN, 3=ERROR）
- `-f <file>`: 日志文件路径（默认: 仅控制台输出）
- `-h`: 显示帮助信息

#### 2. 启动客户端（车辆检测器）

```bash
# 连接到本地服务器
./bin/client_demo

# 连接到远程服务器并指定参数
./bin/client_demo -s 192.168.1.100 -p 40000 -a 110100 -t 2 -i 100
```

**客户端参数说明：**
- `-s <server>`: 服务器IP地址（默认: 127.0.0.1）
- `-p <port>`: 服务器端口（默认: 40000）
- `-a <admin>`: 行政区划代码（默认: 110100）
- `-t <type>`: 设备类型（默认: 2，线圈检测器）
- `-i <id>`: 设备编号（默认: 100）
- `-l <level>`: 日志级别
- `-f <file>`: 日志文件路径
- `-h`: 显示帮助信息

**设备类型对照表：**
- `1` - 线圈检测器
- `2` - 地磁检测器  
- `4` - 超声波检测器
- `8` - 视频检测器
- `16` - 微波断面检测器
- `32` - 多目标雷达检测器
- `64` - RFID检测器

### 完整演示流程

#### 场景1：单检测器连接测试

**终端1 - 启动服务端：**
```bash
./bin/server_demo -p 40000 -l 1 -f server.log
```

**终端2 - 启动客户端：**
```bash
./bin/client_demo -s 127.0.0.1 -p 40000 -t 2 -i 100 -l 1 -f client.log
```

#### 场景2：多检测器并发测试

**终端1 - 启动服务端：**
```bash
./bin/server_demo -p 40000 -l 1 -f server.log
```

**终端2-5 - 启动多个客户端：**
```bash
# 检测器1 - 线圈检测器
./bin/client_demo -s 127.0.0.1 -i 101 -t 1 -l 1 &

# 检测器2 - 地磁检测器
./bin/client_demo -s 127.0.0.1 -i 102 -t 2 -l 1 &

# 检测器3 - 视频检测器
./bin/client_demo -s 127.0.0.1 -i 103 -t 8 -l 1 &

# 检测器4 - 雷达检测器
./bin/client_demo -s 127.0.0.1 -i 104 -t 32 -l 1 &
```

## 协议功能验证

### 实时数据传输
客户端会自动发送模拟的交通流实时数据，包括：
- 各类车辆流量统计（A/B/C类车）
- 时间占有率
- 车辆速度、长度
- 车头时距、车间时距
- 停车统计信息

### 统计数据上报
每60秒自动上报统计数据，包括：
- 周期内车辆流量汇总
- 平均速度、占有率
- 平均车头时距等统计指标

### 心跳机制
- 服务端每5秒发送心跳查询
- 客户端立即响应心跳查询
- 15秒无响应自动断开连接

### 设备状态监控
定期上报设备工作状态：
- 各检测通道运行状态
- 设备异常告警信息

## 日志分析

### 查看连接状态
```bash
# 查看服务端日志
tail -f server.log | grep "Connected\|Disconnected"

# 查看客户端连接过程
./bin/client_demo -l 0  # DEBUG级别查看详细连接过程
```

### 监控数据传输
```bash
# 查看实时数据传输
tail -f server.log | grep "realtime\|statistics"

# 查看心跳状态
tail -f server.log | grep "heartbeat"
```

## 开发和调试

### 编译选项

```bash
# 编译调试版本
make CFLAGS="-Wall -Wextra -std=c99 -D_GNU_SOURCE -g -DDEBUG"

# 编译发布版本
make CFLAGS="-Wall -Wextra -std=c99 -D_GNU_SOURCE -O2 -DNDEBUG"

# 清理编译文件
make clean
```

### 调试技巧

1. **使用GDB调试：**
```bash
gdb ./bin/server_demo
(gdb) set args -p 40000 -l 0
(gdb) run
```

2. **网络抓包分析：**
```bash
# 抓取本地环回接口数据包
sudo tcpdump -i lo -X port 40000

# 使用Wireshark分析协议数据
wireshark -i lo -f "port 40000"
```

3. **内存检查：**
```bash
# 使用Valgrind检查内存泄漏
valgrind --leak-check=full ./bin/server_demo
valgrind --leak-check=full ./bin/client_demo
```

## 性能测试

### 并发连接测试
```bash
# 启动服务端
./bin/server_demo -p 40000 -l 2

# 批量启动100个客户端测试并发性能
for i in {1..100}; do
    ./bin/client_demo -i $i -l 3 &
done
```

### 数据传输压力测试
修改 `REALTIME_UPLOAD_INTERVAL` 和 `STATISTICS_UPLOAD_INTERVAL` 常量可以调整数据上报频率，测试高频数据传输性能。

## 常见问题

### Q1: 编译时出现"command not found"错误
**A:** 确保安装了GCC编译器：
```bash
# Ubuntu/Debian
sudo apt-get install build-essential

# CentOS/RHEL
sudo yum install gcc gcc-c++ make
```

### Q2: 客户端无法连接到服务器
**A:** 检查以下几点：
1. 服务端是否正常启动并监听端口
2. 防火墙是否开放对应端口
3. IP地址和端口配置是否正确

```bash
# 检查端口监听状态
netstat -tlnp | grep 40000

# 测试网络连通性
telnet 127.0.0.1 40000
```

### Q3: 程序运行时出现段错误
**A:** 使用调试版本和GDB定位问题：
```bash
make clean && make CFLAGS="-g -DDEBUG"
gdb ./bin/server_demo
(gdb) run
(gdb) bt  # 查看堆栈信息
```

### Q4: 修改模拟数据参数
**A:** 编辑 `src/client/vehicle_detector.c` 中的以下函数：
- `update_simulation_data()` - 修改数据更新逻辑
- `init_simulation_data()` - 修改初始数据设置

## 协议扩展

### 添加新的消息类型
1. 在 `src/common/protocol.h` 中定义新的对象标识符
2. 在服务端和客户端相应的消息处理函数中添加处理逻辑
3. 重新编译测试

### 自定义设备类型
修改 `DEVICE_TYPE_*` 常量定义，支持新的检测设备类型。

## 项目贡献

欢迎提交Issue和Pull Request来改进项目：
1. Fork 项目
2. 创建特性分支
3. 提交代码变更
4. 创建Pull Request

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 技术支持

如有技术问题，请：
1. 查看本README的常见问题部分
2. 检查项目的Issue列表
3. 创建新的Issue描述问题

---

**注意**: 本项目仅用于学习和测试目的，生产环境使用前请进行充分的测试和验证。