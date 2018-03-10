#include <stdio.h>
#include "csapp.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
typedef struct sockaddr SA;

typedef struct {
	char method[MAXLINE];
	char port[MAXLINE];
	char url[MAXLINE];
	char resource[MAXLINE];
	char protocol[MAXLINE];
	char version[MAXLINE];
	int server_fd;
}request_t;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
int handle_server_request(int connfd);
int parse_request(request_t *request,char *buff);



int main(int argc, char **argv)
{
	//char port;
	int listenfd, connfd;
	char hostname[MAXLINE], port[MAXLINE];
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;

	/* Check command line args */
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}
	else if(argv[1] < 0){
		fprintf(stderr, "usage: %s%s\n <port>\n", argv[0],"Ports are numbers > 0");
		exit(1);
	}
	else{
		listenfd = Open_listenfd(argv[1]);
		if(!(listenfd < 0)){
			while (1) {
				clientlen = sizeof(clientaddr);
				connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
				//gets the name info
				Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE,
						port, MAXLINE, 0);
				printf("Accepted connection from (%s, %s)\n", hostname, port);

				//line:netp:tiny:doit
				//take request we recieved and parse it. Determine if valid. If it is then:
				//Proxy then opens a connection to the host given and sends an HTTP request of
				//my own for the object client wanted
				handle_server_request(connfd);
				//Seperate the hostname/query and all else.
				//Then read response and forward it to the client
				//something like GET "query that was sent to me" HTTP/1.0\r\n (so send them everything after the hostname, though make sure
				// that 1.1's become 1.0s)
				//Close(connfd);                                            //line:netp:tiny:close
				//printf("%s", user_agent_hdr);

			}
		}
		else{
			//error listening to port
			fprintf(stderr, "port being used");
			exit(1);
		}
	}
	return 0;
}

int handle_server_request(int connfd){
	//Parts of the Request/etc
	/*char method[MAXLINE];
	char port[MAXLINE];
	char resource[MAXLINE];
	char protocol[MAXLINE];
	char version[MAXLINE];*/
	request_t request;
/*	int server_fd;*/

	//Holds the request
	char buff[MAXLINE];
	rio_t rio;
	Rio_readinitb(&rio,connfd);
	if(Rio_readlineb(&rio,buff,MAXLINE) < 0){
		fprintf(stderr,"An error accord in handle_request");
		return -1;
	}
	else{

		//EX: GET http://www.cmu.edu/hub/index.html HTTP/1.0
		if(parse_request(&request,buff) < 0){
			fprintf(stderr,"An error accord in parse_request");
			return -1;
		}
		else{
			//return forward_request(buff,rio,request.server_fd,request.port);
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

int parse_request(request_t *request,char *buff){
	if(strlen(buff) < 1 || strstr(buff,"/") == NULL){
		return -1;
	}
	//EX GET http://www.cmu.edu/hub/index.html HTTP/1.0
	strcpy(request->resource,"/");
	//in ordeR: GET,http://www.cmu.edu/hub/index.html,HTTP/1.0
	sscanf(buff,"%s %s %s",request->method,request->url,request->version);
	if(strstr(request->url,"://") != NULL){//if there is a url there
		//so looking inside the url we are going to grab the below pieces of the URL
		//in order http,www.cmu.edu,/hub/index.html
		sscanf(request->url,"%[^:]://%[^/]%s",request->protocol,request->port,request->resource);
	}
	else{
		sscanf(request->url,"%[^/]%s",request->port,request->resource);
	}
	char *ptr;
	char * p = request->port;
	if((ptr = strstr(request->port,":")) != NULL){
		ptr++;
		strncpy(p,ptr,sizeof(p));
	}
	else{
		memset(p, 0, MAXLINE);
		strncpy(p,"80",2);
	}
	return 0;
}
