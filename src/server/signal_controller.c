/**
 * @file signal_controller.c
 * @brief 交通信号控制机实现
 */

#include "signal_controller.h"
#include "../utils/socket_utils.h"
#include "../utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

/**
 * @brief 初始化信号控制机
 */
int signal_controller_init(signal_controller_t *controller, 
                          uint32_t admin_code, uint16_t device_id, int port) {
    if (!controller) {
        LOG_ERROR("Controller pointer is NULL");
        return -1;
    }
    
    memset(controller, 0, sizeof(signal_controller_t));
    
    // 设置设备标识
    controller->device_id = create_device_id(admin_code, DEVICE_TYPE_SIGNAL, device_id);
    controller->port = port;
    controller->server_sockfd = -1;
    controller->client_count = 0;
    controller->running = 0;
    controller->last_heartbeat_check = time(NULL);
    
    // 初始化客户端数组
    for (int i = 0; i < MAX_CLIENTS; i++) {
        controller->clients[i].sockfd = -1;
        controller->clients[i].connected = 0;
    }
    
    LOG_INFO("Signal controller initialized - Admin: %06X, ID: %04X, Port: %d",
             admin_code, device_id, port);
    
    return 0;
}

/**
 * @brief 启动信号控制机服务
 */
int signal_controller_start(signal_controller_t *controller) {
    if (!controller) {
        LOG_ERROR("Controller pointer is NULL");
        return -1;
    }
    
    // 创建服务器socket
    controller->server_sockfd = create_tcp_server(controller->port);
    if (controller->server_sockfd < 0) {
        LOG_ERROR("Failed to create server socket");
        return -1;
    }
    
    controller->running = 1;
    LOG_INFO("Signal controller started on port %d", controller->port);
    
    // 主循环
    fd_set readfds;
    int max_fd;
    struct timeval timeout;
    
    while (controller->running) {
        // 准备select的文件描述符集合
        FD_ZERO(&readfds);
        FD_SET(controller->server_sockfd, &readfds);
        max_fd = controller->server_sockfd;
        
        // 添加客户端socket到监听集合
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (controller->clients[i].connected && controller->clients[i].sockfd > 0) {
                FD_SET(controller->clients[i].sockfd, &readfds);
                if (controller->clients[i].sockfd > max_fd) {
                    max_fd = controller->clients[i].sockfd;
                }
            }
        }
        
        // 设置超时时间为1秒
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            LOG_ERROR("Select error: %s", strerror(errno));
            break;
        }
        
        if (activity > 0) {
            // 处理新连接
            if (FD_ISSET(controller->server_sockfd, &readfds)) {
                handle_new_connection(controller);
            }
            
            // 处理客户端消息
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (controller->clients[i].connected && 
                    controller->clients[i].sockfd > 0 &&
                    FD_ISSET(controller->clients[i].sockfd, &readfds)) {
                    if (handle_client_message(controller, i) < 0) {
                        disconnect_client(controller, i);
                    }
                }
            }
        }
        
        // 定期发送心跳查询和检查超时
        time_t current_time = time(NULL);
        if (current_time - controller->last_heartbeat_check >= HEARTBEAT_INTERVAL) {
            // 发送心跳查询
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (controller->clients[i].connected) {
                    send_heartbeat_query(controller, i);
                }
            }
            
            // 检查心跳超时
            check_heartbeat_timeout(controller);
            controller->last_heartbeat_check = current_time;
        }
    }
    
    return 0;
}

/**
 * @brief 停止信号控制机服务
 */
void signal_controller_stop(signal_controller_t *controller) {
    if (!controller) {
        return;
    }
    
    controller->running = 0;
    
    // 关闭所有客户端连接
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (controller->clients[i].connected) {
            disconnect_client(controller, i);
        }
    }
    
    // 关闭服务器socket
    if (controller->server_sockfd >= 0) {
        close(controller->server_sockfd);
        controller->server_sockfd = -1;
    }
    
    LOG_INFO("Signal controller stopped");
}

/**
 * @brief 处理新客户端连接
 */
