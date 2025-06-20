#include "kcp_inc.h"
#include <cerrno>

#define UDP_MTU             1460
int udp_sendData_loop(const char *buffer, int len, ikcpcb *kcp, void *user)
{ 
    UdpDef *pUdpClientDef = (UdpDef *)user;

    int sended = 0;
    while(sended < len){ 
        size_t s = (len - sended);
        if(s > UDP_MTU) s = UDP_MTU;
        ssize_t ret = ::sendto(pUdpClientDef->fd, buffer + sended, s, MSG_DONTWAIT, (struct sockaddr*) &pUdpClientDef->remote_addr,sizeof(struct sockaddr));
        if(ret < 0){ 
            return -1;
        }
        sended += s;
    }
    return (size_t) sended;
}

int32_t CreateUdp(UdpDef *udp, uint32_t remoteIp, int32_t remotePort, int32_t plocalPort)
{ 
    if (udp == NULL)		return -1;
    udp->fd = -1;

    udp->fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    fcntl(udp->fd, F_SETFL, O_NONBLOCK);//设置非阻塞
    if(udp->fd < 0)
    { 
        printf("[CreateUdpClient] create udp socket failed,errno=[%d],remoteIp=[%u],remotePort=[%d]",errno,remoteIp,remotePort);
        return -1;
    }

    udp->remote_addr.sin_family = AF_INET;
    udp->remote_addr.sin_port = htons(remotePort);
    udp->remote_addr.sin_addr.s_addr = remoteIp;

    udp->local_addr.sin_family = AF_INET;
    udp->local_addr.sin_port = htons(plocalPort);
    udp->local_addr.sin_addr.s_addr = INADDR_ANY;

    //2.socket参数设置
    int opt = 1;
    setsockopt(udp->fd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));//chw
    fcntl(udp->fd, F_SETFL, O_NONBLOCK);//设置非阻塞

    if (bind(udp->fd, (struct sockaddr*) &udp->local_addr,sizeof(struct sockaddr_in)) < 0)
    { 
        close(udp->fd);
        printf("[CreateUdpServer] Udp server bind failed,errno=[%d],plocalPort=[%d]",errno,plocalPort);
        return -2;
    }
    return 0;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

int main() {
    // 创建UDP套接字
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 准备服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    // 绑定套接字
    if (bind(sockfd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port 8080...\n");

    while (1) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sockfd, &readfds);

        // 设置超时时间为1秒
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        // 调用select
        int activity = select(sockfd + 1, &readfds, NULL, NULL, &timeout);

        if (activity < 0) {
            perror("select error");
            break;
        } else if (activity == 0) {
            // 超时，可以执行其他任务
            printf("No data received, doing other work...\n");
            continue;
        }

        // 检查套接字是否可读
        if (FD_ISSET(sockfd, &readfds)) {
            char buffer[1024] = {0};
            struct sockaddr_in client_addr;
            socklen_t client_addr_len = sizeof(client_addr);

            int bytes_received = recvfrom(sockfd, buffer, sizeof(buffer), 0,
                                         (struct sockaddr *)&client_addr, &client_addr_len);
            if (bytes_received < 0) {
                perror("recvfrom error");
                break;
            }

            printf("Received %d bytes from %s:%d\n", bytes_received,
                   inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            printf("Data: %s\n", buffer);
        }
    }

    close(sockfd);
    return 0;
}