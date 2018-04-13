/*
 * eventnode.c
 *
 *  Created on: Apr 13, 2018
 *      Author: tdbrady
 */
#include "eventnode.h"

struct list_node *init_list_node(int connfd,int state){
	list_node *node = (list_node*) malloc(sizeof(list_node));
	node->bytes_read = 0;
	node->bytes_sent = 0;
	node->state = state;
	node->connfd = connfd;
	node->serverfd = -1;
	my_list->size +=1;
	node->next = NULL;
	node->data = (data_struct*)malloc(sizeof(data_struct));
	data_struct *data = (data_struct*) node->data;
	data->do_cache = 0;
	memset(node->buffer,0,strlen(node->buffer));
	return node;
}

void init_my_event(){
	my_list = (List*)malloc(sizeof(List));
	my_list->first = NULL;
	my_list->size = 0;
}
void free_all_event_nodes(){
	struct list_node *n = my_list->first;
	while(my_list->size > 0){
		delete_event_node(n);
		free_event_node(n);
	}
}
void free_event_node(struct list_node *node){
	free(node->data);
	free(node);
}

struct list_node *delete_event_node(struct list_node *node){
	struct list_node *n = my_list->first;
	if(n == node){
		my_list->first = node->next;
		my_list->size--;
		return node;
	}
	while(n != NULL){
		if( n->next == node ){
			n->next = n->next->next;
			my_list->size--;
			break;
		}
		n = n->next;
	}
	return node;
}


struct list_node *getNode(int fd){
	list_node *n = my_list->first;
	if(n != NULL){
		int cnt = 0;
		while(n != NULL){
			if(n->connfd == fd || n->serverfd == fd){
				return n;
			}
			else{
				n = n->next;
			}
			cnt++;
		}
	}
	return NULL;
}

/**
 * Handles the adding of nodes to our list of connections
 * */
void add_event(int connfd,int state){
	//if its the first/only connection
	if(my_list->first == NULL){
		my_list->first = init_list_node(connfd,state);
		return;
	}
	//if it not the only connection we are holding
	list_node *n = my_list->first;
	while(n->next != NULL){
		if(n->next == NULL){
			n->next = init_list_node(connfd,state);
			break;
		}
		else{
			n = n->next;
		}
	}
}

