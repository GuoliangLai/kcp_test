#ifndef __DATATRANS__
#define __DATATRANS__

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <memory>
#include <netinet/in.h>
#include <iostream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h> /* for close() for socket */
#include <fcntl.h>
#include <sstream>
#include <regex>


#include "config.h"
#include "ikcp.h"
#include "util.h"
#include "dataprocess.h"

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
#elif !defined(__unix)
#define __unix
#endif

#ifdef __unix
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#define UDP_MTU             1460


/* get system time */
static inline void m_timeofday(long *sec, long *usec)
{
	#if defined(__unix)
	struct timeval time;
	gettimeofday(&time, NULL);
	if (sec) *sec = time.tv_sec;
	if (usec) *usec = time.tv_usec;
	#else
	static long mode = 0, addsec = 0;
	BOOL retval;
	static IINT64 freq = 1;
	IINT64 qpc;
	if (mode == 0) {
		retval = QueryPerformanceFrequency((LARGE_INTEGER*)&freq);
		freq = (freq == 0)? 1 : freq;
		retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
		addsec = (long)time(NULL);
		addsec = addsec - (long)((qpc / freq) & 0x7fffffff);
		mode = 1;
	}
	retval = QueryPerformanceCounter((LARGE_INTEGER*)&qpc);
	retval = retval * 2;
	if (sec) *sec = (long)(qpc / freq) + addsec;
	if (usec) *usec = (long)((qpc % freq) * 1000000 / freq);
	#endif
}

/* get clock in millisecond 64 */
static inline IINT64 m_clock64(void)
{
	long s, u;
	IINT64 value;
	m_timeofday(&s, &u);
	value = ((IINT64)s) * 1000 + (u / 1000);
	return value;
}

static inline IUINT32 m_clock()
{
	return (IUINT32)(m_clock64() & 0xfffffffful);
}

/* sleep in millisecond */
static inline void m_sleep(unsigned long millisecond)
{
	#ifdef __unix 	/* usleep( time * 1000 ); */
	usleep((millisecond << 10) - (millisecond << 4) - (millisecond << 3));
	#elif defined(_WIN32)
	Sleep(millisecond);
	#endif
}


class KCPSocket : public std::enable_shared_from_this<KCPSocket> {
public:
  typedef std::shared_ptr<KCPSocket> ptr;
  KCPSocket(){
    memset(&send_dest_addr, 0, sizeof(send_dest_addr));
  }

  KCPSocket(int conv, void *user) {
    _kcp_conv = conv;
    _kcp_user = user;
    _kcp = ikcp_create(_kcp_conv, _kcp_user);
    memset(&send_dest_addr, 0, sizeof(send_dest_addr));
  }

  ~KCPSocket(){
    if (_kcp) {
      free(_kcp);    
    }
    closeUDP();
  }

  int getSockFd () const {return socks_fd;}
  void setSendDst(const char* dst_ip, int dst_port);
  /**
   * @brief 创建socket套接字
   *
   * @param domain ip家族
   * @param type TCP或UDP传输，SOCK_DGRAM为UDP
   * @param protocol 传输方式，IP地址的形式
   * @return int 套接字
   */
  IUINT32 CreateUDPSocket(IUINT32 domain = PF_INET, IUINT32 type = SOCK_DGRAM,
                          IUINT32 protocol = IPPROTO_UDP);

  /**
   * @brief 创建UDP实体，配置UDP传输
   *
   * @param ip_family 传输形式：IP
   * @param in_addr 需要将UDP绑定到的ip地址，0.0.0.0表示本机所有ip
   * @param listen_port 需要监听的端口
   * @return struct sockaddr_in  返回创建的UDP连接实体
   */
  struct sockaddr_in CreateUDP(IUINT32 ip_family = PF_INET,
                               const char *in_addr = "0.0.0.0",
                               IUINT32 listen_port = 1856);

  /**
   * @brief 绑定端口和ip
   *
   * @param sock 创建的socket句柄
   * @param sa 创建的UDP实体
   * @return int 是否创建成功，成功返回0；失败返回-1
   */
  int BindUDP();

  /**
   * @brief Set the Send Timeout object
   *
   * @param v 超时时间，单位毫秒
   * @return int 0为成功，-1为失败
   */
  int setSendTimeout(int64_t v);

  /**
   * @brief Set the Recv Timeout object
   *
   * @param v 超时时间，单位毫秒
   * @return int 0为成功，-1为失败
   */
  int setRecvTimeout(int64_t v);

  /**
   * @brief 开启udp接收数据线程
   *
   * @return int 0：线程创建成功，-1：线程创建失败
   */
  int startKcpRecvThread();
/**
 * @brief 发送UDP报文到指定ip地址和端口
 * 
 * @param send_msg 消息内容
 * @param msg_len 消息长度
 * @return int 
 */
  int udpSend(const char *send_msg, int msg_len);
  /**
   * @brief 实例化udp传输socket及设置地址和端口，默认为全ip地址监听，端口为1856.同时本函数还设置kcp传输的基本参数：conv（回话id，作为kcp的发送方和接收方其id需一致，否则接收方无法通过kcp库解析）
   * user（用户——void*指针，用于kcp区分数据传输是发给谁的，来自于谁的；需要注意的是，发送方和接收方该值应一样，一般使用对象地址作为其user值）
   *
   * @return int 0表示成功，-1表示失败
   */
  int createKCP(const char* ip_addr="0.0.0.0", int port=1856);


  /**
   * @brief 关闭UDP连接
   * 
   * @return int 成功：0；失败：-1
   */
  int closeUDP(){
	if(close(socks_fd) != -1){
    std::cout << "socket 连接已关闭" << std::endl;
		return 0;
	}
	return -1;
  }

  ikcpcb * getKcpSocket() {return _kcp;}

  static int kcp_udp_output(const char *buf, int len, ikcpcb *kcp, void *user);
  

  std::shared_ptr<KCPSocket> getSharedPtr() { return shared_from_this(); }

  /**
   * @brief 发送经过kcp封装的消息
   * 
   * @param buffer 需要发送的消息内容
   * @param len 消息内容长度
   * @return int 0：success，-1：faild
   */
  int kcpSend(const char* buffer, const int len);

private:
  int socks_fd;
  struct sockaddr_in sa;
  int _kcp_conv;
  void *_kcp_user;
  ikcpcb *_kcp;
  struct sockaddr_in send_dest_addr;
  char *recv_msg;
};

class KcpConnect : public KCPSocket {
public:
	KcpConnect(int a){
	}
private:
  std::shared_ptr<KCPSocket> kcp_socket;
};
// 发送一个 udp包

#endif