/**
 * @file socket_utils.h
 * @brief Socket工具函数
 */

#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/**
 * @brief 创建TCP服务端socket
 * @param port 监听端口
 * @return socket文件描述符，-1表示失败
 */
int create_tcp_server(int port);

/**
 * @brief 创建TCP客户端socket并连接到服务器
 * @param server_ip 服务器IP地址
 * @param server_port 服务器端口
 * @return socket文件描述符，-1表示失败
 */
int create_tcp_client(const char *server_ip, int server_port);

/**
 * @brief 设置socket为非阻塞模式
 * @param sockfd socket文件描述符
 * @return 0成功，-1失败
 */
int set_nonblocking(int sockfd);

/**
 * @brief 设置socket选项SO_REUSEADDR
 * @param sockfd socket文件描述符
 * @return 0成功，-1失败
 */
int set_reuse_addr(int sockfd);

/**
 * @brief 安全地从socket接收数据
 * @param sockfd socket文件描述符
 * @param buffer 接收缓冲区
 * @param size 缓冲区大小
 * @param timeout_ms 超时时间(毫秒)，0表示不超时
 * @return 接收到的字节数，-1表示错误，0表示连接关闭
 */
int recv_with_timeout(int sockfd, void *buffer, size_t size, int timeout_ms);

/**
 * @brief 安全地向socket发送数据
 * @param sockfd socket文件描述符
 * @param buffer 发送缓冲区
 * @param size 数据大小
 * @return 发送的字节数，-1表示错误
 */
int send_all(int sockfd, const void *buffer, size_t size);

#endif // SOCKET_UTILS_H