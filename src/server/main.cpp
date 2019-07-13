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

int AcceptConnection(int fSocket)
{
    socklen_t addrlen = 0;
    struct sockaddr_in Client_addr;
    addrlen = sizeof(Client_addr);
    return accept(fSocket, (struct sockaddr*)&Client_addr, &addrlen);
}

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
ssize_t sendfile(int out_fd, int in_fd, off_t * offset, size_t count )
{
    off_t orig;
    char buf[BUF_SIZE];
    size_t toRead, numRead, numSent, totSent;

    if (offset != NULL) {
        /* Save current file offset and set offset to value in '*offset' */
        orig = lseek(in_fd, 0, SEEK_CUR);
        if (orig == -1)
            return -1;
        if (lseek(in_fd, *offset, SEEK_SET) == -1)
            return -1;
    }

    totSent = 0;
    while (count > 0) {
        toRead = count<BUF_SIZE ? count : BUF_SIZE;

        numRead = read(in_fd, buf, toRead);
        if (numRead == -1)
            return -1;
        if (numRead == 0)
            break;                      /* EOF */

        numSent = write(out_fd, buf, numRead);
        if (numSent == -1)
            return -1;
        if (numSent == 0) {               /* Should never happen */
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
void ftp_retr(Command *cmd, State *state)
{

  if(fork()==0){
    int connection;
    int fd;
    struct stat stat_buf;
    off_t offset = 0;
    int sent_total = 0;
    if(state->logged_in){

      /* Passive mode */
      if(state->mode == SERVER){
        if(access(cmd->arg,R_OK)==0 && (fd = open(cmd->arg,O_RDONLY))){
          fstat(fd,&stat_buf);
          
          state->message = "150 Opening BINARY mode data connection.\n";
          
          write_state(state);
          
          connection = accept_connection(state->sock_pasv);
          close(state->sock_pasv);
          if(sent_total = sendfile(connection, fd, &offset, stat_buf.st_size)){
            
            if(sent_total != stat_buf.st_size){
              perror("ftp_retr:sendfile");
              exit(EXIT_SUCCESS);
            }

            state->message = "226 File send OK.\n";
          }else{
            state->message = "550 Failed to read file.\n";
          }
        }else{
          state->message = "550 Failed to get file\n";
        }
      }else{
        state->message = "550 Please use PASV instead of PORT.\n";
      }
    }else{
      state->message = "530 Please login with USER and PASS.\n";
    }

    close(fd);
    close(connection);
    write_state(state);
    exit(EXIT_SUCCESS);
  }
  state->mode = NORMAL;
  close(state->sock_pasv);
}
int main()
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
    nAddr.sin_port = htons(CMD_PORT);
    nAddr.sin_addr.s_addr = INADDR_ANY;
    bzero(&(nAddr.sin_zero), sizeof(nAddr.sin_zero));

    ::bind(nSockfd, (const struct sockaddr*)&nAddr, sizeof(nAddr));

    if(listen(nSockfd, backlog) == -1)
    {
        cerr<<"listening"<<endl;
    }

    cout<<"listening..."<<endl;

    struct sockaddr_in nClient;
    socklen_t length = sizeof(nClient);

    while(1)
    {
        int nConn;
        nConn = accept(nSockfd, (struct sockaddr*)&nClient, &length);
        
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
                            cout<<"file\n";
                        }
                        else if(nPath.st_mode & S_IFDIR)
                        {
                            char nFileList[BSIZE];
                            memset(nFileList, 0, sizeof(nFileList));
                            
                            if(DealDir(nCommand->GetArg(), nFileList) < 0)
                            {
                                cerr<<"path error"<<endl;
                                continue;
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
