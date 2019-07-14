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
ssize_t ReceiveFile(int out_fd, int in_fd, off_t * offset, size_t count )
{
    off_t orig;
    char buf[BSIZE];
    size_t toRead, numRead, numSent, totSent;

    if(offset != NULL)
    {
        // Save current file offset and set offset to value in '*offset'
        orig = lseek(in_fd, 0, SEEK_CUR);
        if (orig == -1)
            return -1;
        if (lseek(in_fd, *offset, SEEK_SET) == -1)
            return -1;
    }

    totSent = 0;
    while (count > 0) 
    {
        toRead = count<BSIZE ? count : BSIZE;

        numRead = read(in_fd, buf, toRead);
        if (numRead == -1)
            return -1;
        if (numRead == 0)
            break;                      // EOF 

        numSent = write(out_fd, buf, numRead);
        if (numSent == -1)
            return -1;
        if (numSent == 0) 
        {               
            perror("sendfile: write() transferred 0 bytes");
            exit(-1);
        }

        count -= numSent;
        totSent += numSent;
    }

    if (offset != NULL) {
        // Return updated file offset in '*offset', and reset the file offset
        //   to the value it had when we were called.
        *offset = lseek(in_fd, 0, SEEK_CUR);
        if (*offset == -1)
            return -1;
        if (lseek(in_fd, orig, SEEK_SET) == -1)
            return -1;
    }
    return totSent;
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
                    if(error == 0) ret = true;
                        else ret = false;
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
    {
        cout<<"Connected."<<endl;
    }
    
    char sendbuf[BSIZE];
 
    while (fgets(sendbuf, sizeof(sendbuf), stdin) != NULL)
    {
        Command *nCommand = new Command;
        nCommand->Init(sendbuf);
        
        write(nSockfd, sendbuf, strlen(sendbuf)); 
        
        if(strncmp(nCommand->GetCommand(), "GET", 3) == 0)
        {
            if(fork()==0)
            {
                signal(SIGCHLD, SIG_IGN);
                close(nSockfd);
                
                int nFileConn = ConnectSocket(pIp, TRAN_PORT);
                if(nFileConn < 0 )
                {
                    exit(0);
                }

                char *nFilePath = nCommand->GetCommand();
                int nNameLen = strlen(nFilePath);
                int nPos = nNameLen-1;
                for(; nPos >= 0 ; nPos--)
                {
                    if(nFilePath[nPos] == '/')
                    {
                        break;
                    }
                }
                char nFileName[BSIZE];
                memset(nFileName, 0, sizeof(nFileName));
                strncpy(nFileName, nFilePath+nPos+1, nNameLen-nPos-1);
                
                int nFd = open(nFileName, O_WRONLY|O_CREAT);
                if(nFd < 0)
                {
                    cerr<<"open file"<<endl;
                }

                struct stat nFstat;
                fstat(nFd, &nFstat);

                off_t nOffset = 0;
                int nRecvtot = ReceiveFile(nFd, nFileConn, &nOffset, nFstat.st_size);

                if(nRecvtot != nFstat.st_size)
                {
                    cout<<"recvfile error"<<endl;
                    exit(1);
                }
                exit(0);
            }
            else
            {
                char recvbuf[BSIZE];
                memset(recvbuf, 0, sizeof(recvbuf));
                int nReadnum = read(nSockfd, recvbuf, BSIZE);
                if(nReadnum > BSIZE)
                {
                    cerr<<"receive limit exceed"<<endl;
                }
            } 
        }
        else if(strncmp(nCommand->GetCommand(), "QUIT", 4) == 0)
            break;

        delete nCommand;
        memset(sendbuf, 0, sizeof(sendbuf));
    }
    close(nSockfd);
    return 0;
}
