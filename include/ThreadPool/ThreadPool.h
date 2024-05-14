#include <pthread.h>
#include <signal.h>
#include "packDef.h"

#define _DEF_THREADPOOL_THREAD_MAX              (300)
#define _DEF_THREADPOOL_THREAD_MIN              (10)
#define _DEF_THREADPOOL_TASK_MAX                (50000)

#define lock(a)         pthread_mutex_lock(a)
#define wait(a, b)      pthread_cond_wait(a, b)
#define unlock(a)       pthread_mutex_unlock(a)
#define signal(a)       pthread_cond_signal(a)
#define sleeptime(a)    usleep(a * 1000000)

typedef void* (*FUNC)(void*);

class CKernel;

// 消费者参数结构体
struct create_thread_arg_t{
    int idx;
    void* object;
};

// 任务结构体
struct task_t {
    FUNC bussiness;
    void* arg;
};

// 线程信息结构体
struct threadinfo {
    threadinfo();
    threadinfo(int max, int min);
    ~threadinfo();
    // 以下变量用于管理者管理管理线程池
    // 管理包含：当线程太多的时候，删掉一些线程，当线程过少的时候，添加线程
    int threadRun;
    int threadMax;
    int threadMin;
    // 添加跟删除线程只由管理者进行，所以不存在多线程竞争 threadAlive 变量
    int threadAlive;
    // 线程执行自己获取的任务之前会先将 threadBusy 加一，所以会存在竞争，需要加锁
    int threadBusy;
    // 自杀线程数
    int threadKill;
    // 消费者数组
    pthread_t* tids;
    // 管理者
    pthread_t mid;
    bool running();
};

// 任务队列结构体
struct taskqueue {
    taskqueue();
    taskqueue(int _max);
    ~taskqueue();
    task_t* tasks;
    int rear;
    int front;
    int max;
    int cur;
    // 向队列添加任务
    bool addTask(task_t task);
    // 拿取任务
    int getTask(task_t& task);
    // 返回队列是否为空
    bool empty();
    // 返回队列中元素数量
    int count();
    // 返回队列是否已满
    bool full();
};

// 线程池
class ThreadPool{
public:
    // kernel 对象, 非本类所有，析构函数无需考虑
    CKernel* m_pKernel;
    // 全局线程池变量
    static ThreadPool* G_THREADPOOL;
    // 线程池管理参数，其中 threadBusy 为临界值
    threadinfo* pool;
    // 任务队列，临界值
    taskqueue* tq;
    // 条件变量&锁
    pthread_mutex_t lock_task;
    pthread_mutex_t lock_thread_busy;
    pthread_mutex_t lock_thread_alive;
    pthread_mutex_t lock_thread_kill;
    pthread_mutex_t lock_thread_run;
    pthread_cond_t cond_task_not_empty;
    pthread_cond_t cond_task_not_full;


    ThreadPool(CKernel* kernel);
    ThreadPool(CKernel* kernel, int _threadMax, int _threadMin, int max);
    ~ThreadPool();

    // 初始化线程池
    void initThreadPool(int _threadMax, int _threadMin, int max);
    // 消费者
    static void* customer(void* _arg);
    // 添加任务
    void addTask(task_t task);
    // 添加管理者
    static void* manager(void* arg);
    // 查看线程是否存活
    bool ifThreadAlive(pthread_t tid);
};
