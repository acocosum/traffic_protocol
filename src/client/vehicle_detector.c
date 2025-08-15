/**
 * @file vehicle_detector.c
 * @brief 车辆检测器客户端实现 - 修复日志输出问题
 */

 #include "vehicle_detector.h"
 #include "../utils/socket_utils.h"
 #include "../utils/logger.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <unistd.h>
 #include <sys/select.h>
 #include <errno.h>
 
 /**
  * @brief 初始化车辆检测器
  */
 int vehicle_detector_init(vehicle_detector_t *detector,
                          uint32_t admin_code, uint16_t device_type, uint16_t device_id,
                          const char *server_ip, int server_port) {
     if (!detector || !server_ip) {
         LOG_ERROR("Invalid parameters");
         return -1;
     }
     
     memset(detector, 0, sizeof(vehicle_detector_t));
     
     // 设置设备标识
     detector->device_id = create_device_id(admin_code, device_type, device_id);
     detector->server_id = create_device_id(admin_code, DEVICE_TYPE_SIGNAL, 1);
     
     // 设置服务器信息 - 安全的字符串复制
     size_t ip_len = strlen(server_ip);
     if (ip_len >= sizeof(detector->server_ip)) {
         LOG_ERROR("Server IP address too long");
         return -1;
     }
     strcpy(detector->server_ip, server_ip);
     detector->server_port = server_port;
     
     detector->sockfd = -1;
     detector->connected = 0;
     detector->running = 0;
     detector->active_channels = 4;
     
     // 初始化时间
     detector->last_connect_try = 0;
     detector->last_realtime_upload = 0;
     detector->last_statistics_upload = 0;
     detector->last_heartbeat = 0;
     
     // 初始化模拟数据
     init_simulation_data(detector);
     
     LOG_INFO("Vehicle detector initialized - Admin: %06X, Type: %04X, ID: %04X",
              admin_code, device_type, device_id);
     LOG_INFO("Server: %s:%d", server_ip, server_port);
     
     return 0;
 }
 
 /**
  * @brief 启动车辆检测器
  */
 int vehicle_detector_start(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     detector->running = 1;
     LOG_INFO("Starting vehicle detector...");
     LOG_INFO("Vehicle detector starting...");
     
     // 主循环
     fd_set readfds;
     struct timeval timeout;
     time_t current_time;
     
     while (detector->running) {
         current_time = time(NULL);
         
         // 检查连接状态
         if (!detector->connected) {
             if (current_time - detector->last_connect_try >= CONNECT_RETRY_INTERVAL) {
                 if (detector_connect(detector) == 0) {
                     LOG_INFO("Connected to server successfully");
                     if (send_connection_request(detector) < 0) {
                         LOG_ERROR("Failed to send connection request");
                         detector_disconnect(detector);
                         detector->last_connect_try = current_time;
                         continue;
                     }
                 }
                 detector->last_connect_try = current_time;
             }
         } else {
             // 检查心跳超时
             if (current_time - detector->last_heartbeat > 15) {
                 LOG_WARN("Heartbeat timeout, disconnecting from server");
                 detector_disconnect(detector);
                 continue;
             }
         }
         
         if (detector->connected) {
             // 准备select
             FD_ZERO(&readfds);
             FD_SET(detector->sockfd, &readfds);
             
             timeout.tv_sec = 1;
             timeout.tv_usec = 0;
             
             int activity = select(detector->sockfd + 1, &readfds, NULL, NULL, &timeout);
             
             if (activity < 0 && errno != EINTR) {
                 LOG_ERROR("Select error: %s", strerror(errno));
                 detector_disconnect(detector);
                 continue;
             }
             
             // 处理服务器消息
             if (activity > 0 && FD_ISSET(detector->sockfd, &readfds)) {
                 if (handle_server_message(detector) < 0) {
                     detector_disconnect(detector);
                     continue;
                 }
             }
             
             // 更新模拟数据
             update_simulation_data(detector);
             
             // 定期发送实时数据
             if (current_time - detector->last_realtime_upload >= REALTIME_UPLOAD_INTERVAL) {
                 if (send_realtime_traffic_data(detector) < 0) {
                     LOG_ERROR("Failed to send realtime data");
                 }
                 detector->last_realtime_upload = current_time;
             }
             
             // 定期发送统计数据
             if (current_time - detector->last_statistics_upload >= STATISTICS_UPLOAD_INTERVAL) {
                 if (send_statistics_data(detector) < 0) {
                     LOG_ERROR("Failed to send statistics data");
                 }
                 detector->last_statistics_upload = current_time;
             }
         }
         
         // 短暂休眠
         usleep(100000); // 100ms
     }
     
     detector_disconnect(detector);
     LOG_INFO("Vehicle detector stopped");
     return 0;
 }
 
 /**
  * @brief 停止车辆检测器
  */
 void vehicle_detector_stop(vehicle_detector_t *detector) {
     if (detector) {
         detector->running = 0;
         detector_disconnect(detector);
     }
 }
 
 /**
  * @brief 连接到服务器
  */
 int detector_connect(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     if (detector->connected) {
         return 0;
     }
     
     detector->sockfd = create_tcp_client(detector->server_ip, detector->server_port);
     if (detector->sockfd < 0) {
         LOG_DEBUG("Failed to connect to server %s:%d", 
                  detector->server_ip, detector->server_port);
         return -1;
     }
     
     detector->connected = 1;
     detector->last_heartbeat = time(NULL);
     LOG_INFO("Connected to server %s:%d", detector->server_ip, detector->server_port);
     
     return 0;
 }
 
 /**
  * @brief 断开与服务器的连接
  */
 void detector_disconnect(vehicle_detector_t *detector) {
     if (!detector) {
         return;
     }
     
     if (detector->connected && detector->sockfd >= 0) {
         close(detector->sockfd);
         detector->sockfd = -1;
         detector->connected = 0;
         LOG_INFO("Disconnected from server");
     }
 }
 
 /**
  * @brief 发送连接请求
  */
 int send_connection_request(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     LOG_INFO("Sending connection request to server");
     return send_message(detector, OP_SET_REQUEST, OBJ_COMMUNICATION, NULL, 0);
 }
 
 /**
  * @brief 处理服务器消息
  */
 int handle_server_message(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     uint8_t buffer[MAX_FRAME_SIZE];
     
     int recv_len = recv(detector->sockfd, buffer, sizeof(buffer), 0);
     if (recv_len <= 0) {
         if (recv_len == 0) {
             LOG_INFO("Server disconnected");
         } else {
             LOG_ERROR("Recv error: %s", strerror(errno));
         }
         return -1;
     }
     
     LOG_DEBUG("Received %d bytes from server", recv_len);
     
     // 解析协议帧
     protocol_frame_t frame;
     protocol_result_t result = decode_frame(buffer, recv_len, &frame);
     if (result != PROTOCOL_SUCCESS) {
         LOG_WARN("Failed to decode frame from server, error: %d", result);
         return 0;
     }
     
     // 处理不同类型的消息
     switch (frame.data.object_id) {
         case OBJ_COMMUNICATION:
             if (frame.data.operation == OP_SET_RESPONSE) {
                 LOG_INFO("Connection request accepted by server");
             } else if (frame.data.operation == OP_QUERY_REQUEST) {
                 LOG_DEBUG("Received heartbeat query from server");
                 send_heartbeat_response(detector);
             }
             detector->last_heartbeat = time(NULL);
             break;
             
         case OBJ_TRAFFIC_STATS:
             if (frame.data.operation == OP_UPLOAD_RESPONSE) {
                 LOG_DEBUG("Statistics data upload acknowledged");
             }
             break;
             
         case OBJ_DETECTOR_STATUS:
             if (frame.data.operation == OP_UPLOAD_RESPONSE) {
                 LOG_DEBUG("Device status upload acknowledged");
             }
             break;
             
         default:
             LOG_DEBUG("Received message with object ID 0x%04X", frame.data.object_id);
             break;
     }
     
     free_frame(&frame);
     return 0;
 }
 
 /**
  * @brief 发送心跳应答
  */
 int send_heartbeat_response(vehicle_detector_t *detector) {
     LOG_DEBUG("Sending heartbeat response to server");
     return send_message(detector, OP_QUERY_RESPONSE, OBJ_COMMUNICATION, NULL, 0);
 }
 
 /**
  * @brief 发送实时交通数据
  */
 int send_realtime_traffic_data(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     // 构建实时交通数据包
     uint8_t content[MAX_CONTENT_SIZE];
     size_t content_len = 0;
     
     // 检查缓冲区大小
     if (content_len + 6 >= MAX_CONTENT_SIZE) {
         LOG_ERROR("Content buffer too small for realtime data");
         return -1;
     }
     
     // 添加时间戳 (6字节)
     device_time_t current_time = get_current_time();
     content[content_len++] = current_time.timestamp & 0xFF;
     content[content_len++] = (current_time.timestamp >> 8) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 16) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 24) & 0xFF;
     content[content_len++] = current_time.milliseconds & 0xFF;
     content[content_len++] = (current_time.milliseconds >> 8) & 0xFF;
     
     // 添加检测通道数
     if (content_len >= MAX_CONTENT_SIZE) {
         LOG_ERROR("Content buffer overflow");
         return -1;
     }
     content[content_len++] = detector->active_channels;
     
     // 添加各通道的实时数据
     for (int i = 0; i < detector->active_channels; i++) {
         // 检查剩余缓冲区空间 (每个通道需要大约20字节)
         if (content_len + 20 >= MAX_CONTENT_SIZE) {
             LOG_ERROR("Content buffer too small for channel %d", i);
             break;
         }
         
         traffic_realtime_t *data = &detector->traffic_data[i];
         
         content[content_len++] = data->channel_id;
         content[content_len++] = data->vehicle_count_a;
         content[content_len++] = data->vehicle_count_b;
         content[content_len++] = data->vehicle_count_c;
         
         // 时间占有率 (2字节)
         content[content_len++] = data->time_occupancy & 0xFF;
         content[content_len++] = (data->time_occupancy >> 8) & 0xFF;
         
         content[content_len++] = data->vehicle_speed;
         
         // 车辆长度 (2字节)
         content[content_len++] = data->vehicle_length & 0xFF;
         content[content_len++] = (data->vehicle_length >> 8) & 0xFF;
         
         content[content_len++] = data->headway;
         content[content_len++] = data->gap_time;
         content[content_len++] = data->stop_count;
         content[content_len++] = data->stop_duration;
         content[content_len++] = data->occupy_sample_count;
         
         // 车辆占有信息 (简化处理，只添加1字节)
         content[content_len++] = 0x55; // 模拟占有模式
         
         // 保留字节 (4字节)
         content[content_len++] = 0;
         content[content_len++] = 0;
         content[content_len++] = 0;
         content[content_len++] = 0;
     }
     
     LOG_DEBUG("Sending realtime traffic data (%zu bytes)", content_len);
     return send_message(detector, OP_UPLOAD, OBJ_TRAFFIC_REALTIME, content, content_len);
 }
 
 /**
  * @brief 发送统计数据
  */
 int send_statistics_data(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     uint8_t content[MAX_CONTENT_SIZE];
     size_t content_len = 0;
     
     device_time_t current_time = get_current_time();
     
     // 检查缓冲区大小
     if (content_len + 13 >= MAX_CONTENT_SIZE) {
         LOG_ERROR("Content buffer too small for statistics header");
         return -1;
     }
     
     // 统计起始时间 (6字节)
     uint32_t start_time = current_time.timestamp - STATISTICS_UPLOAD_INTERVAL;
     content[content_len++] = start_time & 0xFF;
     content[content_len++] = (start_time >> 8) & 0xFF;
     content[content_len++] = (start_time >> 16) & 0xFF;
     content[content_len++] = (start_time >> 24) & 0xFF;
     content[content_len++] = 0; // 毫秒保留
     content[content_len++] = 0;
     
     // 统计结束时间 (6字节)
     content[content_len++] = current_time.timestamp & 0xFF;
     content[content_len++] = (current_time.timestamp >> 8) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 16) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 24) & 0xFF;
     content[content_len++] = 0; // 毫秒保留
     content[content_len++] = 0;
     
     // 检测通道数
     content[content_len++] = detector->active_channels;
     
     // 各通道统计数据
     for (int i = 0; i < detector->active_channels; i++) {
         // 检查剩余缓冲区空间 (每个通道需要大约20字节)
         if (content_len + 20 >= MAX_CONTENT_SIZE) {
             LOG_ERROR("Content buffer too small for statistics channel %d", i);
             break;
         }
         
         content[content_len++] = i + 1; // 通道编号
         
         // A类车总流量 (2字节)
         uint16_t total_a = detector->total_vehicles_a;
         content[content_len++] = total_a & 0xFF;
         content[content_len++] = (total_a >> 8) & 0xFF;
         
         // B类车总流量 (2字节)
         uint16_t total_b = detector->total_vehicles_b;
         content[content_len++] = total_b & 0xFF;
         content[content_len++] = (total_b >> 8) & 0xFF;
         
         // C类车总流量 (2字节)
         uint16_t total_c = detector->total_vehicles_c;
         content[content_len++] = total_c & 0xFF;
         content[content_len++] = (total_c >> 8) & 0xFF;
         
         // 平均时间占有率 (2字节)
         uint16_t avg_occupancy = detector->traffic_data[i].time_occupancy;
         content[content_len++] = avg_occupancy & 0xFF;
         content[content_len++] = (avg_occupancy >> 8) & 0xFF;
         
         // 平均车辆速度
         content[content_len++] = detector->traffic_data[i].vehicle_speed;
         
         // 平均车辆长度 (2字节)
         content[content_len++] = detector->traffic_data[i].vehicle_length & 0xFF;
         content[content_len++] = (detector->traffic_data[i].vehicle_length >> 8) & 0xFF;
         
         // 平均车头时距
         content[content_len++] = detector->traffic_data[i].headway;
         
         // 平均车间时距
         content[content_len++] = detector->traffic_data[i].gap_time;
         
         // 平均停车次数
         content[content_len++] = detector->traffic_data[i].stop_count;
         
         // 平均停车时长
         content[content_len++] = detector->traffic_data[i].stop_duration;
         
         // 保留字节 (4字节)
         content[content_len++] = 0;
         content[content_len++] = 0;
         content[content_len++] = 0;
         content[content_len++] = 0;
     }
     
     LOG_INFO("Sending statistics data (%zu bytes)", content_len);
     return send_message(detector, OP_UPLOAD, OBJ_TRAFFIC_STATS, content, content_len);
 }
 
 /**
  * @brief 发送设备状态
  */
 int send_device_status(vehicle_detector_t *detector) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     uint8_t content[MAX_CONTENT_SIZE];
     size_t content_len = 0;
     
     // 检查缓冲区大小
     if (content_len + 7 >= MAX_CONTENT_SIZE) {
         LOG_ERROR("Content buffer too small for device status header");
         return -1;
     }
     
     // 设备状态生成时间 (6字节)
     device_time_t current_time = get_current_time();
     content[content_len++] = current_time.timestamp & 0xFF;
     content[content_len++] = (current_time.timestamp >> 8) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 16) & 0xFF;
     content[content_len++] = (current_time.timestamp >> 24) & 0xFF;
     content[content_len++] = current_time.milliseconds & 0xFF;
     content[content_len++] = (current_time.milliseconds >> 8) & 0xFF;
     
     // 检测器通道数
     content[content_len++] = detector->active_channels;
     
     // 各通道状态
     for (int i = 0; i < detector->active_channels; i++) {
         // 检查剩余缓冲区空间
         if (content_len + 4 >= MAX_CONTENT_SIZE) {
             LOG_ERROR("Content buffer too small for channel %d status", i);
             break;
         }
         
         content[content_len++] = i + 1; // 通道编号
         content[content_len++] = detector->channel_status[i].status; // 运行状态
         content[content_len++] = 0; // 保留字节
         content[content_len++] = 0; // 保留字节
     }
     
     LOG_INFO("Sending device status (%zu bytes)", content_len);
     return send_message(detector, OP_UPLOAD, OBJ_DETECTOR_STATUS, content, content_len);
 }
 
 /**
  * @brief 更新模拟数据
  */
 void update_simulation_data(vehicle_detector_t *detector) {
     if (!detector) {
         return;
     }
     
     static time_t last_update = 0;
     time_t current_time = time(NULL);
     
     if (current_time - last_update < 1) {
         return; // 每秒更新一次
     }
     last_update = current_time;
     
     for (int i = 0; i < detector->active_channels; i++) {
         traffic_realtime_t *data = &detector->traffic_data[i];
         
         // 模拟车流量变化
         data->vehicle_count_a = rand() % 3;
         data->vehicle_count_b = rand() % 5;
         data->vehicle_count_c = rand() % 8;
         
         // 累计统计
         detector->total_vehicles_a += data->vehicle_count_a;
         detector->total_vehicles_b += data->vehicle_count_b;
         detector->total_vehicles_c += data->vehicle_count_c;
         
         // 模拟其他参数变化
         data->time_occupancy = 200 + rand() % 300; // 20%-50%
         data->vehicle_speed = 30 + rand() % 41;     // 30-70 km/h
         data->vehicle_length = 40 + rand() % 80;    // 4-12m
         data->headway = 15 + rand() % 20;           // 1.5-3.5s
         data->gap_time = 10 + rand() % 15;          // 1.0-2.5s
         
         // 随机设备状态 (99%正常)
         detector->channel_status[i].status = (rand() % 100) < 99 ? 0 : 1;
     }
 }
 
 /**
  * @brief 初始化模拟数据
  */
 void init_simulation_data(vehicle_detector_t *detector) {
     if (!detector) {
         return;
     }
     
     srand(time(NULL)); // 初始化随机数种子
     
     for (int i = 0; i < detector->active_channels; i++) {
         traffic_realtime_t *data = &detector->traffic_data[i];
         channel_status_t *status = &detector->channel_status[i];
         
         data->channel_id = i + 1;
         data->vehicle_count_a = 0;
         data->vehicle_count_b = 0;
         data->vehicle_count_c = 0;
         data->time_occupancy = 250; // 25%
         data->vehicle_speed = 50;   // 50 km/h
         data->vehicle_length = 60;  // 6m
         data->headway = 20;         // 2s
         data->gap_time = 15;        // 1.5s
         data->stop_count = 0;
         data->stop_duration = 0;
         data->occupy_sample_count = 10;
         data->occupy_info = NULL;
         
         status->channel_id = i + 1;
         status->status = 0; // 正常
     }
     
     detector->total_vehicles_a = 0;
     detector->total_vehicles_b = 0;
     detector->total_vehicles_c = 0;
     
     LOG_INFO("Simulation data initialized for %d channels", detector->active_channels);
 }
 
 /**
  * @brief 发送消息到服务器
  */
 int send_message(vehicle_detector_t *detector, uint8_t operation, uint16_t object_id,
                 const uint8_t *content, uint16_t content_len) {
     if (!detector) {
         LOG_ERROR("Detector pointer is NULL");
         return -1;
     }
     
     if (!detector->connected) {
         LOG_ERROR("Not connected to server");
         return -1;
     }
     
     // 创建数据表
     data_table_t data_table = create_data_table(
         detector->device_id,
         detector->server_id,
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
         result = send_all(detector->sockfd, buffer, frame_len);
         if (result > 0) {
             LOG_DEBUG("Sent message: op=0x%02X, obj=0x%04X, len=%d",
                      operation, object_id, frame_len);
         } else {
             LOG_ERROR("Failed to send message to server");
         }
     } else {
         LOG_ERROR("Failed to encode frame");
     }
     
     free_frame(&frame);
     return result > 0 ? 0 : -1;
 }