int handle_new_connection(signal_controller_t *controller) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    int client_sockfd = accept(controller->server_sockfd, 
                              (struct sockaddr*)&client_addr, &addr_len);
    if (client_sockfd < 0) {
        LOG_ERROR("Accept failed: %s", strerror(errno));
        return -1;
    }
    
    // 查找空闲的客户端槽位
    int client_idx = -1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!controller->clients[i].connected) {
            client_idx = i;
            break;
        }
    }
    
    if (client_idx < 0) {
        LOG_WARN("Too many clients, rejecting connection from %s",
                inet_ntoa(client_addr.sin_addr));
        close(client_sockfd);
        return -1;
    }
    
    // 初始化客户端信息
    controller->clients[client_idx].sockfd = client_sockfd;
    controller->clients[client_idx].connected = 1;
    controller->clients[client_idx].last_heartbeat = time(NULL);
    strcpy(controller->clients[client_idx].ip_addr, inet_ntoa(client_addr.sin_addr));
    
    controller->client_count++;
    
    LOG_INFO("New client connected from %s (slot %d), total clients: %d",
             controller->clients[client_idx].ip_addr, client_idx, controller->client_count);
    
    return 0;
}

/**
 * @brief 处理客户端消息
 */
int handle_client_message(signal_controller_t *controller, int client_idx) {
    uint8_t buffer[MAX_FRAME_SIZE];
    
    // 接收数据
    int recv_len = recv(controller->clients[client_idx].sockfd, buffer, sizeof(buffer), 0);
    if (recv_len <= 0) {
        if (recv_len == 0) {
            LOG_INFO("Client %d disconnected", client_idx);
        } else {
            LOG_ERROR("Recv error from client %d: %s", client_idx, strerror(errno));
        }
        return -1;
    }
    
    LOG_DEBUG("Received %d bytes from client %d", recv_len, client_idx);
    
    // 解析协议帧
    protocol_frame_t frame;
    protocol_result_t result = decode_frame(buffer, recv_len, &frame);
    if (result != PROTOCOL_SUCCESS) {
        LOG_WARN("Failed to decode frame from client %d, error: %d", client_idx, result);
        
        // 发送错误应答
        protocol_frame_t error_frame = create_error_frame(
            controller->device_id, 
            controller->clients[client_idx].device_id,
            ERROR_CRC
        );
        
        uint8_t error_buffer[MAX_FRAME_SIZE];
        int error_len = encode_frame(&error_frame, error_buffer, sizeof(error_buffer));
        if (error_len > 0) {
            send_all(controller->clients[client_idx].sockfd, error_buffer, error_len);
        }
        free_frame(&error_frame);
        return 0;
    }
    
    // 根据对象标识处理不同类型的消息
    switch (frame.data.object_id) {
        case OBJ_COMMUNICATION:
            if (frame.data.operation == OP_SET_REQUEST) {
                handle_connection_request(controller, client_idx, &frame);
            } else if (frame.data.operation == OP_QUERY_RESPONSE) {
                handle_heartbeat_response(controller, client_idx, &frame);
            }
            break;
            
        case OBJ_TRAFFIC_REALTIME:
            if (frame.data.operation == OP_UPLOAD) {
                handle_realtime_data(controller, client_idx, &frame);
            }
            break;
            
        case OBJ_TRAFFIC_STATS:
            if (frame.data.operation == OP_UPLOAD) {
                handle_statistics_data(controller, client_idx, &frame);
            }
            break;
            
        case OBJ_DETECTOR_STATUS:
            if (frame.data.operation == OP_UPLOAD) {
                LOG_INFO("Received device status from client %d", client_idx);
                send_response(controller, client_idx, OP_UPLOAD_RESPONSE, 
                             OBJ_DETECTOR_STATUS, NULL, 0);
            }
            break;
            
        default:
            LOG_WARN("Unknown object ID 0x%04X from client %d", 
                    frame.data.object_id, client_idx);
            break;
    }
    
    free_frame(&frame);
    return 0;
}

/**
 * @brief 处理连接请求
 */
int handle_connection_request(signal_controller_t *controller, int client_idx, 
                             const protocol_frame_t *frame) {
    // 保存客户端设备标识
    controller->clients[client_idx].device_id = frame->data.sender;
    controller->clients[client_idx].last_heartbeat = time(NULL);
    
    LOG_INFO("Connection request from device Admin=%06X, Type=%04X, ID=%04X",
             frame->data.sender.admin_code,
             frame->data.sender.device_type,
             frame->data.sender.device_id);
    
    // 发送连接应答
    return send_response(controller, client_idx, OP_SET_RESPONSE, 
                        OBJ_COMMUNICATION, NULL, 0);
}

