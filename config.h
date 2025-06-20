#ifndef __BASECONFIG__
#define __BASECONFIG__

#include "yaml-cpp/yaml.h"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <string>
#include <unordered_map> //头文件

struct NetAddr {
  std::string local_ip;
  std::string remote_ip;
  int local_port;
  int remote_port;
};

enum Role { invalid, client, server, all };
enum TranTool { udp, kcp, enet, udt, usrsctp, uret, rudp };

class BaseConfig : public std::enable_shared_from_this<BaseConfig> {
  typedef std::shared_ptr<BaseConfig> ptr;

public:
  BaseConfig() {
    _transfer_tool.clear();
    _transfer_tool.empty();
  }
  BaseConfig(int kcp_conv, const char *local_ip_addr, int listen_port,
             const char *remote_ip_addr, int remote_port) {
    _kcp_conv = kcp_conv;
    if (local_ip_addr != NULL) {
      addr.local_ip = local_ip_addr;
    } else {
      addr.local_ip = "0.0.0.0";
    }
    addr.local_port = listen_port;
    addr.remote_port = remote_port;
    _transfer_tool.clear();
    _transfer_tool.empty();
  }
  const int fromConfigGetKcpConv() const { return _kcp_conv; }

  const int fromConfigGetListenPort() const { return addr.local_port; }

  const int fromConfigGetRemotePort() const { return addr.remote_port; }

  const std::string fromConfigGetRemoteIp() const { return addr.remote_ip; }

  const std::string fromConfigGetLocalIp() const { return addr.local_ip; }

  const struct NetAddr fromConfigGetAddr() const { return addr; }

  const char *getConfigFilePath() const { return config_file_path; }

  const Role fromeConfigGetRole() const { return _role; }

  const std::unordered_map<TranTool, int> fromeConfigGetTransTool() const {
    return _transfer_tool;
  }

  /**
   * @brief 从yaml文件中读取软件参数
   *
   * @return int 0表示成功，-1 表示失败
   */
  int fromYamlLoadConfig();

  /**
   * @brief 将当前配置写入到yaml文件中，方便下次重启程序后能按照当前的配置运行
   *
   * @return int 0表示写入成功，-1表示写入失败
   */
  int writeYamlConfig();

  /**
   * @brief Set the Kcp Conv Config object
   *
   * @param user_kcp_conv 自定义的conv值
   * @return int
   */
  int setKcpConvConfig(const int user_kcp_conv);

private:
  struct NetAddr addr;
  int _kcp_conv;
  const char *config_file_path = "../config/config.yaml";
  // 当前运行的角色：服务端、客户端
  enum Role _role;
  // std::list<TranTool> _transfer_tool;
  std::unordered_map<TranTool, int> _transfer_tool; // 定义一个map
};

/**
namespace glFramework {
class BaseConfig {
  typedef std::shared_ptr<BaseConfig> ptr;

public:
  BaseConfig() { int tmp = 0; }
  const std::string &getName() const { return _name; }
  const std::string &getDescription() const { return _description; }

private:
  std::string _ver = "0";
  // 选择需要测试的网络模式
  enum Transfer_Mode { udp, kcp, udt, enet, usrsctp, uret, rudp, all };
  std::string _name = "default";
  std::string _description = "default config file.";
  // 模拟数据传输的参数
  struct NetSimulatorParm {
    // 丢包率：10%
    uint8_t _lostrate = 10;
    // rtt最小和最大时间
    uint8_t _rttmin = 60;
    uint8_t _rttmax = 125;
    // 模拟报文乱序：1%;
    uint8_t _disortrate = 1;
  };
};
} // namespace glFramework
*/
#endif