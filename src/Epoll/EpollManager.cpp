#include <sys/epoll.h>
#include <iostream>
#include "EpollManager.h"
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include "ThreadPool.h"
#include "CKernel.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

using namespace std;

EpollManager* EpollManager::G_EPOLL;

// deal_data_arg_t 析构函数
deal_data_arg_t::~deal_data_arg_t(){
    if(szBuf){
        delete szBuf;
    }
    szBuf = NULL;
    iFd = 0;
    iBufLen = 0;
}

// buffer_t 构造函数
buffer_t::buffer_t()
{
    buffer = NULL;
    initBuffer();
}

// buffer_t 析构函数
buffer_t::~buffer_t(){
    initBuffer();
}

// buffer 进行初始化
void buffer_t::initBuffer(){
    sizeLen = 0;
    pos = 0;
    contentSize = 0;
    if(buffer){
        delete buffer;
    }
    buffer = NULL;
}

// 如果 contentSize 不为 0 就为 buffer 申请空间
void buffer_t::allocBuffer(){
    if(buffer){
        delete buffer;
    }
    if(contentSize > 0){
        buffer = new char[contentSize];
    }
}

// event_t 构造函数
event_t::event_t(EpollManager* _epollManager){
    // 初始化 Buffer 状态
    Buffer = new buffer_t;
    // 初始化变量
    fd = -1;
    epfd = -1;
    hasAdd = 0;
    event.data.ptr = (void*)this;
    epollManager = _epollManager;
}

// event_t 构造函数（int iFd）
event_t::event_t(EpollManager* _epollManager, int iFd){
    // 初始化 Buffer 状态
    Buffer = new buffer_t;
    // 初始化变量
    fd = iFd;
    epfd = -1;
    hasAdd = 0;
    event.data.ptr = (void*)this;
    epollManager = _epollManager;
}

// event_t 构造函数（int iFd, int iEpfd）
event_t::event_t(EpollManager* _epollManager, int iFd, int iEpfd){
    // 初始化 Buffer 状态
    Buffer = new buffer_t;
    // 初始化变量
    fd = iFd;
    epfd = iEpfd;
    hasAdd = 0;
    event.data.ptr = (void*)this;
    epollManager = _epollManager;
}

// event_t 析构函数
event_t::~event_t(){
    // 首先删除 epoll 对本 event 的监听
    delEvent(epfd);
    // 删除 Buffer 套接字接收状态
    delete Buffer;
    // 关闭套接字
    close(fd);
    // 清除变量
    fd = 0;
    epfd = 0;
    event.data.ptr = NULL;
    epollManager = NULL;
}

// 添加 events 到 epoll 中（int _epfd, uint32_t events）
void event_t::addEvent(int _epfd, uint32_t events){
    event.events = events;
    int op;
    if(hasAdd){
        op = EPOLL_CTL_MOD;
    }else{
        op = EPOLL_CTL_ADD;
    }
    if(epoll_ctl(_epfd, op, fd, &event) == -1){
        cout << __func__ << " error: ";
        cout << "add or mod failed" << endl;
        return;
    }
    hasAdd = 1;
}

// 添加 events 到 epoll 中（uint32_t events）
void event_t::addEvent(uint32_t events){
    event.events = events;
    int op;
    if(hasAdd){
        op = EPOLL_CTL_MOD;
    }else{
        op = EPOLL_CTL_ADD;
    }
    if(epoll_ctl(epfd, op, fd, &event) == -1){
        cout << __func__ << " error: ";
        cout << "add or mod failed" << endl;
        return;
    }
    hasAdd = 1;
}

// 将 events 从 epoll 中删除
void event_t::delEvent(int epfd){
    if(hasAdd){
        if(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, 0) == -1){
            cout << __func__ << " error: ";
            cout << "del failed" << endl;
            return;
        }
        hasAdd = 0;
    }
}

// EpollManager 构造函数
EpollManager::EpollManager(CKernel* kernel){
    // 初始化 kernel
    m_pKernel = kernel;
    // 初始化静态类对象
    G_EPOLL = this;
    // 初始化 Epoll 和监听套接字
    initEpollManager();
}

// EpollManager 构造函数（int MaxListen）
EpollManager::EpollManager(CKernel* kernel, int MaxListen){
    // 初始化 kernel
    m_pKernel = kernel;
    // 初始化静态类对象
    G_EPOLL = this;
    // 初始化监听 epoll 和监听套接字
    initEpollManager(MaxListen);
}

// EpollManager 析构函数
EpollManager::~EpollManager(){
    // 停止监听
    m_iEpollRunning = 0;
    // 删除 m_pKernel
    m_pKernel = 0;
    // 删除监听套接字
    delete m_pListenEvent;
    epfd = 0;
}

