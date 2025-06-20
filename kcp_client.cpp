#include "kcp_client.h"
#include <pthread.h>
#include <string>

/**
 * @brief run_udp_thread udp线程处理函数
 * @param obj UdpHandle
 * @return
 */
void* run_udp_thread(void *obj)
{ 
    kcp_client *client = (kcp_client*) obj;

    char buffer[2048] = {  0 };
    int32_t len = 0;
    socklen_t src_len = sizeof(struct sockaddr_in);

    while (client->m_isLoop)
    { 
        ///5.核心模块，循环调用，处理数据发送/重发等
        ikcp_update(client->pkcp,iclock());

        struct sockaddr_in src;
        memset(&src, 0, src_len);
        ///6.udp接收到数据
        if ((len = recvfrom(client->pUdpDef->fd, buffer, 2048, 0,	(struct sockaddr*) &src, &src_len)) > 0)
        { 
//            printf("rcv=%s,len=%d\n\n",buffer,len);//可能时kcp控制报文

            ///7.预接收数据:调用ikcp_input将裸数据交给KCP，这些数据有可能是KCP控制报文
            int ret = ikcp_input(client->pkcp, buffer, len);
            if(ret < 0)//检测ikcp_input是否提取到真正的数据
            { 
                continue;
            }

            ///8.kcp将接收到的kcp数据包还原成应用数据
            char rcv_buf[2048] = {  0 };
            ret = ikcp_recv(client->pkcp, rcv_buf, len);
            if(ret >= 0)//检测ikcp_recv提取到的数据
            { 
                printf("ikcp_recv ret = %d,buf=%s\n",ret,rcv_buf);

                //9.测试用，自动回复一条消息
                if(strcmp(rcv_buf,"hello") == 0)
                { 
                    std::string msg = "hello back.";

                    client->sendData(msg.c_str(),msg.size());
                }
            }
        }

        isleep(1);
    }

    close(client->pUdpDef->fd);

    return NULL;
}

kcp_client::kcp_client(char *serIp, uint16_t serPort, uint16_t localPort)
{ 
    pUdpDef = new UdpDef;

    ///1.创建udp,指定远端IP和PORT，以及本地绑定PORT
    uint32_t remoteIp  = inet_addr(serIp);
    CreateUdp(pUdpDef,remoteIp,serPort,localPort);

    ///2.创建kcp实例，两端第一个参数conv要相同
    pkcp = ikcp_create(0x1, (void *)pUdpDef);

    ///3.kcp参数设置
    pkcp->output = udp_sendData_loop;//设置udp发送接口

    // 配置窗口大小：平均延迟200ms，每20ms发送一个包，
    // 而考虑到丢包重发，设置最大收发窗口为128
    ikcp_wndsize(pkcp, 128, 128);

    // 判断测试用例的模式
    int mode = 0;
    if (mode == 0) { 
        // 默认模式
        ikcp_nodelay(pkcp, 0, 10, 0, 0);
    }
    else if (mode == 1) { 
        // 普通模式，关闭流控等
        ikcp_nodelay(pkcp, 0, 10, 0, 1);
    }	else { 
        // 启动快速模式
        // 第二个参数 nodelay-启用以后若干常规加速将启动
        // 第三个参数 interval为内部处理时钟，默认设置为 10ms
        // 第四个参数 resend为快速重传指标，设置为2
        // 第五个参数 为是否禁用常规流控，这里禁止
        ikcp_nodelay(pkcp, 2, 10, 2, 1);
        pkcp->rx_minrto = 10;
        pkcp->fastresend = 1;
    }

    ///4.启动线程，处理udp收发
    m_isLoop = true;
    pthread_t tid;
    pthread_create(&tid,NULL,run_udp_thread,this);
}

int kcp_client::sendData(const char *buffer, int len)
{ 
    //这里只是把数据加入到发送队列
    int	ret = ikcp_send(pkcp,buffer,len);

    return ret;
}