#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <fcntl.h> 

#define r_no 128
#define		MAX_UNIT_NUM    		128		//Maximum number of connections

#define REQ_COST 0x01
#define RES_COST 0x02
#define REQ_FILE 0x03
#define RES_FILE 0x04
#define RES_FINISH_FILE 0x05
#define REQ_EXFILE 0x06
#define RES_EXFILE 0x07

typedef struct
{
	char ip[16];
	unsigned short port;
	int sock;
}StHostInfo;

typedef struct
{
	StHostInfo hostinfo[12];
	int cnt;
}StHostData;

StHostData g_hostdata;

typedef struct
{
	char arrfile[24][12];
	char executefile[24];
	int cnt;
}StFileInfo;

StFileInfo g_stsrcFile;
StFileInfo g_stobjFile;


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


typedef struct __st_client__
{
	int    iFd;//sock fd
	struct sockaddr_in 	addr;
	int 	lasttime;  //Can be used to judge the connection time is too long
	int	datalen;
	char   data[128+1];
}UNIT_CLIENT;

UNIT_CLIENT  *g_stUnitClient = NULL;
char *lines[r_no];
int rows = 0;


int Connect(char *rraddr, int port)
{
	int sock;
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1)
	{
	    perror("Could not create socket");
	    return -1;
	} 
	
	struct sockaddr_in server;
	
		
	server.sin_addr.s_addr = inet_addr(rraddr);//Itself is ip
	if (inet_aton(rraddr, &server.sin_addr) == 0)//Fill in the IP address
	{
	    printf("Wrong IP");
	}
	
	server.sin_family = AF_INET;
	server.sin_port = htons((unsigned short)port);	//The port is specified on the command line	

	//connect to the server
	if (connect(sock,
	            (struct sockaddr *) &server,
	            sizeof(server)) == -1)
	{
	    printf("Error: Failed to make connection to %s:%d\n", rraddr, port);
	    return -1;
	}
				
	return sock;
}

void InitUnitClient()
{
	g_stUnitClient = (UNIT_CLIENT *)malloc(MAX_UNIT_NUM * sizeof(UNIT_CLIENT));
	if(g_stUnitClient == NULL)
	{
		exit(-1);
	}
	int i;
	//Initialize the client structure
	for(i = 0; i < MAX_UNIT_NUM; i++) 
	{
		g_stUnitClient[i].iFd = -1;
		g_stUnitClient[i].datalen = -1;
		memset(g_stUnitClient[i].data, 0x00, sizeof(g_stUnitClient[i].data));
	}
}


/*
Read file by line by line
Ignore line starting with '#' and '\r'(return carriage).
allocate memory for line and store into array
*/
int readfile(char *filename)
{
    size_t bufsize = 0;
    ssize_t read;
    char *line;
    FILE *fp = fopen(filename, "r");

    if(!fp)
	{
        fprintf(stderr, "Fail to open file '%s'\n", filename);
        return EXIT_FAILURE;
    }

    while((read = getline(&line, &bufsize, fp)) != -1)
	{
		if (line[0] == '#')
			continue;
	
		char *pos = strchr(line, '\r');
		if (pos)
			*pos = '\0';

		pos = strchr(line, '\n');
		if (pos)
			*pos = '\0';
		if (0 == strlen(line))
			continue;
		
        lines[rows] = malloc(strlen(line) + 1);
        strcpy(lines[rows], line);
        ++rows;
        
    }
    free(line);
    fclose(fp);
    return EXIT_SUCCESS;
}

void clearfset(fd_set *rset, int sockfd, int i)
{
	shutdown(sockfd, 2);
	close(sockfd);
	FD_CLR(sockfd, rset);
	g_stUnitClient[i].iFd = -1;
	g_stUnitClient[i].datalen = 0;
}

int AddSock(int sock)
{
	int i;
	for(i = 0; i < MAX_UNIT_NUM; i++) //Find an empty array of customer structure, it is recommended to use a linked list to apply for a new node 
	{
		if(g_stUnitClient[i].iFd < 0)
		{
			g_stUnitClient[i].iFd = sock;                       
			//g_stUnitClient[i].addr = addr;
			g_stUnitClient[i].datalen = 0;                        
			g_stUnitClient[i].data[0] = '\0';
			g_stUnitClient[i].lasttime = time(NULL);
			
			break;
        }						
    }
	return 0;
}

