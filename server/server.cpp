#include "server.h"

Command::Command()
{
    memset(command, 0, sizeof(command));
    memset(arg, 0, sizeof(arg));
}
Command::void Init(char *fCommand)
{
    sscanf(fCommand, "%s %s", this->command, this->arg);
}
Command::char* GetCommand()
{
    return static_cast<char*>(command);
}
Command::char* GetArg()
{
    return static_cast<char*>(arg);
}

inline void EchoBack(int nConn, const char nMessage[])
{
    write(nConn, nMessage, BSIZE);
}
inline void  ErrorExit(const char nError[])
{
    perror(nError);
    exit(EXIT_FAILURE);
}
int CreateSocket(int fPort)
{
    struct sockaddr_in nAddr;
    int nSockfd = socket(AF_INET,SOCK_STREAM,0);
    if(nSockfd == -1)
        ErrorExit("socket");
    
    // solve error:"bind: Address already in use"
    int reuse = 1;
    if(setsockopt(nSockfd, SOL_SOCKET,SO_REUSEADDR, &reuse, sizeof(reuse)) < 0)
        ErrorExit("setsockopt");
    
    nAddr.sin_family = AF_INET;
    nAddr.sin_port = htons(fPort);
    nAddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(nAddr.sin_zero), sizeof(nAddr.sin_zero));

    ::bind(nSockfd, (const struct sockaddr*)&nAddr, sizeof(nAddr));

    if(listen(nSockfd, SOMAXCONN) == -1)
        ErrorExit("listen");
    
    return nSockfd;
}
/*
 *给定时限连接,wait_second为时限,为0是是正常的接收连接
 */
int AcceptConnection(int fd, unsigned int wait_seconds)
{
    int ret;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    if (wait_seconds > 0)
    {
        fd_set accept_fdset;
        struct timeval timeout;
        FD_ZERO(&accept_fdset);
        FD_SET(fd, &accept_fdset);
        timeout.tv_sec = wait_seconds;
        timeout.tv_usec = 0;
        do
        {
            ret=select(fd+1, &accept_fdset, nullptr, nullptr, &timeout);
        } while (ret < 0 && errno == EINTR);
        if (ret == -1)
            return  -1;
        else if (ret == 0)
        {
            errno = ETIMEDOUT;
            return -1;
        }
    }
    
    ret = accept(fd, (struct sockaddr*)&addr, &addrlen);
    if (ret == -1)
        ErrorExit("Accept");

    printf("ip=%s, port=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
    return ret;
}
ssize_t SendFile(int out_fd, int in_fd, off_t * offset, size_t count )
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
        
        //cout<<"numRead: "<<numRead<<endl;
        if(numRead == -1)
            return -1;
        if(numRead == 0)
            break;                      /* EOF */

        numSent = write(out_fd, buf, numRead);
        if(numSent == -1)
            return -1;
        if(numSent == 0) 
        {               
            cerr<<"sendfile: write() transferred 0 bytes\n";
            return -1;
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
void DealFile(int nConn, Command* nCommand)
{
    if(fork()==0)
    {
        close(nConn);
        int nTranSock = CreateSocket(TRAN_PORT);
        int nFileConn = AcceptConnection(nTranSock, 1);
        
        if(nFileConn > 0)
            cout<<"File transmition port open.\n";
        else
        {
            cerr<<"failed to transmit file.\n";
            EchoBack(nConn, "failed to transmit file.\n");
            return;
        }
        
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
        
        close(nFd);
        close(nFileConn);
        exit(0);
    }
    else
    {
        EchoBack(nConn, "File transmition.\n");
        wait(nullptr);
    }
}
int Split(char *fPath, char *nResult)
{
    if(fPath == nullptr)
    {
        return -1;
    }
    DIR *nDir = opendir(fPath);
    struct dirent *pDir = nullptr;

    if(nDir == nullptr)
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

void DealDir(int fConn,Command* nCommand)
{
    char nFileList[BSIZE];
    memset(nFileList, 0, sizeof(nFileList));
    
    if(Split(nCommand->GetArg(), nFileList) < 0)
    {
        cerr<<"path error"<<endl;
        return;
    }
    
    if(strlen(nFileList) > BSIZE)
    {
        memset(nFileList, 0, sizeof(nFileList));
        strcat(nFileList, "too many files");
    }
    write(fConn, nFileList, BSIZE);
}

int FileTransmit(int nConn)
{
    char nBuff[BSIZE];
    memset(nBuff, 0, sizeof(nBuff));
    
    int nByteRead = read(nConn, nBuff, BSIZE);
    if(nByteRead > BSIZE)
    {
        cerr<<"server read"<<endl;
        return -1;
    }
    
    Command *nCommand = new Command;
    nCommand->Init(nBuff);
    cout<<"Recive message: "<<nBuff<<endl;

    if(strncmp(nCommand->GetCommand(), "QUIT", 4) == 0)
        return 1;
    else if(strncmp(nCommand->GetCommand(), "GET", 3) == 0)
    {
        struct stat nPath;
        if(stat(nCommand->GetArg(), &nPath) != 0){
            EchoBack(nConn, "Error: file or dir does not exit\n");
            return 0;
        }

        if(nPath.st_mode & S_IFREG)
            DealFile(nConn, nCommand);
        else if(nPath.st_mode & S_IFDIR)
            DealDir(nConn, nCommand);
    }
    else
    {
        EchoBack(nConn, "Usage: GET file/dir, QUIT to quit\n");
        return 0;
    }
    delete nCommand;
    return 0;
}

