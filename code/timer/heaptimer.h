/*
 * @Author       : mark
 * @Date         : 2020-06-17
 * @copyleft Apache 2.0
 */ 
#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

// 回调函数
typedef std::function<void()> TimeoutCallBack;
// 表示高分辨率时间（根据硬件可以达到纳秒级别）
typedef std::chrono::high_resolution_clock Clock;
// 表示毫秒时间间隔C11
typedef std::chrono::milliseconds MS;
// 表示记录特定事件发生的事件
typedef Clock::time_point TimeStamp;

struct TimerNode {
    // 唯一计时器节点
    int id;
    // 时间戳，expires存储了该操作的预定过期时间
    TimeStamp expires;
    // 时间到期所执行的回调函数
    TimeoutCallBack cb;
    // 重载运算符号，使其支持将TimerNode对象放在可排序的容器内
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};
class HeapTimer {
public:
    HeapTimer() { heap_.reserve(64); }

    ~HeapTimer() { clear(); }
    
    void adjust(int id, int newExpires);

    void add(int id, int timeOut, const TimeoutCallBack& cb);

    void doWork(int id);

    void clear();

    void tick();

    void pop();

    int GetNextTick();

private:
    void del_(size_t i);
    
    void siftup_(size_t i);

    bool siftdown_(size_t index, size_t n);

    void SwapNode_(size_t i, size_t j);

    // 使用vector来存储时间戳
    std::vector<TimerNode> heap_;
    // 使用map来做索引
    std::unordered_map<int, size_t> ref_;
};

#endif //HEAP_TIMER_H