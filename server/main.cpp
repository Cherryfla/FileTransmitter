#include "server.h"
#include "ProcessPool.h"
using namespace std;

int main()
{
    int nSockfd = CreateSocket(CMD_PORT); 
    ProcessPool *nPool = ProcessPool::Create(nSockfd, 2, FileTransmit);
    if(nPool)
    {
        nPool->Run();
        delete nPool;
    }
    close(nSockfd);
    return 0;
}