#include <QCoreApplication>]
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include "CKernel.h"

using namespace std;

void* add(void* arg){
    int c = *(int*)arg + *((int*)arg+1);
    cout << c << endl;
    return (void*)&c;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
//    int nums[] = {1, 2};
//    task_t task;
//    task.bussiness = add;
//    task.arg = (void*)nums;
//    for(int i=0; i<1000; i++){
//        pool->addTask(task);
//    }
    // 开启 Kernel ：初始化线程池、初始化 EpollManager 、初始化CLogic
    CKernel* kernel = new CKernel(50, 5, 50000, 100000);
    kernel->EpollEventLoop();

//    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//    if(fd == -1){
//        cout << "socket" << errno << endl;
//        return 0;
//    }

//    struct sockaddr_in addr;
//    addr.sin_addr.s_addr = inet_addr("0.0.0.0");
//    addr.sin_port = htons(12345);
//    addr.sin_family = AF_INET;

//    if(bind(fd, (sockaddr*)&addr, sizeof(addr)) == -1){
//        cout << "bind" << errno << endl;
//        return 0;
//    }

//    if(listen(fd, 128) == -1){
//        cout << "listen" << errno << endl;
//        return 0;
//    }

//    while(1){
//        struct sockaddr_in caddr;
//        int size = sizeof(caddr);
//        int cfd = accept(fd, (sockaddr*)&caddr, (socklen_t*)&size);
//        if(fd == -1){
//            cout << "accept" << errno << endl;
//            return 0;
//        }
//        char s[1024];
//        int res = recv(cfd, s, 1024, 0);
//        cout << s << endl;
//    }

    while(1){
        usleep(500000);
    }

    return a.exec();
}
