#ifndef _MY_REDIS_H_
#define _MY_REDIS_H_
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <hiredis/hiredis.h>

#define BSIZE 1024

class MyRedis{
    private:
        redisContext *context;
        redisReply *reply;
        bool isConnected;
    public:
        void Connect(const char *Ip, short nPort); //redis连接
        bool JudgeConnect();                    //判断是否已建立连接
        void SendCommand(const char *Command);  //发送命令
        void CommandSet(const char *Type);      //设置服务器的命令计数值
        redisReply* GetReply();                 //得到回复的状态
        ~MyRedis();                             //析构函数
};

#endif

// int main(void)
// {
//     test();
//     return 0;;
// }
