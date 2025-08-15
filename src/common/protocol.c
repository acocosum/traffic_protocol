/**
 * @file protocol.c
 * @brief GB/T 43229-2023 协议实现
 */

#include "protocol.h"
#include "crc16.h"
#include "../utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

/**
 * @brief 打印十六进制数据用于调试
 * @param prefix 前缀信息
 * @param data 数据指针
 * @param len 数据长度
 */
static void hex_dump_debug(const char *prefix, const uint8_t *data, size_t len) {
    if (!data || len == 0) {
        return;
    }
    
    // 限制打印长度，避免日志过长
    size_t print_len = (len > 64) ? 64 : len;
    
    char hex_str[256];
    char *pos = hex_str;
    size_t remaining = sizeof(hex_str);
    
    for (size_t i = 0; i < print_len && remaining > 3; i++) {
        int written = snprintf(pos, remaining, "%02X ", data[i]);
        if (written > 0 && (size_t)written < remaining) {
            pos += written;
            remaining -= written;
        } else {
            break;
        }
    }
    
    if (len > print_len) {
        snprintf(pos, remaining, "... (%zu more bytes)", len - print_len);
    }
    
    LOG_DEBUG("%s: %s", prefix, hex_str);
}

/**
 * @brief 对数据进行转义编码
 * 将数据中的0xC0和0xDB进行转义处理
 */
int escape_data(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_size) {
    if (!input || !output || input_len == 0) {
        return -1;
    }
    
    size_t output_pos = 0;
    
    for (size_t i = 0; i < input_len; i++) {
        if (output_pos >= output_size - 1) {
            return -1; // 输出缓冲区不足
        }
        
        if (input[i] == FRAME_START || input[i] == FRAME_END) {
            // 转义帧标识符 0xC0 -> 0xDB 0xDC
            output[output_pos++] = ESCAPE_CHAR;
            if (output_pos >= output_size) return -1;
            output[output_pos++] = ESCAPE_START;
        } else if (input[i] == ESCAPE_CHAR) {
            // 转义转义字符 0xDB -> 0xDB 0xDD
            output[output_pos++] = ESCAPE_CHAR;
            if (output_pos >= output_size) return -1;
            output[output_pos++] = ESCAPE_ESCAPE;
        } else {
            // 普通字符直接复制
            output[output_pos++] = input[i];
        }
    }
    
    return (int)output_pos;
}

/**
 * @brief 对数据进行转义解码
 * 将转义的数据还原为原始数据
 */
int unescape_data(const uint8_t *input, size_t input_len, uint8_t *output, size_t output_size) {
    if (!input || !output || input_len == 0) {
        return -1;
    }
    
    size_t output_pos = 0;
    size_t i = 0;
    
    while (i < input_len) {
        if (output_pos >= output_size) {
            return -1; // 输出缓冲区不足
        }
        
        if (input[i] == ESCAPE_CHAR && i + 1 < input_len) {
            // 处理转义序列
            if (input[i + 1] == ESCAPE_START) {
                // 0xDB 0xDC -> 0xC0
                output[output_pos++] = FRAME_START;
                i += 2;
            } else if (input[i + 1] == ESCAPE_ESCAPE) {
                // 0xDB 0xDD -> 0xDB
                output[output_pos++] = ESCAPE_CHAR;
                i += 2;
            } else {
                // 无效的转义序列
                return -1;
            }
        } else {
            // 普通字符直接复制
            output[output_pos++] = input[i];
            i++;
        }
    }
    
    return (int)output_pos;
}

/**
 * @brief 序列化设备标识到缓冲区
 */
static void serialize_device_id(const device_id_t *device_id, uint8_t *buffer) {
    // 行政区划代码 (3字节，小端序)
    buffer[0] = device_id->admin_code & 0xFF;
    buffer[1] = (device_id->admin_code >> 8) & 0xFF;
    buffer[2] = (device_id->admin_code >> 16) & 0xFF;
    
    // 设备类型 (2字节，小端序)
    buffer[3] = device_id->device_type & 0xFF;
    buffer[4] = (device_id->device_type >> 8) & 0xFF;
    
    // 设备编号 (2字节，小端序)
    buffer[5] = device_id->device_id & 0xFF;
    buffer[6] = (device_id->device_id >> 8) & 0xFF;
}

/**
 * @brief 从缓冲区反序列化设备标识
 */
static void deserialize_device_id(const uint8_t *buffer, device_id_t *device_id) {
    // 行政区划代码 (3字节，小端序)
    device_id->admin_code = buffer[0] | (buffer[1] << 8) | (buffer[2] << 16);
    
    // 设备类型 (2字节，小端序)
    device_id->device_type = buffer[3] | (buffer[4] << 8);
    
    // 设备编号 (2字节，小端序)
    device_id->device_id = buffer[5] | (buffer[6] << 8);
}

/**
 * @brief 将协议帧编码为字节流
 */
