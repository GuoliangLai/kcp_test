//=====================================================================
//
// test.cpp - kcp 测试用例
//
// 说明：
// gcc test.cpp -o test -lstdc++
//
//=====================================================================

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "config.h"
#include "dataprocess.h"
#include "datatrans.h"
#include "ikcp.c"
#include "ikcp.h"
#include "util.h"
#include "test.h"

// 饿汉模式单例，不允许创建其他实例，确保消息队列唯一，否则会导致消息队列无法正常收发


void test_kcp_send(const char *local_ip, int local_port, const char *remote_ip,
                   int remote_port) {
  std::cout << "开始测试kcp发送!!" << std::endl;
  std::shared_ptr<KCPSocket> kcp_socket(new KCPSocket());
  // 必须注意：每一个客户端或服务端必须初始化收发函数（由于kcp类似于tcp的ack确认机制，接收方收到数据后，需要回复相关信令给发送方，发送方以判断是否需要重发）
  // 因此，无论是发送方还是接收方，即使发送方只负责发送，也必须要存在接收的回调函数，而接收方即使只收数据，也必须给出发送的功能。
  kcp_socket->createKCP(local_ip, local_port);
  kcp_socket->setSendDst(remote_ip, remote_port);
  kcp_socket->startKcpRecvThread();
  IINT64 current = iclock64();
  IINT64 slap = current + 20;
  IINT64 index = 0;
  char buffer[2048];
  while (1) {
    isleep(1);
    current = iclock64();
    ikcp_update(kcp_socket->getKcpSocket(), current);
    // 每隔 20ms，kcp1发送数据
    for (; current >= slap; slap += 20) {
      // std::cout << "now :" << current << " slap:" << slap << "index:" <<
      // index << std::endl;
      std::stringstream ss;
	  //第一次发送不需要重发
      ss << "index:" << index++ << "current:" << current << "retrans:" << 0 << std::endl;
      // std::cout << ss.str().c_str() << "字符串长度为：" <<
      //strlen(ss.str().c_str()) << std::endl;
      uint16_t str_len = strlen(ss.str().c_str());
      memccpy(buffer, ss.str().c_str(), 0, str_len);
      //std::cout << "发送消息的内容为：" << buffer << std::endl;
      //  发送上层协议包
      kcp_socket->kcpSend(buffer, str_len);
      //每次都需要清空
      ss.clear();
    }
  }
}

void start_kcp(std::shared_ptr<BaseConfig> config) {
  std::cout << "启动kcp测试!!" << std::endl;
  std::cout << "local_ip:" << config->fromConfigGetAddr().local_ip.c_str()
            << " local_port:" << config->fromConfigGetAddr().local_port
            << " remote_ip:" << config->fromConfigGetAddr().remote_ip.c_str()
            << " remote_port:" << config->fromConfigGetAddr().remote_port
            << std::endl;
  // 客户端不断向服务端发送数据，将收到服务端回传的消息，分析回传消息的传输时间RTT及丢包率
  if (config->fromeConfigGetRole() == client) {
	std::cout << "启动为client模式!!" << std::endl;
    test_kcp_send(config->fromConfigGetAddr().local_ip.c_str(),
                  config->fromConfigGetAddr().local_port,
                  config->fromConfigGetAddr().remote_ip.c_str(),
                  config->fromConfigGetAddr().remote_port);
  }
  // 服务端不断接收数据并转发数据回客户端
  if (config->fromeConfigGetRole() == server) {
	std::cout << "启动为server模式!!" << std::endl;
    std::shared_ptr<KCPSocket> kcp_socket(new KCPSocket());
    kcp_socket->createKCP(config->fromConfigGetAddr().local_ip.c_str(),
                          config->fromConfigGetAddr().local_port);
    kcp_socket->setSendDst(config->fromConfigGetAddr().remote_ip.c_str(),
                           config->fromConfigGetAddr().remote_port);
    kcp_socket->startKcpRecvThread();
    while (1) {
		isleep(1*1000);
      // 保持接收子线程运行
    }
  }
  // 自发自收，或又是客户端又是服务端
  if (config->fromeConfigGetRole() == all) {
    std::shared_ptr<KCPSocket> kcp_socket(new KCPSocket());
    kcp_socket->createKCP(config->fromConfigGetAddr().local_ip.c_str(),
                          config->fromConfigGetAddr().local_port);
    kcp_socket->setSendDst(config->fromConfigGetAddr().remote_ip.c_str(),
                           config->fromConfigGetAddr().remote_port);
    kcp_socket->startKcpRecvThread();
    test_kcp_send(config->fromConfigGetAddr().local_ip.c_str(),
                  config->fromConfigGetAddr().local_port,
                  config->fromConfigGetAddr().remote_ip.c_str(),
                  config->fromConfigGetAddr().remote_port);
  }
}

void start_udp(std::shared_ptr<BaseConfig> config) {
  std::cout << "启动udp测试!!" << std::endl;
}

int main() {
  DataProcess *dataprocess = DataProcess::getInstance();
  // 启动消息队列开始接收数据
  dataprocess->receiveMsgAndProcess();
  // test(0);	// 默认模式，类似 TCP：正常模式，无快速重传，常规流控
  // test(1);	// 普通模式，关闭流控等
  // test(2);	// 快速模式，所有开关都打开，且关闭流控
  // 加载配置文件
  std::shared_ptr<BaseConfig> config(new BaseConfig());
  config->fromYamlLoadConfig();
  // 检查需要发送数据的手段：udp、kcp等
  std::unordered_map<TranTool, int> trans_tools =
      config->fromeConfigGetTransTool();
  for (auto iterator = trans_tools.begin(); iterator != trans_tools.end();
       iterator++) {
    if (iterator->second) {
      switch (iterator->first) {
      case kcp:
        start_kcp(config);
        break;
      case udp:
        start_udp(config);
        break;
      default:
        break;
      }
    }
  }
  return 0;
}

/*
default mode result (20917ms):
avgrtt=740 maxrtt=1507

normal mode result (20131ms):
avgrtt=156 maxrtt=571

fast mode result (20207ms):
avgrtt=138 maxrtt=392
*/
