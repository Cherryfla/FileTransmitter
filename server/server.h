#ifndef _SERVER_H_
#define _SERVER_H_

#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

#define CMD_PORT 8080
#define TRAN_PORT 8081
#define REDIS_PORT 6379
#define BSIZE 1024
#define COMMAND_NUM 2

const char RedisKey[COMMAND_NUM][20]=
{
    "SERVER:GET:FILE",
    "SERVER:GET:DIR"
};

class Command{
    private:
        char command[5];
        char arg[BSIZE];
    public:
        Command();
        void Init(char *fCommand);
        char* GetCommand();
        char* GetArg();
};

inline int EchoBack(int nConn, const char nMessage[]) //向连接返回信息
{
    return write(nConn, nMessage, BSIZE);
}
inline void ErrorExit(const char nError[])            //错误提示并推出
{
    perror(nError);
    exit(EXIT_FAILURE);
}

int CreateSocket(int fPort);                            //创建套接字
int AcceptConnection(int fd, unsigned int wait_seconds);//给定时限接受套接字
int Split(char *fPath, char *nResult);                  //得到目录下的文件列表
ssize_t SendFile(int out_fd, int in_fd, off_t * offset, size_t count);//传送文件
void DealDir(int fConn, Command* nCommand);             //处理GET请求且为目录的情况
void DealFile(int nConn, Command* nCommand);            //处理GET请求且为文件的情况
int FileTransmit(void* arg);                            //处理GET命令

#endif