int encode_frame(const protocol_frame_t *frame, uint8_t *buffer, size_t buffer_size) {
    if (!frame || !buffer || buffer_size < 20) {
        return -1;
    }
    
    // 构建数据表 (未转义的)
    uint8_t data_table[MAX_FRAME_SIZE];
    size_t data_table_len = 0;
    
    // 链路地址 (2字节)
    data_table[data_table_len++] = frame->data.link_addr & 0xFF;
    data_table[data_table_len++] = (frame->data.link_addr >> 8) & 0xFF;
    
    // 发送方标识 (7字节)
    serialize_device_id(&frame->data.sender, &data_table[data_table_len]);
    data_table_len += 7;
    
    // 接收方标识 (7字节)
    serialize_device_id(&frame->data.receiver, &data_table[data_table_len]);
    data_table_len += 7;
    
    // 协议版本 (1字节)
    data_table[data_table_len++] = frame->data.protocol_ver;
    
    // 操作类型 (1字节)
    data_table[data_table_len++] = frame->data.operation;
    
    // 对象标识 (2字节，小端序)
    data_table[data_table_len++] = frame->data.object_id & 0xFF;
    data_table[data_table_len++] = (frame->data.object_id >> 8) & 0xFF;
    
    // 消息内容
    if (frame->data.content && frame->data.content_len > 0) {
        if (data_table_len + frame->data.content_len > sizeof(data_table)) {
            return -1; // 数据太大
        }
        memcpy(&data_table[data_table_len], frame->data.content, frame->data.content_len);
        data_table_len += frame->data.content_len;
    }
    
    // 计算CRC校验码
    uint16_t crc = calculate_crc16(data_table, data_table_len);
    
    // 添加CRC到数据表末尾
    data_table[data_table_len++] = crc & 0xFF;
    data_table[data_table_len++] = (crc >> 8) & 0xFF;
    
    // 对数据表进行转义编码
    uint8_t escaped_data[MAX_FRAME_SIZE];
    int escaped_len = escape_data(data_table, data_table_len, escaped_data, sizeof(escaped_data));
    if (escaped_len < 0) {
        return -1;
    }
    
    // 构建最终帧
    size_t frame_len = 0;
    
    // 帧开始
    if (frame_len >= buffer_size) return -1;
    buffer[frame_len++] = FRAME_START;
    
    // 转义后的数据表
    if (frame_len + escaped_len >= buffer_size) return -1;
    memcpy(&buffer[frame_len], escaped_data, escaped_len);
    frame_len += escaped_len;
    
    // 帧结束
    if (frame_len >= buffer_size) return -1;
    buffer[frame_len++] = FRAME_END;
    
    return (int)frame_len;
}

/**
 * @brief 将字节流解码为协议帧
 */
protocol_result_t decode_frame(const uint8_t *buffer, size_t buffer_len, protocol_frame_t *frame) {
    if (!buffer || !frame || buffer_len < 4) {
        return PROTOCOL_ERROR_INVALID_PARAM;
    }
    
    // 检查帧开始和结束标识
    if (buffer[0] != FRAME_START || buffer[buffer_len - 1] != FRAME_END) {
        return PROTOCOL_ERROR_FORMAT;
    }
    
    // 提取中间的数据部分 (去除帧开始和结束标识)
    const uint8_t *escaped_data = &buffer[1];
    size_t escaped_len = buffer_len - 2;
    
    // 对数据进行转义解码
    uint8_t unescaped_data[MAX_FRAME_SIZE];
    int unescaped_len = unescape_data(escaped_data, escaped_len, unescaped_data, sizeof(unescaped_data));
    if (unescaped_len < 0) {
        return PROTOCOL_ERROR_ESCAPE;
    }
    
    // 检查最小数据长度 (18字节数据表头 + 2字节CRC)
    if (unescaped_len < 20) {
        return PROTOCOL_ERROR_INCOMPLETE;
    }
    
    // 提取CRC (最后2字节)
    uint16_t received_crc = unescaped_data[unescaped_len - 2] | 
                           (unescaped_data[unescaped_len - 1] << 8);
    
    // 计算数据表的CRC (不包括CRC本身)
    uint16_t calculated_crc = calculate_crc16(unescaped_data, unescaped_len - 2);
    
    if (received_crc != calculated_crc) {
        // 详细的CRC校验失败日志
        LOG_ERROR("CRC verification failed: received=0x%04X, calculated=0x%04X, data_len=%d", 
                  received_crc, calculated_crc, unescaped_len - 2);
        
        // 打印原始接收数据的十六进制dump用于调试
        hex_dump_debug("Raw received data", buffer, buffer_len);
        
        // 打印转义解码后的数据
        hex_dump_debug("Unescaped data", unescaped_data, unescaped_len);
        
        return PROTOCOL_ERROR_CRC;
    }
    
    // 解析数据表
    size_t pos = 0;
    
    // 链路地址 (2字节)
    frame->data.link_addr = unescaped_data[pos] | (unescaped_data[pos + 1] << 8);
    pos += 2;
    
    // 发送方标识 (7字节)
    deserialize_device_id(&unescaped_data[pos], &frame->data.sender);
    pos += 7;
    
    // 接收方标识 (7字节)
    deserialize_device_id(&unescaped_data[pos], &frame->data.receiver);
    pos += 7;
    
    // 协议版本 (1字节)
    frame->data.protocol_ver = unescaped_data[pos++];
    
    // 操作类型 (1字节)
    frame->data.operation = unescaped_data[pos++];
    
    // 对象标识 (2字节)
    frame->data.object_id = unescaped_data[pos] | (unescaped_data[pos + 1] << 8);
    pos += 2;
    
    // 消息内容长度
    frame->data.content_len = (unescaped_len - 2) - pos; // 减去CRC长度
    
    // 分配并复制消息内容
    if (frame->data.content_len > 0) {
        frame->data.content = malloc(frame->data.content_len);
        if (!frame->data.content) {
            return PROTOCOL_ERROR_INVALID_PARAM;
        }
        memcpy(frame->data.content, &unescaped_data[pos], frame->data.content_len);
    } else {
        frame->data.content = NULL;
    }
    
    // 设置帧标识
    frame->frame_start = FRAME_START;
    frame->frame_end = FRAME_END;
    frame->crc = received_crc;
    
    return PROTOCOL_SUCCESS;
}