// 初始化 EpollManager
void EpollManager::initEpollManager(int MaxListen){
    // 创建 epoll
    epfd = epoll_create(MaxListen);
    if(epfd == -1){
        cout << __func__ << " error: ";
        cout << errno << endl;
        exit(0);
    }
    // 初始化 listenSock
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    m_pListenEvent = new event_t(this, listenFd, epfd);
    if(m_pListenEvent->fd == -1){
        cout << __func__ << " error: ";
        cout << errno << endl;
        exit(0);
    }
    // 绑定套接字
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    addr.sin_port = htons(12345);
    addr.sin_family = AF_INET;
    // 监听
    if(bind(m_pListenEvent->fd, (sockaddr*)&addr, sizeof(addr)) == -1){
        cout << __func__ << " error: ";
        cout << errno << endl;
        exit(0);
    }
    if(listen(m_pListenEvent->fd, 128) == -1){
        cout << __func__ << " error: ";
        cout << errno << endl;
        exit(0);
    }
    // 设置套接字为非阻塞

    // 将 listenSock 添加到 epoll 中
    m_pListenEvent->addEvent(epfd, EPOLLIN | EPOLLET);
    // 启动循环监听事件
    m_iEpollRunning = 1;
}

// epoll 事件循环 (EPOLLET + EPOLLONESHOT)
void EpollManager::EventLoop(){
    int ready;
    task_t task;
    epoll_event* recvEvents = new epoll_event[_DEF_EPOLL_WAITRECV_NUM];
    // 开启事件监听循环
    while(m_iEpollRunning){
        // 等待事件到来（0 => 非阻塞）
        ready = epoll_wait(epfd, recvEvents, _DEF_EPOLL_WAITRECV_NUM, 0);
        // 处理接收到的事件
        for(int i=0; i<ready; i++){
            epoll_event event = recvEvents[i];
            event_t* eventManager = (event_t*)event.data.ptr;
            if(event.events & EPOLLIN){
                task.arg = eventManager;
                // 如果是listenSock，就执行accept操作
                // 如果是客户端套接字，就添加一个接收数据的任务到线程池
                task.bussiness = eventManager->fd == m_pListenEvent->fd \
                        ? listenSocketAccept : clientSocketRecv;
                // 将任务添加进任务队列
                m_pKernel->addTask(task);
            }else if(event.events == EPOLLOUT){
                continue;
            }
        }
    }
    delete []recvEvents;
}