/**
 * @brief 处理心跳应答
 */
int handle_heartbeat_response(signal_controller_t *controller, int client_idx, 
                             const protocol_frame_t *frame) {
    // 消除未使用参数警告
    (void)frame;
    
    controller->clients[client_idx].last_heartbeat = time(NULL);
    LOG_DEBUG("Heartbeat response from client %d", client_idx);
    return 0;
}

/**
 * @brief 处理实时数据上传
 */
int handle_realtime_data(signal_controller_t *controller, int client_idx, 
                        const protocol_frame_t *frame) {
    // 消除未使用参数警告
    (void)controller;
    
    LOG_INFO("Received realtime traffic data from client %d, size: %d bytes",
             client_idx, frame->data.content_len);
    
    // 这里可以添加数据处理逻辑
    // 例如：解析交通流数据，存储到数据库等
    
    // 实时数据不需要应答
    return 0;
}

/**
 * @brief 处理统计数据上传
 */
int handle_statistics_data(signal_controller_t *controller, int client_idx, 
                          const protocol_frame_t *frame) {
    LOG_INFO("Received statistics data from client %d, size: %d bytes",
             client_idx, frame->data.content_len);
    
    // 这里可以添加统计数据处理逻辑
    
    // 统计数据需要应答
    return send_response(controller, client_idx, OP_UPLOAD_RESPONSE, 
                        frame->data.object_id, NULL, 0);
}

/**
 * @brief 发送心跳查询
 */
int send_heartbeat_query(signal_controller_t *controller, int client_idx) {
    return send_response(controller, client_idx, OP_QUERY_REQUEST, 
                        OBJ_COMMUNICATION, NULL, 0);
}

/**
 * @brief 检查客户端心跳超时
 */
void check_heartbeat_timeout(signal_controller_t *controller) {
    time_t current_time = time(NULL);
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (controller->clients[i].connected) {
            if (current_time - controller->clients[i].last_heartbeat > HEARTBEAT_TIMEOUT) {
                LOG_WARN("Client %d heartbeat timeout, disconnecting", i);
                disconnect_client(controller, i);
            }
        }
    }
}

/**
 * @brief 断开客户端连接
 */
void disconnect_client(signal_controller_t *controller, int client_idx) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS) {
        return;
    }
    
    if (controller->clients[client_idx].connected) {
        close(controller->clients[client_idx].sockfd);
        controller->clients[client_idx].sockfd = -1;
        controller->clients[client_idx].connected = 0;
        controller->client_count--;
        
        LOG_INFO("Client %d disconnected, remaining clients: %d", 
                client_idx, controller->client_count);
    }
}

/**
 * @brief 发送应答消息
 */
int send_response(signal_controller_t *controller, int client_idx,
                 uint8_t operation, uint16_t object_id,
                 const uint8_t *content, uint16_t content_len) {
    if (client_idx < 0 || client_idx >= MAX_CLIENTS || 
        !controller->clients[client_idx].connected) {
        return -1;
    }
    
    // 创建应答帧
    data_table_t data_table = create_data_table(
        controller->device_id,
        controller->clients[client_idx].device_id,
        operation,
        object_id,
        content,
        content_len
    );
    
    protocol_frame_t frame;
    frame.frame_start = FRAME_START;
    frame.frame_end = FRAME_END;
    frame.data = data_table;
    
    // 编码并发送
    uint8_t buffer[MAX_FRAME_SIZE];
    int frame_len = encode_frame(&frame, buffer, sizeof(buffer));
    
    int result = -1;
    if (frame_len > 0) {
        result = send_all(controller->clients[client_idx].sockfd, buffer, frame_len);
        if (result > 0) {
            LOG_DEBUG("Sent response to client %d: op=0x%02X, obj=0x%04X, len=%d",
                     client_idx, operation, object_id, frame_len);
        } else {
            LOG_ERROR("Failed to send response to client %d", client_idx);
        }
    }
    
    free_frame(&frame);
    return result > 0 ? 0 : -1;
}