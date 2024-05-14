#include "ThreadPool.h"
#include "packDef.h"
#include <iostream>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

using namespace std;

ThreadPool* ThreadPool::G_THREADPOOL;

threadinfo::threadinfo() {
    threadRun = 1;
    threadMax = _DEF_THREADPOOL_THREAD_MAX;
    threadMin = _DEF_THREADPOOL_THREAD_MIN;
    threadAlive = 0;
    threadBusy = 0;
    threadKill = 0;
    mid = 0;
    tids = (pthread_t*)malloc(threadMax * sizeof(pthread_t));
    memset(tids, 0, threadMax * sizeof(pthread_t));
}

threadinfo::threadinfo(int _max, int _min) {
    threadRun = 1;
    threadMax = _max;
    threadMin = _min;
    threadAlive = 0;
    threadBusy = 0;
    threadKill = 0;
    mid = 0;
    tids = (pthread_t*)malloc(threadMax * sizeof(pthread_t));
    memset(tids, 0, threadMax * sizeof(pthread_t));
}

threadinfo::~threadinfo()
{

    // 变量置 0
    threadRun = 0;
    // 等待线程结束
    for(int i=0; i<threadMax; i++){
        if(tids[i]){
            pthread_join(tids[i], NULL);
        }
    }
    // 释放 tids 申请的空间
    bzero(tids, threadMax * sizeof(pthread_t));
    free(tids);
    threadMax = 0;
    threadMin = 0;
    threadAlive = 0;
    threadBusy = 0;
    threadKill = 0;
    mid = 0;
}

bool threadinfo::running(){
    return threadRun;
}

taskqueue::taskqueue(){
    rear = 0;
    front = 0;
    max = _DEF_THREADPOOL_TASK_MAX;
    cur = 0;
    tasks = (task_t*)malloc(max * sizeof(task_t));
}

taskqueue::taskqueue(int _max) : max(_max){
    rear = 0;
    front = 0;
    cur = 0;
    tasks = (task_t*)malloc(max * sizeof(task_t));
}

taskqueue::~taskqueue() {
    // 释放 tasks 空间
    bzero(tasks, max * sizeof(task_t));
    free(tasks);
    rear = 0;
    front = 0;
    cur = 0;
    max = 0;
}

bool taskqueue::addTask(task_t task) {
    if(cur < max){
        tasks[front] = task;
        front = (front + 1) % max;
        cur++;
        return 0;
    }
    return 1;
}

int taskqueue::getTask(task_t& task)
{
    if(cur > 0){
        task = tasks[rear];
        rear = (rear + 1) % max;
        cur--;
        return 0;
    }
    return 1;
}

bool taskqueue::empty() {
    return cur == 0;
}

int taskqueue::count() {
    return cur;
}

bool taskqueue::full() {
    return cur == max;
}

// 初始化线程池
ThreadPool::ThreadPool(CKernel* kernel)
{
    // 初始化 kernel
    m_pKernel = kernel;
    // 线程参数初始化
    pool = new threadinfo();
    // 队列初始化
    tq = new taskqueue();
    // 初始化条件变量&锁
    pthread_mutex_init(&lock_task, NULL);
    pthread_mutex_init(&lock_thread_busy, NULL);
    pthread_mutex_init(&lock_thread_kill, NULL);
    pthread_mutex_init(&lock_thread_alive, NULL);
    pthread_cond_init(&cond_task_not_full, NULL);
    pthread_cond_init(&cond_task_not_empty, NULL);
    // 全局静态线程池指针
    G_THREADPOOL = this;
    // 创建初始线程
    for(int i=0; i<pool->threadMin; i++){
        if(pthread_create(&pool->tids[i], NULL, customer, this)){
            cout << __func__ << " error: ";
            cout << "thread create error" << endl;
            this->~ThreadPool();
            return;
        }
    }
    // 创建管理者
    if(pthread_create(&pool->mid, NULL, manager, this)){
        cout << __func__ << " error: ";
        cout << "manager create error" << endl;
        this->~ThreadPool();
        return;
    }
}

