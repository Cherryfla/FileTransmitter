#include "ProcessPool.h"

ProcessPool* ProcessPool::Instance = nullptr;
static int SigPipeFd[2];
static void SigHandler(int Sig)
{
	int save_errno = errno;  
    int msg = Sig;  
    send(SigPipeFd[1], (char*)&msg, 1, 0);  
    errno = save_errno;  
}
static void AddSig(int sig, void (handler)(int), bool restart = true)  
{  
    struct sigaction sa;  
    memset(&sa, '\0', sizeof(sa));  
    sa.sa_handler = handler;  
    if(restart) {  
        sa.sa_flags |= SA_RESTART;  
    }  
    sigfillset(&sa.sa_mask);  
    assert(sigaction(sig, &sa, NULL) != -1);  
} 
int SetNonBlock(int nFd)  
{  
    int old_option = fcntl(nFd, F_GETFL);  
    int new_option = old_option | O_NONBLOCK;  
    fcntl(nFd, F_SETFL, new_option);  
    return old_option;  
} 
void AddFd(int nEpollfd, int fd)  
{  
    epoll_event event;  
    event.data.fd = fd;  
    event.events = EPOLLIN | EPOLLET;  
    if(epoll_ctl(nEpollfd, EPOLL_CTL_ADD, fd, &event))
    {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
    
}
void RemoveFd(int nEpollfd, int fd)
{
    epoll_ctl(nEpollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

ProcessPool::ProcessPool(int nListenfd, int nProcessNumber, int (*Route)(void *arg)):
nListenfd(nListenfd), nProcessNumber(nProcessNumber), nIndex(-1), nStopFlag(false), Route(Route)
{  
    nSubProcess = new Process[nProcessNumber];  
  
    for (int i=0; i<nProcessNumber; i++) {  
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, nSubProcess[i].nPipeFd);  
        if (ret < 0)
            perror("socketpair");
  
        nSubProcess[i].nPid = fork();  
        assert(nSubProcess[i].nPid >= 0);  
  
        if(nSubProcess[i].nPid > 0) {  
            close(nSubProcess[i].nPipeFd[1]);  
            continue;  
        }  
        else 
		{  
			close(nSubProcess[i].nPipeFd[0]);  
            nIndex = i;
            break;  
        }  
    }
}
void ProcessPool::SetupSigPipe()
{
	nEpollfd = epoll_create1(EPOLL_CLOEXEC);
	if(nEpollfd < 0)
	{
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}
	
	if(socketpair(PF_UNIX, SOCK_STREAM, 0, SigPipeFd))
	{
		perror("socketpair");
		exit(EXIT_FAILURE);
	}
	SetNonBlock(SigPipeFd[1]);
	AddFd(nEpollfd, SigPipeFd[0]);
	
	AddSig(SIGCHLD, SigHandler);
	AddSig(SIGTERM, SigHandler);  
    AddSig(SIGINT, SigHandler);  
    AddSig(SIGPIPE, SIG_IGN); 
}
void ProcessPool::Run()
{
    nIndex == -1? RunParent():RunChild();
}
void ProcessPool::RunChild()
{
	SetupSigPipe();
    int nPipeFd = nSubProcess[nIndex].nPipeFd[1];
	AddFd(nEpollfd,nPipeFd);
	
	int nCurrentSize = 16;
	epoll_event *nEvents = new epoll_event[nCurrentSize];
	
	int nReady = 0;
	
	while(!nStopFlag)
	{
		nReady = epoll_wait(nEpollfd, nEvents, nCurrentSize, -1);
        
		if(nReady < 0){
			if(errno == EINTR)
				continue;
			else{
				perror("epoll_wait");
				break;
			}
		}

		if (nCurrentSize == nReady)
        {
            nCurrentSize *= 2;
            epoll_event *nTemp = new epoll_event[nCurrentSize];

            for(int i = 0; i < nReady; i++)
                nTemp[i] = nEvents[i];
            
            delete [] nEvents;
            nEvents = nTemp;
        }
		for(int i=0; i < nReady; i++)
		{
			int nReadyFd = nEvents[i].data.fd;
			if(nReadyFd == nPipeFd)
			{
				int nTemp = 0;
				int nRes = read(nReadyFd, (char*)&nTemp, sizeof(nTemp));
				
				if((nRes < 0 && errno != EAGAIN) || nRes == 0)
					continue;
				else
				{  
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(struct sockaddr_in);
                    int nConn = accept(nListenfd, (struct sockaddr*)&addr, &addrlen);
					if( nConn < 0)
					{
						perror("accept");
						continue;
					}
                    printf("ip=%s, port=%d\n",inet_ntoa(addr.sin_addr),htons(addr.sin_port));
					AddFd(nEpollfd, nConn);
				}
			}
			else if(nReadyFd == SigPipeFd[0])
			{  
                char Signals[BSIZE];  
                int Res = recv(SigPipeFd[0], Signals, sizeof(Signals), 0);  
                if(Res <= 0) 
                    continue;    
				for(int i=0; i<Res; i++)
				{  
					switch(Signals[i])
					{
						case SIGCHLD:   
							pid_t pid;   
							while((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {  
								continue;  
							}  
							break;  
						case SIGTERM:  
						case SIGINT: {  
							nStopFlag = true;  
							break;  
						}  
						default:  
							break;  
					}  
				}   
            } 
			else
			{
                if(nReadyFd < 0)
                    continue;
                
				int nRes = Route((void*)&nReadyFd);
                if (nRes == -1)
                {
                    printf("File transmit error.\n");
                    exit(1);
                }
                else if (nRes == 1)
                {
                    break;
                }
			}
		}
	}
	
	delete [] nEvents;
	close(nPipeFd);
	close(nEpollfd);
}
void ProcessPool::RunParent()
{	
	SetupSigPipe();
	AddFd(nEpollfd, nListenfd);
	int nCurrentSize = 16;
	epoll_event *nEvents = new epoll_event[nCurrentSize];
	
	int nReady = 0;
	int nProcessCounter = 0;
	while(!nStopFlag)
	{
		nReady = epoll_wait(nEpollfd, nEvents, nCurrentSize, -1);
        
		if(nReady < 0){
			if(errno == EINTR)
				continue;
			else{
				perror("epoll_wait");
				break;
			}
		}
		if (nCurrentSize == nReady)
        {
            nCurrentSize *= 2;
            epoll_event *nTemp = new epoll_event[nCurrentSize];

            for(int i = 0; i < nReady; i++)
                nTemp[i] = nEvents[i];
            
            delete [] nEvents;
            nEvents = nTemp;
        }
		for(int i=0; i < nReady; i++)
		{
			int nReadyFd = nEvents[i].data.fd;
			if(nReadyFd == nListenfd)
			{
				int i = nProcessCounter;
				//RR
				do
				{
					if (nSubProcess[i].nPid != -1)
						break;
					i = (i+1)%nProcessNumber;
				}while(i != nProcessCounter);
				
				if(nSubProcess[i].nPid == -1)
				{
					nStopFlag = 1;
					break;
				}
				nProcessCounter = (i+1)%nProcessNumber;
				int nTemp = 1;
				write(nSubProcess[i].nPipeFd[0], (char*)&nTemp, sizeof(nTemp));
				printf("Send request to child: %d\n", i);
			}
			else if (nReadyFd == SigPipeFd[0])
			{   
				char Signals[BSIZE];  
                int Res = recv(SigPipeFd[0], Signals, sizeof(Signals), 0);
				if (Res <= 0)
				{
					perror("recv");
					exit(EXIT_FAILURE);
				}
                else if (Res > 0){  
                    for(int i=0; i<Res; i++) 
					{  
                        switch(Signals[i]) 
						{  
                            case SIGCHLD: 
							{  
                                pid_t pid;  
                                while((pid = waitpid(-1, nullptr, WNOHANG)) > 0) {  
                                    for(int i=0; i<nProcessNumber; i++) {   
                                        if(nSubProcess[i].nPid == pid) {  
                                            printf("child %d exit\n", i);  
                                            close(nSubProcess[i].nPipeFd[0]);  
                                            nSubProcess[i].nPid = -1;  //标记为-1  											
										}  
                                    }  
                                }  
                                nStopFlag = true;  
                                for(int i=0; i<nProcessNumber; ++i) {  
                                    if(nSubProcess[i].nPid != -1)  
										nStopFlag = false;  
                                }  
                                break;  
                            }  
                            case SIGTERM:  
                            case SIGINT: 
                                printf("kill all the children now\n");  
                                for(int i=0; i<nProcessNumber; ++i) {  
                                    int pid = nSubProcess[i].nPid;  
									if(pid != -1)  
                                        kill(pid, SIGTERM);  
                                break;  
                            }  
                            default:  
                                break;  
                        }  
                    }  
                }  
            } 
		}
	}
	
	delete [] nEvents;
	close(nEpollfd);	
}

