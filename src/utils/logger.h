/**
 * @file logger.h
 * @brief 简单的日志系统
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>

// 日志级别定义
typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN, 
    LOG_LEVEL_ERROR
} log_level_t;

/**
 * @brief 初始化日志系统
 * @param level 日志级别
 * @param filename 日志文件名 (NULL表示只输出到控制台)
 * @return 0成功，-1失败
 */
int logger_init(log_level_t level, const char *filename);

/**
 * @brief 记录日志
 * @param level 日志级别
 * @param format 格式字符串
 * @param ... 可变参数
 */
void logger_log(log_level_t level, const char *format, ...);

/**
 * @brief 关闭日志系统
 */
void logger_close(void);

// 便捷宏定义 - 修复参数传递问题
#define LOG_DEBUG(fmt, ...) logger_log(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  logger_log(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  logger_log(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) logger_log(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif // LOGGER_H