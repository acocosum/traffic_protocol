/**
 * @file frame_processing_test.c
 * @brief 交通信号控制机帧处理测试脚本
 * 
 * 该测试脚本验证交通信号控制机的帧处理能力，包括：
 * 1. 单帧正常传输
 * 2. 双帧紧邻粘连（心跳响应 + 实时数据连续传输）
 * 3. 跨包不完整帧
 * 4. 含无效前缀噪声/垃圾字节
 * 5. 超长帧/异常帧丢弃与恢复
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

// 包含必要的头文件
#include "../src/common/protocol.h"
#include "../src/server/signal_controller.h"
#include "../src/utils/logger.h"

// 测试统计
typedef struct {
    int total_tests;
    int passed_tests;
    int failed_tests;
} test_stats_t;

static test_stats_t g_stats = {0, 0, 0};

// 测试辅助宏
#define TEST_ASSERT(condition, message) \
    do { \
        g_stats.total_tests++; \
        if (condition) { \
            g_stats.passed_tests++; \
            printf("[PASS] %s\n", message); \
        } else { \
            g_stats.failed_tests++; \
            printf("[FAIL] %s\n", message); \
        } \
    } while(0)

#define TEST_HEADER(test_name) \
    printf("\n=== %s ===\n", test_name)

// 辅助函数：打印十六进制数据
void print_hex_data(const char* label, const uint8_t* data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0) printf("\n");
    }
    if (len % 16 != 0) printf("\n");
}

// 辅助函数：创建测试用的协议帧
protocol_frame_t create_test_frame(uint8_t operation, uint16_t object_id, 
                                  const uint8_t* content, uint16_t content_len) {
    protocol_frame_t frame = {0};
    
    frame.frame_start = FRAME_START;
    frame.data.link_addr = 0x0000;
    frame.data.sender = create_device_id(0x123456, DEVICE_TYPE_SIGNAL, 0x0001);
    frame.data.receiver = create_device_id(0x123456, DEVICE_TYPE_COIL, 0x0002);
    frame.data.protocol_ver = PROTOCOL_VERSION;
    frame.data.operation = operation;
    frame.data.object_id = object_id;
    frame.data.content_len = content_len;
    
    if (content && content_len > 0) {
        frame.data.content = malloc(content_len);
        memcpy(frame.data.content, content, content_len);
    } else {
        frame.data.content = NULL;
    }
    
    frame.frame_end = FRAME_END;
    
    return frame;
}

// 测试用例1：单帧正常传输
void test_single_frame_normal() {
    TEST_HEADER("测试用例1：单帧正常传输");
    
    // 创建心跳响应帧
    uint8_t heartbeat_content[] = {0x01, 0x02, 0x03, 0x04};
    protocol_frame_t frame = create_test_frame(OP_QUERY_RESPONSE, OBJ_COMMUNICATION, 
                                              heartbeat_content, sizeof(heartbeat_content));
    
    // 编码帧
    uint8_t encoded_buffer[MAX_FRAME_SIZE];
    int encoded_len = encode_frame(&frame, encoded_buffer, sizeof(encoded_buffer));
    
    TEST_ASSERT(encoded_len > 0, "帧编码成功");
    print_hex_data("编码后的帧数据", encoded_buffer, encoded_len);
    
    // 模拟缓冲区接收
    uint8_t recv_buffer[MAX_FRAME_SIZE];
    memcpy(recv_buffer, encoded_buffer, encoded_len);
    size_t recv_buffer_len = encoded_len;
    
    // 提取完整帧
    size_t frame_start, frame_len;
    int extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
    
    TEST_ASSERT(extract_result == 1, "成功提取到完整帧");
    TEST_ASSERT(frame_start == 0, "帧开始位置正确");
    TEST_ASSERT(frame_len == (size_t)encoded_len, "帧长度正确");
    
    // 解码验证
    protocol_frame_t decoded_frame;
    protocol_result_t decode_result = decode_frame(recv_buffer + frame_start, 
                                                  frame_len, &decoded_frame);
    
    TEST_ASSERT(decode_result == PROTOCOL_SUCCESS, "帧解码成功");
    TEST_ASSERT(decoded_frame.data.operation == OP_QUERY_RESPONSE, "操作类型正确");
    TEST_ASSERT(decoded_frame.data.object_id == OBJ_COMMUNICATION, "对象标识正确");
    TEST_ASSERT(decoded_frame.data.content_len == sizeof(heartbeat_content), "内容长度正确");
    
    // 清理内存
    free_frame(&frame);
    free_frame(&decoded_frame);
}

// 测试用例2：双帧紧邻粘连
void test_adjacent_dual_frames() {
    TEST_HEADER("测试用例2：双帧紧邻粘连（心跳响应 + 实时数据）");
    
    // 创建心跳响应帧
    uint8_t heartbeat_content[] = {0x01};
    protocol_frame_t heartbeat_frame = create_test_frame(OP_QUERY_RESPONSE, OBJ_COMMUNICATION, 
                                                        heartbeat_content, sizeof(heartbeat_content));
    
    // 创建实时数据帧
    uint8_t realtime_content[] = {0x01, 0x10, 0x15, 0x20, 0x34, 0x50, 0x0B, 0x05, 0x08, 0x02, 0x03, 0x05};
    protocol_frame_t realtime_frame = create_test_frame(OP_UPLOAD, OBJ_TRAFFIC_REALTIME, 
                                                       realtime_content, sizeof(realtime_content));
    
    // 编码两个帧
    uint8_t heartbeat_buffer[MAX_FRAME_SIZE];
    uint8_t realtime_buffer[MAX_FRAME_SIZE];
    
    int heartbeat_len = encode_frame(&heartbeat_frame, heartbeat_buffer, sizeof(heartbeat_buffer));
    int realtime_len = encode_frame(&realtime_frame, realtime_buffer, sizeof(realtime_buffer));
    
    TEST_ASSERT(heartbeat_len > 0 && realtime_len > 0, "两个帧编码成功");
    
    // 创建粘连的数据包
    uint8_t combined_buffer[MAX_FRAME_SIZE];
    memcpy(combined_buffer, heartbeat_buffer, heartbeat_len);
    memcpy(combined_buffer + heartbeat_len, realtime_buffer, realtime_len);
    size_t combined_len = heartbeat_len + realtime_len;
    
    print_hex_data("粘连的帧数据", combined_buffer, combined_len);
    
    // 模拟缓冲区处理
    uint8_t recv_buffer[MAX_FRAME_SIZE];
    memcpy(recv_buffer, combined_buffer, combined_len);
    size_t recv_buffer_len = combined_len;
    
    int frames_extracted = 0;
    
    // 第一次提取
    size_t frame_start, frame_len;
    int extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
    if (extract_result == 1) {
        frames_extracted++;
        printf("提取到第1个帧：起始位置=%zu，长度=%zu\n", frame_start, frame_len);
        
        // 移除已处理的帧
        size_t remaining_data = recv_buffer_len - (frame_start + frame_len);
        if (remaining_data > 0) {
            memmove(recv_buffer, recv_buffer + frame_start + frame_len, remaining_data);
        }
        recv_buffer_len = remaining_data;
        
        // 第二次提取
        extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
        if (extract_result == 1) {
            frames_extracted++;
            printf("提取到第2个帧：起始位置=%zu，长度=%zu\n", frame_start, frame_len);
        }
    }
    
    TEST_ASSERT(frames_extracted == 2, "成功提取到两个完整帧");
    
    // 清理内存
    free_frame(&heartbeat_frame);
    free_frame(&realtime_frame);
}

// 测试用例3：跨包不完整帧
void test_incomplete_cross_packet_frame() {
    TEST_HEADER("测试用例3：跨包不完整帧");
    
    // 创建一个测试帧
    uint8_t test_content[] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    protocol_frame_t frame = create_test_frame(OP_UPLOAD, OBJ_TRAFFIC_STATS, 
                                              test_content, sizeof(test_content));
    
    // 编码帧
    uint8_t encoded_buffer[MAX_FRAME_SIZE];
    int encoded_len = encode_frame(&frame, encoded_buffer, sizeof(encoded_buffer));
    
    TEST_ASSERT(encoded_len > 0, "帧编码成功");
    
    // 模拟第一个包只接收到一半数据
    size_t split_point = encoded_len / 2;
    uint8_t recv_buffer[MAX_FRAME_SIZE];
    size_t recv_buffer_len = 0;
    
    // 第一次接收（不完整）
    memcpy(recv_buffer, encoded_buffer, split_point);
    recv_buffer_len = split_point;
    
    print_hex_data("第一次接收的数据", recv_buffer, recv_buffer_len);
    
    size_t frame_start, frame_len;
    int extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
    
    TEST_ASSERT(extract_result == 0, "第一次接收未能提取到完整帧");
    
    // 第二次接收（补全剩余数据）
    memcpy(recv_buffer + recv_buffer_len, encoded_buffer + split_point, 
           encoded_len - split_point);
    recv_buffer_len += encoded_len - split_point;
    
    print_hex_data("第二次接收后的完整数据", recv_buffer, recv_buffer_len);
    
    extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                           &frame_start, &frame_len);
    
    TEST_ASSERT(extract_result == 1, "第二次接收成功提取到完整帧");
    TEST_ASSERT(frame_len == (size_t)encoded_len, "完整帧长度正确");
    
    // 验证解码
    protocol_frame_t decoded_frame;
    protocol_result_t decode_result = decode_frame(recv_buffer + frame_start, 
                                                  frame_len, &decoded_frame);
    
    TEST_ASSERT(decode_result == PROTOCOL_SUCCESS, "跨包帧解码成功");
    
    // 清理内存
    free_frame(&frame);
    free_frame(&decoded_frame);
}

// 测试用例4：含无效前缀噪声/垃圾字节
void test_invalid_prefix_noise() {
    TEST_HEADER("测试用例4：含无效前缀噪声/垃圾字节");
    
    // 创建测试帧
    uint8_t test_content[] = {0xAA, 0xBB, 0xCC};
    protocol_frame_t frame = create_test_frame(OP_SET_RESPONSE, OBJ_DEVICE_TIME, 
                                              test_content, sizeof(test_content));
    
    // 编码帧
    uint8_t encoded_buffer[MAX_FRAME_SIZE];
    int encoded_len = encode_frame(&frame, encoded_buffer, sizeof(encoded_buffer));
    
    TEST_ASSERT(encoded_len > 0, "帧编码成功");
    
    // 创建带噪声前缀的数据
    uint8_t noise_data[] = {0xFF, 0x00, 0x55, 0xAA, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    uint8_t noisy_buffer[MAX_FRAME_SIZE];
    size_t noisy_len = 0;
    
    // 添加噪声前缀
    memcpy(noisy_buffer, noise_data, sizeof(noise_data));
    noisy_len += sizeof(noise_data);
    
    // 添加有效帧数据
    memcpy(noisy_buffer + noisy_len, encoded_buffer, encoded_len);
    noisy_len += encoded_len;
    
    print_hex_data("含噪声前缀的数据", noisy_buffer, noisy_len);
    
    // 模拟接收处理
    uint8_t recv_buffer[MAX_FRAME_SIZE];
    memcpy(recv_buffer, noisy_buffer, noisy_len);
    size_t recv_buffer_len = noisy_len;
    
    size_t frame_start, frame_len;
    int extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
    
    TEST_ASSERT(extract_result == 1, "从噪声数据中成功提取到完整帧");
    TEST_ASSERT(frame_start == sizeof(noise_data), "帧开始位置跳过了噪声数据");
    
    // 验证解码
    protocol_frame_t decoded_frame;
    protocol_result_t decode_result = decode_frame(recv_buffer + frame_start, 
                                                  frame_len, &decoded_frame);
    
    TEST_ASSERT(decode_result == PROTOCOL_SUCCESS, "噪声环境下帧解码成功");
    TEST_ASSERT(decoded_frame.data.operation == OP_SET_RESPONSE, "操作类型正确");
    TEST_ASSERT(decoded_frame.data.object_id == OBJ_DEVICE_TIME, "对象标识正确");
    
    // 清理内存
    free_frame(&frame);
    free_frame(&decoded_frame);
}

// 测试用例5：超长帧/异常帧丢弃与恢复
void test_oversized_abnormal_frame_recovery() {
    TEST_HEADER("测试用例5：超长帧/异常帧丢弃与恢复");
    
    // 创建正常帧
    uint8_t normal_content[] = {0x01, 0x02, 0x03};
    protocol_frame_t normal_frame = create_test_frame(OP_QUERY_REQUEST, OBJ_DETECTOR_STATUS, 
                                                     normal_content, sizeof(normal_content));
    
    uint8_t normal_buffer[MAX_FRAME_SIZE];
    int normal_len = encode_frame(&normal_frame, normal_buffer, sizeof(normal_buffer));
    
    TEST_ASSERT(normal_len > 0, "正常帧编码成功");
    
    // 测试1：带噪声前缀的异常帧后跟正常帧
    uint8_t test_buffer[MAX_FRAME_SIZE];
    size_t test_len = 0;
    
    // 添加噪声数据（没有正确的帧结构，避免包含0xC0）
    uint8_t noise[] = {0xFF, 0xAA, 0x55, 0x88, 0x12, 0x34, 0x56, 0xAB, 0xCD, 0xEF};
    memcpy(test_buffer, noise, sizeof(noise));
    test_len += sizeof(noise);
    
    // 添加正常帧
    memcpy(test_buffer + test_len, normal_buffer, normal_len);
    test_len += normal_len;
    
    print_hex_data("噪声+正常帧数据", test_buffer, test_len > 50 ? 50 : test_len);
    
    // 模拟接收处理
    uint8_t recv_buffer[MAX_FRAME_SIZE];
    memcpy(recv_buffer, test_buffer, test_len);
    size_t recv_buffer_len = test_len;
    
    size_t frame_start, frame_len;
    int extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
    
    TEST_ASSERT(extract_result == 1, "跳过噪声数据，成功提取到正常帧");
    
    // 验证提取到的是正常帧
    protocol_frame_t decoded_frame;
    protocol_result_t decode_result = decode_frame(recv_buffer + frame_start, 
                                                  frame_len, &decoded_frame);
    
    TEST_ASSERT(decode_result == PROTOCOL_SUCCESS, "正常帧解码成功");
    if (decode_result == PROTOCOL_SUCCESS) {
        TEST_ASSERT(decoded_frame.data.operation == OP_QUERY_REQUEST, "正常帧操作类型正确");
        TEST_ASSERT(decoded_frame.data.object_id == OBJ_DETECTOR_STATUS, "正常帧对象标识正确");
        free_frame(&decoded_frame);
    }
    
    // 测试2：CRC错误恢复测试
    uint8_t crc_error_buffer[MAX_FRAME_SIZE];
    memcpy(crc_error_buffer, normal_buffer, normal_len);
    
    // 故意破坏CRC（修改倒数第三和第二个字节）
    if (normal_len >= 3) {
        crc_error_buffer[normal_len - 3] ^= 0xFF;
        crc_error_buffer[normal_len - 2] ^= 0xFF;
    }
    
    // 添加第二个正常帧
    memcpy(crc_error_buffer + normal_len, normal_buffer, normal_len);
    size_t crc_test_len = normal_len * 2;
    
    memcpy(recv_buffer, crc_error_buffer, crc_test_len);
    recv_buffer_len = crc_test_len;
    
    // 第一次提取（CRC错误的帧）
    extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                           &frame_start, &frame_len);
    if (extract_result == 1) {
        protocol_frame_t crc_error_frame = {0};
        decode_result = decode_frame(recv_buffer + frame_start, frame_len, &crc_error_frame);
        
        TEST_ASSERT(decode_result == PROTOCOL_ERROR_CRC, "CRC错误被正确检测到");
        
        // 移除错误帧，继续处理
        size_t remaining_data = recv_buffer_len - (frame_start + frame_len);
        if (remaining_data > 0) {
            memmove(recv_buffer, recv_buffer + frame_start + frame_len, remaining_data);
        }
        recv_buffer_len = remaining_data;
        
        // 第二次提取（正常帧）
        extract_result = extract_complete_frame(recv_buffer, &recv_buffer_len, 
                                               &frame_start, &frame_len);
        if (extract_result == 1) {
            protocol_frame_t recovery_frame = {0};
            decode_result = decode_frame(recv_buffer + frame_start, frame_len, &recovery_frame);
            
            TEST_ASSERT(decode_result == PROTOCOL_SUCCESS, "CRC错误后成功恢复处理正常帧");
            if (decode_result == PROTOCOL_SUCCESS) {
                free_frame(&recovery_frame);
            }
        }
    }
    
    // 清理内存
    free_frame(&normal_frame);
}

// 运行所有测试
void run_all_tests() {
    printf("=== 交通信号控制机帧处理测试脚本 ===\n");
    printf("测试开始时间：%s", ctime(&(time_t){time(NULL)}));
    
    // 初始化日志系统
    logger_init(LOG_LEVEL_INFO, NULL);
    
    // 运行所有测试用例
    test_single_frame_normal();
    test_adjacent_dual_frames();
    test_incomplete_cross_packet_frame();
    test_invalid_prefix_noise();
    test_oversized_abnormal_frame_recovery();
    
    // 输出测试结果统计
    printf("\n=== 测试结果统计 ===\n");
    printf("总测试数：%d\n", g_stats.total_tests);
    printf("通过数：%d\n", g_stats.passed_tests);
    printf("失败数：%d\n", g_stats.failed_tests);
    printf("通过率：%.2f%%\n", 
           g_stats.total_tests > 0 ? (float)g_stats.passed_tests / g_stats.total_tests * 100 : 0);
    
    if (g_stats.failed_tests == 0) {
        printf("\n🎉 所有测试通过！帧处理功能工作正常。\n");
    } else {
        printf("\n❌ 有 %d 个测试失败，请检查帧处理逻辑。\n", g_stats.failed_tests);
    }
    
    logger_close();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    run_all_tests();
    
    return g_stats.failed_tests == 0 ? 0 : 1;
}