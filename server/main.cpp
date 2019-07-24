#include "server.h"

using namespace std;

int main()
{
    int nSockfd = CreateSocket(CMD_PORT); 
    
    int nEpollfd;
    nEpollfd = epoll_create1(EPOLL_CLOEXEC);
    if (nEpollfd < 0)
        ErrorExit("epoll_create1");
    struct epoll_event nEvent;
    nEvent.data.fd = nSockfd;
    nEvent.events = EPOLLIN | EPOLLET;
    int nEpollCtl = epoll_ctl(nEpollfd, EPOLL_CTL_ADD, nSockfd, &nEvent);
    if (nEpollCtl < 0)
        ErrorExit("epoll_ctl");

    int nConnCount = 0;
    vector<int>nClients;

    int nCurrentSize = 1;
   // vector<struct epoll_event>nEvents(16);

    epoll_event *nEvents = new epoll_event [nCurrentSize];
    while(1)
    {
        int nReady = epoll_wait(nEpollfd, nEvents, nCurrentSize, -1);
        //int nReady = epoll_wait(nEpollfd, &*nEvents.begin(), static_cast<int>(nEvents.size()), -1);

        if (nReady == -1){
            if (errno == EINTR)
                continue;
            ErrorExit("epoll_wait");
        }
        if (nReady == 0)
            continue;
        if (nCurrentSize == nReady)
        {
            nCurrentSize *= 2;
            epoll_event *nTemp = new epoll_event[nCurrentSize];

            for(int i = 0; i < nReady; i++)
                nTemp[i] = nEvents[i];
            
            delete [] nEvents;
            nEvents = nTemp;
        }
        // if ((size_t)nReady == nEvents.size())
        //     nEvents.resize(nEvents.size()*2);

        for (int i = 0 ; i < nReady; i++)
        {
            if (nEvents[i].data.fd == nSockfd)
            {
                int nConn = AcceptConnection(nSockfd, 0);
                if (nConn < 0)
                    ErrorExit("AcceptConnection");

                cout<<"Countion Count = "<<++nConnCount<<endl;
                nClients.push_back(nConn);

                nEvent.data.fd = nConn;
                nEvent.events = EPOLLIN | EPOLLET;
                epoll_ctl(nEpollfd, EPOLL_CTL_ADD, nConn, &nEvent);
                EchoBack(nConn, "Welcome to the server.\n");
            }
            else if (nEvents[i].events & EPOLLIN)
            {
                int nConn = nEvents[i].data.fd;
                if (nConn < 0)
                    continue;
                
                int nRes = FileTransmit(nConn); 
                if (nRes == -1)
                {
                    cerr<<"File transmit error.\n";
                    exit(1);
                }
                else if (nRes == 1)
                {
                    break;
                }
            }
        }
    }
    
    close(nSockfd);
    return 0;
}