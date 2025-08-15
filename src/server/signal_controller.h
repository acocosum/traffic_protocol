/**
 * @file signal_controller.h
 * @brief 交通信号控制机实现
 */

#ifndef SIGNAL_CONTROLLER_H
#define SIGNAL_CONTROLLER_H

#include "../common/protocol.h"
#include <time.h>

#define MAX_CLIENTS 64          // 最大客户端连接数
#define HEARTBEAT_INTERVAL 5    // 心跳间隔(秒)
#define HEARTBEAT_TIMEOUT 15    // 心跳超时(秒)
#define DEFAULT_PORT 40000      // 默认端口

/**
 * @brief 客户端连接信息结构体
 */
typedef struct {
    int sockfd;                 // socket文件描述符
    device_id_t device_id;      // 客户端设备标识
    time_t last_heartbeat;      // 最后心跳时间
    int connected;              // 连接状态
    char ip_addr[16];           // 客户端IP地址
} client_info_t;

/**
 * @brief 信号控制机结构体
 */
typedef struct {
    int server_sockfd;          // 服务器socket
    device_id_t device_id;      // 本机设备标识
    int port;                   // 监听端口
    client_info_t clients[MAX_CLIENTS]; // 客户端连接数组
    int client_count;           // 当前客户端数量
    int running;                // 运行状态标志
    time_t last_heartbeat_check; // 上次心跳检查时间
} signal_controller_t;

/**
 * @brief 初始化信号控制机
 * @param controller 控制机指针
 * @param admin_code 行政区划代码
 * @param device_id 设备编号
 * @param port 监听端口
 * @return 0成功，-1失败
 */
int signal_controller_init(signal_controller_t *controller, 
                          uint32_t admin_code, uint16_t device_id, int port);

/**
 * @brief 启动信号控制机服务
 * @param controller 控制机指针
 * @return 0成功，-1失败
 */
int signal_controller_start(signal_controller_t *controller);

/**
 * @brief 停止信号控制机服务
 * @param controller 控制机指针
 */
void signal_controller_stop(signal_controller_t *controller);

/**
 * @brief 处理客户端连接
 * @param controller 控制机指针
 * @return 0成功，-1失败
 */
int handle_new_connection(signal_controller_t *controller);

/**
 * @brief 处理客户端消息
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @return 0成功，-1失败
 */
int handle_client_message(signal_controller_t *controller, int client_idx);

/**
 * @brief 发送心跳查询
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @return 0成功，-1失败
 */
int send_heartbeat_query(signal_controller_t *controller, int client_idx);

/**
 * @brief 检查客户端心跳超时
 * @param controller 控制机指针
 */
void check_heartbeat_timeout(signal_controller_t *controller);

/**
 * @brief 断开客户端连接
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 */
void disconnect_client(signal_controller_t *controller, int client_idx);

/**
 * @brief 处理连接请求
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @param frame 协议帧
 * @return 0成功，-1失败
 */
int handle_connection_request(signal_controller_t *controller, int client_idx, 
                             const protocol_frame_t *frame);

/**
 * @brief 处理心跳应答
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @param frame 协议帧
 * @return 0成功，-1失败
 */
int handle_heartbeat_response(signal_controller_t *controller, int client_idx, 
                             const protocol_frame_t *frame);

/**
 * @brief 处理实时数据上传
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @param frame 协议帧
 * @return 0成功，-1失败
 */
int handle_realtime_data(signal_controller_t *controller, int client_idx, 
                        const protocol_frame_t *frame);

/**
 * @brief 处理统计数据上传
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @param frame 协议帧
 * @return 0成功，-1失败
 */
int handle_statistics_data(signal_controller_t *controller, int client_idx, 
                          const protocol_frame_t *frame);

/**
 * @brief 发送应答消息
 * @param controller 控制机指针
 * @param client_idx 客户端索引
 * @param operation 操作类型
 * @param object_id 对象标识
 * @param content 消息内容
 * @param content_len 内容长度
 * @return 0成功，-1失败
 */
int send_response(signal_controller_t *controller, int client_idx,
                 uint8_t operation, uint16_t object_id,
                 const uint8_t *content, uint16_t content_len);

#endif // SIGNAL_CONTROLLER_H