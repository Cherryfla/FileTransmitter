#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/shm.h>
#include <iostream>

#define BUFFER_SIZE 1024
 
using namespace std;

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        cout<<"Usage: client ip port\n"<<endl;
        exit(1);
    }

    char *pIp = argv[1];
    int nPort = atoi(argv[2]);

    cout<<pIp<<' '<<nPort<<endl;
    
    int sock_cli = socket(AF_INET,SOCK_STREAM, 0);
 
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(nPort); 
    servaddr.sin_addr.s_addr = inet_addr(pIp);
    
    if (connect(sock_cli, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    {
        cerr<<"connect"<<endl;
        exit(1);
    }
 
    char sendbuf[BUFFER_SIZE];
    char recvbuf[BUFFER_SIZE];
 
    while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL)
    {
        send(sock_cli, sendbuf, strlen(sendbuf),0); 
        if(strcmp(sendbuf,"exit")==0)
            break;
        recv(sock_cli, recvbuf, sizeof(recvbuf),0); 
        fputs(recvbuf, stdout);
 
        memset(sendbuf, 0, sizeof(sendbuf));
        memset(recvbuf, 0, sizeof(recvbuf));
    }
    close(sock_cli);
    return 0;
}
