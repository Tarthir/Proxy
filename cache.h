/*
 * cache.h
 *
 *  Created on: Mar 22, 2018
 *      Author: tdbrady
 */

#ifndef MARJ_CACHE_H
#define MARJ_CACHE_H

#include <stdlib.h>
#include <stdio.h>
#include "csapp.h"
/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400


//typedef struct CachedItem CachedItem;

typedef struct CachedItem {
  char* url;
  void *item_p;
  unsigned int size;
  struct CachedItem *next;
  struct CachedItem *prev;
} CachedItem;

typedef struct CacheList{
  CachedItem* first;
  CachedItem* last;
  sem_t read;
  sem_t write;
  unsigned int read_count;
  unsigned remaining_size;
} CacheList;


/**Our Cache List*/
CacheList *list;

extern void cache_init();
extern int cache_URL(char *res_id, void *item, unsigned int size);
extern void evict(unsigned int size);
CachedItem *get_node(void * cache,char *res_id);
extern int read_from_cache(char *res_id,void * cache,unsigned int *cache_len);
//extern CachedItem get_cache(char *URL, CacheList *list);
extern void move_to_front(char *URL);
void move_to_end(char *res_id);
CachedItem *delete_node(char *res_id);
//extern void print_URLs(CacheList *list);
//extern void cache_destruct(CacheList *list);

#endif

