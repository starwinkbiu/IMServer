#include <iostream>
#include <errno.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <time.h>
#include <QDateTime>
#include <QMutexLocker>

#include "EpollManager.h"
#include "ThreadPool.h"
#include "CKernel.h"

using namespace std;

EpollManager* EpollManager::G_EPOLL;

// deal_data_arg_t 析构函数
deal_data_arg_t::~deal_data_arg_t(){
    if(mt){
        delete mt;
    }
//    if(szBuf){
//        delete szBuf;
//    }
//    szBuf = NULL;
//    iFd = 0;
//    iBufLen = 0;
}

// message_t 析构函数
message_t::~message_t(){
    initMessage();
}

void message_t::initMessage()
{
   type = false;
   content = NULL;
   size = 0;
   protocol = 0;
}

// buffer_t 构造函数
buffer_t::buffer_t(): sizeLen(0), contentSize(0), pos(0), buffer(NULL){}

// buffer_t 析构函数
buffer_t::~buffer_t(){
    clearBuffer();
}

// buffer 进行初始化
void buffer_t::clearBuffer(){
    sizeLen = 0;
    pos = 0;
    contentSize = 0;
    mt.initMessage();
    if(buffer){
        delete buffer;
    }
    buffer = NULL;
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
    // 初始化函数映射表
    initFuncMap();
    // 创建 epoll
    epfd = epoll_create(MaxListen);
    if(epfd == -1){
        cout << __func__ << " error: ";
        cout << "epoll_create: "<< errno << endl;
        exit(0);
    }
    // 初始化 listenSock
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    // 设置套接字可以重复使用地址
    int opt = 1;
    if (setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        close(listenFd);
        exit(EXIT_FAILURE);
    }
    m_pListenEvent = new event_t(this, listenFd, epfd);
    if(m_pListenEvent->fd == -1){
        cout << __func__ << " error: ";
        cout << "event_t: " << errno << endl;
        exit(0);
    }
    // 绑定套接字
    struct sockaddr_in addr;
    addr.sin_addr.s_addr = inet_addr("192.168.150.128");
    addr.sin_port = htons(12345);
    addr.sin_family = AF_INET;
    // 监听
    if(bind(m_pListenEvent->fd, (sockaddr*)&addr, sizeof(addr)) == -1){
        cout << __func__ << " error: ";
        cout << "bind: " << errno << endl;
        exit(0);
    }
    if(listen(m_pListenEvent->fd, 128) == -1){
        cout << __func__ << " error: ";
        cout << "listen: " << errno << endl;
        exit(0);
    }
    // 设置套接字为非阻塞

    // 将 listenSock 添加到 epoll 中
    m_pListenEvent->addEvent(epfd, EPOLLIN | EPOLLET);
    // 启动循环监听事件
    m_iEpollRunning = 1;
}

void EpollManager::initFuncMap()
{
    funcMap[_DEF_SYS_HEART_REQ - 1] = &EpollManager::dealHeartReq;
}

