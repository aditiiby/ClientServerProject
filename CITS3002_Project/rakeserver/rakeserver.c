#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <netdb.h>
#include <pthread.h>
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 
#include <sys/wait.h>


#define MYPORT "1299"
#define TXTLEN 1024
#define LISTEN_BACKLOG 20
#define BACKLOG 5

#define REQ_COST 0x01
#define RES_COST 0x02
#define REQ_FILE 0x03
#define RES_FILE 0x04
#define RES_FINISH_FILE 0x05
#define REQ_EXFILE 0x06
#define RES_EXFILE 0x07

#define FILENAME "filename"
#define NC "nc"
#define ECHO "echo"
#define HOSTNAME "localhost"
#define TEMPLATE "tmp/mt-XXXXXX"
int running = 1;
#define BUFSIZE 1024

// the argument we will pass to the connection-handler threads
struct connection {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    int fd;
};

typedef struct
{
	unsigned int cmd;
	unsigned int serverseq;
}StCostReq;

typedef struct
{
	unsigned int cmd;
	unsigned int serverseq;
	unsigned int cost;
}StCostRes;

//File upload request package
typedef struct
{
	int cmd;//command id
	char upName[24];//upload name
	int fileidx;
	int filesize;//file size
}ReqStUpMsg;

//File upload response package
typedef struct
{
	int cmd;//command id
	char upName[128];//upload name
	int fileidx;
	int status;//response status
}ResUpMsg;

//File upload response package
typedef struct
{
	int cmd;//command id
	char upName[128];//upload name
	int fileidx;
	int status;//response status
}ResUpFinishMsg;

//File upload request package
typedef struct
{
	int cmd;//command id
	char arrfile[256];
	char executefile[24];
}ReqExeMsg;

//File upload response package
typedef struct
{
	int cmd;//command id
	int status;//response status
	char upName[24];//upload name
	int filesize;//file size
}ResExeMsg;

void handler(int signal)
{
	printf("[signal=%d\n", signal);
	running = 0;
}