/**
 * @brief 创建设备标识
 */
device_id_t create_device_id(uint32_t admin_code, uint16_t device_type, uint16_t device_id) {
    device_id_t id;
    id.admin_code = admin_code & 0xFFFFFF; // 只取低24位
    id.device_type = device_type;
    id.device_id = device_id;
    return id;
}

/**
 * @brief 创建数据表
 */
data_table_t create_data_table(device_id_t sender, device_id_t receiver, 
                              uint8_t operation, uint16_t object_id,
                              const uint8_t *content, uint16_t content_len) {
    data_table_t table;
    table.link_addr = 0x0000; // 保留字段
    table.sender = sender;
    table.receiver = receiver;
    table.protocol_ver = PROTOCOL_VERSION;
    table.operation = operation;
    table.object_id = object_id;
    table.content_len = content_len;
    
    if (content && content_len > 0) {
        table.content = malloc(content_len);
        if (table.content) {
            memcpy(table.content, content, content_len);
        }
    } else {
        table.content = NULL;
    }
    
    return table;
}

/**
 * @brief 创建错误应答帧
 */
protocol_frame_t create_error_frame(device_id_t sender, device_id_t receiver, uint8_t error_type) {
    protocol_frame_t frame;
    frame.frame_start = FRAME_START;
    frame.frame_end = FRAME_END;
    
    uint8_t error_content = error_type;
    frame.data = create_data_table(sender, receiver, OP_ERROR_RESPONSE, 0x0000, 
                                  &error_content, 1);
    
    return frame;
}

/**
 * @brief 释放协议帧占用的内存
 */
void free_frame(protocol_frame_t *frame) {
    if (frame && frame->data.content) {
        free(frame->data.content);
        frame->data.content = NULL;
        frame->data.content_len = 0;
    }
}

/**
 * @brief 获取当前设备时间
 */
device_time_t get_current_time(void) {
    device_time_t dev_time;
    struct timeval tv;
    struct tm *tm_info;
    
    gettimeofday(&tv, NULL);
    tm_info = localtime(&tv.tv_sec);
    
    dev_time.timestamp = (uint32_t)tv.tv_sec;
    dev_time.milliseconds = (uint16_t)(tv.tv_usec / 1000);
    dev_time.timezone_offset = tm_info->tm_gmtoff; // 时区偏移(秒)
    
    return dev_time;
}

/**
 * @brief 打印协议帧信息 (调试用)
 */
void print_frame_info(const protocol_frame_t *frame) {
    if (!frame) {
        printf("Frame is NULL\n");
        return;
    }
    
    printf("=== Protocol Frame Info ===\n");
    printf("Frame Start: 0x%02X\n", frame->frame_start);
    printf("Link Addr: 0x%04X\n", frame->data.link_addr);
    printf("Sender: Admin=%06X, Type=%04X, ID=%04X\n", 
           frame->data.sender.admin_code, 
           frame->data.sender.device_type, 
           frame->data.sender.device_id);
    printf("Receiver: Admin=%06X, Type=%04X, ID=%04X\n", 
           frame->data.receiver.admin_code, 
           frame->data.receiver.device_type, 
           frame->data.receiver.device_id);
    printf("Protocol Ver: 0x%02X\n", frame->data.protocol_ver);
    printf("Operation: 0x%02X\n", frame->data.operation);
    printf("Object ID: 0x%04X\n", frame->data.object_id);
    printf("Content Len: %d\n", frame->data.content_len);
    printf("CRC: 0x%04X\n", frame->crc);
    printf("Frame End: 0x%02X\n", frame->frame_end);
    
    if (frame->data.content && frame->data.content_len > 0) {
        printf("Content: ");
        for (int i = 0; i < frame->data.content_len && i < 16; i++) {
            printf("%02X ", frame->data.content[i]);
        }
        if (frame->data.content_len > 16) {
            printf("...");
        }
        printf("\n");
    }
    printf("===========================\n");
}