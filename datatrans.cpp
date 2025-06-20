#include "datatrans.h"
#include "config.h"
#include "ikcp.h"
#include "util.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

void parseWithRegex(const std::string &input, int &index, std::string &data,
                    int &retrans) {
  // 定义正则表达式模式
  std::regex pattern("index:(\\d+)current:(\\d+)retrans:(\\d+)");
  std::smatch matches;

  if (std::regex_search(input, matches, pattern)) {
    if (matches.size() > 3) {
      index = std::stoll(matches[1].str());
      data = matches[2].str();
      retrans = std::stoll(matches[3].str());
    }
  }
}

/**
 * @brief kcp接收线程
 *
 * @param obj
 * @return void*
 */
void *run_udp_thread(void *obj) {
  KCPSocket *client = (KCPSocket *)obj;
  // 用于发送数据到消息队列进行解析
  DataProcess *dataprocess = DataProcess::getInstance();
  // printf("run_udp_thread线程的dataprocess地址为%p\n", dataprocess);

  char buffer[2048] = {0};
  int32_t len = 0;
  socklen_t src_len = sizeof(struct sockaddr_in);

  while (1) {
    struct sockaddr_in src;
    memset(&src, 0, src_len);
    int start_flag = 0;
    // 核心模块，循环调用，处理数据发送/重发等
    ikcp_update(client->getKcpSocket(), iclock64());
    /// 6.udp接收数据，阻塞式性能、实时性较高，非阻塞最好使用select
    if ((len = recvfrom(client->getSockFd(), buffer, 2048, 0,
                        (struct sockaddr *)&src, &src_len)) > 0) {
      printf("当前时间:%ld收到报文，报文长度为：%d内容为：", iclock64(), len);
      for (int i = 0; i < len; i++) {
        if (buffer[i] != 0) {
          if ((buffer[i] <= 58 && buffer[i] >= 48) ||
              (buffer[i] <= 122 && buffer[i] >= 97)) {
            if (buffer[i] == 105) {
              start_flag = 1;
            }
            if (start_flag == 1) {
              printf("%c", buffer[i]);
            }
          }
        }
      }
      printf("\n");
      /// 7.预接收数据:调用ikcp_input将裸数据交给KCP，这些数据有可能是KCP控制报文
      int ret = ikcp_input(client->getKcpSocket(), buffer, len);
      if (ret < 0) // 检测ikcp_input是否提取到真正的数据
      {
        std::cout << "非KCP数据!!" << std::endl;
        std::cout << "收到数据:" << buffer << std::endl;
        continue;
      }
      IUINT32 next_time = 0;
      next_time = ikcp_check(client->getKcpSocket(), iclock64());
      std::cout << "下一次调用时间为： " << next_time << std::endl;
      /// 8.kcp将接收到的kcp数据包还原成应用数据
      char rcv_buf[2048] = {0};
      int recv_len = 0;
      ret = ikcp_recv(client->getKcpSocket(), rcv_buf, recv_len);
      if (ret >= 0) // 检测ikcp_recv提取到的数据
      {
        std::cout << "获取的数据个数为： " << ret << std::endl;
        int index = -1;
        std::string data;
        int retrans = 0;
        std::cout << "当前时间" << iclock64() << "收到内容：" << rcv_buf
                  << std::endl;
        parseWithRegex(rcv_buf, index, data, retrans);

        if (index == -1) {
          std::cout << "消息内容格式有误!!" << std::endl;
          continue;
        }
        if (retrans == 0) {
          std::stringstream ss;
          // 需要重发
          // std::cout << "需要重发数据：" << std::endl;
          ss << "index:" << index << "current:" << data << "retrans:" << 1
             << std::endl;
          uint16_t str_len = strlen(ss.str().c_str());
          memccpy(buffer, ss.str().c_str(), 0, str_len);
          // std::cout << buffer << std::endl;
          client->kcpSend(buffer, str_len);
        } else {
          // 非重传消息不需要发送数据到共享队列中分析rtt时间
          dataprocess->sendMessage(index, data);
        }

        // printf("ikcp_recv ret = %d,buf=%s\n", ret, rcv_buf);
      }
    }

    m_sleep(10);
  }

  // close(client->getSockFd());

  return NULL;
}

IUINT32 KCPSocket::CreateUDPSocket(IUINT32 domain, IUINT32 type,
                                   IUINT32 protocol) {
  // 默认情况下是PF_INET, SOCK_DGRAM, IPPROTO_UDP
  socks_fd = socket(domain, type, protocol);
  // 2.socket参数设置
  int opt = 1;
  int nonblock = 1;
  setsockopt(socks_fd, SOL_SOCKET, SOCK_NONBLOCK, &nonblock, sizeof(nonblock));
  setsockopt(socks_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  fcntl(socks_fd, F_SETFL, O_NONBLOCK); // 设置非阻塞
  return socks_fd;
}

struct sockaddr_in KCPSocket::CreateUDP(IUINT32 ip_family, const char *in_addr,
                                        IUINT32 listen_port) {
  memset(&sa, 0, sizeof(sa));
  sa.sin_family = ip_family;
  sa.sin_addr.s_addr = inet_addr(in_addr);
  sa.sin_port = htons(listen_port);
  std::cout << "create UDP on ip:" << in_addr << "port is:" << listen_port
            << "!" << std::endl;
  return sa;
}

int KCPSocket::BindUDP() {
  if (-1 == bind(socks_fd, (struct sockaddr *)&sa, sizeof(struct sockaddr))) {
    perror("error bind failed");
    close(socks_fd);
    return -1;
  }
  return 0;
}

void KCPSocket::setSendDst(const char *dst_ip, int dst_port) {
  if (dst_ip != NULL) {
    send_dest_addr.sin_family = AF_INET;
    send_dest_addr.sin_port = htons(dst_port);
    send_dest_addr.sin_addr.s_addr = inet_addr(dst_ip);
  } else {
    send_dest_addr.sin_family = AF_INET;
    send_dest_addr.sin_port = htons(dst_port);
    send_dest_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  }
}
int KCPSocket::setSendTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  if (setsockopt(socks_fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv))) {
    perror("error set send timeout failed");
    close(socks_fd);
    return -1;
  }
  return 0;
}

