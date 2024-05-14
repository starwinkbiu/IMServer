#include <sys/types.h>
#include <sys/epoll.h>

#define _DEF_EPOLL_LISTEN_NUM           (100000)
#define _DEF_EPOLL_WAITRECV_NUM         (200)

#define _DEF_EPOLL_LISTEN_MODEL         (EPOLLIN)
#define _DEF_EPOLL_CLIENT_MODEL         (EPOLLIN | EPOLLET | EPOLLONESHOT)

#define _DEF_EPOLL_RECV_MAX_COUNT       (4096)

class CKernel;
class EpollManager;

// 调用处理函数的arg模板
struct deal_data_arg_t{
    ~deal_data_arg_t();
    int iFd;
    char* szBuf;
    int iBufLen;
};

struct buffer_t{
    buffer_t();
    ~buffer_t();
    // 长度字节的长度（正常为4, 如果小于 4 就说明长度没有接收完整）
    int sizeLen;
    // 预期内容长度
    int contentSize;
    // 内容指针
    int pos;
    char* buffer;
    void initBuffer();
    void allocBuffer();
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
    EpollManager(CKernel* kernel);
    EpollManager(CKernel* kernel, int MaxListen);
    ~EpollManager();
    // 初始化 EpollManager
    void initEpollManager(int MaxListen = _DEF_EPOLL_LISTEN_NUM);
    // epoll 事件循环
    void EventLoop();
    // 发送数据（不使用多线程发送）
    void sendData(int iClientFd, char* szSendBuf, int iSize);
    // 多线程处理接收的数据
    static void* clientSocketRecv(void* arg);
    // 多线程处理接收客户端连接
    static void* listenSocketAccept(void* arg);
    // 为 sock 设置非阻塞模式
    static void setNonBlock(int iFd);
};
