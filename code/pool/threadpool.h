/*
 * @Author       : mark
 * @Date         : 2020-06-15
 * @copyleft Apache 2.0
 */ 

#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);
            // 循环创建线程，并且分离线程
            for(size_t i = 0; i < threadCount; i++) {
                // [pool = pool_] 这个部分就是定义了一个名为 pool 的局部变量，
                // 并将其初始化为外部的 pool_ 变量的副本。这种捕获方式被称为“复制捕获”
                // （它允许在 lambda 函数内部使用外部的变量 pool_ 的拷贝。
                // 这个拷贝在 lambda 函数内部称为 pool，而 pool_ 仍然是外部线程池对象的原始引用。
                // 通过对pool的加锁就可以避免外部直接使用pool_产生竞争
                std::thread([pool = pool_] {
                    // 在每个线程内创建了一个unique_lock互斥锁，使其进入临界状态
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        // 任务不为空，有任务要处理
                        if(!pool->tasks.empty()) {
                            // 使用move将任务移动到变量task里，减少拷贝
                            auto task = std::move(pool->tasks.front());
                            pool->tasks.pop();
                            // 允许线程可以执行任务
                            locker.unlock();
                            task();
                            // 任务结束重新获得锁，用来保证能正常从任务队列中取任务
                            locker.lock();
                        } 
                        else if(pool->isClosed) break;
                        // 让线程进入等待状态
                        else pool->cond.wait(locker);
                    }
                }).detach();
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        // 判断线程池是否被创建
        if(static_cast<bool>(pool_)) {
            {
                // 对线程池自身的锁加锁
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            // 通知所有的线程确保他们退出，等所有线程完成任务后再销毁线程池
            pool_->cond.notify_all();
        }
    }
    // 像线程池中添加任务
    template<class T>
    void AddTask(T&& task) {
        {
            // 自动释放锁
            std::lock_guard<std::mutex> locker(pool_->mtx);
            // emplace它用于在容器中构造一个元素，而不是像 push_back 或 insert 一样将一个已经构造好的元素复制或移动到容器中
            // std::forward<F>(task) 是将传递给 AddTask 函数的任务函数 task 转发到 emplace 函数中。
            // 这确保了在 emplace 中构造任务对象的过程中，使用了正确的参数类型和引用类型。
            pool_->tasks.emplace(std::forward<T>(task));
        }
        pool_->cond.notify_one();
    }

private:
    struct Pool {
        // 线程池自带锁可以保证锁的正常使用与释放，防止外部加锁而忘记解锁
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        // 任务队列
        std::queue<std::function<void()>> tasks;
    };
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H