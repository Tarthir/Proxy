#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<stdio.h>
#include<sys/epoll.h>
#include<sys/socket.h>
#include<string.h>
#include "csapp.h"
#include "logger.h"
#include "cache.h"
#include "eventnode.h"

//#define NUM_OF_THREADS 3
#define DO_CACHE 10
#define SBUFFSIZE 3
#define	MAXLINE	 8192  /* Max text line length */
#define MAXBUF   8192  /* Max I/O buffer size */
#define MAXEVENTS 64
#define READ_REQUEST 10
#define SEND_REQUEST 11
#define READ_RESPONSE 12
#define SEND_RESPONSE 13
#define NO_MORE_IO -10

typedef struct sockaddr SA;



//sbuf_t sbuf;//The buffer for our connections

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
int handle_server_request(struct list_node *node);
int get_request(char *buff,char *port,char *resource,char *method,char *host);
int read_request(struct list_node *node);
void Cclose(int serverfd, int clientfd);
int read_response(struct list_node *node);
int do_server_request(struct list_node *node);
int send_cached_items(int connfd,char *cache,unsigned int cache_len);
int cache_it(int *objsize,char *buf,char *cache,unsigned int *cache_len);
void add_event(int connfd,int state);
struct list_node *getNode(int idx);
void create_event(int efd,int fd,struct epoll_event *event,int macro,int to_modify);
int send_data(int fd, char *buffer,int *bytes_sent);



/*
 * If client ready to be read from
 *States:10,11,12,13
 *READ_REQUEST: reading from client, until the entire request has been read from the client.
  SEND_REQUEST: writing to server, until the entire request has been sent to the server.
  READ_RESPONSE: reading from server, until the entire response has been received.
  SEND_RESPONSE: writing to client, until the entire response has been sent to the client.

  Will use a struct to hold current state, how much has been read/written,how much needs to be read/written, the buffer we are using,connfd and serverfd.
  These structs will be in a linked list structure. Their key will be their fd's they are reading/writing from/to.
 * */

//TODO maybe have void * ptr in event.data thing point to my struct

int main(int argc, char **argv)
{
	int listenfd, connfd,efd,i = 0;
	size_t n = 0;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	struct epoll_event event;
	struct epoll_event *events;
	init_my_event();

	//ignore SIGPIPE
	Signal(SIGPIPE,SIG_IGN);

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
		if((listenfd < 0)){
			//error listening to port
			fprintf(stderr, "port being used\n");
			exit(1);
		}
		cache_init();
		//set fd to non-blocking
		if(fcntl(listenfd,F_SETFL,fcntl(listenfd,F_GETFL,0) | O_NONBLOCK) < 0 ){
			fprintf(stderr,"error setting socket option\n");
			exit(1);
		}
		if((efd = epoll_create1(0)) < 0){
			fprintf(stderr,"error creating epoll\n");
			exit(1);
		}
		event.data.fd = listenfd;
		event.events = EPOLLIN | EPOLLET;
		if(epoll_ctl(efd,EPOLL_CTL_ADD,listenfd,&event) < 0){
			fprintf(stderr,"error adding event\n");
			exit(1);
		}
		events = calloc(MAXEVENTS,sizeof(event));


		//Then as we get connections hook them up with the threads
		//have threads go to a function that detaches them and handles requests
		while (1) {
			n = epoll_wait(efd,events,MAXEVENTS,1000);
			fprintf(stdout,"n is: %lu\n",n);
			//if(n == 0){continue;}
			for(i = 0; i < n; i++){
				if((events[i].events & EPOLLERR) ||(events[i].events & EPOLLHUP)||(events[i].events & EPOLLRDHUP)){
					fprintf(stderr,"epoll error on fd %d\n",events[i].data.fd);
					close(events[i].data.fd);
					continue;
				}
			}
			if (listenfd == events[i].data.fd) { //line:conc:select:listenfdready
				clientlen = sizeof(struct sockaddr_storage);

				// loop and get all the connections that are available
				while ((connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen)) > 0) {

					create_event(efd,connfd, &event,EPOLLIN,0);
					add_event(event.data.fd,READ_REQUEST);
				}

				if (errno == EWOULDBLOCK || errno == EAGAIN) {
					// no more clients to accept()
				} else {
					perror("error accepting\n");
				}
			} else if (my_list->size > 0 && n > 0){ //line:conc:select:listenfdready
				int ret;
				list_node *node = getNode(event.data.fd);
				if(node->state == READ_REQUEST){

					if((ret = do_server_request(node)) >= 0){
						// TODO if there is stuff in the cache, you need to SEND_RESPONSE

						if(node->state == SEND_RESPONSE){
							create_event(efd,node->connfd, &event,EPOLLOUT,1);
							node->bytes_read = 0;
						}
						else{
							node->state = SEND_REQUEST;
							create_event(efd,node->serverfd, &event,EPOLLOUT,0);
							node->bytes_read = 0;
						}
					}
					else{
						fprintf(stdout,"An Error occurred while reading from client\n");
					}
				}
				else if(node->state == SEND_REQUEST){
					//fprintf(stdout,"sending to server: %s",node->sendbuff);
					ret = send_data(node->serverfd,node->sendbuff,&node->bytes_sent);
					//fprintf(stdout,"sent to server: %d",node->bytes_sent);
					if(ret >= 0){
						create_event(efd,node->serverfd,&event,EPOLLIN,1);
						node->state = READ_RESPONSE;
						memset(node->buffer,0,MAXLINE);
						memset(node->sendbuff,0,MAXLINE);
						node->bytes_sent = 0;
					}
					else{
						fprintf(stdout,"An Error occurred while writing to server\n");
					}
				}
				else if(node->state == READ_RESPONSE){
					ret = read_response(node);
					if(ret >= 0){
						create_event(efd,node->connfd,&event,EPOLLOUT,1);
						node->state = SEND_RESPONSE;
					}
					else{
						fprintf(stdout,"An Error occurred while reading response from server\n");
					}
				}
				else if(node->state == SEND_RESPONSE){
					data_struct *data = (data_struct*) node->data;
					//send to cache if we found something in the cache
					if(data->do_cache){
						ret = send_data(node->connfd,data->cache,&node->bytes_sent);
					}
					else{
						ret = send_data(node->connfd,node->buffer,&node->bytes_sent);
					}
					fprintf(stdout,"sent to client: %d",node->bytes_sent);
					if(ret >= 0){
						//read everything
						Cclose(node->serverfd,node->connfd);
						struct list_node *n = delete_event_node(node);
						free_event_node(n);
					}
					else{
						fprintf(stdout,"An Error occurred while sending server response to client\n");
					}
				}
			}

		}
	}
	free(events);
	//TODO free mylist and its list members
	//Cclose(request.server_fd,connfd,hostname,port);
	return 0;
}



