/*
 * @Author       : mark
 * @Date         : 2020-06-16
 * @copyleft Apache 2.0
 */ 
#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    static SqlConnPool *Instance();

    MYSQL *GetConn();
    void FreeConn(MYSQL * conn);
    int GetFreeConnCount();

    void Init(const char* host, int port,
              const char* user,const char* pwd, 
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    // 最大链接数
    int MAX_CONN_;
    // 使用链接
    int useCount_;
    // 空闲链接
    int freeCount_;

    // 使用STL的queue创建的连接对象池
    std::queue<MYSQL *> connQue_;
    std::mutex mtx_;

    sem_t semId_;
};


#endif // SQLCONNPOOL_H