// 初始化线程池 int _threadMax, int _threadMin, int _max
ThreadPool::ThreadPool(CKernel* kernel, int _threadMax, int _threadMin, int _max)
{
    if(_threadMax < _threadMin){
        cout << __func__ << " error: ";
        cout << "_threadMax must bigger than _threadMin" << endl;
        return;
    }
    // 初始化 kernel
    m_pKernel = kernel;
    // 线程参数初始化
    pool = new threadinfo(_threadMax, _threadMin);
    // 队列初始化
    tq = new taskqueue(_max);
    // 初始化条件变量&锁
    pthread_mutex_init(&lock_task, NULL);
    pthread_mutex_init(&lock_thread_busy, NULL);
    pthread_mutex_init(&lock_thread_kill, NULL);
    pthread_mutex_init(&lock_thread_alive, NULL);
    pthread_cond_init(&cond_task_not_full, NULL);
    pthread_cond_init(&cond_task_not_empty, NULL);
    // 全局静态线程池指针
    G_THREADPOOL = this;
    // 初始化线程
    for(int i=0; i<pool->threadMin; i++){
        create_thread_arg_t* arg = new create_thread_arg_t;
        arg->object = this;
        arg->idx = i;
        if(pthread_create(&pool->tids[i], NULL, customer, arg)){
            cout << __func__ << " error: ";
            cout << "thread create error" << endl;
            this->~ThreadPool();
            return;
        }else{
//            cout << __func__ << " success: ";
//            cout << "customer " << pthread_self() << " running..." << endl;
            pool->threadAlive++;
        }
    }
    // 创建管理者
    if(pthread_create(&pool->mid, NULL, manager, this)){
        cout << __func__ << " error: ";
        cout << "manager create error" << endl;
        this->~ThreadPool();
        return;
    }
}

// 销毁线程池
ThreadPool::~ThreadPool()
{
    // 结束所有线程
    delete pool;
    // 删除队列
    delete tq;
    // 删除锁和条件变量
    pthread_mutex_destroy(&lock_task);
    pthread_mutex_destroy(&lock_thread_busy);
    pthread_mutex_destroy(&lock_thread_kill);
    pthread_mutex_destroy(&lock_thread_run);
    pthread_cond_destroy(&cond_task_not_full);
    pthread_cond_destroy(&cond_task_not_empty);
    // 清理静态线程池变量
    G_THREADPOOL = NULL;
    // kernel = 0
    m_pKernel = 0;
}

// 向线程池里添加任务
void ThreadPool::addTask(task_t task){
    if(pool->threadRun){
        // 申请任务队列锁
        lock(&lock_task);
        // 判断条件
        while(tq->full()){
            wait(&cond_task_not_full, &lock_task);
            if(!pool->running()){
                cout << __func__ << " success: ";
                cout << "exit" << endl;
                unlock(&lock_task);
                return;
            }
        }
        // 添加任务
        tq->addTask(task);
        // 解锁
        unlock(&lock_task);
        // 通知消费者有任务可以拿了
        signal(&cond_task_not_empty);
    }
}

// 消费者循环拿取任务 static
void* ThreadPool::customer(void* _arg){
//    cout << "yes" << endl;
    create_thread_arg_t* arg = (create_thread_arg_t*)_arg;
    ThreadPool* pThis = (ThreadPool*)arg->object;
    int idx = arg->idx;
    // 判断线程池是否在运行
//    lock(&pThis->lock_thread_run);
//    int run = pThis->pool->threadRun;
//    unlock(&pThis->lock_thread_run);
    while(pThis->pool->threadRun){
        // 申请任务队列锁
        lock(&pThis->lock_task);
        while(pThis->tq->empty()){
            wait(&pThis->cond_task_not_empty, &pThis->lock_task);
            if(!pThis->pool->running()){
                cout << __func__ << " success: ";
                cout << "customer " << pthread_self() << " exit success" << endl;
                unlock(&pThis->lock_task);
                return NULL;
            }
            if(pThis->pool->threadKill){
                // 更新 tids
                pThis->pool->tids[idx] = 0;
                // 更新 threadKill
                pThis->pool->threadKill--;
                // 减去对应 Alive 线程数
                lock(&pThis->lock_thread_alive);
                pThis->pool->threadAlive--;
                unlock(&pThis->lock_thread_alive);
//                cout << "delete thread [" << arg->idx << "] success" << endl;
                // 清除 _arg 空间
                delete (create_thread_arg_t*)_arg;
                // 解锁
                unlock(&pThis->lock_task);
                // 退出线程
                return NULL;
            }
        }
        // 接取任务
        task_t task;
        pThis->tq->getTask(task);
        // 解锁
        unlock(&pThis->lock_task);
        // 如果threadKill==0,唤醒下一个消费者
        // 如果不为0, 就说明manager正在进行
        signal(&pThis->cond_task_not_empty);
        // 添加Busy线程：添加锁
        lock(&pThis->lock_thread_busy);
        pThis->pool->threadBusy++;
        unlock(&pThis->lock_thread_busy);
        // 执行任务
        if(task.bussiness){
            task.bussiness(task.arg);
        }
        // 减少Busy线程：添加锁
        lock(&pThis->lock_thread_busy);
        pThis->pool->threadBusy--;
        unlock(&pThis->lock_thread_busy);
//        lock(&pThis->lock_thread_run);
//        run = pThis->pool->threadRun;
//        unlock(&pThis->lock_thread_run);
    }
    return NULL;
}