int GetIpInfo()
{
	int i;
	int type = 0;
	int cnt = 0;
	int port;				
	char *pos;
	int hostcnt = 0;
	for (i=0; i<rows; i++)
	{
		if (lines[i][0] != 9 
			&& strstr(lines[i], "actionset") == NULL)
		{
			if (strstr(lines[i], "PORT") != NULL)
			{
				type = 1;
			}
			else if (strstr(lines[i], "HOSTS") != NULL)
			{
				type = 2;
			}
			
			cnt = 0;
			char *word = strtok(lines[i]," ");
			
			while(word != NULL)
			{		
				if (cnt >=2)
				{
					if (type == 1)
						port = atoi(word);
					else if (type == 2) 
					{
						pos = strstr(word, ":");
						if (pos == NULL)
						{
							snprintf(g_hostdata.hostinfo[hostcnt].ip, 
								sizeof(g_hostdata.hostinfo[hostcnt].ip), 
								"%s", word);
							g_hostdata.hostinfo[hostcnt].port = port;
						}				
						else
						{
							strncpy(g_hostdata.hostinfo[hostcnt].ip, word, pos - word);
							g_hostdata.hostinfo[hostcnt].port = atoi(pos+1);
						}

						hostcnt++;
					}
						
				}
				
				//printf("word=%s\n", word);	
				word = strtok(NULL, " ");
				cnt++;
			}					
		}
		else if (lines[i][0] == 9)
		{
			//printf("line11=%s\n", lines[i]);				
		}
	}

	g_hostdata.cnt = hostcnt;
	return 0;
}

int GetCompileInfo()
{
	int i;	
	int cnt = 0;
	int filecnt = 0;
	for (i=0; i<rows; )
	{
		if (strstr(lines[i], "actionset") != NULL)
		{
			if (strstr(lines[i], "actionset1") != NULL)
			{
				while (strstr(lines[i], "actionset2") == NULL)
				{
					if (strstr(lines[i], "remote-cc") != NULL)
					{
						cnt = 0;
						char *word = strtok(lines[i]," ");
				
						while(word != NULL)
						{		
							if (cnt >=2)
							{	
								snprintf(g_stsrcFile.arrfile[filecnt],
									sizeof(g_stsrcFile.arrfile[filecnt]),
									"%s", word);
								filecnt++;
								//printf("word22=%s\n", word);		
							}
							
							//
							word = strtok(NULL, " ");
							cnt++;
						}		
					}
					
					i++;
				}				
			}
			else if (strstr(lines[i], "actionset2") != NULL)
			{
				while (strstr(lines[i], "actionset3") == NULL)
				{
					if (strstr(lines[i], "remote-cc") != NULL)
					{
						cnt = 0;
						char *word = strtok(lines[i]," ");
				
						while(word != NULL)
						{		
							if (cnt >=2)
							{
								snprintf(g_stsrcFile.arrfile[filecnt],
									sizeof(g_stsrcFile.arrfile[filecnt]),
									"%s", word);
								filecnt++;		
							}
							
							//
							word = strtok(NULL, " ");
							cnt++;
						}		
					}
					
					i++;
				}				
			}
			else if (strstr(lines[i], "actionset3") != NULL)
			{
				i++;
				i++;
				StFileInfo tmpfileinfo;
				tmpfileinfo.cnt = 0;
				int tmpcnt = 0;
				if (strstr(lines[i], "requires") != NULL)
				{
					cnt = 0;
					char *word = strtok(lines[i]," ");
			
					while(word != NULL)
					{		
						if (cnt >=1)
						{
							//printf("word22=%s\n", word);
							snprintf(tmpfileinfo.arrfile[tmpcnt],
									sizeof(tmpfileinfo.arrfile[tmpcnt]),
									"%s", word);
								tmpcnt++;	
						}
						
						//
						word = strtok(NULL, " ");
						cnt++;
					}		
				}

				for (int j=0; j<tmpcnt; j++)
				{
					//printf("file=%s tmpcnt=%d\n", tmpfileinfo.arrfile[i],
						//tmpcnt);	
				}
				
				i--;
				g_stobjFile.cnt = 0;
				int objcnt = 0;
				if (strstr(lines[i], "remote-cc") != NULL)
				{
					cnt = 0;
					char *word = strtok(lines[i]," ");
			
					while(word != NULL)
					{		
						if (cnt >=2)
						{
							int exist = 0;
							for (int j=0; j<tmpcnt; j++)
							{
								if (0 == strcmp(word, tmpfileinfo.arrfile[j]))
									exist = 1;
							}

							if (exist)
							{
								snprintf(g_stobjFile.arrfile[objcnt],
									sizeof(g_stobjFile.arrfile[objcnt]),
									"%s", word);
								objcnt++;	
							}
							else
							{
								snprintf(g_stobjFile.executefile,
									sizeof(g_stobjFile.executefile),
									"%s", word);
							}
							
							
							//printf("word11=%s\n", word);		
						}
						
						//
						word = strtok(NULL, " ");
						cnt++;
					}		
				}

				for (int j=0; j<objcnt; j++)
				{
					//printf("file=%s tmpcnt=%d\n", g_stobjFile.arrfile[j],
						//objcnt);
				}

				//printf("executefile=%s tmpcnt=%d\n", g_stobjFile.executefile,
						//objcnt);
				g_stobjFile.cnt = objcnt;
				i++;
								
			}
		}
		else
		{
			i++;
		}
	}

	g_stsrcFile.cnt = filecnt;
	return 0;
}


