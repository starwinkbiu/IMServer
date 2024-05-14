#ifndef CKERNEL_H
#define CKERNEL_H

struct task_t;

class EpollManager;
class ThreadPool;
class CLogic;

class CKernel{
public:
    static CKernel* kernel;
    EpollManager* m_pEpollManager;
    ThreadPool* m_pThreadPool;
    CLogic* m_pLogic;
    CKernel();
    CKernel(int _threadMax, int _threadMin, int _max, int _MaxListen);
    ~CKernel();
    // 启动 epoll 事件监听循环
    void EpollEventLoop();
    // kernel：调用EpollManager的sendData函数
    void sendData(int iClientFd, char* szSendBuf, int iSize);
    // kernel：调用 CLogic 中的deaData函数来处理接收到的数据
    static void* dealData(void* arg);
    // kernel：添加任务到任务队列
    void addTask(task_t task);
};

#endif // CKERNEL_H
