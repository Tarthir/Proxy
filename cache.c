/*
 * cache.c
 *
 *  Created on: Mar 22, 2018
 *      Author: tdbrady
 */
#include "cache.h"
//struct CachedItem {
//  char* url;
//  void *item_p;
//  size_t size;
//  CachedItem *prev;
//  CachedItem *next;
//};
//
//typedef struct {
//  size_t size;
//  CachedItem* first;
//  CachedItem* last;
//  size_t max_size;
//  sem_t read;
//  sem_t write;
//  size_t remaining_size;
//} CacheList;

extern void cache_init(){
	list = (CacheList*) malloc(sizeof(CacheList));
	list->first = NULL;
	list->last = NULL;
	list->size = 0;
	Sem_init(&list->read,0,1);
	Sem_init(&list->write,0,1);
	list->max_size = MAX_CACHE_SIZE;
	list->remaining_size = 0;
}

CachedItem *init_cache_item(char *URL,void *item,size_t size){
	CachedItem *it = (CachedItem*) Malloc(sizeof(CachedItem));
	it->url = URL;
	it->item_p = item;
	it->size = size;
	return it;
}
void add_item(CachedItem * item){
	//add to head
	if(list->first == NULL){
		list->first = item;
		list->last = item;
	}
	//add to tail
	else{
		list->last->next = item;
		item->prev = list->last;
	}
	//list->size += item.size;
	list->remaining_size -= item->size;
}

extern void cache_URL(char *URL, void *item, size_t size){
	if(list != NULL && URL != NULL && size > 0 && item != NULL){
		CachedItem *item = NULL;
		item = init_cache_item(URL,item,size);
		P(&(list->write));
		while(list->remaining_size < item->size){
			//evict until space? or clear out?
		}
		//add item to cache
		add_item(item);
		V(&(list->write));
	}
}



extern void evict(){

}

extern CachedItem *find(char *URL){
	if (list != NULL){
		CachedItem *item = list->first;
		P(&(list->write));//lock
		while(item != NULL){
			if (strcmp(URL,item->url) == 0){
				return item;
			}
			item = item->next;
		}
		V(&(list->write));//release
	}
	return NULL;

}


extern void move_to_front(char *URL){

}

//extern void print_URLs(CacheList *list){
//
//}
//
//extern void cache_destruct(CacheList *list){
//
//}
