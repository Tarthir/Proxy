/*
 * eventnode.h
 *
 *  Created on: Apr 13, 2018
 *      Author: tdbrady
 */

#ifndef SRC_EVENTNODE_H_
#define SRC_EVENTNODE_H_

#include<stdlib.h>
#include<stdio.h>
#include "csapp.h"
#define MAX_OBJECT_SIZE 102400

typedef struct data_struct{
	char port[MAXLINE];
	char resource[MAXLINE];
	char host[MAXLINE];
	char cache[MAX_OBJECT_SIZE];
	unsigned int cache_len;
	char cache_id[MAXLINE];
	char method[MAXLINE];
	int content_size;
	int do_cache;
}data_struct;


typedef struct list_node{
	struct list_node *next;
	int connfd;
	int serverfd;
	int bytes_read;
	int bytes_sent;
	int state;
	char buffer[MAXLINE];
	char sendbuff[MAXLINE];
	//port,resource,host,cache_id,cache,cache_len
	void *data;


}list_node;

typedef struct List{
	list_node *first;
	size_t size;
}List;

struct List *my_list;

void init_my_event();


struct list_node *delete_event_node(struct list_node *node);
void free_event_node(struct list_node *node);
void free_all_event_nodes();
struct list_node *init_list_node(int connfd,int state);
struct list_node *getNode(int fd);
void add_event(int connfd,int state);

#endif /* SRC_EVENTNODE_H_ */
