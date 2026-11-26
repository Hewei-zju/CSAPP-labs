/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
typedef struct cache_t{
    char *url;
    char *data;
    size_t size;
    size_t timestamp;
    struct cache_t *next;
}cache_t;

void init_cache();
int add_cache(cache_t *new_cache_block);
int delete_LRU_cache();
cache_t* find_cache(char *url);
int update_timestamp(cache_t *cache_block);
ssize_t hit_cache(char *bf,char *url);
int save_cache(char *url,char *cache_buf,size_t cache_size);