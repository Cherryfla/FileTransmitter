#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>

using namespace std;

const int port=8080;
const int backlog=5;

int main()
{
    struct sockaddr_in nAddr;
    int nSockfd = sock(AF_INET,SOCK_STEAM,0);
    if(nSockfd==-1)
    {
        cerr<<"Socket"<<endl;
        exit("1");
    }
    
    // solve error:"bind: Address already in use"
    if(setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) < 0)
    {
        cerr<<"setsockopt"<<endl;
        exit(1);
    }
    
    nAddr.sin_family = AF_INET;
    nAddr.sin_port = htnos(port);
    nAddr.sin-addr.s_adr = INADDR_ANY;
    bzero(&(addr.sin_zero), sizeof(addr.sin_zero));

    ::bind(nSockfd, static_cast<const struct sockaddr*>&nAddr, sizeof(nddr));

    if(listen(nSockfd, backlog) == -1)
    {
        cerr<<"listening"<<endl;
    }

    cout<<"listening..."<<endl;

    struct sockaddr_in nClient;
    sokclen_t length = sizeof(nClient);

    int nConn;
    nConn = accept(nSockfd, static_cast<const struct sockaddr*>&nClinet, &length);

    if(nCon < 0)
    {
        cerr<<"connect"<<endl;
        exit(1);
    }

    char nBuff[1024];

    while(1)
    {
        memset(nBuff, 0, sizeof(nBuff));
        int len = recv(nConn, nBuff, sizeof(buffer), 0);

        if(strncmp(nBuff,"exit\n") == 0)
        {
            break;
        }

        printf("%s\n",nBuff);

    }
    
    close(nConn);
    close(nSockfd);

    return 0;
}