void *echo(void *arg)
{
	pthread_t thread_id = pthread_self();
	//Create detached thread
	pthread_detach(thread_id);
    char host[100], port[10], buf[BUFSIZE];
    struct connection *c = (struct connection *) arg;
    int error, nread;

	// find out the name and port of the remote host
    error = getnameinfo((struct sockaddr *) &c->addr, c->addr_len, host, 100, port, 10, NI_NUMERICSERV);
    	// we provide:
    	// the address and its length
    	// a buffer to write the host name, and its length
    	// a buffer to write the port (as a string), and its length
    	// flags, in this case saying that we want the port as a number, not a service name
    if (error != 0) 
	{
        fprintf(stderr, "getnameinfo: %s", gai_strerror(error));
        close(c->fd);
        return NULL;
    }

    printf("[%s:%s] connection\n", host, port);	
    while ((nread = recv(c->fd, buf, BUFSIZE, 0)) > 0) 
	{
		int cmd = ntohl(*(int *)buf);
        printf("cmd=%d\n", cmd);
		if (REQ_COST == cmd)
		{
			StCostReq *req = (StCostReq *)buf;
			printf("req->serverseq=%d\n", ntohl(req->serverseq));
 			StCostRes res;
			res.cmd = htonl(RES_COST);
			res.cost = htonl(rand() % 100+1);
			res.serverseq = req->serverseq;	
			//printf("serverseq=%d\n", ntohl(req->serverseq));
			send(c->fd, (char *)&res, sizeof(res), 0);
		}
		else if (REQ_FILE == cmd)
		{
			if (access("./file", F_OK) != 0)
			{
				system("mkdir file");
			}
			
			ReqStUpMsg 	*stUpMsg = (ReqStUpMsg *)buf;
			
			printf("name=[%s] file=%d\n", stUpMsg->upName,
				ntohl(stUpMsg->filesize));
			char file[256];
			snprintf(file, sizeof(file),
				"file/%s", stUpMsg->upName);
			ResUpMsg upMsg;
			upMsg.cmd = htonl(RES_FILE);
			upMsg.status = htonl(0);
			snprintf(upMsg.upName, sizeof(upMsg.upName),
				"%s", stUpMsg->upName);
			send(c->fd, (char *)&upMsg, sizeof(upMsg), 0);//Return correct message
			//printf("len=%d id=%d upName=%s\n", len, *id, stUpMsg->upName);
		
			char filedata[1024];
			int size = ntohl(stUpMsg->filesize);
			int ret;
			unlink(file);
			int fd = open(file, O_CREAT | O_APPEND | O_WRONLY, 0777);	/* open the file to be sent back */
		    if (fd < 0) 
			{
				continue;
			}

			//Receiving file data cyclically
			while (size > 0)
			{
				ret = read(c->fd, filedata, 1024);//Read file data
				//printf("ret=%d size=%d\n", ret, size);
				size -= ret;
				write(fd, filedata, ret);//Write file data
			}

			close(fd);
			char dest[1024];
			snprintf(dest, sizeof(dest), "%s", file);
			char *args[12];
			args[0] = "gcc";
			args[1] = "-c";
			file[strlen(file)-1] = 'c';
			args[2] = file;
			args[3] = "-o";
			
			dest[strlen(dest)-1] = 'o';
			args[4] = dest;
			args[5] = NULL;
		    //char *args[] = {binaryPath, "-c", "func1.c", "-o", "func1.o", NULL};
			pid_t pid;
			int status;

			switch(pid = fork()) //Create child process
			{
		      // If fork error occurs, you can prompt maintenance information
		      case -1:
		              printf("The current shell is under maintenance...");
		              return NULL;
		      // 处理子进程
		      case 0:
				execvp(args[0], args);//Execute command function
				// If the child process is successfully replaced, it is impossible to execute the following code
				fprintf(stderr, "%s: command not found\n", args[0]);
				exit(1);//Error executing the command Exit the child process
		             
		      default:
				waitpid(pid, &status, 0); //Wait for the child process to return
				//printf("status=%d dest=%s\n", status, dest);
				if (0 == status)
				{
					ResUpMsg upMsg;
					upMsg.cmd = htonl(RES_FILE);
					upMsg.status = htonl(0);
					snprintf(upMsg.upName, sizeof(upMsg.upName),
						"%s", stUpMsg->upName);

					ResUpFinishMsg res;
					res.cmd = htonl(RES_FINISH_FILE);
					res.status = htonl(0);
					
					snprintf(res.upName, sizeof(upMsg.upName),
											"%s", stUpMsg->upName);
					res.fileidx = stUpMsg->fileidx;
					send(c->fd, (char *)&res, sizeof(res), 0);
				}
				break;
			}    

			
		}
		else if (REQ_EXFILE == cmd)
		{
			ReqExeMsg *exeMsg = (ReqExeMsg *)buf;
			//printf("arrfile=%s executefile=%s\n", 
			//	exeMsg->arrfile, exeMsg->executefile);
			char *word = strtok(exeMsg->arrfile,";");	
			
			char dest[24][24];
			int idx = 0;
			char *args[12];
			args[0] = "gcc";
			while(word != NULL)
			{						
				//printf("word=%s\n", word);
				snprintf(dest[idx], sizeof(dest[idx]), "file/%s", 
					word);
				args[idx+1] = dest[idx];
				word = strtok(NULL, ";");
				idx++;
			}

			idx++;
			args[idx] = "-o";
			idx++;
			char exefile[1024];
			
			snprintf(exefile, sizeof(exefile), "file/%s", 
					exeMsg->executefile);
			unlink(exefile);
			args[idx] = exefile;
			idx++;
			args[idx] = NULL;
			pid_t pid;
			int status;

			switch(pid = fork()) //Create child process
			{
		      // If fork error occurs, you can prompt maintenance information
		      case -1:
		              printf("The current shell is under maintenance...");
		              return NULL;
		      // 处理子进程
		      case 0:
				execvp(args[0], args);//Execute command function
				// If the child process is successfully replaced, it is impossible to execute the following code
				fprintf(stderr, "%s: command not found\n", args[0]);
				exit(1);//Error executing the command Exit the child process
		             
		      default:
				waitpid(pid, &status, 0); //Wait for the child process to return
				//printf("status=%d\n", status);
				if (0 == status)
				{
					//snprintf(exefile, sizeof(exefile), "./file/%s", 
						//exeMsg->executefile);
					//system(exefile);
					ResExeMsg resexe;
					resexe.cmd = htonl(RES_EXFILE);
					resexe.status = htonl(0);					
					snprintf(resexe.upName, sizeof(resexe.upName),
											"%s", exeMsg->executefile);
					int fd = open((char *)exefile, O_RDONLY);	/* Get file size  */
				    if (fd < 0) 
					{
						printf("open file %s failure\n", exefile);
						//return -1;
					}

					int flesize = lseek(fd, 0, SEEK_END);/* Get file size  */
					//printf("file neon size is [%d]\n", flesize);
					lseek(fd, 0, SEEK_SET);
					resexe.filesize = htonl(flesize);/* Get file size  */
					send(c->fd, (char *)&resexe, sizeof(resexe), 0);
					char filedata[1024];
					while (1)
					{
						int bytes = read(fd, filedata, 1024);	/* read from file */
						if (bytes <= 0) break;		/* check for end of file */ 					
						write(c->fd, filedata, bytes);//write to server
					}
				
					close(fd);	
				}
				break;
			}
		}
		
    }

    printf("[%s:%s] got EOF\n", host, port);
    close(c->fd);
    free(c);
    return NULL;
}