int KCPSocket::setRecvTimeout(int64_t v) {
  struct timeval tv {
    int(v / 1000), int(v % 1000 * 1000)
  };
  if (setsockopt(socks_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
    perror("error set send timeout failed");
    close(socks_fd);
    return -1;
  }
  return 0;
}

int KCPSocket::startKcpRecvThread() {
  pthread_t tid;
  if (!pthread_create(&tid, NULL, run_udp_thread, this)) {
    return -1;
  }
  return 0;
}

int KCPSocket::udpSend(const char *send_msg, int msg_len) {
  int sended = 0;
  // 发送数据需要做切片操作，若需要发送的数据超过UDP的MTU最大值，则将其进行切片，若不切片将导致发送的数据被截断
  int start_flag = 0;
  printf("当前发送报文，报文内容为：");
  while (sended < msg_len) {
    for (int i = 0; i < msg_len; i++) {
      if (send_msg[i] != 0) {
        if ((send_msg[i] <= 58 && send_msg[i] >= 48) ||
            (send_msg[i] <= 122 && send_msg[i] >= 97)) {
          if (send_msg[i] == 105) {
            start_flag = 1;
          }
          if (start_flag == 1) {
            printf("%c", send_msg[i]);
          }
        }
      }
    }
    printf("\n");
    size_t s = (msg_len - sended);
    if (s > UDP_MTU)
      s = UDP_MTU;
    ssize_t ret =
        ::sendto(socks_fd, send_msg + sended, s, MSG_DONTWAIT,
                 (struct sockaddr *)&send_dest_addr, sizeof(struct sockaddr));
    if (ret < 0) {
      return -1;
    }
    sended += s;
  }
  return 0;
}
/**
 * @brief
 * kcp回调函数，每次使用kcp发送数据时，都会调用该函数，该函数内应实现udp报文发送（也可自定义发送方式，如tcp等）
 *
 * @param buf 发送的消息内容
 * @param len 消息长度
 * @param kcp
 * @param user
 * 自定义的用户名（本例中用对象的地址作为用户名；注意：发送方用户名和接收方的用户名应不一致）
 * @return int 0
 */
int KCPSocket::kcp_udp_output(const char *buf, int len, ikcpcb *kcp,
                              void *user) {
  if (((KCPSocket *)user)->udpSend(buf, len)) {
    std::cout << "数据:" << buf << "发送失败" << std::endl;
  }
  return 0;
}

int KCPSocket::createKCP(const char *ip_addr, int port) {
  // conv（会话id，作为kcp的发送方和接收方其id需一致，否则接收方无法通过kcp库解析）
  // user（用户——void*指针，用于kcp区分数据传输是发给谁的，来自于谁的；需要注意的是，发送方和接收方该值应一样，一般使用对象地址作为其user值）
  _kcp = ikcp_create(0x11223344, (void *)this);
  CreateUDPSocket();
  CreateUDP(PF_INET, ip_addr, port);
  BindUDP();
  if (_kcp == NULL) {
    assert("无法创建kcp实例!!");
    return -1;
  }
  // 设置回调函数
  _kcp->output = kcp_udp_output;
  // 设置kcp发送接收的窗口大小，kcp使用滑动窗口的方式去校验数据包是否准确，窗口值会影响传输速率，在网络环境较好的情况下，设置较大可以达到更大的带宽
  // 网络环境较差的情况下，过大的值会导致重传的数据包也更大，由于数据重传的因素将导致带宽降低，延迟提高
  // 小的窗口可解决丢包率较大的情况下的延迟不断增长的问题，但一旦出现包延迟较大将导致延迟不断随着发包数无限增长（可修改rto值）。
  ikcp_wndsize(_kcp, 2048, 2048);
  _kcp->rx_minrto = 200;
  std::cout << "接收窗口为:1" << "发送窗口为:1" << std::endl;
  // 第二个参数 nodelay-为1表示启用以后若干常规加速将启动
  // 第三个参数 interval为内部处理时钟，默认设置为 10ms
  // 第四个参数 resend为快速重传指标，设置为0
  // 第五个参数 为是否禁用常规流控，0表示不禁止，1表示禁止
  ikcp_nodelay(_kcp, 1, 10, 2, 1);
  return 0;
}

int KCPSocket::kcpSend(const char *buffer, const int len) {
  // 发送前需要更新状态
  //  以一定频率调用 ikcp_update来更新 kcp状态，并且传入当前时钟（毫秒单位）
  //  如 10ms调用一次，或用 ikcp_check确定下次调用 update的时间不必每次调用
  ikcp_update(this->_kcp, iclock64());
  ikcp_send(this->_kcp, buffer, len);
  // 填入内容后需要再次更新状态
  ikcp_update(this->_kcp, iclock64());
  return 0;
}
