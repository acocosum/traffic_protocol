/**
 * @file vehicle_detector.h
 * @brief 车辆检测器客户端实现
 */

#ifndef VEHICLE_DETECTOR_H
#define VEHICLE_DETECTOR_H

#include "../common/protocol.h"
#include <time.h>

#define MAX_RETRY_COUNT 3       // 最大重试次数
#define CONNECT_RETRY_INTERVAL 5 // 连接重试间隔(秒)
#define REALTIME_UPLOAD_INTERVAL 2 // 实时数据上传间隔(秒)
#define STATISTICS_UPLOAD_INTERVAL 60 // 统计数据上传间隔(秒)

/**
 * @brief 车辆检测器结构体
 */
typedef struct {
    int sockfd;                 // socket文件描述符
    device_id_t device_id;      // 本机设备标识
    device_id_t server_id;      // 服务器设备标识
    char server_ip[16];         // 服务器IP地址
    int server_port;            // 服务器端口
    int connected;              // 连接状态
    int running;                // 运行状态标志
    
    // 时间管理
    time_t last_connect_try;    // 上次连接尝试时间
    time_t last_realtime_upload; // 上次实时数据上传时间
    time_t last_statistics_upload; // 上次统计数据上传时间
    time_t last_heartbeat;      // 上次收到心跳时间
    
    // 模拟数据
    traffic_realtime_t traffic_data[MAX_CHANNELS]; // 交通流实时数据
    channel_status_t channel_status[MAX_CHANNELS]; // 通道状态
    int active_channels;        // 活跃通道数
    
    // 统计数据
    uint32_t total_vehicles_a;  // A类车总数
    uint32_t total_vehicles_b;  // B类车总数
    uint32_t total_vehicles_c;  // C类车总数
    
} vehicle_detector_t;

/**
 * @brief 初始化车辆检测器
 * @param detector 检测器指针
 * @param admin_code 行政区划代码
 * @param device_type 设备类型
 * @param device_id 设备编号
 * @param server_ip 服务器IP地址
 * @param server_port 服务器端口
 * @return 0成功，-1失败
 */
int vehicle_detector_init(vehicle_detector_t *detector,
                         uint32_t admin_code, uint16_t device_type, uint16_t device_id,
                         const char *server_ip, int server_port);

/**
 * @brief 启动车辆检测器
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int vehicle_detector_start(vehicle_detector_t *detector);

/**
 * @brief 停止车辆检测器
 * @param detector 检测器指针
 */
void vehicle_detector_stop(vehicle_detector_t *detector);

/**
 * @brief 连接到服务器
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int detector_connect(vehicle_detector_t *detector);

/**
 * @brief 断开与服务器的连接
 * @param detector 检测器指针
 */
void detector_disconnect(vehicle_detector_t *detector);

/**
 * @brief 发送连接请求
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int send_connection_request(vehicle_detector_t *detector);

/**
 * @brief 处理服务器消息
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int handle_server_message(vehicle_detector_t *detector);

/**
 * @brief 发送心跳应答
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int send_heartbeat_response(vehicle_detector_t *detector);

/**
 * @brief 发送实时交通数据
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int send_realtime_traffic_data(vehicle_detector_t *detector);

/**
 * @brief 发送统计数据
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int send_statistics_data(vehicle_detector_t *detector);

/**
 * @brief 发送设备状态
 * @param detector 检测器指针
 * @return 0成功，-1失败
 */
int send_device_status(vehicle_detector_t *detector);

/**
 * @brief 更新模拟数据
 * @param detector 检测器指针
 */
void update_simulation_data(vehicle_detector_t *detector);

/**
 * @brief 初始化模拟数据
 * @param detector 检测器指针
 */
void init_simulation_data(vehicle_detector_t *detector);

/**
 * @brief 发送消息到服务器
 * @param detector 检测器指针
 * @param operation 操作类型
 * @param object_id 对象标识
 * @param content 消息内容
 * @param content_len 内容长度
 * @return 0成功，-1失败
 */
int send_message(vehicle_detector_t *detector, uint8_t operation, uint16_t object_id,
                const uint8_t *content, uint16_t content_len);

#endif // VEHICLE_DETECTOR_H