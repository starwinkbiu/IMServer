#ifndef EPOLLMANAGER_H
#define EPOLLMANAGER_H

#include <sys/types.h>
#include <sys/epoll.h>
#include <QMutex>
#include <QMap>
#include "packDef.h"

#define _DEF_EPOLL_LISTEN_NUM           (100000)
#define _DEF_EPOLL_WAITRECV_NUM         (200)

#define _DEF_EPOLL_LISTEN_MODEL         (EPOLLIN)
#define _DEF_EPOLL_CLIENT_MODEL         (EPOLLIN | EPOLLET | EPOLLONESHOT)

#define _DEF_EPOLL_RECV_MAX_COUNT       (4096)

#define CONTENTSIZE_MASK ((1 << (sizeof(short) * 8 - 1)) - 1)

class CKernel;
class EpollManager;
struct message_t;
struct event_t;

// 调用处理函数的arg模板
struct deal_data_arg_t{
    ~deal_data_arg_t();
    event_t* eventManager;
    message_t* mt;
    EpollManager* pThis;
//    char* szBuf;
//    int iBufLen;
};

typedef struct message_t{
    ~message_t();
    bool type; // 标识二进制数据还是协议数据
    short size; // 消息的长度（协议+内容）
    short protocol; // 协议
    char* content; // 消息内容
    void initMessage();
    void setMessage(const char *_content, int _contentsize, bool _type, short _protocol);
    void getMessage(const char *_content, int _allsize, bool _type, short _protocol);
} message_t;

struct buffer_t{
    buffer_t();
    ~buffer_t();
    // 长度字节的长度（正常为4, 如果小于 4 就说明长度没有接收完整）
    short sizeLen;
    // 预期内容长度
    short contentSize;
    // 内容指针
    int pos;
    char* buffer;
    message_t mt;
    void clearBuffer();
};

struct event_t{
    event_t(EpollManager* _epollManager);
    event_t(EpollManager* _epollManager, int iFd);
    event_t(EpollManager* _epollManager, int iFd, int iEpfd);
    ~event_t();
    int fd;
    int epfd;
    int hasAdd;
    buffer_t* Buffer;
    epoll_event event;
    // EpollManager 指针
    EpollManager* epollManager;
    void addEvent(int epfd, uint32_t events);
    void addEvent(uint32_t events);
    void delEvent(int epfd);
};

class EpollManager{
public:
    // kernel 对象, 非本类所有，析构函数无需考虑
    CKernel* m_pKernel;
    // 静态全局对象
    static EpollManager* G_EPOLL;
    // epoll 文件描述符
    int epfd;
    // 启动标志
    int m_iEpollRunning;
    // 监听套接字
    event_t* m_pListenEvent;
    // 函数映射表
    Func funcMap[_DEF_MAX_MAP_LEN];
    // 记录客户端心跳时间
    QMap<event_t*, qint64> heartMap;
    // 锁
    QMutex heartMapMutex;
    // timerThreadId
    pthread_t timerThreadId;
    EpollManager(CKernel* kernel);
    EpollManager(CKernel* kernel, int MaxListen);
    ~EpollManager();
    // 初始化 EpollManager
    void initEpollManager(int MaxListen = _DEF_EPOLL_LISTEN_NUM);
    // 初始化函数映射表
    void initFuncMap();
    // epoll 事件循环
    void EventLoop();
    // 发送数据（不使用多线程发送）
    void sendData(int iClientFd, char* szSendBuf, int iSize);
    // 底层数据处理模块
    void dealMessage(message_t* mt, event_t* eventManager);
    // 定时器
    static void* timer(void* arg);
    // 多线程处理接收的数据
    static void* clientSocketRecv(void* arg);
    // 多线程处理接收客户端连接
    static void* listenSocketAccept(void* arg);
    // 为 sock 设置非阻塞模式
    static void setNonBlock(int iFd);
    // --------协议处理函数--------
    static void dealHeartReq(void* arg);
};

#endif
