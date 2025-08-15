/**
 * @file client_demo.c
 * @brief 车辆检测器演示程序
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <signal.h>
 #include <unistd.h>
 #include "client/vehicle_detector.h"
 #include "utils/logger.h"
 
 static vehicle_detector_t *g_detector = NULL;
 
 /**
  * @brief 信号处理函数 - 优雅关闭程序
  */
 void signal_handler(int sig) {
     printf("\nReceived signal %d, shutting down...\n", sig);
     if (g_detector) {
         vehicle_detector_stop(g_detector);
     }
     exit(0);
 }
 
 /**
  * @brief 显示使用帮助
  */
 void show_usage(const char *program_name) {
     printf("Usage: %s [options]\n", program_name);
     printf("Options:\n");
     printf("  -s <server>   Server IP address (default: 127.0.0.1)\n");
     printf("  -p <port>     Server port (default: 40000)\n");
     printf("  -a <admin>    Admin code (default: 110100)\n");
     printf("  -t <type>     Device type (default: 2 for coil detector)\n");
     printf("  -i <id>       Device ID (default: 100)\n");
     printf("  -l <level>    Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR) (default: 1)\n");
     printf("  -f <file>     Log file (default: console only)\n");
     printf("  -h            Show this help\n");
     printf("\nDevice Types:\n");
     printf("  1  - Coil detector\n");
     printf("  2  - Magnetic detector\n");
     printf("  4  - Ultrasonic detector\n");
     printf("  8  - Video detector\n");
     printf("  16 - Microwave detector\n");
     printf("  32 - Multi-target radar\n");
     printf("  64 - RFID detector\n");
     printf("\nExample:\n");
     printf("  %s -s 127.0.0.1 -p 40000 -a 110100 -t 2 -i 100\n", program_name);
 }
 
 int main(int argc, char *argv[]) {
     char server_ip[16] = "127.0.0.1";
     int server_port = 40000;
     uint32_t admin_code = 110100; // 北京市东城区
     uint16_t device_type = DEVICE_TYPE_COIL; // 线圈检测器
     uint16_t device_id = 100;
     log_level_t log_level = LOG_LEVEL_INFO;
     char *log_file = NULL;
     
     // 解析命令行参数
     int opt;
     while ((opt = getopt(argc, argv, "s:p:a:t:i:l:f:h")) != -1) {
         switch (opt) {
             case 's':
                 strncpy(server_ip, optarg, sizeof(server_ip) - 1);
                 server_ip[sizeof(server_ip) - 1] = '\0'; // 确保字符串结束
                 break;
             case 'p':
                 server_port = atoi(optarg);
                 if (server_port <= 0 || server_port > 65535) {
                     fprintf(stderr, "Invalid port: %s\n", optarg);
                     return 1;
                 }
                 break;
             case 'a':
                 admin_code = strtoul(optarg, NULL, 10);
                 if (admin_code > 0xFFFFFF) {
                     fprintf(stderr, "Invalid admin code: %s\n", optarg);
                     return 1;
                 }
                 break;
             case 't':
                 device_type = atoi(optarg);
                 if (device_type == 0 || device_type > 255) {
                     fprintf(stderr, "Invalid device type: %s\n", optarg);
                     return 1;
                 }
                 break;
             case 'i':
                 device_id = atoi(optarg);
                 if (device_id == 0) {
                     fprintf(stderr, "Invalid device ID: %s\n", optarg);
                     return 1;
                 }
                 break;
             case 'l':
                 log_level = atoi(optarg);
                 if (log_level < 0 || log_level > 3) {
                     fprintf(stderr, "Invalid log level: %s\n", optarg);
                     return 1;
                 }
                 break;
             case 'f':
                 log_file = optarg;
                 break;
             case 'h':
                 show_usage(argv[0]);
                 return 0;
             default:
                 show_usage(argv[0]);
                 return 1;
         }
     }
     
     // 初始化日志系统
     if (logger_init(log_level, log_file) < 0) {
         fprintf(stderr, "Failed to initialize logger\n");
         return 1;
     }
     
     // 注册信号处理函数
     signal(SIGINT, signal_handler);
     signal(SIGTERM, signal_handler);
     
     // 创建并初始化车辆检测器
     vehicle_detector_t detector;
     g_detector = &detector;
     
     if (vehicle_detector_init(&detector, admin_code, device_type, device_id,
                              server_ip, server_port) < 0) {
         LOG_ERROR("Failed to initialize vehicle detector");
         logger_close();
         return 1;
     }
     
     printf("Vehicle Detector Demo\n");
     printf("=====================\n");
     printf("Server: %s:%d\n", server_ip, server_port);
     printf("Admin Code: %06X\n", admin_code);
     printf("Device Type: %d\n", device_type);
     printf("Device ID: %d\n", device_id);
     printf("Log Level: %d\n", log_level);
     if (log_file) {
         printf("Log File: %s\n", log_file);
     }
     printf("=====================\n");
     printf("Press Ctrl+C to stop\n\n");
     
     // 启动检测器
     LOG_INFO("Starting vehicle detector...");
     int result = vehicle_detector_start(&detector);
     
     // 清理资源
     vehicle_detector_stop(&detector);
     logger_close();
     
     return result;
 }