// epoll 事件循环 (EPOLLET + EPOLLONESHOT)
void EpollManager::EventLoop(){
    int ready;
    task_t task;
    epoll_event* recvEvents = new epoll_event[_DEF_EPOLL_WAITRECV_NUM];
    // 首先开启timer线程
//    if(pthread_create(&timerThreadId, NULL, &EpollManager::timer, this)){
//        cout << __func__ << " error: ";
//        cout << "thread create error" << endl;
//        return;
//    }
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

void EpollManager::dealMessage(message_t *mt, event_t* eventManager)
{

    deal_data_arg_t* dt = new deal_data_arg_t;
    dt->eventManager = eventManager;
    dt->pThis = this;
    // 重新申请一个mt
    message_t* new_mt = new message_t;
    // 复制到新的new_mt
    new_mt->size = mt->size;
    new_mt->type = mt->type;
    new_mt->protocol = mt->protocol;
    new_mt->content = new char[new_mt->size + 1];
    memcpy(new_mt->content, mt->content, mt->size);
    new_mt->content[new_mt->size] = 0;
    dt->mt = new_mt;
    if(mt->type){
        // 初始化数据
        task_t task;
        task.bussiness = m_pKernel->dealData;
        task.arg = dt;
        // 发往上层处理器(多线程)
        m_pKernel->addTask(task);
        return;
    }
    // 否则底层直接处理
    funcMap[mt->protocol - _DEF_SYS_PROTOCOL_BASE - 1](dt);
    return;
}

void *EpollManager::timer(void *arg)
{
    EpollManager* pThis = (EpollManager*)arg;
    timespec tp;
    tp.tv_sec = 1;
    tp.tv_nsec = 0;
    while(pThis->m_iEpollRunning){
        // 获取时间戳
        qint64 curtime = QDateTime::currentSecsSinceEpoch();
        QMutexLocker locker(&pThis->heartMapMutex);
        for(auto ite = pThis->heartMap.begin(); ite != pThis->heartMap.end(); ++ite){
            event_t* eventManager = ite.key();
            qint64 heartbeat = ite.value();
            if(curtime - heartbeat >= 30){
                // 打印日志
                cout << "client [" << eventManager->fd << ":" << heartbeat << "] Timeout, closing the connect with this client" << endl;
                // 如果大于等于30秒, 与客户端断开连接
                delete eventManager;
                // 擦除与此连接相关的心跳记录
                pThis->heartMap.erase(ite);
            }
        }
        // 休眠1秒
        nanosleep(&tp, NULL);
    }
}

// 多线程接收数据
void* EpollManager::clientSocketRecv(void* arg){
    event_t* eventManager = (event_t*)arg;
    int fd = eventManager->fd;
    // fd 对应的缓存状态空间，用来存储当前 fd 的接收状态
    buffer_t* Buffer = eventManager->Buffer;
    // 存储接收数据长度
    int recvLen;
    cout << "clientSocketRecv" << endl;

    do{
        // 判断长度字节有没有接受完整
        if(Buffer->sizeLen < (int)sizeof(Buffer->contentSize)){
            // 接收长度字节
            recvLen = recv(fd, (char*)(&Buffer->contentSize) + eventManager->Buffer->sizeLen, \
                           sizeof(Buffer->contentSize)-eventManager->Buffer->sizeLen, 0);
            if(recvLen <= 0){
                // 可能出错了
                if(recvLen == 0){
                    // 代表客户端断开了连接
                    // 跳出循环处理错误
                    break;
                }
                // 代表出错了
                if(errno != EWOULDBLOCK){
                    // 代表不是缓冲区无数据
                    // 跳出循环处理错误
                    break;
                }
                // 是缓冲区无数据
                // 重置 EPOLLONESHOT 事件
                eventManager->addEvent(_DEF_EPOLL_CLIENT_MODEL);
                return NULL;
            }else{
                // 保存 Buffer 状态
                eventManager->Buffer->sizeLen += recvLen;
                // 判断接受完毕没有
                if(Buffer->sizeLen < (int)sizeof(Buffer->contentSize)){
                    // 如果还没有接受完毕，就继续接收
                    continue;
                }
                // 接受完毕，处理长度字节
                Buffer->mt.type = Buffer->contentSize & (CONTENTSIZE_MASK + 1);
                Buffer->mt.size = Buffer->contentSize & CONTENTSIZE_MASK;
                // 接受完毕，如果字节长度小于等于0, 关闭客户端连接
                if(Buffer->mt.size <= 0){
                    // 打印错误日志
                    cout << "client[" << fd << "] send nothing, shutting down the socket..." << endl;
                    // 删除eventManager
                    delete eventManager;
                    return NULL;
                }
                // 申请空间
                Buffer->buffer = new char[Buffer->mt.size + 1];
            }
        }
        // 长度字节接受完毕，开始接受数据字节
        recvLen = recv(fd, Buffer->buffer + Buffer->pos, Buffer->mt.size - Buffer->pos, 0);
        // 判断返回值
        if(recvLen <= 0){
            // 出现问题
            if(recvLen == 0){
                // 客户端断开了连接, 退出循环处理问题
                break;
            }
            if(errno != EWOULDBLOCK){
                // 代表不是缓冲区无数据
                // 跳出循环处理错误
                break;
            }
            // 是缓冲区无数据
            // 重置 EPOLLONESHOT 事件
            eventManager->addEvent(_DEF_EPOLL_CLIENT_MODEL);
            return NULL;
        }else{
            Buffer->pos += recvLen;
            // 判断数据是否接受完毕
            if(Buffer->pos < Buffer->mt.size){
                // 没有接受完毕，继续接受
                continue;
            }
            // 接受完毕，填充mt
            Buffer->mt.protocol = *(short*)Buffer->buffer;
            Buffer->mt.content = Buffer->buffer + sizeof(Buffer->mt.protocol);
            // 更改size
            Buffer->mt.size -= sizeof(Buffer->mt.protocol);
            Buffer->mt.content[Buffer->mt.size] = 0;
            // 处理数据
            eventManager->epollManager->dealMessage(&Buffer->mt, eventManager);
            // 清除Buffer
            Buffer->clearBuffer();
        }
    }while(true);
    if(recvLen == 0){
        cout << "client[" << fd << "] closed from server" << endl;
    }else{
        cout << "client[" << fd << "] curried something worng, shutting down the socket..." << endl;
    }
    delete eventManager;
    return NULL;

    /*
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
    */
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

void message_t::setMessage(const char *_content, int _contentsize, bool _type, short _protocol)
{
    content = new char[_contentsize + 1];
    type = _type;
    _protocol = protocol;
    size = _contentsize + sizeof(protocol);
    // 复制内容
    memcpy(content, _content, _contentsize);
}

void message_t::getMessage(const char *_content, int _allsize, bool _type, short _protocol)
{
    size = _allsize - sizeof(protocol);
    content = new char[size + 1];
    type = _type;
    _protocol = protocol;
    // 复制内容
    memcpy(content, _content, size);
}

void EpollManager::dealHeartReq(void *_arg)
{
    // 解析fd和mt
    deal_data_arg_t* arg = (deal_data_arg_t*)_arg;
    EpollManager* pThis = arg->pThis;
    event_t* eventManager = arg->eventManager;
    message_t* mt = arg->mt;
    cout << "message info: ";
    cout << "syspro: [" << mt->type << "] ";
    cout << "size: [" << mt->size << "] ";
    cout << "protocol: [" << mt->protocol << "]\n";
    cout << "content: \n" << mt->content << endl;
    // 开始处理心跳请求
    // 首先解析content
    QString qs(mt->content);
    QJsonDocument jd = QJsonDocument::fromJson(qs.toUtf8());
    if(jd.isNull()){
        cout << "QJsonDocument is null" << endl;
        return;
    }
    if(jd.isObject()){
        // 如果是对象，就解析对象
        QJsonObject object = jd.object();
        // 判断包不包含timestamp
        if(object.contains("TimeStamp")){
            // 如果包含，获取内容
            qint64 timestamp = static_cast<qint64>(object["TimeStamp"].toDouble());
            // 上锁
            QMutexLocker locker(&eventManager->epollManager->heartMapMutex);
            // 更新当前连接客户端的heartbeat时间
            pThis->heartMap[eventManager] = timestamp;
            cout << "recved client [" << eventManager->fd << ":" << timestamp << "] heart beat" << endl;
        }
    }
    // 删除arg
    delete arg;
    return;
}





