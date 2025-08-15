/**
 * @file server_demo.c
 * @brief 信号控制机演示程序
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include "server/signal_controller.h"
#include "utils/logger.h"

static signal_controller_t *g_controller = NULL;

/**
 * @brief 信号处理函数 - 优雅关闭程序
 */
void signal_handler(int sig) {
    printf("\nReceived signal %d, shutting down...\n", sig);
    if (g_controller) {
        signal_controller_stop(g_controller);
    }
    exit(0);
}

/**
 * @brief 显示使用帮助
 */
void show_usage(const char *program_name) {
    printf("Usage: %s [options]\n", program_name);
    printf("Options:\n");
    printf("  -p <port>     Listen port (default: 40000)\n");
    printf("  -a <admin>    Admin code (default: 110100)\n");
    printf("  -i <id>       Device ID (default: 1)\n");
    printf("  -l <level>    Log level (0=DEBUG, 1=INFO, 2=WARN, 3=ERROR) (default: 1)\n");
    printf("  -f <file>     Log file (default: console only)\n");
    printf("  -h            Show this help\n");
    printf("\nExample:\n");
    printf("  %s -p 40000 -a 110100 -i 1 -l 1\n", program_name);
}

int main(int argc, char *argv[]) {
    int port = 40000;
    uint32_t admin_code = 110100; // 北京市东城区
    uint16_t device_id = 1;
    log_level_t log_level = LOG_LEVEL_INFO;
    char *log_file = NULL;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "p:a:i:l:f:h")) != -1) {
        switch (opt) {
            case 'p':
                port = atoi(optarg);
                if (port <= 0 || port > 65535) {
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
    
    // 创建并初始化信号控制机
    signal_controller_t controller;
    g_controller = &controller;
    
    if (signal_controller_init(&controller, admin_code, device_id, port) < 0) {
        LOG_ERROR("Failed to initialize signal controller");
        logger_close();
        return 1;
    }
    
    printf("Traffic Signal Controller Demo\n");
    printf("==============================\n");
    printf("Admin Code: %06X\n", admin_code);
    printf("Device ID: %d\n", device_id);
    printf("Listen Port: %d\n", port);
    printf("Log Level: %d\n", log_level);
    if (log_file) {
        printf("Log File: %s\n", log_file);
    }
    printf("==============================\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // 启动服务
    LOG_INFO("Starting signal controller service...");
    int result = signal_controller_start(&controller);
    
    // 清理资源
    signal_controller_stop(&controller);
    logger_close();
    
    return result;
}