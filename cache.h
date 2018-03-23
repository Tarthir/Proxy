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


typedef struct CachedItem CachedItem;

struct CachedItem {
  char* url;
  void *item_p;
  size_t size;
  CachedItem *prev;
  CachedItem *next;
};

typedef struct {
  size_t size;
  CachedItem* first;
  CachedItem* last;
  size_t max_size;
  sem_t read;
  sem_t write;
  size_t remaining_size;
} CacheList;


/**Our Cache List*/
CacheList *list;

extern void cache_init();
extern void cache_URL(char *URL, void *item, size_t size);
extern void evict();
extern CachedItem *find(char *URL);
//extern CachedItem get_cache(char *URL, CacheList *list);
extern void move_to_front(char *URL);
//extern void print_URLs(CacheList *list);
//extern void cache_destruct(CacheList *list);

#endif

