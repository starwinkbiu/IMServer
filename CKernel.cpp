#include "CKernel.h"
#include "ThreadPool.h"
#include "EpollManager.h"
#include <iostream>
#include "CLogic.h"

using namespace std;

CKernel* CKernel::kernel;

// 构造函数
CKernel::CKernel(){
    m_pThreadPool = new ThreadPool(this);
    m_pEpollManager = new EpollManager(this);
//    m_pLogic = new CLogic(this);
}

// 构造函数（int _threadMax, int _threadMin, int _max, int _MaxListen）
CKernel::CKernel(int _threadMax, int _threadMin, int _max, int _MaxListen){
    m_pThreadPool = new ThreadPool(this, _threadMax, _threadMin, _max);
    m_pEpollManager = new EpollManager(this, _MaxListen);
//    m_pLogic = new CLogic(this);
}

// 析构函数
CKernel::~CKernel(){
    // 首先删除 EpollManager 防止新的监听事件到来
    delete m_pEpollManager;
    // 然后删除 ThreadPool 等待关闭所有线程
    delete m_pThreadPool;
    // 然后删除 CLogic
    delete m_pLogic;
}

// 启动 epoll 事件监听循环
void CKernel::EpollEventLoop(){
    m_pEpollManager->EventLoop();
}

// kernel：添加任务到任务队列
// EpollManager->ThreadPool 事件循环会使用线程池处理事件
void CKernel::addTask(task_t task){
    m_pThreadPool->addTask(task);
}

// kernel：根据协议头处理接收到的数据
// EpollManager->CLogic
void* CKernel::dealData(void *_arg){
    deal_data_arg_t* arg = (deal_data_arg_t*)_arg;
    int fd = arg->iFd;
    int bufLen = arg->iBufLen;
    char* buffer = arg->szBuf;
    // 调用 CLogic 中的 autoProtoDeal 函数根据协议头映射执行对应的处理函数
//    kernel->m_pLogic->autoProtoDeal(fd, buffer, bufLen);
//    cout << "[" << fd << "] recved: " << buffer << endl;
    delete arg;
    return NULL;
}
