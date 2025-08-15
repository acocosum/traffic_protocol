/**
 * @file protocol.h
 * @brief GB/T 43229-2023 交通信号控制机与车辆检测器通信协议定义
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

// 协议常量定义
#define PROTOCOL_VERSION    0x10        // 协议版本号
#define FRAME_START         0xC0        // 帧开始标识
#define FRAME_END           0xC0        // 帧结束标识
#define ESCAPE_CHAR         0xDB        // 转义字符
#define ESCAPE_START        0xDC        // 转义帧开始/结束
#define ESCAPE_ESCAPE       0xDD        // 转义转义字符本身

#define MAX_FRAME_SIZE      2048        // 最大帧长度
#define MAX_CONTENT_SIZE    1500        // 最大消息内容长度
#define MAX_CHANNELS        128         // 最大检测通道数
#define DEVICE_ID_SIZE      7           // 设备标识长度
#define BROADCAST_ID        0xFFFF      // 广播设备ID

// 操作类型定义
#define OP_QUERY_REQUEST    0x80        // 查询请求
#define OP_SET_REQUEST      0x81        // 设置请求  
#define OP_UPLOAD           0x82        // 主动上传
#define OP_QUERY_RESPONSE   0x83        // 查询应答
#define OP_SET_RESPONSE     0x84        // 设置应答
#define OP_UPLOAD_RESPONSE  0x85        // 主动上传应答
#define OP_ERROR_RESPONSE   0x86        // 出错应答

// 对象标识定义
#define OBJ_COMMUNICATION      0x0101   // 通信连接
#define OBJ_DEVICE_TIME        0x0201   // 设备时间
#define OBJ_SERIAL_PARAMS      0x0202   // 串口通信参数
#define OBJ_ETHERNET_PARAMS    0x0203   // 以太网通信参数
#define OBJ_DETECTOR_CONFIG    0x0204   // 车辆检测器配置参数
#define OBJ_DETECTOR_STATUS    0x0205   // 车辆检测器工作状态
#define OBJ_TRAFFIC_REALTIME   0x0301   // 交通流实时信息
#define OBJ_TRAFFIC_STATS      0x0302   // 交通流统计数据
#define OBJ_TRAFFIC_HISTORY    0x0303   // 交通流历史数据
#define OBJ_PASSAGE_REALTIME   0x0401   // 通行状态实时信息
#define OBJ_PASSAGE_STATS      0x0402   // 通行状态统计数据
#define OBJ_PASSAGE_HISTORY    0x0403   // 通行状态历史数据
#define OBJ_VEHICLE_IDENTITY   0x0501   // 车辆身份信息
#define OBJ_ABNORMAL_EVENT     0x0601   // 异常事件信息
#define OBJ_ABNORMAL_HISTORY   0x0602   // 异常事件历史数据
#define OBJ_BICYCLE_REALTIME   0x0701   // 非机动车检测实时信息
#define OBJ_BICYCLE_STATS      0x0702   // 非机动车检测统计数据
#define OBJ_BICYCLE_HISTORY    0x0703   // 非机动车检测历史数据

// 设备类型位定义
#define DEVICE_TYPE_SIGNAL      (1 << 0)    // 交通信号控制机
#define DEVICE_TYPE_COIL        (1 << 1)    // 线圈检测
#define DEVICE_TYPE_MAGNETIC    (1 << 2)    // 地磁检测
#define DEVICE_TYPE_ULTRASONIC  (1 << 3)    // 超声波检测
#define DEVICE_TYPE_VIDEO       (1 << 4)    // 视频检测
#define DEVICE_TYPE_MICROWAVE   (1 << 5)    // 微波断面检测
#define DEVICE_TYPE_RADAR       (1 << 6)    // 多目标雷达检测
#define DEVICE_TYPE_RFID        (1 << 7)    // RFID检测

// 错误类型定义
#define ERROR_FRAME_START       1           // 帧开始错误
#define ERROR_FRAME_END         2           // 帧结束错误
#define ERROR_CRC               3           // 校验码错误
#define ERROR_LINK_ADDR         4           // 链路地址错误
#define ERROR_PROTOCOL_VERSION  5           // 协议版本不兼容
#define ERROR_OPERATION_TYPE    6           // 操作类型错误
#define ERROR_OBJECT_ID         7           // 对象标识错误
#define ERROR_CONTENT           128         // 消息内容错误

/**
 * @brief 设备标识结构体
 */
typedef struct {
    uint32_t admin_code;    // 行政区划代码 (24位有效)
    uint16_t device_type;   // 设备类型
    uint16_t device_id;     // 设备编号
} device_id_t;

/**
 * @brief 数据表结构体
 */
typedef struct {
    uint16_t link_addr;     // 链路地址 (保留为0x0000)
    device_id_t sender;     // 发送方标识
    device_id_t receiver;   // 接收方标识
    uint8_t protocol_ver;   // 协议版本
    uint8_t operation;      // 操作类型
    uint16_t object_id;     // 对象标识
    uint16_t content_len;   // 消息内容长度
    uint8_t *content;       // 消息内容指针
} data_table_t;