void create_event(int efd,int fd,struct epoll_event *event,int macro,int do_modify){
	// set fd to non-blocking (set flags while keeping existing flags)

	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
		fprintf(stderr, "error setting socket option\n");
		//exit(1);
	}

	// add event to epoll file descriptor
	(*event).data.fd = fd;
	(*event).events = macro | EPOLLET; // use edge-triggered monitoring
	if(!do_modify){
		if (epoll_ctl(efd, EPOLL_CTL_ADD, fd, event) < 0) {
			fprintf(stderr, "error adding event\n");
			//exit(1);
		}
	}
	else{
		if (epoll_ctl(efd, EPOLL_CTL_MOD, fd, event) < 0) {
			fprintf(stderr, "error modifying event\n");
			//exit(1);
		}
	}
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

/**This function controls the logic which handles the reading of the request we get from the client
 * as well as sending that request of by calling two helper methods
 * @param connfd The client connection file descriptor
 * */
int do_server_request(struct list_node *node){
	int val;
	int connfd = node->connfd;

	//handles parsing/reading/sending of client request to server
	if((val = handle_server_request(node)) < 0){
		if(val == -4){
			//logg("Only Get request supported\n");

		}
		else if(val == NO_MORE_IO){
			printf("No more IO on connfd: %d\n",connfd);
			return val;
		}
		Cclose(node->serverfd,connfd);
		return val;
	}
	//read items from cache and send
	else if(val == DO_CACHE){
		//TODO HANDLE sending of caching
		node->state = SEND_RESPONSE; // cache
		struct data_struct *data = node->data;
		data->do_cache = 1;
		//		if(send_cached_items(connfd,cache,cache_len) < 0){
		//			//logg("Error sending cache to client");
		//			Cclose(node->serverfd,connfd);
		//			return;
		//		}
	}
	return 0;
}
/**Sends cached item to client*/
int send_cached_items(int connfd,char *cache,unsigned int cache_len){
	//cache = (char *) cache;
	if(Rio_writen(connfd,cache,cache_len) < 0){
		return -1;
	}
	else{
		return 0;
	}
}

/**
 * Parses through GET requests and prepares them for sendoff
 * @oaram connfd The client file descriptor
 * @param serverfd The server file descriptor
 * @return whether we were succesful or not
 * */
