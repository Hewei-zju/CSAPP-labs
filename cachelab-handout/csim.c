#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include "cachelab.h"

typedef struct Cacheline{
    uint64_t valid_bit;//有效位
    uint64_t set_index;
    uint64_t tag;
    size_t timestamp;
}Cacheline;
typedef struct Cache{
    size_t num_sets;
    size_t lines_per_set; 
    size_t num_lines;
    uint64_t s,E,b;
    Cacheline *cachelines;
}Cache;
void init_cache(int s,int E,int b);
int Operate(char op,uint64_t addr);

int load_cache(uint64_t addr);
size_t update_cache(uint64_t addr,uint64_t tag);
int update_timestamp(uint64_t set_index,size_t hit_i);
int hits,misses,evictions;
Cache cache;
int main(int argc, char *argv[])
{
    int opt;
    int s = -1;
    int E = -1;
    int b = -1;
    int verbose = 0;
    char *tracefile = NULL;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1) {
        switch (opt) {
            case 'h':
                return 0;
            case 'v':
                verbose = 1;
                break;
            case 's':
                s = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'b':
                b = atoi(optarg);
                break;
            case 't':
                tracefile = optarg;
                break;
            default:
                return 1;
        }
    }

    if (s < 0 || E <= 0 || b < 0 || tracefile == NULL){
        fprintf(stderr,"Invalid argument\n");
        return 1;
    }
    



    //cache stimulate
    init_cache(s,E,b);
    FILE *fp = fopen(tracefile,"r");
    char op;
    uint64_t addr;
    int size;
    while(fscanf(fp, " %c %" SCNx64 ",%d", &op, &addr, &size) == 3) {
        if (Operate(op,addr) != 0) {
            fprintf(stdout,"op failed\n");
            fclose(fp);
            return 0;
        }
    }
    if (verbose == 1) {
        fprintf(stdout,"verbose\n");
    }
    printSummary(hits,misses,evictions);
    free(cache.cachelines);
    fclose(fp);
    return 0;
}


void init_cache(int s,int E,int b) {
    cache.s = s;
    cache.E = E;
    cache.b = b;
    cache.num_sets = (size_t)1<<s;
    cache.lines_per_set = (size_t)E;
    cache.num_lines = cache.num_sets*cache.lines_per_set;
    cache.cachelines = malloc(sizeof(Cacheline)*cache.num_lines);
    size_t i;
    for (i = 0;i < cache.num_lines;i++) {
        cache.cachelines[i].valid_bit = 0;
        cache.cachelines[i].tag = 0;
        cache.cachelines[i].set_index = i/E;
        cache.cachelines[i].timestamp = 0;
    }
}

int Operate(char op,uint64_t addr) {
    switch (op) {
        case 'I':
            break;
        case 'L':
        case 'S':
            //store
            load_cache(addr);
            break;
        case 'M':
            //modify
            load_cache(addr);
            load_cache(addr);
            break;
        default:
            fprintf(stdout,"Invalid op\n");
            return 1;
    }
    return 0;
}

int load_cache(uint64_t addr) {
    uint64_t tag = addr >> (cache.s + cache.b);
    uint64_t set_index = (addr >> cache.b) & ((1ULL << cache.s) - 1);
    int hit = 0;
    size_t hit_i;
    size_t i;
    for (i = 0;i < cache.lines_per_set;i++) {
        if (cache.cachelines[i+cache.E*set_index].valid_bit == 1 && cache.cachelines[i+cache.E*set_index].tag == tag) {
            hit += 1;
            hit_i = i;
        }
    }
    if (hit == 1) {
        hits += 1;
        update_timestamp(set_index,hit_i);
    }else if (hit > 1) {
        fprintf(stdout,"Duplicate hit in one set\n");
        return 1;
    }else {
        misses += 1;
        hit_i = update_cache(set_index,tag);
        update_timestamp(set_index,hit_i);
    }
    return 0;
}

size_t update_cache(uint64_t set_index,uint64_t tag) {
    ssize_t invalid_i = -1;
    size_t LRU_i = 0;
    size_t LRU_timestamp = 0;
    size_t i;
    for (i = 0;i < cache.lines_per_set;i++) {
        if (cache.cachelines[i+cache.E*set_index].valid_bit == 0) {
            invalid_i = cache.E*set_index + i;
            break;
        }
        if (cache.cachelines[i+cache.E*set_index].timestamp > LRU_timestamp) {
            LRU_timestamp = cache.cachelines[i+cache.E*set_index].timestamp;
            LRU_i = i;
        }
    }
    if (invalid_i != -1) {
        //如果有空位
        cache.cachelines[invalid_i].tag = tag;
        cache.cachelines[invalid_i].valid_bit = 1;
        cache.cachelines[invalid_i].timestamp = SIZE_MAX;
        return invalid_i - cache.E*set_index;
    }else {
        //无空位，使用LRU算法淘汰,返回淘汰组内index
        cache.cachelines[cache.E*set_index+LRU_i].tag = tag;
        cache.cachelines[cache.E*set_index+LRU_i].timestamp = SIZE_MAX;
        evictions += 1;
        return LRU_i;
    }
}

int update_timestamp(uint64_t set_index,size_t hit_i) {
    size_t timeshr = cache.cachelines[cache.E*set_index+hit_i].timestamp;
    size_t i;
    for (i = cache.E*set_index;i < cache.E*set_index + cache.lines_per_set;i++) {
        if (i == cache.E*set_index + hit_i) {
            cache.cachelines[i].timestamp = 0;
        }else if (cache.cachelines[i].timestamp < timeshr) {
            cache.cachelines[i].timestamp += 1;
        }
    }
    return 0;
}