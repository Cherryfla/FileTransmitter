#include "MyRedis.h"

void MyRedis::Connect(const char *Ip, short nPort)
{
    context = redisConnect(Ip, nPort);
    if (context->err) {
        redisFree(context); 
        printf("connect redisServer err:%s\n", context->errstr);
        return ;
    }
    isConnected = true;
}
bool MyRedis::JudgeConnect()
{
    return isConnected;
}
void MyRedis::SendCommand(const char *Command)
{
    reply = (redisReply *)redisCommand(context, Command);
    if(reply == nullptr) {
        printf("command %s failure\n", Command);
        redisFree(context);
    }
    else
    {
        printf("command %s success\n",Command);
    }
    
}
void MyRedis::CommandSet(const char *Type)
{
    char nGetVal[BSIZE] = "GET ";
    strcat(nGetVal, Type);
    this->SendCommand(nGetVal);

    if(reply->type == REDIS_REPLY_NIL)
    {
        char nSetVal[BSIZE] = "INCR ";
        strcat(nSetVal, Type);
        this->SendCommand(nSetVal);
        //printf("%s\n", nSetVal);

        memset(nSetVal, 0 ,sizeof(nSetVal));
        strcat(nSetVal, "EXPIRE ");
        strcat(nSetVal, Type);
        strcat(nSetVal, " ");

        time_t now = time(0);
        tm *ltm = localtime(&now);
        int nExpire = 86400-ltm->tm_hour*3600-ltm->tm_min*60-ltm->tm_sec;
        
        char nStrExpire[BSIZE];
        memset(nStrExpire, 0, sizeof(nStrExpire));
        sprintf(nStrExpire, "%d", nExpire);
        strcat(nSetVal, nStrExpire);
        //printf("%s %s\n",nStrExpire, nSetVal);
        this->SendCommand(nSetVal);
        //printf("%s\n", nSetVal);
    }
    else
    {
        char nSetVal[] = "INCR ";
        strcat(nSetVal, Type);
        this->SendCommand(nSetVal);
        //printf("%s\n", nSetVal);
    }
    
}
redisReply* MyRedis::GetReply()
{
    return reply;
}
MyRedis::~MyRedis()
{
    redisFree(this->context);
    if (this->reply != nullptr)
        freeReplyObject(this->reply); 
}

void test(void) 
{
    MyRedis nRedis;
    nRedis.Connect("127.0.0.1",6379);
    printf("connect redisServer success\n"); 

    nRedis.SendCommand("SET test 100");
    printf("SET test 100 execute success\n");

    const char *getVal = "GET test";
    nRedis.SendCommand(getVal);

    if((nRedis.GetReply())->type != REDIS_REPLY_STRING)
    {
        printf("command execute failure:%s\n", getVal); 
        return ;
    }

    printf("GET test:%s\n", (nRedis.GetReply())->str);

}

// int main(void)
// {
//     test();
//     return 0;;
// }
