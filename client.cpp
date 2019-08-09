#include <sys/types.h>
#include <sys/socket.h>
#include <cstdio>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/shm.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>

#define BSIZE 1024
#define CMD_PORT 8080
#define TRAN_PORT 8081
#define TIME_OUT_TIME 1

using namespace std;

class Command{
    private:
        char command[5];
        char arg[BSIZE];
    public:
        Command()
        {
            memset(command, 0, sizeof(command));
            memset(arg, 0, sizeof(arg));
        }
        void Init(char *fCommand)
        {
            sscanf(fCommand, "%s %s", this->command, this->arg);
        }
        char* GetCommand()
        {
            return static_cast<char*>(command);
        }
        char* GetArg()
        {
            return static_cast<char*>(arg);
        }
};
ssize_t ReceiveFile(FILE *out_fd, int in_fd)
{
    char buf[BSIZE];
    size_t numRead = 0;
    size_t totRecv = 0;
    while(1)
    {
        numRead = read(in_fd, buf, BSIZE);
        // if(numRead == -1)
        //     return -1;
        if(numRead == 0)
            break;
        fwrite(buf, 1, numRead, out_fd);
        totRecv += numRead;
    }
    return totRecv;
}
int DoQuery(){
    char nInput[5];
    memset(nInput, 0 ,sizeof(nInput));
    do{
        printf("File exist, Cover it? (y/n)\n");
        scanf("%s",nInput);
        if(nInput[0] == 'Y')
            nInput[0] = 'y';
        else if(nInput[0] == 'N')
            nInput[0] = 'n';
    }while(nInput[0] != 'n' && nInput[0] != 'y');
    
    return nInput[0]=='y'?1:0;
}
int ConnectSocket(char *fServerIp, int  fServerPort)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0) 
        exit(1);

    struct sockaddr_in nServAddr;
    memset(&nServAddr, 0, sizeof(nServAddr));
    nServAddr.sin_family = AF_INET;
    nServAddr.sin_port = htons(fServerPort); 
    nServAddr.sin_addr.s_addr = inet_addr(fServerIp);
    
    int error=-1;
    int len = sizeof(int);
    timeval tm;
    fd_set set;
    unsigned long ul = 1;

    ioctl(sockfd, FIONBIO, &ul); //设置为非阻塞模式
    bool ret = false;

    if( connect(sockfd, (struct sockaddr *)&nServAddr, sizeof(nServAddr)) == -1)
    {
        tm.tv_sec = TIME_OUT_TIME;
        tm.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        if( select(sockfd+1, NULL, &set, NULL, &tm) > 0)
        {
            getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, (socklen_t *)&len);
            if(error == 0) 
                ret = true;
            else 
                ret = false;
        } 
        else
            ret = false;
    }
    else 
        ret = true;
    ul = 0;
    ioctl(sockfd, FIONBIO, &ul); //设置为阻塞模式
    if(ret)
        return sockfd;
    else
        return -1;
}

void TryRecvFile(char* fServerIp, Command *nCommand)
{
    int nFileConn;

    for(int i=0; i < 10; i++)
    {
        nFileConn = ConnectSocket(fServerIp, TRAN_PORT);
        if(nFileConn > 0)
            break;
    }
    if(nFileConn < 0)
        exit(0);

    char *nFilePath = nCommand->GetArg();
    int nNameLen = strlen(nFilePath);
    int nPos = nNameLen-1;
    for(; nPos >= 0; nPos--)
        if(nFilePath[nPos] == '/')
            break;

    char nFileName[BSIZE];
    memset(nFileName, 0, sizeof(nFileName));
    strncpy(nFileName, nFilePath+nPos+1, nNameLen-nPos-1);
    
    if (access(nFileName, F_OK) == 0){
        int nQueryRes = DoQuery();
        if(nQueryRes == 0){
            cout<<"Terminated.\n";
            close(nFileConn);
            exit(0);
        }
    }
    FILE *pFd = fopen(nFileName, "ab");
    if(pFd == nullptr)
    {
        cerr<<"open file"<<endl;
    }

    int nRecvtot = ReceiveFile(pFd, nFileConn);
    if(nRecvtot < 0)
    {
        cerr<<"Recvfile error"<<endl;
        exit(1);
    }

    fclose(pFd);
    close(nFileConn);
}

int main(int argc, char *argv[])
{
    if(argc != 2)
    {
        cout<<"Usage: "<<argv[0]<<" ip\n"<<endl;
        exit(1);
    }

    char *pIp = argv[1];
    
    int nSockfd = ConnectSocket(pIp, CMD_PORT);
    
    if(nSockfd < 0)
    {
        cerr<<"Connection"<<endl;
        exit(1);
    }
    else
        cout<<"Connected."<<endl;
    
    char sendbuf[BSIZE];
    char recvbuf[BSIZE];
    
    /* int nReadnum = read(nSockfd, recvbuf, BSIZE);
    if(nReadnum > 0)
        cerr<<recvbuf<<endl; */
    
    fd_set nRset,nAllset;
    FD_ZERO(&nRset);
    FD_SET(STDIN_FILENO, &nRset);
    FD_SET(nSockfd, &nRset);
    FD_SET(STDIN_FILENO, &nAllset);
    FD_SET(nSockfd, &nAllset);

    while(true)
    {
        nRset = nAllset;
        int nSelectRes = select(nSockfd+1, &nRset, nullptr, nullptr, nullptr);
        if(nSelectRes == -1 && errno == EINTR )
            continue;
        else if(nSelectRes == -1)
        {
            perror("Select");
            exit(1);
        }
        if(FD_ISSET(STDIN_FILENO, &nRset))
        {
            fgets(sendbuf, sizeof(sendbuf), stdin);
            
            Command *nCommand = new Command;
            nCommand->Init(sendbuf);
           // printf("Message to send :%s\n",sendbuf);
            write(nSockfd, sendbuf, sizeof(sendbuf)); 
            
            if(strncmp(nCommand->GetCommand(), "GET", 3) == 0)
            {
                if(fork()==0)
                {
                    close(nSockfd);
                    TryRecvFile(pIp, nCommand);

                    cout<<"Completed.\n";
                    exit(0);
                }
                else
                {
                    wait(nullptr);
                }
            }
            else if(strncmp(nCommand->GetCommand(), "QUIT", 4) == 0)
                break;

            delete nCommand;
            
            memset(sendbuf, 0, sizeof(sendbuf));
            memset(recvbuf, 0, sizeof(recvbuf));
        }
        
        if(FD_ISSET(nSockfd, &nRset))
        {
            int nReadnum = read(nSockfd, recvbuf, BSIZE);
            if(nReadnum > BSIZE)
            {
                cerr<<"Receive limit exceed."<<endl;
            }
            if(nReadnum == 0){
                cout<<"Section terminated.\n";
                break;
            }
            cout<<recvbuf<<endl;
        }
    }
    
    close(nSockfd);
    return 0;
}
