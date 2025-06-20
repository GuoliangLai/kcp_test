
#include "config.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

int BaseConfig::fromYamlLoadConfig() {
  YAML::Node config = YAML::LoadFile(getConfigFilePath());
  if (config["role"]) {
    std::cout << config["role"].as<std::string>().c_str() << std::endl;
    if (strcmp("client", config["role"].as<std::string>().c_str()) == 0) {
      _role = client;
    } else if (strcmp("server", config["role"].as<std::string>().c_str()) == 0) {
      _role = server;
    } else if (strcmp("all", config["role"].as<std::string>().c_str()) == 0) {
      _role = all;
    } else {
      _role = invalid;
      assert("无法获取当前程序运行的角色（服务端、客户端），请检查配置文件中该"
             "值是否正确填写！");
    }
  } else {
    assert("无法获取当前程序运行的角色（服务端、客户端）！");
    return -1;
  }

  if (config["transfer_tool"]) {
    size_t trans_tools_len = config["transfer_tool"].size();
    for (int i = 0; i < trans_tools_len; ++i) {
      if (strcmp("udp", config["transfer_tool"][i].as<std::string>().c_str()) == 0) {
        _transfer_tool[udp] = 1;
      } else if (strcmp("kcp",
                        config["transfer_tool"][i].as<std::string>().c_str()) == 0) {
        _transfer_tool[kcp] = 1;
      } else if (strcmp("udt",
                        config["transfer_tool"][i].as<std::string>().c_str()) == 0) {
        _transfer_tool[udt] = 1;
      } else if (strcmp("enet",
                        config["transfer_tool"][i].as<std::string>().c_str()) == 0) {
        _transfer_tool[enet] = 1;
      } else if (strcmp("usrsctp",
                        config["transfer_tool"][i].as<std::string>().c_str()) == 0) {
        _transfer_tool[usrsctp] = 1;
      }
    }
  } else {
    assert("无法获取当前程序所设置的传输方式！");
    return -1;
  }

  // 如果读取不到参数则使用123456
  if (config["kcp"] && config["kcp"]["conv"] &&
      config["kcp"]["conv"].as<int>() != 0) {
    _kcp_conv = config["kcp"]["conv"].as<int>();
  } else {
    _kcp_conv = 123456;
  }

  if (config["net"]) {
    if (config["net"]["local_ip"]) {
      addr.local_ip = config["net"]["local_ip"].as<std::string>();
    }
    if (config["net"]["remote_ip"]) {
      addr.remote_ip = config["net"]["remote_ip"].as<std::string>();
    }
    if (config["net"]["local_port"]) {
      addr.local_port = config["net"]["local_port"].as<int>();
    }
    if (config["net"]["remote_port"]) {
      addr.remote_port = config["net"]["remote_port"].as<int>();
    }
  } else {
    assert("无法获取当前程序所配置的ip地址，请检查配置文件！");
    return -1;
  }
  return 0;
}

int BaseConfig::writeYamlConfig() {
  std::ofstream fout(getConfigFilePath());
  YAML::Node config;
  switch (_role) {
  case invalid:
    config['role'] = "invalid";
    break;
  case client:
    config['role'] = "client";
    break;
  case server:
    config['role'] = "server";
    break;
  case all:
    config['role'] = "all";
    break;
  default:
    break;
  }
  auto iterator = _transfer_tool.begin();
  std::vector<std::string> tmp_transfer_tools;
  tmp_transfer_tools.clear();
  for (; iterator != _transfer_tool.end(); iterator++) {
    switch (iterator->first) {
    case udp:
      if (iterator->second == 1) {
        tmp_transfer_tools.push_back("udp");
      }
      break;
    case kcp:
      if (iterator->second == 1) {
        tmp_transfer_tools.push_back("kcp");
      }
      break;
    case udt:
      if (iterator->second == 1) {
        tmp_transfer_tools.push_back("udt");
      }
      break;
    case enet:
      if (iterator->second == 1) {
        tmp_transfer_tools.push_back("enet");
      }
      break;
    case usrsctp:
      if (iterator->second == 1) {
        tmp_transfer_tools.push_back("usrsctp");
      }
      break;
    default:
      break;
    }
  }
  config["transfer_tool"] = tmp_transfer_tools;
  config["kcp"]["conv"] = _kcp_conv;
  config["net"]["local_ip"] = addr.local_ip;
  config["net"]["local_port"] = addr.local_port;
  config["net"]["remote_ip"] = addr.remote_ip;
  config["net"]["remote_port"] = addr.remote_port;
  fout << config;
  fout.close();
  return 0;
}

int BaseConfig::setKcpConvConfig(const int user_kcp_conv){
    _kcp_conv = user_kcp_conv;
    return 0;

}