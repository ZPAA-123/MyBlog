# MyBlog
使用C11muduo并且写了一个小前端

前端使用BootStrap v5 模板生成，使用简单的JS代码负责给后端传输数据。

后端使用自己开发的C++服务器，此服务器有以下功能：

1.利用IO复用技术Epoll与线程池实现多线程的Reactor高并发模型
2. 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求
3.利用标准库容器封装char，实现自动增长的缓冲区
4.基于小根堆实现的定时器，关闭超时的非活动连接
5.利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态
6.利用RAII机制实现了数据库连接池，减少数据库连接建立与关闭的开销.

# 数据库构建

// 建立yourdb库
create database yourdb;

// 创建user表
USE yourdb;
CREATE TABLE user(
    username char(50) NULL,
    password char(50) NULL
)ENGINE=InnoDB;

// 添加数据
INSERT INTO user(username, password) VALUES('name', 'password');
