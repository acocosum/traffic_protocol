/**
 * @file crc16.h
 * @brief CRC16校验算法头文件
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>

/**
 * @brief 计算CRC16校验码
 * 使用多项式 X^16 + X^15 + X^2 + 1 (0x8005)
 * 初始值: 0xFFFF
 * 结果异或值: 0x0000
 * @param data 数据指针
 * @param len 数据长度
 * @return CRC16校验码
 */
uint16_t calculate_crc16(const uint8_t *data, size_t len);

#endif // CRC16_H