#include <cstdio>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <dirent.h>
#include <string>

#define CMD_PORT 8080
#define TRAN_PORT 8081
#define BSIZE 1024

using namespace std;

const int backlog=5;

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

int DealDir(char *fPath, char *nResult)
{
    if(fPath == nullptr)
    {
        return -1;
    }
    DIR *nDir;
    struct dirent *pDir;
    nDir = opendir(fPath);

    if(pDir == nullptr)
    {
        strcat(nResult, "Can't open this dictionary");
    }
    else
    {
        while(1)
        {
            pDir = readdir(nDir);
            if(pDir == nullptr)
            {
                break;
            }
            strcat(nResult, pDir->d_name);
            strcat(nResult, "\n");
        }
        closedir(nDir);
    }
    return 0;
}
ssize_t SendFile(int out_fd, int in_fd, off_t * offset, size_t count )
{
    off_t orig;
    char buf[BSIZE];
    size_t toRead, numRead, numSent, totSent;

    if(offset != NULL)
    {
        /* Save current file offset and set offset to value in '*offset' */
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
            break;                      /* EOF */

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
        /* Return updated file offset in '*offset', and reset the file offset
           to the value it had when we were called. */
        *offset = lseek(in_fd, 0, SEEK_CUR);
        if (*offset == -1)
            return -1;
        if (lseek(in_fd, orig, SEEK_SET) == -1)
            return -1;
    }
    return totSent;
}
int CreateSocket(int fPort)
{
    struct sockaddr_in nAddr;
    int nSockfd = socket(AF_INET,SOCK_STREAM,0);
    if(nSockfd == -1)
    {
        cerr<<"Socket"<<endl;
        exit(1);
    }
    
    // solve error:"bind: Address already in use"
    int reuse = 1;
    if(setsockopt(nSockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) < 0)
    {
        cerr<<"setsockopt"<<endl;
        exit(1);
    }
    
    nAddr.sin_family = AF_INET;
    nAddr.sin_port = htons(fPort);
    nAddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(nAddr.sin_zero), sizeof(nAddr.sin_zero));

    ::bind(nSockfd, (const struct sockaddr*)&nAddr, sizeof(nAddr));

    if(listen(nSockfd, backlog) == -1)
    {
        cerr<<"listen"<<endl;
    }

    cout<<"Port "<<fPort<<" listening..."<<endl;
    
    return nSockfd;
}
int AcceptConnection(int fSocket)
{
    socklen_t addrlen = 0;
    struct sockaddr_in Client_addr;
    addrlen = sizeof(Client_addr);
    return accept(fSocket, (struct sockaddr*)&Client_addr, &addrlen);
}
int main()
{
    int nSockfd = CreateSocket(CMD_PORT); 
    
    while(1)
    {
        //int nConn = AcceptConnection(nSockfd);
        socklen_t addrlen = 0;
        struct sockaddr_in Client_addr;
        addrlen = sizeof(Client_addr);
        int nConn = accept(nSockfd, (struct sockaddr*)&Client_addr, &addrlen);
        cout<<"Accepted."<<endl;

        if(nConn < 0)
        {
            cerr<<"connect"<<endl;
            exit(1);
        }


        char nBuff[1024];
        memset(nBuff, 0, sizeof(nBuff));

        Command *nCommand = new Command;
        int pid = fork();

        if(pid < 0)
        {
            fprintf(stderr, "Cannot create child process.");
            exit(1);
        }
        
        if(pid == 0)
        {
            close(nSockfd);

            char nWelcome[BSIZE] = "Welcome to the server.\n";
            write(nConn, nWelcome, sizeof(nWelcome));
            
            //to avoid child process to be a zombie process
            signal(SIGCHLD, SIG_IGN);

            int nByteRead;
            while(nByteRead = read(nConn, nBuff, BSIZE))
            {
                if(nByteRead > BSIZE)
                {
                    cerr<<"server read"<<endl;
                    continue;
                }                    
                Command *nCommand = new Command;
                nCommand->Init(nBuff);
                
                if(strncmp(nCommand->GetCommand(), "QUIT", 3) == 0)
                {
                    break;
                }
                else if(strncmp(nCommand->GetCommand(), "GET", 3) == 0)
                {
                
                    struct stat nPath;
                    if(stat(nCommand->GetArg(), &nPath)==0)
                    {
                        if(nPath.st_mode & S_IFREG)
                        {
                            if(fork()==0)
                            {
                                close(nConn);
                                int nTranSock = CreateSocket(TRAN_PORT);
                                int nFileConn = AcceptConnection(nTranSock);
                            
                                int nFd = open(nCommand->GetArg(), O_RDONLY);

                                struct stat nFstat;
                                fstat(nFd, &nFstat);
                                
                                off_t nOffset = 0;
                                int nSendtot = SendFile(nFileConn, nFd, &nOffset, nFstat.st_size);

                                if(nSendtot != nFstat.st_size)
                                {
                                    cerr<<"sendfile error"<<endl;
                                    exit(1);
                                }

                                exit(0);
                            }
                            else
                            {
                                char nMessage[BSIZE]="Sending file..";
                                write(nConn, nMessage, sizeof(nMessage));
                            }

                        }
                        else if(nPath.st_mode & S_IFDIR)
                        {
                            char nFileList[BSIZE];
                            memset(nFileList, 0, sizeof(nFileList));
                            
                            if(DealDir(nCommand->GetArg(), nFileList) < 0)
                            {
                                cerr<<"path error"<<endl;
                                break;
                            }
                            
                            if(strlen(nFileList) > BSIZE)
                            {
                                memset(nFileList, 0, sizeof(nFileList));
                                strcat(nFileList, "too many files");
                            }
                            write(nConn, nFileList, sizeof(nFileList));

                        }
                        else
                        {
                            printf("not file or dir");
                        }
                    }
                    else
                    {
                        char nError[BSIZE] = "Error: file or dir does not exit\n";
                        write(nConn, nError, sizeof(nError));
                    }
                }
                else
                {
                    char nUsage[BSIZE] = "Usage: GET file/dir,QUIT to quit";
                    write(nConn, nUsage, sizeof(nUsage));
                    continue;
                }
                delete nCommand;
            }

        }

        close(nConn);
    }
    
    close(nSockfd);

    return 0;
}
