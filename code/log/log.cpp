/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {
        // 消息队列非空，执行写操作
        while(!deque_->empty()) {
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}
// 初始化日志系统，可以设置日志级别、日志路径、文件后缀以及最大队列大小。如果启用了异步写入，会创建一个日志写入线程。
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    // maxqueuesize>0表示启动了异步日志模式
    if(maxQueueSize > 0) {
        isAsync_ = true;
        if(!deque_) {
            // 如果阻塞队列没有创建就创建
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            // 将新创建的队列移动给成员
            deque_ = move(newDeque);
            // 创建一个单例日志的线程
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    } else {
        isAsync_ = false;
    }

    lineCount_ = 0;
    // 获取当前时间
    time_t timer = time(nullptr);
    // 将时间转化为tm时间结构体，用于格式化输出
    struct tm *sysTime = localtime(&timer);
    struct tm t = *sysTime;
    // 设置日志文件路径
    path_ = path;
    // 设置日志文件后缀
    suffix_ = suffix;
    // 初始化日志文件名
    char fileName[LOG_NAME_LEN] = {0};
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    // 设置成员变量日期
    toDay_ = t.tm_mday;

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();
        // 文件如果已经打开，直接写入
        if(fp_) { 
            flush();
            fclose(fp_); 
        }

        // 没有打开就重新打开
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {
            // 没有找到路径就直接创建一个
            mkdir(path_, 0777);
            // 重新打开文件
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);
    }
}

void Log::write(int level, const char *format, ...) {
    // 存储当前时间
    struct timeval now = {0, 0};
    // TODO：这里似乎不会频繁调用，似乎那种暂存系统时间戳的方法不是很合适
    // 使用系统调用获取当前时间戳并且存储在now中
    gettimeofday(&now, nullptr);
    // 获取now中的秒级时间戳
    time_t tSec = now.tv_sec;
    // 将时间戳转化为tm结构体，方便格式化输出
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    // 申明一个变长参数列表
    va_list vaList;

    /* 日志日期 日志行数 如果日期不对或者日志已经写满了*/
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        // 先存储当前时间
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        // 如果是日期不对，那么就获取当前日期，并且将行号置零
        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            // 不置零行号，直接添加为分文件
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        // 写入日志
        flush();
        // 关闭当前文件
        fclose(fp_);
        // 将文件标识符赋值为新文件
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        // 加锁不仅仅使为了写缓冲区，也方便同步写入日志
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        // 将格式化好的日期写入缓冲区里
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
        // 更新写入的位置 
        buff_.HasWritten(n);
        // 添加前缀到缓冲区中
        AppendLogLevelTitle_(level);

        // 将消息格式化写入
        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);
        // 更新写入的位置
        buff_.HasWritten(m);
        // 最后添加换行符
        buff_.Append("\n\0", 2);

        if(isAsync_ && deque_ && !deque_->full()) {
            // 将所有的内容当作字符串放入阻塞队列
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            // 直接写入当前的日志文件
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}
// 添加日志前缀标签
void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[DEBUG]: ", 9);
        break;
    case 1:
        buff_.Append("[INFO] : ", 9);
        break;
    case 2:
        buff_.Append("[WARN] : ", 9);
        break;
    case 3:
        buff_.Append("[ERROR]: ", 9);
        break;
    default:
        buff_.Append("[UNKNOW] : ", 9);
        break;
    }
}
// 写入日志
void Log::flush() {
    if(isAsync_) { 
        // 确保会有生产者来消费
        deque_->flush(); 
    }
    // 刷新文件缓冲区,确保文件写入
    fflush(fp_);
}

void Log::AsyncWrite_() {
    string str = "";
    // 传入空str对象,用于换出任务
    while(deque_->pop(str)) {
        lock_guard<mutex> locker(mtx_);
        // 将日志消息写入文件
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    // 创建Log类单例实例
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    // 启动异步写入线程
    Log::Instance()->AsyncWrite_();
}