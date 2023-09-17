/*
 * @Author       : mark
 * @Date         : 2020-06-26
 * @copyleft Apache 2.0
 */ 
#include "buffer.h"
// 传入默认的缓冲区并且初始化缓冲区的大小，并且将读与写位置置零
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}
// 返回未读字节大小
size_t Buffer::ReadableBytes() const {
    return writePos_ - readPos_;
}
// 返回可写字节大小
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}
// 返回已读大小(可以删除的大小)
size_t Buffer::PrependableBytes() const {
    return readPos_;
}
// 返回当前读取位置的指针
const char* Buffer::Peek() const {
    // 通过缓冲区的起始指针加上读取位置的偏移量
    return BeginPtr_() + readPos_;
}
// 移动读取的位置
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}
// 移动读取位置直到指定位置
void Buffer::RetrieveUntil(const char* end) {
    // 这可读位置只能是之后可读的位置
    assert(Peek() <= end );
    // 两个指针相减就是两个指针之间的偏移量
    Retrieve(end - Peek());
}
// 重置缓冲区，清空所有数据
void Buffer::RetrieveAll() {
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}
// 读取所有数据并返回字符串
std::string Buffer::RetrieveAllToStr() {
    // 当前读取位置指针加上可以读大小的偏移量
    std::string str(Peek(), ReadableBytes());
    // 清空
    RetrieveAll();
    // 控制台输出日志
    std::cout << str << std::endl;
    return str;
}
// 返回当前可写位置的常量指针
const char* Buffer::BeginWriteConst() const {
    // 缓冲区起始指针加减已写大小的偏移量
    return BeginPtr_() + writePos_;
}
// 返回当前可写位置的常量指针
char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}
// 更新写入位置
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 
// 追加字符串
void Buffer::Append(const std::string& str) {
    // 将string转为char*
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    // 判空
    assert(data);
    // 转为char*
    Append(static_cast<const char*>(data), len);
}

void Buffer::Append(const char* str, size_t len) {
    // 判空
    assert(str);
    // 确保有可用大小的缓冲区
    EnsureWriteable(len);
    // TODO：copy函数是直接复制的，复杂度是线性的
    // 将要写入的数据复制到可写位置
    std::copy(str, str + len, BeginWrite());
    // 更新写入位置
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}
// 确保可写字节数足够
void Buffer::EnsureWriteable(size_t len) {
    // 如果可写空间不够直接腾出一片空间
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}
// 从文件描述符 fd 中读取数据到缓冲区（仿照moduo网络库）
ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    char buff[65535];
    struct iovec iov[2];
    const size_t writable = WritableBytes();
    /* 分散读， 保证数据全部读完 */
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    // 这表示如果数据长度超过了可写入的字节数，剩余的数据将被读取到 buff 中。
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);
    // len（已读字节数） =readv (文件描述符socketfd iovec结构体 iovec缓冲区区块个数)
    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        // 写入本身的缓冲区
        writePos_ += len;
    }
    else {
        // 将可读位置直接设置为buffer最后
        writePos_ = buffer_.size();
        // 将文件剩余部分直接添加到buffer里（一定会触发makespace）
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    size_t readSize = ReadableBytes();
    //len==已经写入的数据长度， 将所有要发送的数据都写入发送缓冲区
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    // 读完数据==发送完数据
    readPos_ += len;
    return len;
}
// 返回缓冲区数据起始位置的非常量指针
char* Buffer::BeginPtr_() {
    return &*buffer_.begin();
}
// 返回缓冲区数据起始位置的常量指针
const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

// 如果缓冲区大小可以满足要求写入的大小，那么就直接删除已经读过的数据，并且将未读的数据移动到最前面
// 如果缓冲区大小不够，那么就扩大缓冲区大小
void Buffer::MakeSpace_(size_t len) {
    if(WritableBytes() + PrependableBytes() < len) {
        // TODO：我认为直接拓展缓冲区大小的整数倍最好(reszie是非常耗时的)
        // 直接扩展出所需的空间大小
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        size_t readable = ReadableBytes();
        // BeginPtr_() + readPos_ 到 BeginPtr_() + writePos_ 之间的数据将被复制到 BeginPtr_() 开始的位置
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        // TODO：这不是一定会触发断言嘛？
        assert(readable == ReadableBytes());
    }
}