// 管理者
void* ThreadPool::manager(void* arg){
    ThreadPool* pThis = (ThreadPool*)arg;
    int _taskCount;
    int _busy;
    int _alive;
    int _free;
    int _threadMax = pThis->pool->threadMax;
    int _threadMin = pThis->pool->threadMin;
    int addNum = 0;
    int delNum = 0;
    int i;
    int count = 0;
    int nsec = 50000000;
    int sleepTime = 10; // sec
//    lock(&pThis->lock_thread_run);
//    int run = pThis->pool->threadRun;
//    unlock(&pThis->lock_thread_run);
    while(pThis->pool->threadRun){
//        cout << pthread_create(a, NULL, ThreadPool::customer, NULL) << endl;
        // 访问队列加锁，获取任务数量
        lock(&pThis->lock_task);
        _taskCount = pThis->tq->count();
        unlock(&pThis->lock_task);
        // 获取线程总数和繁忙线程以及空闲线程数量
        lock(&pThis->lock_thread_busy);
        _busy = pThis->pool->threadBusy;
        unlock(&pThis->lock_thread_busy);
        lock(&pThis->lock_thread_alive);
        _alive = pThis->pool->threadAlive;
        unlock(&pThis->lock_thread_alive);
        _free = _alive - _busy;
//        if(_busy > 0)
//        cout << "busy: [" << _busy << "] alive: [" << _alive << "] _free: [" << _free << "]" << " taskCount: [" << _taskCount << "]" << endl;
        // 添加线程规则
        // 1、当任务队列中的任务数量多余空闲线程的时候
        // 2、当busy线程占总线程百分之70以上的时候
        // 3、当先线程数量必须小于threadmax
//        if(((float)_busy / _alive * 100 > 80) && _alive < _threadMax){
        if((_taskCount > _free ||(float)_busy / _alive * 100 > 70) && _alive < _threadMax){
            // 如果添加 thread_min 个线程之后，小于等于thread_max
            if(_alive + _threadMin <= _threadMax){
                addNum = _threadMin;
            }else{
                addNum = _threadMax - _alive;
            }
            // 添加 addNum 个新线程
            for(i=0; i<_threadMax && addNum; i++){
                if(pThis->pool->tids[i] == 0 || !pThis->ifThreadAlive(pThis->pool->tids[i])){
                    // 添加线程
                    create_thread_arg_t* arg = new create_thread_arg_t;
                    arg->idx = i;
                    arg->object = pThis;
                    lock(&pThis->lock_task);
                    if(pthread_create(&pThis->pool->tids[i], NULL, customer, arg)){
                        cout << __func__ << " error: ";
                        cout << "thread create error" << endl;
                        return NULL;
                    }else{
                        // 添加存活线程数
                        lock(&pThis->lock_thread_alive);
                        pThis->pool->threadAlive += 1;
                        unlock(&pThis->lock_thread_alive);
                    }
                    unlock(&pThis->lock_task);
                    // addNum减一
                    addNum--;
                }
            }
            // 添加完成，打印一下现状
//            cout << "busy: [" << _busy << "] alive: [" << _alive << "] _free: [" << _free << "]" << " taskCount: [" << _taskCount << "]" << endl;
            // 如果添加完成，就设置B需要在10秒之内，如果没有再次新增线程的话，才能执行
            count = (int)(1000000000 / nsec * sleepTime);
            cout << count << endl;
        }else{
            // 如果 count 小于等于 0 才能删除线程
            if(--count <= 0){
                // 删除线程规则
                // 1、busy线程占总线程数量的百分之33以下
                // 2、当前线程数必须大于 threadmin
                // 3、空闲状态的线程必须大于 threadmin（因为删除线程数最大为threadMin，而且能够被唤醒的线程只能是空闲线程）
                if((float)_busy / _alive * 100 < 33 && _alive > _threadMin){
                    // 如果_alive - thread_min 小于 thread_min
                    if(_alive - _threadMin < _threadMin){
                        delNum = _alive - _threadMin;
                    }else{
                        delNum = _threadMin;
                    }
                    pThis->pool->threadKill = delNum;
                    for(int i=0; i<delNum; i++){
                        signal(&pThis->cond_task_not_empty);
                    }
                }
                count = 0;
            }
        }
        timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = nsec;
        nanosleep(&ts, NULL);
    }
    return NULL;
}


bool ThreadPool::ifThreadAlive(pthread_t tid){
    if(pthread_kill(tid, 0) == -1){
        if(errno == ESRCH)
            return false;
    }
    return true;
}




















