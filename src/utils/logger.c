/**
 * @file logger.c
 * @brief 简单的日志系统实现
 */

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

static log_level_t current_level = LOG_LEVEL_INFO;
static FILE *log_file = NULL;

/**
 * @brief 获取当前时间字符串
 */
static void get_time_string(char *buffer, size_t size) {
    if (!buffer || size < 20) {
        return;
    }
    
    time_t rawtime;
    struct tm *timeinfo;
    
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    
    if (timeinfo) {
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", timeinfo);
    } else {
        strncpy(buffer, "0000-00-00 00:00:00", size - 1);
        buffer[size - 1] = '\0';
    }
}

/**
 * @brief 获取日志级别字符串
 */
static const char* get_level_string(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO ";
        case LOG_LEVEL_WARN:  return "WARN ";
        case LOG_LEVEL_ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

/**
 * @brief 初始化日志系统
 */
int logger_init(log_level_t level, const char *filename) {
    current_level = level;
    
    // 先关闭之前的日志文件（如果有的话）
    if (log_file && log_file != stdout && log_file != stderr) {
        fclose(log_file);
        log_file = NULL;
    }
    
    if (filename && strlen(filename) > 0) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
            return -1;
        }
        
        // 设置日志文件为行缓冲
        setvbuf(log_file, NULL, _IOLBF, 0);
        
        // 写入启动标记
        fprintf(log_file, "\n=== Log started at ");
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));
        fprintf(log_file, "%s ===\n", time_str);
        fflush(log_file);
    }
    
    return 0;
}

/**
 * @brief 记录日志
 */
void logger_log(log_level_t level, const char *format, ...) {
    // 检查参数有效性
    if (!format) {
        return;
    }
    
    // 检查日志级别
    if (level < current_level) {
        return;
    }
    
    // 获取时间字符串
    char time_str[32];
    get_time_string(time_str, sizeof(time_str));
    
    // 准备可变参数
    va_list args;
    va_start(args, format);
    
    // 输出到控制台
    printf("[%s] [%s] ", time_str, get_level_string(level));
    vprintf(format, args);
    printf("\n");
    fflush(stdout);
    
    // 重新启动va_list用于文件输出
    va_end(args);
    va_start(args, format);
    
    // 输出到文件 (如果有)
    if (log_file) {
        fprintf(log_file, "[%s] [%s] ", time_str, get_level_string(level));
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    
    va_end(args);
}

/**
 * @brief 关闭日志系统
 */
void logger_close(void) {
    if (log_file && log_file != stdout && log_file != stderr) {
        // 写入结束标记
        char time_str[32];
        get_time_string(time_str, sizeof(time_str));
        fprintf(log_file, "=== Log ended at %s ===\n\n", time_str);
        fflush(log_file);
        
        fclose(log_file);
        log_file = NULL;
    }
}