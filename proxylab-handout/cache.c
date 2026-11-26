#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include "cache.h"
static cache_t cache_obj;
static cache_t *cache = &cache_obj;
static pthread_rwlock_t cache_lock;
static pthread_mutex_t timestamp_lock;
void init_cache() {
    cache->url = NULL;
    cache->data = NULL;
    cache->size = 0;
    cache->timestamp = 0;
    cache->next = NULL;
    pthread_rwlock_init(&cache_lock,NULL);
    pthread_mutex_init(&timestamp_lock,NULL);
}

int add_cache(cache_t *new_cache_block) {
    if (cache->size + new_cache_block->size > MAX_CACHE_SIZE) {
        return 0;
    }
    cache->size += new_cache_block->size;
    cache_t *next = cache->next;
    cache->next = new_cache_block;
    new_cache_block->next = next;
    return 1;
}
ssize_t hit_cache(char *bf,char *url) {
    pthread_rwlock_rdlock(&cache_lock);
    cache_t *cache_block = NULL;
    ssize_t size;
    if ((cache_block = find_cache(url)) != NULL) {
        pthread_mutex_lock(&timestamp_lock);
        update_timestamp(cache_block);
        pthread_mutex_unlock(&timestamp_lock);
        memcpy(bf,cache_block->data,cache_block->size);
        size = cache_block->size;
        pthread_rwlock_unlock(&cache_lock);
        return size;
    }
    pthread_rwlock_unlock(&cache_lock);
    return -1;
}
int update_timestamp(cache_t *cache_block) {
    cache_t *cur = cache;
    size_t timeshr = cache_block->timestamp;
    while(cur->next != NULL) {
        if (cur->next == cache_block) {
            cur->next->timestamp = 0;
        }else if (cur->next->timestamp < timeshr) {
            cur->next->timestamp += 1;
        }
        cur = cur->next;
    }
    return 1;
}
int save_cache(char *url,char *cache_buf,size_t cache_size) {
    //create new cache block
    cache_t *new_cache_block = malloc(sizeof(cache_t));
    new_cache_block->url = malloc(strlen(url)+1);
    new_cache_block->data = malloc(cache_size);
    strcpy(new_cache_block->url,url);
    memcpy(new_cache_block->data,cache_buf,cache_size);
    new_cache_block->size = cache_size;
    new_cache_block->timestamp = SIZE_MAX;
    new_cache_block->next = NULL;
    
    pthread_rwlock_wrlock(&cache_lock);
    if (find_cache(url)) {
        goto cleanup;
    }
    while(add_cache(new_cache_block) == 0) {
        if (delete_LRU_cache() == 0) {
            goto cleanup;
        }
    }
    pthread_mutex_lock(&timestamp_lock);
    int n =  update_timestamp(new_cache_block);
    pthread_mutex_unlock(&timestamp_lock);

    pthread_rwlock_unlock(&cache_lock);
    return n;
cleanup:
    free(new_cache_block->url);
    free(new_cache_block->data);
    free(new_cache_block);
    pthread_rwlock_unlock(&cache_lock);
    return 0;
}

cache_t* find_cache(char *url) {
    cache_t *cur = cache;
    while(cur->next != NULL) {
        if (strcmp(cur->next->url,url) == 0) {
            break;
        }
        cur = cur->next;
    }
    cur = cur->next;
    return cur;
}

int delete_LRU_cache() {
    cache_t *cur = cache;
    size_t max_time = 0;
    cache_t *max_cache = NULL;
    cache_t *max_pre = NULL;
    while(cur->next != NULL) {
        if (cur->next->timestamp >= max_time) {
            max_time = cur->next->timestamp;
            max_cache = cur->next;
            max_pre = cur;
        }
        cur = cur->next;
    }
    if (max_cache != NULL && max_pre != NULL) {
        max_pre->next = max_cache->next;
        cache->size -= max_cache->size;
        free(max_cache->url);
        free(max_cache->data);
        free(max_cache);
        return 1;
    }else {
        fprintf(stderr,"delete_LRU_cache:Error! No space for any single cache block\n");
        return 0;
    }
}