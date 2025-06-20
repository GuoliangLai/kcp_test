#ifndef __DATAPROCESS__
#define __DATAPROCESS__
#include <memory>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <string>
#include <cassert>
#include <memory>
#include <arpa/inet.h>
#include <cassert>
#include <cstring>
#include <netinet/in.h>
#include <pthread.h>
#include <thread>
#include <sys/socket.h>
#include <utility>
#include <queue>
#include <sys/types.h>
#include <utility>

#include "util.h"
#include "ikcp.h"

static std::mutex mutex;
static std::condition_variable condition;


// 消息基类
class Message {
public:
    virtual ~Message() = default;
    virtual std::string toString() const = 0;
    virtual int getindex() const = 0;
    virtual std::string getdata() const = 0;
};

// 具体消息类型
class DataMessage : public Message {
private:
    int id;
    std::string data;
public:
    DataMessage(int id, const std::string& data) : id(id), data(data) {}
    
    std::string toString() const override {
        return "DataMessage(id=" + std::to_string(id) + ", data=" + data + ")";
    }

    int getindex() const override {
        return id;
    }

    std::string getdata() const override {
        return data;
    }
    
    int getId() const { return id; }
    const std::string& getData() const { return data; }
};
static std::queue<std::shared_ptr<DataMessage>> queue;

// 消息队列类
class MessageQueue {
private:
    bool stopped = false;

public:
    // 发送消息
    void send(std::shared_ptr<DataMessage> message) {
        {
            std::unique_lock<std::mutex> lock(mutex);
            queue.push(message);
            condition.notify_all();
            lock.unlock();
        }
        
    }
    
    // 接收消息（阻塞）
    std::shared_ptr<DataMessage> receive() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [this] { return !queue.empty() || stopped; });
        if (stopped && queue.empty()) {
            return nullptr; // 队列已停止且为空
        }
        
        auto message = queue.front();
        queue.pop();
        lock.unlock();
        return message;
    }
    
    // 尝试接收消息（非阻塞）
    std::shared_ptr<DataMessage> tryReceive() {
        std::unique_lock<std::mutex> lock(mutex);
        if (queue.empty()) {
            return nullptr;
        }
        
        auto message = queue.front();
        queue.pop();
        return message;
    }
    
    // 停止队列
    void stop() {
        {
            std::unique_lock<std::mutex> lock(mutex);
            stopped = true;
        }
        condition.notify_all();
    }
    
    // 检查队列是否为空
    bool empty() const {
        std::unique_lock<std::mutex> lock(mutex);
        return queue.empty();
    }
};

class DataProcess :public MessageQueue{
    public:
    static DataProcess* getInstance() {
        if (instance == nullptr) {
            instance = new DataProcess();
            if (messageQueue == nullptr) {
                messageQueue = new MessageQueue();  
            }
            return instance;     
        }else{
            if (messageQueue == nullptr) {
                messageQueue = new MessageQueue();  
            }
            return instance;
        }
    }
    typedef std::shared_ptr<DataProcess> ptr;
    /**
     * @brief 不断从消息队列中获取数据并判断收到的数据是否存在乱序和超时情况，若存在将记录
     * 
     * @return int 0
     */
    int receiveMsgAndProcess();

    /**
     * @brief 发送内容到消息队列
     * 
     * @param index 序号
     * @param data 时间
     */
    void sendMessage(const int index, const std::string& data);

    std::queue<std::pair<int, int>>& getIndexQue(){return index_seq;};

    private:
        DataProcess(){}
        DataProcess(const DataProcess&) = delete;
        DataProcess& operator=(const DataProcess&) = delete;
        // 静态私有实例
        static DataProcess* instance;
        static MessageQueue* messageQueue;
        //记录收到的index序列(含收到的时间戳)用于判断是否丢包或乱序
        std::queue<std::pair<int, int>> index_seq;


};

    // 在程序启动时就创建实例
   // EagerSingleton EagerSingleton::instance;
/**
// 随机数生成器
int getRandomNumber(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

int main() {
    // 创建消息队列
    MessageQueue<Message> messageQueue;
    
    // 生产者线程
    auto producer = [&messageQueue]() {
        for (int i = 0; i < 10; ++i) {
            // 创建消息
            auto message = std::make_shared<DataMessage>(
                i, "消息内容 #" + std::to_string(i));
            
            // 发送消息
            messageQueue.send(message);
            std::cout << "生产者发送: " << message->toString() << std::endl;
            
            // 随机休眠
            std::this_thread::sleep_for(
                std::chrono::milliseconds(getRandomNumber(100, 500)));
        }
        
        // 发送结束信号
        messageQueue.stop();
    };
    
    // 消费者线程
    auto consumer = [&messageQueue]() {
        while (true) {
            // 接收消息（阻塞）
            auto message = messageQueue.receive();
            
            // 检查是否收到结束信号
            if (!message) {
                std::cout << "消费者收到结束信号，退出" << std::endl;
                break;
            }
            
            // 处理消息
            std::cout << "消费者接收: " << message->toString() << std::endl;
            
            // 随机休眠
            std::this_thread::sleep_for(
                std::chrono::milliseconds(getRandomNumber(300, 800)));
        }
    };
    
    // 启动线程
    std::thread producerThread(producer);
    std::thread consumerThread(consumer);
    
    // 等待线程结束
    producerThread.join();
    consumerThread.join();
    
    return 0;
}
**/

#endif