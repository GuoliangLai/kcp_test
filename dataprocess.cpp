#include "dataprocess.h"
#include <cstdint>

DataProcess* DataProcess::instance = nullptr;
MessageQueue* DataProcess::messageQueue = nullptr;


void *run_recv_msg_thread(void *obj) {
  DataProcess *dataprocess = DataProcess::getInstance();
  //DataProcess *messageQueue = (DataProcess *)obj;
  //重传次数
  u_int64_t resent_count = 0;
  //乱序次数
  u_int64_t unordered_count = 0;
  //所有收到的报文次数
  u_int64_t all_post_count = 0;
  while (true) {
    // 接收消息（阻塞）
    std::shared_ptr<Message> message = dataprocess->receive();
    // 检查是否收到结束信号
    if (!message) {
      std::cout << "消费者收到结束信号，退出" << std::endl;
      break;
    }
    all_post_count++;
    // 报文超时
    int64_t send_clock = std::stoll(message->getdata());
    int recv_index = message->getindex();
    // 收到第一条报文
    std::cout << "收到数据index：" << recv_index << "data: " << send_clock << std::endl;
    if (dataprocess->getIndexQue().empty()) {
      dataprocess->getIndexQue().push(std::pair<int, int>(recv_index, send_clock));  
      int64_t current_time = iclock64();
      int rtt = (current_time - send_clock) ;
          // 超时超过10毫秒即表示报文延迟了
      if (rtt > 10) {
        std::cout << "首条报文index:" << recv_index << "报文超时，收到时间为：" << current_time << " 发送时间为：" << send_clock << "；rtt为:" << rtt << std::endl;
      }else {
        //std::cout << "报文rtt为:" << rtt << std::endl;
      }    
      //第一条数据只计算rtt，不判断乱序
      continue;
    }
    std::pair<int, int> last_index = dataprocess->getIndexQue().back();
    // 乱序报文不会进入index队列中，只会将最后的一个报文一直保留，指导下一个最后报文收到
    if (recv_index < last_index.first) {
      std::cout << "报文乱序，当前收到：" << recv_index << "上一个包为："
                << last_index.first << std::endl;
                unordered_count++;
    }
    //接收到下一个报文，计算rtt并将队尾出队，新报文入队
    if (recv_index - last_index.first == 1) {
      int64_t current_time = iclock64();
      int rtt = current_time - send_clock;
          // 超时超过1毫秒即表示报文延迟了
      if (rtt > 10) {
        std::cout << "index:" << recv_index << "报文超时，收到时间为：" << current_time << " 发送时间为：" << send_clock << "；rtt为:" << rtt << "当前队列深度为：" << dataprocess->getIndexQue().size() <<  std::endl;
      }else {
        //std::cout << "报文rtt为:" << rtt << std::endl;
      }
      dataprocess->getIndexQue().pop();
      dataprocess->getIndexQue().push(std::pair<int, int>(recv_index, send_clock));
    }
    //表示由于网络原因，同一条报文接收到了两次，报文重传了，可计算rtt时间，并提示重发
    if (recv_index == last_index.first){
      resent_count++;
      int64_t current_time = iclock64();
      int rtt = current_time - send_clock;
          // 超时超过1毫秒即表示报文延迟了
      if (rtt > 10) {
        std::cout << "报文重发且超时，rtt为:" << rtt << std::endl;
      }else {
        std::cout << "报文重发，该重发的报文rtt为:" << rtt << std::endl;
      }
      dataprocess->getIndexQue().pop(); 
      dataprocess->getIndexQue().push(std::pair<int, int>(recv_index, send_clock));

    }
    // 跳序报文，即使跳序也会保存记录，说明后续再收到中间的报文则表示报文乱序了
    if (recv_index - last_index.first > 1) {
      std::cout << "报文丢失，当前收到：" << recv_index << "上一个包为："
                << last_index.first << "收到的时间为：" << last_index.second << std::endl;
                //网络已出错，将乱序报文
                dataprocess->getIndexQue().pop();
                dataprocess->getIndexQue().push(std::pair<int, int>(recv_index, send_clock));
    }
    // 处理消息
    // 休眠10ms
    //std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  std::cout << "总收到报文：" << all_post_count << "乱序报文为："
                << unordered_count << "重传报文为：" << resent_count << std::endl;
  return NULL;
}

int DataProcess::receiveMsgAndProcess() {
  pthread_t tid;
  if (!pthread_create(&tid, NULL, run_recv_msg_thread, this)) {
    assert("数据处理接收线程启动失败！");
    return -1;
  }
  return 0;
}

void DataProcess::sendMessage(const int index, const std::string &data) {
  // 获取消息队列
  if (messageQueue != nullptr) {
    auto message = std::make_shared<DataMessage>(index, data);
    messageQueue->send(message);
  }
  // 发送结束信号
  messageQueue->stop();
}