// 多线程接收数据
void* EpollManager::clientSocketRecv(void* arg){
    event_t* eventManager = (event_t*)arg;
    int fd = eventManager->fd;
    // fd 对应的缓存状态空间，用来存储当前 fd 的接收状态
    buffer_t* Buffer = eventManager->Buffer;
    // 存储接收数据长度
    int recvLen;
    // 循环获取数据, 如果接收长度不为 0 说明有数据，就继续接收
    do{
        // 首先判断长度是否读取完整
        if((unsigned long)Buffer->sizeLen < sizeof(Buffer->contentSize)){
            // 不完整，继续读取
            // 因为可能多个线程会同时接收同一个套接字的内容，读取不是线程安全操作，需要加锁
            recvLen = recv( fd, (char*)(&Buffer->contentSize) + eventManager->Buffer->sizeLen, \
                            sizeof(Buffer->contentSize)-eventManager->Buffer->sizeLen, 0);
            if(recvLen == -1){
                if(errno != EWOULDBLOCK){
                    // 客户端连接套接字错误
                    break;
                }else{
                    // 如果是 EWOULDBLOCK 的话， 保存Buffer状态， 重新设置 EPOLLONESHOT，
                    // 直接退出接收线程
//                    Buffer->sizeLen += recvLen;
                    // 重置 EPOLLONESHOT 事件
                    eventManager->addEvent(_DEF_EPOLL_CLIENT_MODEL);
                    // 将 Buffer 置 0 防止数据泄漏
                    Buffer = NULL;
                    return NULL;
                }
            }else if(recvLen == 0){
                // 客户端正常退出
                break;
            }else{
                // 保存 Buffer 状态
                eventManager->Buffer->sizeLen += recvLen;
            }
        }
        // 如果长度接收完成并且contentSize要大于零，就继续接收数据
        if( (unsigned long)Buffer->sizeLen >= sizeof(Buffer->contentSize) && (eventManager->Buffer->contentSize > 0)){
            // 如果长度大于 _DEF_EPOLL_RECV_MAX_COUNT 则丢弃此包
            if(Buffer->contentSize > _DEF_EPOLL_RECV_MAX_COUNT){
                // 直接无视此包
                // 重置 Buffer 状态
                Buffer->initBuffer();
                // 清空 fd 当前的 Buffer 则状态：数值置空， 删除 buffer 申请的空间
                Buffer = NULL;
                return NULL;
            }
            // 判断数据是否是接收完毕
            if(Buffer->pos >= Buffer->contentSize){
                // 接收完毕，则将数据打包成任务，丢给线程池去处理
                // 因为是在不同线程处理的数据，所以不能直接把 Buffer->buffer 发送给处理函数，
                // 因为 Buffer 在本次接收完毕之后就会被释放掉， 也就是说， Buffer->buffer 也同样
                // 会被释放，所以需要使用一个 tmpBuffer 来拷贝数据，然后发送给 dealData 函数
                // 去处理， 等 dealData 将tmpBuffer中的数据处理完毕后释放即可
                task_t task;
                task.bussiness = eventManager->epollManager->m_pKernel->dealData;
                deal_data_arg_t* del_data_arg = new deal_data_arg_t;
                del_data_arg->szBuf = new char[Buffer->contentSize];
                memcpy(del_data_arg->szBuf, Buffer->buffer, Buffer->contentSize);
                del_data_arg->iFd = fd;
                del_data_arg->iBufLen = Buffer->contentSize;
                task.arg = del_data_arg;
                eventManager->epollManager->m_pKernel->addTask(task);
                // 清空 fd 当前的 Buffer 则状态：数值置空， 删除 buffer 申请的空间
                Buffer->initBuffer();
                // 一段数据帧接收处理完毕，继续进行下一帧的接收
            }else{
                // 数据未接收完毕，继续接收
                // 首先判断是否是第一次接收
                if(Buffer->pos == 0){
                    // 如果是第一次，先为 Buffer 申请空间
                    Buffer->allocBuffer();
                }
                // 开始接收
                recvLen = recv( fd, Buffer->buffer + Buffer->pos,  \
                                Buffer->contentSize - Buffer->pos, 0);
                if(recvLen == -1){
                    if(errno != EWOULDBLOCK){
                        // 客户端连接套接字错误
                        break;
                    }else{
                        // 如果是 EWOULDBLOCK 的话， 保存Buffer状态， 重新设置 EPOLLONESHOT，
                        // 直接退出接收线程
//                        Buffer->sizeLen += recvLen;
                        eventManager->addEvent(_DEF_EPOLL_CLIENT_MODEL);
                        // 将 Buffer 置 0 防止数据泄漏
                        Buffer = NULL;
                        return NULL;
                    }
                }else if(recvLen == 0){
                    // 客户端正常退出
                    break;
                }else{
                    // 保存 Buffer 状态
                    Buffer->pos += recvLen;
                }
            }
        }

    }while(recvLen > 0);
    // 处理远端断开连接，或者其他错误
    delete eventManager;
    if(recvLen == -1){
        // 如果远端错误断开连接
        // 关闭套接字
        // 把套接字从epoll上删除
        // 清空event_t空间
        // 清空当前局部变量数据，防止数据泄漏
        // 可以暂时不关闭套接字或者说暂时把状态保存起来，以备后用
        if(errno == ECONNRESET){
            // errno -> 104：ECONNRESET 远端重置连接
            cout << "[" << fd << "] ECONNRESET" << endl;
        }else{
            cout << "[" << fd << "] error offline: " << errno << endl;
        }
    }else if(recvLen == 0){
        // 如果远端正常断开连接
        // 关闭套接字
        // 把套接字从 epoll 上删除
        // 清空 Buffer 状态
        // 清空 event_t 空间
        // 清空当前局部变量数据，防止数据泄漏
        cout << "[" << fd << "] offline" << endl;
    }
    return NULL;
}

// 多线程处理接收客户端连接 (EPOLLET + EPOLLONESHOT)
void* EpollManager::listenSocketAccept(void* arg){
    event_t* eventManager = (event_t*)arg;
    int fd = eventManager->fd;
    int epfd = eventManager->epfd;
    struct sockaddr_in caddr;
    int size = sizeof(caddr);
    int cfd = accept(fd, (sockaddr*)&caddr, (socklen_t*)&size);
    if(cfd == -1){
        cout << __func__ << " error: ";
        cout << "accept " << errno << endl;
        return NULL;
    }else{
        cout << "accept " << cfd << " success" << endl;
    }
    // 设置 cfd 为非阻塞模式
    EpollManager::setNonBlock(cfd);
    // 申请事件管理者
    event_t* event = new event_t(eventManager->epollManager, cfd, epfd);
    // 将事件添加进 epoll
    event->addEvent(_DEF_EPOLL_CLIENT_MODEL);
    return NULL;
}

// 为 sock 设置非阻塞模式
void EpollManager::setNonBlock(int iFd){
    int flags = -1;
    if(iFd){
        flags = fcntl(iFd, F_GETFL, 0);
        if(flags == -1){
            cout << __func__ << " error: ";
            cout << "setNonBlock " << errno << endl;
            return;
        }
        flags |= O_NONBLOCK;
        if(fcntl(iFd, F_SETFL, flags) == -1){
            cout << __func__ << " error: ";
            cout << "setNonBlock " << errno << endl;
            return;
        }
    }
}





