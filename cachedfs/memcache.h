#ifndef MEMCACHE_H
#define MEMCACHE_H

#define SPACE_FOR_STRUCTS 200 // if size of structs increases too much, 200 will not be enought
#define MAX_BUCKET_SIZE 1024
#define FAST_MODE 0 // if FAST_MODE is 1, FUSE writing method doesn't wait for 'STORED' messages, hence errors can be caused 

struct bucket_struct {
	int cur_size;

};

/**
 * 
 */
int cache_get(const char * key, char * buffer, int size);
int cache_msg(char * data, char * buffer, int size);
void cache_close_sock();
int cache_set(const char * key, char * val, int val_length);
void cache_init();
#endif //MEMCACHE_H