#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define NUM_OF_THREADS 2
#define SBUFFSIZE 3
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
typedef struct sockaddr SA;


/*typedef struct {
	char method[MAXLINE];
	char port[MAXLINE];
	char url[MAXLINE];
	char resource[MAXLINE];
	char protocol[MAXLINE];
	char version[MAXLINE];
	char host[MAXLINE];
	int server_fd;
}request_t;*/

sbuf_t sbuf;//The buffer for our connections

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
int handle_server_request(int connfd, int *server_fd);
int get_request(char *buff,char *port,char *resource,char *method,char *protocol,char *version,char *host);
int send_request(int connfd,char *buff, rio_t rio, char *port,char *resource,char *method,char *protocol,char *version,char *host,int *server_fd);
void Cclose(int serverfd, int clientfd);
int handle_response(int *server_fd,int clientfd);
int read_response(rio_t *rio, int content_size,char *buf,int clientfd);
void *thread(void *vargp);



int main(int argc, char **argv)
{
	//char port;
	int listenfd, connfd;
	//char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	//ignore SIGPIPE
	Signal(SIGPIPE,SIG_IGN);
	pthread_t tid;//tid for threads

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	else if(argv[1] < 0){
		fprintf(stderr, "usage: %s%s\n <port>\n", argv[0],"Ports are numbers > 0\n");
		exit(1);
	}
	else if(atoi(argv[1]) <= 0){
		fprintf(stderr,"Bro...ports are numbers..duh\n");
		exit(1);
	}
	else{
		listenfd = Open_listenfd(argv[1]);
		sbuf_init(&sbuf,SBUFFSIZE);
		for(int i =0; i < NUM_OF_THREADS; i++){
			Pthread_create(&tid,NULL,thread,NULL);
		}
		//Then as we get connections hook them up with the threads
		//have threads go to a function that detaches them and handles requests
		if(!(listenfd < 0)){
			while (1) {
				clientlen = sizeof(clientaddr);
				connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
				/*Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
							port, MAXLINE, 0);*/
				//fprintf(stdout,"Connected to: %s on port %s",hostname,port);
				//fflush(stdout);
				//sleep(10);
				//fprintf(stdout,"inserting fd:%d\n",connfd);
				sbuf_insert(&sbuf,connfd);
				//Seperate the hostname/query and all else.
				//Then read response and forward it to the client
				//something like GET "query that was sent to me" HTTP/1.0\r\n (so send them everything after the hostname, though make sure
				// that 1.1's become 1.0s)
				//Close(connfd);                                            //line:netp:tiny:close
				//printf("%s", user_agent_hdr);
				//Cclose(request.server_fd,connfd,hostname,port);
			}
		}
		else{
			//error listening to port
			fprintf(stderr, "port being used\n");
			exit(1);
		}
	}
	//Cclose(request.server_fd,connfd,hostname,port);
	return 0;
}
void *thread(void *vargp){
	//Parts of the Request/etc
	//sleep(10);
	Pthread_detach(pthread_self());
	//sleep(10);
	/*//gets the name info
	Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
			port, MAXLINE, 0);*/
	//printf("Accepted connection from (%s, %s)\n", hostname, port);
	while(1){
		//request_t request;

		int connfd = sbuf_remove(&sbuf);
		if(connfd > 0){
			do_server_request(connfd);
		}
	}
	Pthread_exit(NULL);
	return NULL;
}

void do_server_request(int connfd){
	int server_fd = -1;
	int val;
	if((val = handle_server_request(connfd,&server_fd)) < 0){
		if(val == -4){
			fprintf(stderr,"Only Get request supported\n");

		}
		else{
			fprintf(stderr,"An error occurred in handling request\n");
		}
		Cclose(server_fd,connfd);
		//return val;
	}
	//if everything worked
	else if(val == 0 && handle_response(&server_fd,connfd) < 0){
		if(val == -3){
			fprintf(stderr,"An error while writing to client\n");
		}
		else{
			fprintf(stderr,"An error occurred in reading from server\n");
		}
		Cclose(server_fd,connfd);
		//return val;
	}
	/*else{
		Cclose(server_fd,connfd);
	}*/
}
int handle_response(int *serverfd,int clientfd){
	rio_t rio;
	char buf[MAXLINE];//THIS VARIABLE is not safe, it holds the last request for some reason
	int content_size;
	//init the struct
	Rio_readinitb(&rio,(*serverfd));
	//grab first line
	if(Rio_readlineb(&rio,buf,MAXLINE) < 0){
		return -1;
	}
	//send off first line
	else if(Rio_writen(clientfd,buf,strlen(buf)) < 0){
		return -3;
	}
	else{

		while(strcmp(buf,"\r\n") != 0 && strlen(buf) > 0){

			if(Rio_readlineb(&rio,buf,MAXLINE) < 0){
				return -1;
			}
			else{
				//grab the content size
				if(strstr(buf,"Content-Length")){
					sscanf(buf,"Content-Length: %d",&content_size);
				}
				else if(strstr(buf,"Content-length")){
					sscanf(buf,"Content-length: %d",&content_size);
				}
			}
			if(Rio_writen(clientfd,buf,strlen(buf)) < 0){
				return -3;
			}
		}
	}
	return read_response(&rio,content_size,buf,clientfd);
}