int handle_server_request(struct list_node *node){
	int len = 0;
	int connfd = node->connfd;
	data_struct *data = (data_struct*)node->data;
	memset(data->cache,0,MAX_OBJECT_SIZE);

	//grab the first line of the get request
	//EX: GET http://www.cmu.edu/hub/index.html HTTP/1.1
	while((len = recv(connfd,node->buffer,MAXLINE,MSG_DONTWAIT)) != 0){
		if(len < 0){
			//if there is nothing more to read
			if((errno == EAGAIN || errno == EWOULDBLOCK || EPIPE)){
				//you are done reading for now
				if(get_request(node->buffer,data->port,data->resource,data->method,data->host) < 0){
					//logg("An error accord in parse_request\n");
					return -1;
				}
				//read the rest of the request
				else if(strstr(data->method,"GET") != NULL){
					return read_request(node);
				}
				else{
					//logg("Only GET requests supported\n");
					return -4;
					//this is not a GET
				}
			}
			//logg("An error accord in handle_request\n");
			return len;
		}
		else{
			//add to what has been already read
			node->bytes_read += len;
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
int read_request(struct list_node *node){
	data_struct *data = (data_struct*)node->data;
	char *sendbuf = node->sendbuff; //holds what we are going to send to the server'
	char *buff = node->buffer;
	//fprintf(stdout,buff);
	memset(sendbuf,0,strlen(sendbuf)); //reset
	strcat(sendbuf,"GET ");
	strcat(sendbuf,data->resource);
	strcat(sendbuf," HTTP/1.0\r\n");
	int connfd = node->connfd;
	int *server_fd = &node->serverfd;

	char *tok = strtok(buff,"\r\n");
	tok = strtok(NULL,"\r\n");
	while(tok != NULL){
		//Get all the data
		if(strstr(tok,"Host:") != NULL){
			strcat(sendbuf,tok);
		}
		else if(strstr(tok,"Proxy-Connection:")){
			strcat(sendbuf,"Proxy-Connection: close");
		}
		else if(strstr(tok,"Connection:")!= NULL){
			strcat(sendbuf,"Connection: close");
		}
		else if(strstr(tok,"User-Agent") != NULL){
			strcat(sendbuf,user_agent_hdr);
		}
		else{//anything else
			strcat(sendbuf,tok);
		}
		strcat(sendbuf,"\r\n");

		tok = strtok(NULL,"\r\n");
	}
	strcat(sendbuf,"\r\n");

	//fprintf(stdout,sendbuf);
	//Make the id for the cache
	strcpy(data->cache_id,"GET");
	strcat(data->cache_id," ");
	strcat(data->cache_id,data->host);
	strcat(data->cache_id,":");
	strcat(data->cache_id,data->port);
	strcat(data->cache_id," ");
	strcat(data->cache_id,data->resource);
	//see if we have already cached it
	if(read_from_cache(data->cache_id,data->cache,&data->cache_len) != -1){
		return DO_CACHE;
	}
	else{
		//open up connection
		*(server_fd) = Open_clientfd(data->host,data->port);
		if(*(server_fd) == -1){
			//no connection
			return -1;
		}
		else{//if opening connection succeeded
			if(strlen(data->port) > 0){
				strcat(data->host,":");
				strcat(data->host,data->port);
				logg(data->host);
			}
			node->serverfd = *(server_fd);
		}
		if(*(server_fd) < 0){
			//tell client you gave me bad stuff
			strcpy(buff,"BAD REQUEST\n");
			//TODO need to fix this
			send(connfd,buff,strlen(buff),0);
			return -2;
		}
	}
	return 0;
}



/**
 * Handles the parsing/reading/sending off of the response from the server
 * @param serverfd The server file descriptor
 * @param clientfd The client file descriptor
 * @return whether we were successful
 * */
int read_response(struct list_node *node){
	char *buf = node->buffer;
	int objsize = 1;
	int len = 0;
	data_struct *data = (data_struct*)node->data;
	//TODO need to read the headers first? Then read again i guess
	while((len = recv(node->serverfd,buf,MAXLINE,MSG_DONTWAIT)) != 0){
		fprintf(stdout,"reading %d bytes\n",len);
		if(len < 0){
			if((errno == EAGAIN || errno == EWOULDBLOCK || EPIPE)){
				fprintf(stdout,"got errno\n");
				return 0; //TODO may not want to do this
			}
			fprintf(stdout,"no errno\n");
			return len;
		}
		if(cache_it(&objsize,buf,data->cache,&data->cache_len) < 0){
			return -1;
		}
	}
	if(objsize){
		cache_URL(data->cache_id,data->cache,data->cache_len);
	}
	fprintf(stdout,"exited normal: read_response\n");
	return 0;
}


int send_data(int fd, char *buffer,int *bytes_sent){
	int len = 0;
	//size_t t = strlen(node->sendbuff);
	while((len = send(fd,buffer,strlen(buffer),MSG_DONTWAIT)) != 0){
		fprintf(stdout,"Sent %d bytes\n",len);
		if(len < 0){
			//if there is nothing more to read for now
			if((errno == EAGAIN || errno == EWOULDBLOCK || EPIPE)){
				return 0;
			}
			//logg("An error accord in handle_request\n");
			//fprintf(stdout,"An error accord in handle_request\n");
			return len;
		}
		else{
			//add to what has been already read
			(*bytes_sent) += len;
			if(len == strlen(buffer)){
				return 0;
			}
		}
	}
	return 0;
}

// Puts buf into the cache
int cache_it(int *objsize,char *buf,char *cache,unsigned int *cache_len){

	if(*objsize){
		void *pos;
		if(( (*cache_len)+strlen(buf) ) > MAX_OBJECT_SIZE){
			*objsize = 0;
		}
		pos = (void *)((char*)cache + *cache_len);
		memcpy(pos,buf,strlen(buf));
		*cache_len += strlen(buf);
	}
	return 0;
}