/**
 * @brief 完整协议帧结构体
 */
typedef struct {
    uint8_t frame_start;    // 帧开始标识
    data_table_t data;      // 数据表
    uint16_t crc;           // CRC校验码
    uint8_t frame_end;      // 帧结束标识
} protocol_frame_t;

/**
 * @brief 设备时间结构体
 */
typedef struct {
    uint32_t timestamp;     // 地方时秒值
    uint16_t milliseconds;  // 地方时毫秒值
    int32_t timezone_offset; // 与UTC的时差(秒)
} device_time_t;

/**
 * @brief 交通流实时信息结构体
 */
typedef struct {
    uint8_t channel_id;         // 检测通道编号
    uint8_t vehicle_count_a;    // A类车流量 (车长>=12m)
    uint8_t vehicle_count_b;    // B类车流量 (6m<=车长<12m)
    uint8_t vehicle_count_c;    // C类车流量 (车长<6m)
    uint16_t time_occupancy;    // 时间占有率 (0.1%精度)
    uint8_t vehicle_speed;      // 车辆速度 (km/h)
    uint16_t vehicle_length;    // 车辆长度 (0.1m精度)
    uint8_t headway;           // 车头时距 (0.1s精度)
    uint8_t gap_time;          // 车间时距 (0.1s精度)
    uint8_t stop_count;        // 停车次数 (0.1精度)
    uint8_t stop_duration;     // 停车时长 (0.1s精度)
    uint8_t occupy_sample_count; // 车辆占有采集次数
    uint8_t *occupy_info;      // 车辆占有信息 (动态分配)
} traffic_realtime_t;

/**
 * @brief 设备工作状态结构体
 */
typedef struct {
    uint8_t channel_id;     // 检测通道编号
    uint8_t status;         // 运行状态 (0:正常 1:异常)
} channel_status_t;

/**
 * @brief 协议处理结果枚举
 */
typedef enum {
    PROTOCOL_SUCCESS = 0,       // 成功
    PROTOCOL_ERROR_INVALID_PARAM,   // 无效参数
    PROTOCOL_ERROR_BUFFER_SMALL,    // 缓冲区太小
    PROTOCOL_ERROR_CRC,             // CRC校验失败
    PROTOCOL_ERROR_FORMAT,          // 格式错误
    PROTOCOL_ERROR_ESCAPE,          // 转义错误
    PROTOCOL_ERROR_INCOMPLETE       // 数据不完整
} protocol_result_t;

// 函数声明

/**
 * @brief 计算数据的CRC16校验码
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC16校验码
 */
uint16_t calculate_crc16(const uint8_t *data, size_t len);

/**
 * @brief 对数据进行转义编码
 * @param input 输入数据
 * @param input_len 输入数据长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 转义后的数据长度，-1表示错误
 */
int escape_data(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_size);

/**
 * @brief 对数据进行转义解码
 * @param input 输入数据
 * @param input_len 输入数据长度
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 解码后的数据长度，-1表示错误
 */
int unescape_data(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_size);

/**
 * @brief 将协议帧编码为字节流
 * @param frame 协议帧
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 编码后的字节数，-1表示错误
 */
int encode_frame(const protocol_frame_t *frame, uint8_t *buffer, size_t buffer_size);

/**
 * @brief 将字节流解码为协议帧
 * @param buffer 输入缓冲区
 * @param buffer_len 缓冲区长度
 * @param frame 输出协议帧
 * @return 协议处理结果
 */
protocol_result_t decode_frame(const uint8_t *buffer, size_t buffer_len, protocol_frame_t *frame);

/**
 * @brief 创建设备标识
 * @param admin_code 行政区划代码
 * @param device_type 设备类型
 * @param device_id 设备编号
 * @return 设备标识结构体
 */
device_id_t create_device_id(uint32_t admin_code, uint16_t device_type, uint16_t device_id);

/**
 * @brief 创建数据表
 * @param sender 发送方设备标识
 * @param receiver 接收方设备标识
 * @param operation 操作类型
 * @param object_id 对象标识
 * @param content 消息内容
 * @param content_len 消息内容长度
 * @return 数据表结构体
 */
data_table_t create_data_table(device_id_t sender, device_id_t receiver, 
                              uint8_t operation, uint16_t object_id,
                              const uint8_t *content, uint16_t content_len);

/**
 * @brief 创建错误应答帧
 * @param sender 发送方设备标识
 * @param receiver 接收方设备标识
 * @param error_type 错误类型
 * @return 协议帧结构体
 */
protocol_frame_t create_error_frame(device_id_t sender, device_id_t receiver, uint8_t error_type);

/**
 * @brief 释放协议帧占用的内存
 * @param frame 协议帧指针
 */
void free_frame(protocol_frame_t *frame);

/**
 * @brief 获取当前设备时间
 * @return 设备时间结构体
 */
device_time_t get_current_time(void);

/**
 * @brief 打印协议帧信息 (调试用)
 * @param frame 协议帧
 */
void print_frame_info(const protocol_frame_t *frame);

#endif // PROTOCOL_H