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
	list->read_count = 0;
	Sem_init(&list->read,0,1); //TODO more then one reader
	Sem_init(&list->write,0,1);
	list->remaining_size = MAX_CACHE_SIZE;
}
//called by cache_URL()
CachedItem *init_cache_item(char *URL,void *item,unsigned int size){
	CachedItem *it = (CachedItem*) Malloc(sizeof(CachedItem));			/*Malloc for struct*/
	it->url = (char *) Malloc(sizeof(char) * (strlen(URL)+1));			/*Malloc for url*/
	strcpy(it->url,URL);												/*Copy into struct*/
	it->item_p = Malloc(size);											/*Malloc item(response)*/
	it->size = size;
	it->next = NULL;
	it->prev = NULL;
	memcpy(it->item_p,item,size);										/*Copy data over to struct*/
	return it;
}
extern void add_item(CachedItem * item){
	if(list != NULL){
		//add to head
		if(list->first == NULL){
			list->first = item;
			list->last = item;
		}
		//add to tail
		else{
			list->last->next = item;
			item->prev = list->last;
			list->last = item;
		}
		//list->size += item.size;
		list->remaining_size -= item->size;
	}
}
//called by user
extern int cache_URL(char *res_id, void *item, unsigned int size){
	if(list != NULL && res_id != NULL && size > 0 && item != NULL){
		CachedItem *it = NULL;
		it = init_cache_item(res_id,item,size);
		P(&(list->write));
		while(list->remaining_size < it->size){
			//TODO evict until space!Will involve cleanup
			evict(size);
		}
		//add item to cache
		add_item(it);
		V(&(list->write));
	}else{
		return -1;
	}
	return 0;
}



extern void evict(unsigned int size){
	if(list != NULL){
		CachedItem *item = list->first;
		if(item != NULL){
			list->first = item->next;
			list->remaining_size += item->size;
			if(item == list->last){
				list->last = NULL;
			}
			Free(item->url);
			Free(item->item_p);
			Free(item);
		}
	}
}

extern int read_from_cache(char *res_id,void *cache,unsigned int *cache_len){
	if (list != NULL){
		//CachedItem *item = list->first;
		P(&(list->read));
		(list->read_count)++;
		if(list->read_count == 1){
			P(&(list->write));//lock
		}
		V(&(list->read));//unlock read
		CachedItem *item = get_node(cache,res_id);
		if(item != NULL){
			*cache_len = item->size;
			memcpy(cache,item->item_p,*cache_len);
			P(&(list->read));
			(list->read_count)--;
			if(list->read_count == 0){
				V(&(list->write));
			}
			V(&(list->read));
			return 0;
			//TODO move to end
			move_to_end(res_id);
		}
		else{
			//This data has never been cached
			P(&(list->read));
			(list->read_count)--; //decrement
			if(list->read_count == 0){
				V(&(list->write));
			}
			V(&(list->read));
			return -1; //Nothing was found
		}
		V(&(list->write));//release
	}
	return 0;

}

void move_to_end(char *res_id){

	P(&(list->write));
	//if not already at the end
	/*if(strcmp(list->last->item_p,res_id) != 0){
		CachedItem *item = list->first;
		while(strcmp(item->url,res_id) != 0){
			item = item->next;
		}
		item->
	}*/
	CachedItem *item = delete_node(res_id);
	add_item(item);
	V(&(list->write));
}

CachedItem *delete_node(char *res_id){
	if(list != NULL){
		CachedItem *curr = list->first;
		CachedItem *pre = NULL;

		while(curr != NULL){
			if(strcmp(res_id,curr->url) == 0){
				if(list->first == curr){
					list->first = curr->next;
				}
				if(list->last == curr){
					list->last = pre;
				}
				if(pre != NULL){
					pre->next = curr->next;
				}
				curr->next = NULL;

				list->remaining_size -= curr->size;

				return curr;
			}
			else{
				pre = curr;
				curr = curr->next;
			}
		}
	}
	return NULL;
}

CachedItem *get_node(void *cache,char *res_id){
	if(cache != NULL){
		CachedItem *curr = list->first;
		while(curr != NULL){
			if(strcmp(res_id,curr->url) == 0){
				return curr;
			}
			curr = curr->next;
		}
	}
	return NULL;
}

/*extern void move_to_end(){

}*/

//extern void print_URLs(CacheList *list){
//
//}
//
extern void cache_destruct(CacheList *list){
	//TODO clean up
}
