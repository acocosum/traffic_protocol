/**
 * @file socket_utils.c
 * @brief Socket工具函数实现
 */

#include "socket_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <errno.h>

/**
 * @brief 创建TCP服务端socket
 */
int create_tcp_server(int port) {
    int sockfd;
    struct sockaddr_in addr;
    
    // 创建socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket create failed");
        return -1;
    }
    
    // 设置地址重用
    if (set_reuse_addr(sockfd) < 0) {
        close(sockfd);
        return -1;
    }
    
    // 设置服务器地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    // 绑定地址
    if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(sockfd);
        return -1;
    }
    
    // 开始监听
    if (listen(sockfd, 10) < 0) {
        perror("listen failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/**
 * @brief 创建TCP客户端socket并连接到服务器
 */
int create_tcp_client(const char *server_ip, int server_port) {
    int sockfd;
    struct sockaddr_in addr;
    
    // 创建socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket create failed");
        return -1;
    }
    
    // 设置服务器地址
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(server_port);
    
    if (inet_pton(AF_INET, server_ip, &addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid server IP: %s\n", server_ip);
        close(sockfd);
        return -1;
    }
    
    // 连接到服务器
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect failed");
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

/**
 * @brief 设置socket为非阻塞模式
 */
int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        return -1;
    }
    
    flags |= O_NONBLOCK;
    if (fcntl(sockfd, F_SETFL, flags) == -1) {
        perror("fcntl F_SETFL failed");
        return -1;
    }
    
    return 0;
}

/**
 * @brief 设置socket选项SO_REUSEADDR
 */
int set_reuse_addr(int sockfd) {
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return -1;
    }
    return 0;
}

/**
 * @brief 安全地从socket接收数据
 */
int recv_with_timeout(int sockfd, void *buffer, size_t size, int timeout_ms) {
    fd_set readfds;
    struct timeval timeout;
    int result;
    
    // 设置超时时间
    if (timeout_ms > 0) {
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);
        
        timeout.tv_sec = timeout_ms / 1000;
        timeout.tv_usec = (timeout_ms % 1000) * 1000;
        
        result = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
        if (result == 0) {
            // 超时
            return 0;
        } else if (result < 0) {
            perror("select failed");
            return -1;
        }
    }
    
    // 接收数据
    return recv(sockfd, buffer, size, 0);
}

/**
 * @brief 安全地向socket发送数据
 */
int send_all(int sockfd, const void *buffer, size_t size) {
    const char *ptr = (const char*)buffer;
    size_t remaining = size;
    
    while (remaining > 0) {
        int sent = send(sockfd, ptr, remaining, MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue; // 被信号中断，继续发送
            }
            perror("send failed");
            return -1;
        }
        
        ptr += sent;
        remaining -= sent;
    }
    
    return size;
}