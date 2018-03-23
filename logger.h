
#ifndef SRC_LOGGER_H_
#define SRC_LOGGER_H_
#include "csapp.h"
/*
 * logger.c
 *
 *  Created on: Mar 22, 2018
 *      Author: tdbrady
 */

#include "logger.h"

typedef struct LogItem LogItem;

#define NUM_OF_SLOTS 15

struct LogItem {
  char *url;
  char *timestamp;
  LogItem *prev;
  LogItem *next;
};

typedef struct {
  size_t size;
  LogItem* first;
  LogItem* last;
  //size_t max_size;
  sem_t write;
  sem_t slots;       /* Counts available slots */
  sem_t items;       /* Counts available items */
}LogList;

LogList *loglist;

///* $begin logger_t */
//typedef struct {
//    char *buf;          /* Buffer array */
//    int n;             /* Maximum number of slots */
//    int front;         /* buf[(front+1)%n] is first item */
//    int rear;          /* buf[rear%n] is last item */
//    sem_t write;       /* Protects accesses to buf */
//    sem_t slots;       /* Counts available slots */
//    sem_t items;       /* Counts available items */
//} logger_t;
///* $end logger_t */

void logger_init();
void log_item_init(LogItem **item,char *url);
void *logger_routine(void *vargp);
void logger_deinit();
void logger_insert(LogItem *item);
char *logger_remove();
void write_LogItem(char *item,FILE *fd);
void logg(char * message);


#endif /* SRC_LOGGER_H_ */
