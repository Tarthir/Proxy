/*
 * logger.c
 *
 *  Created on: Mar 22, 2018
 *      Author: tdbrady
 */
#include "logger.h"


//struct LogItem {
//  char *url;
//  char *timestamp;
//  LogItem *prev;
//  LogItem *next;
//};
//
//typedef struct {
//  size_t size;
//  LogItem* first;
//  LogItem* last;
//  size_t max_size;
//  sem_t write;
//	sem_t slots;
//	sem_t items;
//}LogList;

void logger_init(){
	if(loglist == NULL){
		loglist = (LogList*) Malloc(sizeof(LogList));
		loglist->first = NULL;
		loglist->last = NULL;
		//loglist->max_size = 1024;
		Sem_init(&loglist->items,0,0);
		Sem_init(&loglist->write,0,1);
		Sem_init(&loglist->slots,0,NUM_OF_SLOTS);
		loglist->size = 0;
		pthread_t tid;//tid for threads
		Pthread_create(&tid,NULL,logger_routine,NULL);
	}
}

void log_item_init(LogItem **item,char *url){
	(*item) = (LogItem*) Malloc(sizeof(LogItem));
	(*item)->next = NULL;
	(*item)->prev = NULL;
	(*item)->url = (char *) Malloc(1024); //TODO memory leaks?
	strcpy(((*item)->url),url);
	//(*item)->url = url;
	time_t timer;
	struct tm* time_stamp;
	//Get current time
	time(&timer);
	time_stamp = localtime(&timer);
	strftime((*item)->timestamp,26,"%Y-%m-%dT%H:%M:%SZ",time_stamp);
}

void logger_deinit(){
	//clear logger
}

void *logger_routine(void *vargp){
	//run this routine forever
	Pthread_detach(pthread_self());
	//open file
	FILE *fd;
	if((fd = fopen("logger.txt","a+")) == NULL){
		fprintf(stderr,"Failure to open logging file");
		exit(-1);
	}
	while(1){
		char *item = logger_remove(); /* Remove message from buffer */
		if(item != NULL){
		//write item
			write_LogItem(item,fd);		 /* Write to file */
		}
	}
	fclose(fd);
	Free(loglist);
}

char *logger_remove(){
	if(loglist != NULL){
		P(&loglist->items);                          /* Wait for available item */
		P(&loglist->write);
		LogItem *item = (loglist->last);
		char *buff = NULL;
		buff = malloc(1024);
		if(item != NULL){
			strcpy(buff,item->url);
			strcat(buff,"\r\nTime: ");
			strcat(buff,item->timestamp);
			strcat(buff,"\r\n");
			//Free(((*item)->url));
			Free(item);
			if(item->prev != NULL){
				item->prev->next = NULL;
			}
			else if(loglist->first == loglist->last){ //If the same object
				loglist->first = NULL;
				loglist->last = NULL;
			}
			//Free((*item)->url);
			//Free(item);
			loglist->size -= 1;
			V(&loglist->write);                       /* Unlock the buffer */
			V(&loglist->slots);						  /* Announce available slot */
			return buff;
		}
		else{
			logg("Logging Error: No item available when there should be");
		}
	}
	 V(&loglist->write);                          	  /* Unlock the buffer */
	 V(&loglist->slots);						      /* Announce available slot */
	return NULL;
}


void logger_insert(LogItem *item){
	if(loglist != NULL && item != NULL){
		P(&loglist->slots);
		P(&loglist->write);							  /* Lock the loglist */
		if(loglist->first == NULL && loglist->last == NULL){
			loglist->first = item;
			loglist->last = item;
			loglist->size += 1;
		}
		else{
			loglist->last->next = item;
			item->prev = loglist->last;
		}
		V(&loglist->write);                          		/* Unlock the loglist */
		V(&loglist->items);							 		/* Announce available item */
	}
}

void write_LogItem(char *item,FILE *fd){
	//write item, then free it
	fprintf(fd,"%s",item);
	fflush(fd);
	Free(item);
}

void clear_logger(){

}

void logg(char *message){
	if(loglist != NULL){
		//LogItem *item;
		//log_item_init(&item,message);
		//insert item into logger which will alert log thread that something is there(like with sbuf)
		//logger will then grab it out of the queue and write it
		//logger_insert(item);
	}
	else{
		fprintf(stderr,"Logger is not initialized");
	}
}

