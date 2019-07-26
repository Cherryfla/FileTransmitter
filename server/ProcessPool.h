#ifndef _PROCESS_POOL_H_
#define _PROCESS_POOL_H_
#include <sys/types.h>  
#include <sys/socket.h>  
#include <netinet/in.h>  
#include <arpa/inet.h>  
#include <assert.h>  
#include <stdio.h>  
#include <unistd.h>  
#include <errno.h>  
#include <string.h>  
#include <fcntl.h>  
#include <stdlib.h>  
#include <sys/epoll.h>  
#include <signal.h>
#include <sys/wait.h> 
#include <sys/stat.h>

#define BSIZE 1024
class Process
{
    public:
        pid_t nPid;
        int nPipeFd[2];
};

class ProcessPool
{
    private:
        int nListenfd;                              //监听套接字
        int nProcessNumber;                         //进程总数
        int nIndex;                                 //子进程在池中的下标
        int nStopFlag;                              //停止标记
        int nEpollfd;                               //epoll文件描述符
        Process* nSubProcess;                       //子进程 
        ProcessPool(int nListenfd, int nProcess,int (*Route)(void *arg));

        void RunChild();
        void RunParent();
		void SetupSigPipe();
        int (*Route)(void* arg);
       static ProcessPool* Instance;
    public:
        //单例模式
        static ProcessPool* Create(int listenfd, int nProcess, int (*Route)(void *arg))
        {
            if(Instance == nullptr)
                Instance = new ProcessPool(listenfd, nProcess, Route);
            return Instance;
        }
        void Run();
};

int SetNonBlock(int nFd);
void AddFd(int nEpollfd, int fd);
void RemoveFd(int nEpollfd, int fd);

#endif