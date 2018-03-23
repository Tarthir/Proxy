#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "logger.h"
#include "cache.h"

#define NUM_OF_THREADS 3
#define DO_CACHE 10
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

//TODO Create logger class
/**
 * Logger thread will log web accesses and requests to a file
 * It will need to do so one message at a time. Have worker threads
 * call a function which will begin the logging. The logging thread will be pre-created and waiting
 * The callee thread will put the message into a log queue(possibly need a lock), the logger thread will then dequeue as it goes
 * and write messages to the appropriate file
//TODO caching
 * We will have a cache class which will store data, being keyed by a request(resource/host/port/etc)
 * When we have read the entire server's response we will attempt to cache it. If there is room we will store all the data.
 * Every cached object will have a timestamp(?) which will determine the last time it was used. If when we try to write to
 * the buffer we find that it is full we will then evict the last thing we wrote to the buffer and try again, keep trying until
 * success
 *
 * We will need to setup locks such that only one thread can write to the cache at a time, BUT multiple threads can read at once
 * See slides for details on how to do so
 */

sbuf_t sbuf;//The buffer for our connections

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";
int handle_server_request(int connfd, int *server_fd,char *cache_id,void *cache,unsigned int *cache_len);
int get_request(char *buff,char *port,char *resource,char *method,char *host);
int send_request(int connfd,char *buff, rio_t rio, char *port,char *resource,char *host,int *server_fd,char *cache_id,void *cache,unsigned int *cache_len);
void Cclose(int serverfd, int clientfd);
int handle_response(int *serverfd,int clientfd,char *cache_id,void *cache);
int read_and_send_response(rio_t *rio, int content_size,char *buf,int clientfd,char *cache_id,void *cache,int *objsize,unsigned *cache_len);
void *thread(void *vargp);
void do_server_request(int connfd);
int send_cached_items(int connfd,void *cache,unsigned int cache_len);
int cache_it(char *buf,int clientfd,int *objsize,void *cache,unsigned int *cache_len,unsigned int num);



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

	//check the command line arguments

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
		sbuf_init(&sbuf,SBUFFSIZE); /**Was told it was okay to use this class by TA*/
		cache_init();
		//		char * i = "123";
		//		char * j = "abc";
		//		size_t s = 10;
		//cache_URL(i, j, s);
		logger_init();
		//create the threads ahead of time to be put in pool
		for(int i =0; i < NUM_OF_THREADS; i++){
			Pthread_create(&tid,NULL,thread,NULL);
		}
		//Then as we get connections hook them up with the threads
		//have threads go to a function that detaches them and handles requests
		if(!(listenfd < 0)){
			while (1) {
				clientlen = sizeof(clientaddr);
				connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); //line:netp:tiny:accept
				//we got a connection, insert file descriptor into fd pool
				sbuf_insert(&sbuf,connfd);
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

/**This function handles the closing of the server and client file descriptors
 * @param serverfd The server file descriptor
 * @param clientfd The client file descriptor
 * */
void Cclose(int serverfd, int clientfd){
	//fprintf(stdout,"Closed connections: (Server:%s,Client:%d)\n",serverfd,clientfd);
	if(serverfd >= 0){
		Close(serverfd);
		serverfd = -1;
	}
	if(clientfd >= 0){
		Close(clientfd);
		clientfd = -1;
	}

}
/**
 * This function is called by each thread when it gets a connection
 * No parameter is passed in since im using a connection pool
 * */
void *thread(void *vargp){
	//Parts of the Request/etc
	//sleep(10);
	Pthread_detach(pthread_self());
	//sleep(10);
	//printf("Accepted connection from (%s, %s)\n", hostname, port);
	while(1){
		//request_t request;
		//wait until a connection is available
		int connfd = sbuf_remove(&sbuf);
		if(connfd > 0){
			//now that we have a connection, lets get some requests
			do_server_request(connfd);
		}
	}
	Pthread_exit(NULL);
	return NULL;
}
/**This function controls the logic which handles the reading of the request we get from the client
 * as well as sending that request of by calling two helper methods
 * @param connfd The client connection file descriptor
 * */
void do_server_request(int connfd){
	int server_fd = -1;
	int val;
	char cache[MAX_OBJECT_SIZE];
	char cache_id[MAXLINE];
	unsigned int cache_len;

	//handles parsing/reading/sending of client request to server
	if((val = handle_server_request(connfd,&server_fd,cache_id,cache,&cache_len)) < 0){
		if(val == -4){
			logg("Only Get request supported\n");

		}
		else{
			logg("An error occurred in handling request\n");
		}
		Cclose(server_fd,connfd);
		return;
	}
	//read items from cache and send
	else if(val == DO_CACHE){
		if(send_cached_items(connfd,cache,cache_len) < 0){
			logg("Error sending cache to client");
			Cclose(server_fd,connfd);
			return;
		}
	}
	//if everything worked
	//handles the parsing/reading/sending of server response to client/caches
	else if(val == 0 && handle_response(&server_fd,connfd,cache_id,cache) < 0){
		if(val == -3){
			logg("An error while writing to client\n");
		}
		else{
			logg("An error occurred in reading from server\n");
		}
		Cclose(server_fd,connfd);
		return;
	}
	/*else{
		Cclose(server_fd,connfd);
	}*/
}
/**Sends cached item to client*/
int send_cached_items(int connfd,void *cache,unsigned int cache_len){
	if(Rio_writen(connfd,cache,cache_len) < 0){
		return -1;
	}
	else{
		return 0;
	}
}

/**
 * Parses through and sends off the client request to the server
 * @oaram connfd The client file descriptor
 * @param serverfd The server file descriptor
 * @return whether we were succesful or not
 * */
int handle_server_request(int connfd, int *server_fd,char *cache_id,void *cache,unsigned int *cache_len){
	//Holds the request items
	char buff[MAXLINE]; //holds what the client sent us
	char port[MAXLINE];
	char resource[MAXLINE];
	char method[MAXLINE];
	char host[MAXLINE];
	rio_t rio;
	memset(cache,0,MAX_OBJECT_SIZE);
	Rio_readinitb(&rio,connfd);
	//grab the first line of the get request
	//EX: GET http://www.cmu.edu/hub/index.html HTTP/1.1
	if(Rio_readlineb(&rio,buff,MAXLINE) < 0){
		logg("An error accord in handle_request\n");
		return -1;
	}
	else{
		//parse through the request
		if(get_request(buff,port,resource,method,host) < 0){
			logg("An error accord in parse_request\n");
			return -1;
		}
		//send off the request
		else if(strstr(method,"GET") != NULL){
			return send_request(connfd,buff,rio,port,resource,host,server_fd,cache_id,cache,cache_len);
		}
		else{
			logg("Only GET requests supported\n");
			return -4;
			//this is not a GET
		}
	}
	return 0;
}

/**
 * Handles the parsing of the first line of the request
 * @param buff The first line of the request
 * @param port Variable which is to hold the port number
 * @param resource Variable which is to hold the particular thing we want from the host
 * @param host Will hold the hostname
 * @return whether we were successful
 * */
int get_request(char *buff,char *port,char *resource,char *method,char *host){
	char protocol[MAXLINE];
	char version[MAXLINE];
	//if invalid request
	if(strlen(buff) < 1 || strstr(buff,"/") == NULL){
		return -1;
	}
	char my_url[MAXLINE];
	//EX GET http://www.cmu.edu/hub/index.html HTTP/1.1
	strcpy(resource,"/");
	//in order: GET,http://www.cmu.edu/hub/index.html,HTTP/1.1
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
		int size = sizeof(p);
		strncpy(p,ptr,size);
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
/**Handles the sending of the request
 * @param buff Holds the request
 * @param rio The rio_t struct which helps read each line
 * @param port Variable which is to hold the port numer
 * @param resource Variable which is to hold the particular thing we want from the host
 * @param host Will hold the hostname
 * @param server_fd holds the server file descriptor
 * @return whether we were successful or not
 * */
int send_request(int connfd,char *buff, rio_t rio, char *port,char *resource,char *host,int *server_fd,char *cache_id,void *cache,unsigned int *cache_len){

	int ret;
	char sendbuf[MAXLINE]; //holds what we are going to send to the server
	strcat(sendbuf,"GET ");
	strcat(sendbuf,resource);
	strcat(sendbuf," HTTP/1.0\r\n");
	while((ret = Rio_readlineb(&rio,buff,MAXLINE)) != 0){
		//while there is more to read (until \r\n) replace things as needed and add everything to sendbuf
		if(ret < 0){
			logg("Error reading request in send_request\n");
		}
		else if(strcmp(buff,"\r\n") == 0){
			strcat(sendbuf,buff);
			break;
		}
		if(strstr(buff,"Host:") != NULL){
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
	//Make the id for the cache
	strcpy(cache_id,"GET");
	strcat(cache_id," ");
	strcat(cache_id,host);
	strcat(cache_id,":");
	strcat(cache_id,port);
	strcat(cache_id," ");
	strcat(cache_id,resource);
	//see if we have already cached it
	if(read_from_cache(cache_id,cache,cache_len) != -1){
		return DO_CACHE;
	}
	else{
		//send off the request
		(*server_fd) = Open_clientfd(host,port);
		if(*(server_fd) == -1){
			//no connection
			return -1;
		}
		else{//if opening connection succeeded
			strcat(host,":");
			strcat(host,port);
			logg(host);
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
		//reset
		memset(sendbuf,0,strlen(sendbuf));
	}
	return 0;
}

/**
 * Handles the parsing/reading/sending off of the response from the server
 * @param serverfd The server file descriptor
 * @param clientfd The client file descriptor
 * @return whether we were successful
 * */
int handle_response(int *serverfd,int clientfd,char *cache_id,void *cache){
	rio_t rio;
	char buf[MAXLINE];
	int content_size;
	unsigned cache_len = 0;
	int objsize = 1;

	//init the struct
	Rio_readinitb(&rio,(*serverfd));
	//grab first line
	if(Rio_readlineb(&rio,buf,MAXLINE) < 0){
		return -1;
	}
	//send off first line
	/*if(Rio_writen(clientfd,buf,strlen(buf)) < 0){
		return -3;
	}*/
	else{
		if(cache_it(buf,clientfd,&objsize,cache,&cache_len,strlen(buf))< 0){
			return -1;
		}
		else{
			//while we don't hit the end of headers
			while(strcmp(buf,"\r\n") != 0 && strlen(buf) > 0){

				if(Rio_readlineb(&rio,buf,MAXLINE) < 0){
					return -1;
				}
				else{
					//grab the content size
					if(strstr(buf,"Content-Length")){
						sscanf(buf,"Content-Length: %d",&content_size);
					}
					//it sometimes has a lower case l
					else if(strstr(buf,"Content-length")){
						sscanf(buf,"Content-length: %d",&content_size);
					}
				}
				if(cache_it(buf,clientfd,&objsize,cache,&cache_len,strlen(buf)) < 0){
					return -1;
				}
				/*//write it all to client
				if(Rio_writen(clientfd,buf,strlen(buf)) < 0){
					return -3;
				}*/
			}
		}
		read_and_send_response(&rio,content_size,buf,clientfd,cache_id,cache,&objsize,&cache_len);
	}
	//now handle the body
	//return read_and_send_response(&rio,content_size,buf,clientfd);
	return 0;
}

/**
 * Handles the reading and sending of the response to client, as well as caching
 * @param rio The struct which helps us read line by line
 * @param content_size Holds the amount we need to read
 * @param buf The temp buffer we are putting the things we read in before it is sent
 * @param clientfd The client file descriptor
 * @return whether we were successful
 * */
int read_and_send_response(rio_t *rio, int content_size,char *buf,int clientfd,char *cache_id,void *cache,int *objsize,unsigned *cache_len){
	int num;
	if(content_size > 0){
		//if the size of the response is too big, send it in chunks
		while(content_size > MAXLINE){
			if((num = Rio_readnb(rio,buf,MAXLINE)) < 0){
				return -1;
			}
			else if(cache_it(buf,clientfd,objsize,cache,cache_len,num) < 0){
				return -1;
			}
			/*else if(Rio_writen(clientfd,buf,num) == -1){
				return -1;
			}*/
			else{
				content_size = content_size - MAXLINE;
			}
		}
		//now send the last bit of the response
		if(content_size > 0){
			if(Rio_readnb(rio,buf,content_size) < 0){
				return -1;
			}
			/*else if(Rio_writen(clientfd,buf,content_size) < 0){
				return -1;
			}*/
			else if(cache_it(buf,clientfd,objsize,cache,cache_len,num) < 0){
				return -1;
			}
		}
	}
	//no response
	else{
		//read anything that is there and send
		while((num = Rio_readlineb(rio,buf,MAXLINE))){
			/*if(Rio_writen(clientfd,buf,num) < 0){
				return -1;
			}*/
			if(cache_it(buf,clientfd,objsize,cache,cache_len,num) < 0){
				return -1;
			}
		}
	}
	if(objsize && (cache_URL(cache_id,cache,*cache_len))< 0){
		return -1;
	}
	return 0;
}

int cache_it(char *buf,int clientfd,int *objsize,void *cache,unsigned int *cache_len,unsigned int num){
	void *pos;
	if(*objsize){
		if(( (*cache_len)+strlen(buf) ) > MAX_OBJECT_SIZE){
			*objsize = 0;
		}
		else{
			pos = (void *)((char*)cache + *cache_len);
			memcpy(pos,buf,strlen(buf));
			*cache_len += strlen(buf);
			*objsize = 1;
		}
	}
	//send off
	if(Rio_writen(clientfd,buf,num) < 0){
		return -1;
	}
	return 0;
}
