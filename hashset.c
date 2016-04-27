

#include "hashset.h"
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static unsigned int hash(unsigned char *str, int max_hash, int length){
        unsigned long hash = 5381;
        int i;

	for (i=0;i<length;i++){
            hash = ((hash << 5) + hash) + str[i]; /* hash * 33 + c */
	}
        return hash % max_hash;
}

hashset_t hashset_create(int table_size){
	hashset_t set = malloc(sizeof(struct hashset_st));
	if (set == NULL) {
		return NULL;
	}
	
	set->table = calloc(table_size, sizeof(void*));
	memset((void*)set->table,0,table_size*sizeof(void*));
	
	set->table_size=table_size;
	set->items_count = 0;
	
    return set;
}

int hashset_num_items(hashset_t set)
{
    return set->items_count;
}

inline static void free_member(struct member * mem){
	if (mem->next){
		free_member(mem->next);
	}
	free(mem);
	return;
}

void hashset_destroy(hashset_t set){
    int i;
    for (i=0;i<set->table_size;i++){
	if (set->table[i]){
		free_member(set->table[i]);
	}
    }
    free(set->table);
    free(set);
}


inline static int compare(char * one, char * two, int size){
	int i;
	for (i=0;i<size;i++){
		if (one[i]!=two[i]){
			return 0;
		}
	}
	return 1;
}

inline static void copy_item(char * from, char * to, int size){
	int i;
	for (i=0;i<size;i++){
		to[i]=from[i];
	}
}

void hashset_add(hashset_t set, void *itemx, int size, sem_t * sem){	
	void * item;
	struct member *mem;
	unsigned int h;
	item = calloc(size, sizeof(char));
	copy_item(itemx,item,size);
	h = hash(item, set->table_size,size);
	sem_wait(sem);
	if (set->table[h]){ /*zaznam uz existuje*/
		mem = set->table[h];
		do{
			if (mem->length==size && compare(mem->data,item,size)){	
				sem_post(sem);				
				return;
			}
		}while(mem->next && (mem=mem->next) );
		mem->next = malloc(sizeof(struct member));
		mem=mem->next;
		mem->next=0;
		mem->data=item;
		mem->length=size;
		set->items_count++;		
	}else{ /* zaznam neexistuje */
		set->table[h] = malloc(sizeof(struct member));
		set->table[h]->next=0;
		set->table[h]->data=item;
		set->table[h]->length=size;
		set->items_count++;

	}
	sem_post(sem);
	return;
}