int read_response(rio_t *rio, int content_size,char *buf,int clientfd){
	int num;
	if(content_size > 0){
		//if the size of the response is too big
		while(content_size > MAXLINE){
			if((num = Rio_readnb(rio,buf,MAXLINE)) < 0){
				return -1;
			}
			else if(Rio_writen(clientfd,buf,num) == -1){
				return -1;
			}
			else{
				content_size = content_size - MAXLINE;
			}
		}
		if(content_size > 0){
			if(Rio_readnb(rio,buf,content_size) < 0){
				return -1;
			}
			else if(Rio_writen(clientfd,buf,content_size) < 0){
				return -1;
			}
		}
	}
	//no response
	else{
		while((num = Rio_readlineb(rio,buf,MAXLINE))){
			if(Rio_writen(clientfd,buf,num) < 0){
				return -1;
			}
		}
	}
	return 0;
}

int handle_server_request(int connfd, int *server_fd){
	//Holds the request
	char buff[MAXLINE];
	char port[MAXLINE];
	char resource[MAXLINE];
	char method[MAXLINE];
	char host[MAXLINE];
	rio_t rio;

	Rio_readinitb(&rio,connfd);
	if(Rio_readlineb(&rio,buff,MAXLINE) < 0){
		fprintf(stderr,"An error accord in handle_request\n");
		return -1;
	}
	else{

		//EX: GET http://www.cmu.edu/hub/index.html HTTP/1.0
		char protocol[MAXLINE];
		char version[MAXLINE];
		if(get_request(buff,port,resource,method,protocol,version,host) < 0){
			fprintf(stderr,"An error accord in parse_request\n");
			return -1;
		}
		else if(strstr(method,"GET") != NULL){
			return send_request(connfd,buff,rio,port,resource,method,protocol,version,host,server_fd);
		}
		else{
			fprintf(stderr,"Only GET requests supported\n");
			return -4;
			//close the connection, this is not a GET
		}
	}
	return 0;
	/*ssize_t numRead = Rio_readlineb(connfd, res, byte_size);*/
	/*
	ssize_t read = recv(connfd,res_ptr,byte_size,0);
	if(read < 0){
		printf("%s","Error getting a response\n");
		exit(errno);
	}
	if(read == 0){
		read = -100;
	}*/
}

int send_request(int connfd,char *buff, rio_t rio, char *port,char *resource,char *method,char *protocol,char *version,char *host,int *server_fd){

	int ret;
	char sendbuf[MAXLINE];
	strcat(sendbuf,"GET ");//TODO DOT ISSUE??
	strcat(sendbuf,resource);
	strcat(sendbuf," HTTP/1.0\r\n");
	while((ret = Rio_readlineb(&rio,buff,MAXLINE)) != 0){
		//fprintf(stdout,"%s\n\n",buff);

		//TODO send GET /hub/index.html HTTP/1.0 first
		if(ret < 0){
			fprintf(stderr,"Error reading request in send_request\n");
		}
		else if(strcmp(buff,"\r\n") == 0){
			strcat(sendbuf,buff);
			break;
		}
		if(strstr(buff,"Host:") != NULL){
			//strcat(sendbuf,"Host: ");
			strcat(sendbuf,buff);
		}
		else if(strstr(buff,"Proxy-Connection:")){
			strcat(sendbuf,"Proxy-Connection: close\r\n");
		}
		else if(strstr(buff,"Connection:")!= NULL){
			strcat(sendbuf,"Connection: close\r\n");
		}
		else if(strstr(buff,"User-Agent") != NULL){
			strcat(sendbuf,user_agent_hdr);
		}
		else{//anything else
			strcat(sendbuf,buff);
		}
	}

	//send of the request
	(*server_fd) = Open_clientfd(host,port);
	if(*(server_fd) == -1){
		//no connection
		return -1;
	}
	if((*server_fd )== -2){
		//tell client you gave me bad stuff
		strcpy(buff,"BAD REQUEST\n");
		Rio_writen(connfd,buff,strlen(buff));
		return -2;
	}
	else if(Rio_writen((*server_fd),sendbuf,strlen(sendbuf)) < 0){
		//write failed
		return -1;
	}
	memset(sendbuf,0,strlen(sendbuf));
	return 0;
}

int get_request(char *buff,char *port,char *resource,char *method,char *protocol,char *version,char *host){
	if(strlen(buff) < 1 || strstr(buff,"/") == NULL){
		return -1;
	}
	char my_url[MAXLINE];
	//EX GET http://www.cmu.edu/hub/index.html HTTP/1.0
	strcpy(resource,"/");
	//in ordeR: GET,http://www.cmu.edu/hub/index.html,HTTP/1.0
	sscanf(buff,"%s %s %s",method,my_url,version);
	if(strstr(my_url,"://") != NULL){//if there is a url there
		//so looking inside the url we are going to grab the below pieces of the URL
		//in order http,www.cmu.edu,/hub/index.html
		sscanf(my_url,"%[^:]://%[^/]%s",protocol,port,resource);
	}
	else{
		sscanf(my_url,"%[^/]%s",port,resource);
	}
	char *ptr;
	char * p = port;
	//seperate out the port
	if((ptr = strstr(port,":")) != NULL){
		ptr++;
		strncpy(p,ptr,sizeof(p));
	}
	else{
		memset(p, 0, MAXLINE);
		strncpy(p,"80",2);
	}
	//get the host name
	char *t = strstr(my_url,"://");
	if(t != NULL){
		t+=3;
		sscanf(t,"%[^/|^:]",host);
	}
	//reset buff
	memset(buff,0,sizeof(char)*strlen(buff));
	return 0;
}

void Cclose(int serverfd, int clientfd){
	//fprintf(stdout,"Closed connections: (Server:%s,Client:%d)\n",serverfd,clientfd);
	if(serverfd >= 0){
		Close(serverfd);
		serverfd = NULL;
	}
	if(clientfd >= 0){
		Close(clientfd);
		clientfd = NULL;
	}

}
