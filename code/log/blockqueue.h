/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

template<class T>
class BlockDeque {
public:
    // 强制显式调用，并且设置初值
    explicit BlockDeque(size_t MaxCapacity = 1000);

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;
    // 队列大小
    size_t capacity_;

    std::mutex mtx_;

    bool isClose_;
    // 条件变量
    std::condition_variable condConsumer_;
    // 条件变量
    std::condition_variable condProducer_;
};


template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

// 关闭队列，清空队列中的所有元素，并设置队列状态为关闭。这将触发所有等待中的生产者和消费者线程终止
template<class T>
void BlockDeque<T>::Close() {
    {   
        // 标准库RAII互斥锁
        std::lock_guard<std::mutex> locker(mtx_);
        // 清空队列
        deq_.clear();
        isClose_ = true;
    }
    // 唤醒所有的消费者和生产者线程
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

// 唤醒一个消费者来提醒有新的数据可以处理
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

// 清理队列中所有的任务，但是不是关闭队列
template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}
// 返回队列第一个元素，但是不删除它
template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}
// 返回队列的最后一个元素，但不删除它。
template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}
// 返回队列中的元素数量。
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}
// 返回队列的最大容量。
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}
// 将元素添加到队列的尾部,并且完成后通知消费者，如果队列已满，则阻塞等待，直到有空间可用。
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        // 让当前线程释放持有的互斥锁,唤醒后依旧会再次持有锁
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}
// 将元素添加到队列的头部，如果队列已满，则阻塞等待，直到有空间可用。
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}
// 检查队列是否为空。
template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}
// 检查队列是否已满。
template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}
// 从队列中移除并返回第一个元素，如果队列为空，则阻塞等待，直到有数据可用。
// 返回值为 true 表示成功，false 表示队列已关闭。item用来存储取出的任务
template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}
// 从队列中移除并返回第一个元素，如果队列为空，则等待指定的超时时间（以秒为单位）。
// 返回值为 true 表示成功，false 表示队列已关闭或等待超时。item用来存储取出的任务
template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H