int server(char *port)
{
    struct addrinfo hint, *info_list, *info;
    struct connection *con;
    int error, sfd;
    pthread_t tid;

    // initialize hints
    memset(&hint, 0, sizeof(struct addrinfo));
    hint.ai_family = AF_UNSPEC;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_PASSIVE;
    	// setting AI_PASSIVE means that we want to create a listening socket

    // get socket and address info for listening port
    // - for a listening socket, give NULL as the host name (because the socket is on
    //   the local host)
    printf("listening on port %s\n", port);
    error = getaddrinfo(NULL, port, &hint, &info_list);
    if (error != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
        return -1;
    }

    // attempt to create socket
    for (info = info_list; info != NULL; info = info->ai_next) {
        sfd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
        
        // if we couldn't create the socket, try the next method
        if (sfd == -1) {
            continue;
        }

		int nVal = 1;
		if(setsockopt(sfd,SOL_SOCKET,SO_REUSEADDR,&nVal,sizeof(int))==-1)//Set the port to be reusable
		{ 
			perror("setsockopt");
			continue;
		}

        // if we were able to create the socket, try to set it up for
        // incoming connections;
        // 
        // note that this requires two steps:
        // - bind associates the socket with the specified port on the local host
        // - listen sets up a queue for incoming connections and allows us to use accept
        if ((bind(sfd, info->ai_addr, info->ai_addrlen) == 0) &&
            (listen(sfd, BACKLOG) == 0)) {
            break;
        }

        // unable to set it up, so try the next method
        close(sfd);
    }

    if (info == NULL) {
        // we reached the end of result without successfuly binding a socket
        fprintf(stderr, "Could not bind\n");
        return -1;
    }

    freeaddrinfo(info_list);

	struct sigaction act;
	act.sa_handler = handler;
	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	sigaction(SIGINT, &act, NULL);
	
	sigset_t mask;
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	
    // at this point sfd is bound and listening
    printf("Waiting for connection\n");
	while (running) 
	{
    	// create argument struct for child thread
		con = malloc(sizeof(struct connection));
        con->addr_len = sizeof(struct sockaddr_storage);
        	// addr_len is a read/write parameter to accept
        	// we set the initial value, saying how much space is available
        	// after the call to accept, this field will contain the actual address length
        
        // wait for an incoming connection
        con->fd = accept(sfd, (struct sockaddr *) &con->addr, &con->addr_len);
        	// we provide
        	// sfd - the listening socket
        	// &con->addr - a location to write the address of the remote host
        	// &con->addr_len - a location to write the length of the address
        	//
        	// accept will block until a remote host tries to connect
        	// it returns a new socket that can be used to communicate with the remote
        	// host, and writes the address of the remote hist into the provided location
        
        // if we got back -1, it means something went wrong
        if (con->fd == -1) 
		{
			free(con);
            perror("accept");
            continue;
        }

		// spin off a worker thread to handle the remote connection
        error = pthread_create(&tid, NULL, echo, con);
		// if we couldn't spin off the thread, clean up and wait for another connection
        if (error != 0) {
            fprintf(stderr, "Unable to create thread: %d\n", error);
            close(con->fd);
            free(con);
            continue;
        }
		
		// otherwise, detach the thread and wait for the next connection request
       // pthread_detach(tid);
    }

	puts("No longer listening.");
    return 0;
}


int main(int argc, char *argv[])
{
	if (argc != 2) 
	{
		printf("Usage: %s [port]\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));
	(void) server(argv[1]);	
}