int main(int argc, char **argv)
{ 
	if (argc < 2)
		printf("%s filename\n", argv[0]);
    readfile(argv[1]);	
	char message[1024] = {0};
	char recvbuf[1024] = {0}; 
	int recvlen;	
	fd_set rset, allset;
	int i;
	int maxfd = 0;
	FD_ZERO(&allset); //Initialize the read set
	int timeoutval = 24*60*60;
	struct timeval timeout;
	InitUnitClient();
	
	memset(&g_hostdata, 0, sizeof(g_hostdata));
	memset(&g_stobjFile, 0, sizeof(g_stobjFile));
	
	GetIpInfo();
	GetCompileInfo();
	for (i=0; i<g_stsrcFile.cnt; i++)
	{
		//printf("file=%s\n", g_stsrcFile.arrfile[i]);	
	}
	
	for (i=0; i<g_hostdata.cnt; i++)
	{
		//printf("ip=%s port=%d\n", g_hostdata.hostinfo[i].ip, 
			//g_hostdata.hostinfo[i].port);
		int sock = Connect(g_hostdata.hostinfo[i].ip, 
			g_hostdata.hostinfo[i].port);//Connect to the server
		if (-1 == sock)
		{
			exit(1);	
		}

		FD_SET(sock,&allset);
	
		strcpy(message, "test function");
		StCostReq costreq;
		costreq.cmd = htonl(REQ_COST);
		costreq.serverseq = htonl(i+1);
		if(send(sock, (char *)&costreq, sizeof(costreq), 0) < 0)//Send data to the server
		{
			perror("Send failed");
			exit(1);
		}
		
		if(sock > maxfd) 
			maxfd = sock;    //Get maxfd 
		
		AddSock(sock);
		g_hostdata.hostinfo[i].sock = sock;
	}

	while (1)
	{
		timeout.tv_sec = timeoutval;
		timeout.tv_usec = 0;
		rset = allset;
		int nready = select(maxfd + 1, &rset, NULL, NULL, &timeout); //Note that it is maxfd+1, which returns the number of ready
		if(nready <= 0) //Returning -1 indicates that an error occurred in the select() function, and the description word set becomes unpredictable at this time
		{
			sleep(1);  //If select() goes wrong, empty the collection and start again
			FD_ZERO(&allset); //Initialize the read set 
	    	//FD_SET(sock, &allset); //Will listen on socket
	        continue;     	
	    }

		int i;
		int sockfd;
		int recvhostcnt = 0;
		int mincost = 10000;
		int minseq = 0;
		for(i = 0; i <= maxfd; i++)
		{
			if((sockfd = g_stUnitClient[i].iFd) < 0)
					continue;	
			if(FD_ISSET(sockfd, &rset))
			{
				recvlen = recv(sockfd, recvbuf, 1024, 0);//Receive client data
				if (recvlen <= 0)//Reception failed or client disconnected
				{
					//printf("recvlen=%d\n", recvlen);	
					close(sockfd);
					exit(1);
				}
				 else if(recvlen > 0)
				{
					memcpy(g_stUnitClient[i].data+g_stUnitClient[i].datalen, recvbuf, recvlen);
					g_stUnitClient[i].datalen += recvlen;
					g_stUnitClient[i].lasttime = time(NULL);
				}
				else if(recvlen < 0)  //Receive error exception
				{
					clearfset(&allset, sockfd, i);
					break;
				}
				
				//printf("recvlen=%d\n", recvlen);
				int cmd = ntohl(*(int *)recvbuf);
        		char filedata[1024];
				if (RES_COST == cmd)
				{
					StCostRes *res = (StCostRes *)recvbuf;
					//printf("cost=%d serverseq=%d\n", 
						//ntohl(res->cost), ntohl(res->serverseq));
					recvhostcnt++;
					int cost = ntohl(res->cost);
					if (mincost > cost)
					{
						mincost = cost;
						minseq = ntohl(res->serverseq);
					}
					
					if (recvhostcnt >= g_hostdata.cnt)
					{
						recvhostcnt = 0;
						//printf("cost=%d serverseq=%d\n", 
							//mincost, minseq);
						ReqStUpMsg file;						
						file.cmd = htonl(REQ_FILE);
						file.fileidx = htonl(0);
						snprintf(file.upName, sizeof(file.upName),
							"%s", g_stsrcFile.arrfile[0]);
						int fd = open((char *)g_stsrcFile.arrfile[0], O_RDONLY);	/* Get file size  */
					    if (fd < 0) 
						{
							printf("open file %s failure\n", "func1.c");
							return -1;
						}

						int flesize = lseek(fd, 0, SEEK_END);/* Get file size  */
						//printf("file neon size is [%d]\n", flesize);
						lseek(fd, 0, SEEK_SET);
						file.filesize = htonl(flesize);/* Get file size  */
						send(g_hostdata.hostinfo[minseq-1].sock, (char *)&file, sizeof(file), 0);
						close(fd);	
					}
				}
				else if (RES_FILE == cmd)
				{				
					ResUpMsg *res = (ResUpMsg *)recvbuf;
					if (0 == res->status)
					{
						int fd = open((char *)res->upName, O_RDONLY);	/* open the file to read content */
					    if (fd < 0) 
						{
							return -1;
						}
						//printf("upName=%s\n", res->upName);
						while (1)
						{
							int bytes = read(fd, filedata, 1024);	/* read from file */
							if (bytes <= 0) break;		/* check for end of file */ 					
							write(sockfd, filedata, bytes);//write to server
						}
				
						close(fd);	
					}					
				}
				else if (RES_FINISH_FILE == cmd)
				{				
				
					ResUpFinishMsg *res = (ResUpFinishMsg *)recvbuf;
					//printf("RES_FINISH_FILE status=%d\n",
						//ntohl(res->status));
					if (0 == ntohl(res->status))
					{
						int idx = ntohl(res->fileidx);
						idx++;
						//printf("RES_FINISH_FILE 11 idx=%d cnt=%d\n", 
							//idx, g_stsrcFile.cnt);
						if (idx >= g_stsrcFile.cnt)
						{
							
							ReqExeMsg exemsg;
							memset(&exemsg, 0, sizeof(exemsg));
							exemsg.cmd = htonl(REQ_EXFILE);
							snprintf(exemsg.executefile,
									sizeof(exemsg.executefile),
									"%s", g_stobjFile.executefile);
							//printf("RES_FINISH_FILE exit idx=%d cnt=%d executefile=%s\n", 
							//idx, g_stsrcFile.cnt, exemsg.executefile);
							for (int j=0; j<g_stobjFile.cnt; j++)
							{
								strcat(exemsg.arrfile, g_stobjFile.arrfile[j]);
								if (j != g_stobjFile.cnt - 1)
									strcat(exemsg.arrfile, ";");
							}

							//printf("RES_FINISH_FILE exit idx=%d cnt=%d executefile=%s arrfile=%s\n", 
							//idx, g_stsrcFile.cnt, exemsg.executefile, exemsg.arrfile);

							send(sockfd, (char *)&exemsg, sizeof(exemsg), 0);
						}
						else
						{
							ReqStUpMsg file;						
							file.cmd = htonl(REQ_FILE);
							file.fileidx = htonl(idx);
							snprintf(file.upName, sizeof(file.upName),
								"%s", g_stsrcFile.arrfile[idx]);
							int fd = open((char *)g_stsrcFile.arrfile[idx], O_RDONLY);	/* Get file size  */
					    	if (fd < 0) 
							{
								printf("open file %s failure\n", "func1.c");
								return -1;
							}

							int flesize = lseek(fd, 0, SEEK_END);/* Get file size  */
							//printf("file neon size is [%d]\n", flesize);
							lseek(fd, 0, SEEK_SET);
							file.filesize = htonl(flesize);/* Get file size  */
							send(sockfd, (char *)&file, sizeof(file), 0);
							close(fd);	
						}
					}					
				}
				else if (RES_EXFILE == cmd)
				{
					ResExeMsg *res = (ResExeMsg *)recvbuf;
					//printf("RES_EXFILE status=%d\n",
						//ntohl(res->status));
					if (0 == ntohl(res->status))
					{
						//printf("RES_EXFILE filename=%s filesize=%d\n",
							//res->upName, ntohl(res->filesize));	
						unlink(res->upName);
						int fd = open(res->upName, O_CREAT | O_APPEND | O_WRONLY, 0777);	/* open the file to be sent back */
					    if (fd < 0) 
						{
							continue;
						}

						int size = ntohl(res->filesize);
						int ret;
						//Receiving file data cyclically
						while (size > 0)
						{
							ret = read(sockfd, filedata, 1024);//Read file data
							//printf("ret=%d size=%d\n", ret, size);
							size -= ret;
							write(fd, filedata, ret);//Write file data
						}

						close(fd);
						char exefile[1024];
						snprintf(exefile, sizeof(exefile), "./%s", 
							res->upName);
						printf("execute file %s run result is\n", 
							res->upName);
						system(exefile);
						return 0;
					}
				}
				
				if(--nready <= 0)
	                 break; 	
			}
		}
	}
	
    return EXIT_SUCCESS;
}
