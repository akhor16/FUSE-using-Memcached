/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * minimal example filesystem using high-level API
 *
 * Compile with:
 *
 *     gcc -Wall hello.c `pkg-config fuse3 --cflags --libs` -o hello
 *
 * ## Source code ##
 * \include hello.c
 */


#define FUSE_USE_VERSION 31

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include "memcache.h"


#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#define MAX_MEMORY_SIZE 2147483648

struct header_struct {
	int is_dir;
	int data_len;
	int num_buckets;
	int parent_index;
	int xattr_num;
	
};

struct stats_struct {
	int total_bytes;
	int block_size;

};

/*
 * Command line options
 *
 * We can't set default values for the char* fields here because
 * fuse_opt_parse would attempt to free() them when the user specifies
 * different values on the command line.
 */
static struct options {
	const char *filename;
	const char *contents;
	int show_help;
} options;

#define OPTION(t, p)                           \
    { t, offsetof(struct options, p), 1 }
static const struct fuse_opt option_spec[] = {
	OPTION("--name=%s", filename),
	OPTION("--contents=%s", contents),
	OPTION("-h", show_help),
	OPTION("--help", show_help),
	FUSE_OPT_END
};

static int hello_getattr(const char *path, struct stat *stbuf,
			 struct fuse_file_info *fi)
{
	printf("in getattr\n");
	(void) fi;
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	char * mabuf = malloc(1280);
	if(cache_get(path, mabuf, 1280) == -1){
		res = -ENOENT;
	}
	struct header_struct * ma_header = (struct header_struct *)(mabuf + sizeof(struct bucket_struct));
	if (ma_header->is_dir) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		stbuf->st_size = ma_header->data_len;
	}
	free(mabuf);
	return res;
}

static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi,
			 enum fuse_readdir_flags flags)
{
	printf("in readdir\n");
	(void) offset;
	(void) fi;
	(void) flags;

	char * buffer = malloc(1280);
	if (cache_get(path, buffer, 1280) == -1){
		return -ENOENT;
	}

	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);

	int cur = ((struct header_struct *)(buffer + sizeof(struct bucket_struct)))->num_buckets;
	printf("current num in readdir: %d\n", cur);
	for(int i = 1; i <= cur; i++){
		char curKey[250];
		sprintf(curKey, "%d", i);
		strcat(curKey, path);
		free(buffer);
		buffer = malloc(1280);
		printf("curKey in readdir: %s\n", curKey);
		cache_get(curKey, buffer, 1280);
		printf("Child got in readdir: %s\n", buffer);
		////// PARSING TO TAKE THE LAST TOKEN
		int startIndex = ((struct bucket_struct *)buffer)->cur_size - 1;
		int lastIndex = startIndex;
		while(buffer[startIndex] != '/'){
			startIndex--;
		}
		startIndex++;
		printf("STARTTIIIIIIIIIIIIIIIIING INDEXXX: %d\n", startIndex);
		printf("filler cur: %s\n", buffer + startIndex);
		char fml[264];
		strncpy(fml, buffer + startIndex, lastIndex - startIndex + 1);
		fml[lastIndex-startIndex+1] = '\0';
		filler(buf, fml, NULL, 0, 0);
	}
	free(buffer);
	

	// filler(buf, options.filename, NULL, 0, 0);

	return 0;
}

static int hello_open(const char *path, struct fuse_file_info *fi)
{
	 return 0;
}

// path = full path of the directory where num should be incremented
// returns incremented value, or -1 on error
int increaseChildrensNum(const char* path){
	printf("in increaseChildrensNum\n");
	char * buffer = malloc(1280);
	if(cache_get(path, buffer, 1280) != -1){
		struct header_struct * cur_struct = (struct header_struct *)(buffer + sizeof(struct bucket_struct));
		int cur = ++(cur_struct->num_buckets);
		printf("current num in parent: %d\n", cur);
		cache_set(path, buffer, sizeof(struct bucket_struct) + sizeof(struct header_struct));
		free(buffer);
		return cur;
	}
	free(buffer);
	return -1;
}

int increaseXattrNum(const char* path){
	char * buffer = malloc(1280);
	if(cache_get(path, buffer, 1280) != -1){
		struct header_struct * cur_struct = (struct header_struct *)(buffer + sizeof(struct bucket_struct));
		int cur = ++(cur_struct->xattr_num);
		cache_set(path, buffer, sizeof(struct bucket_struct) + sizeof(struct header_struct));
		free(buffer);
		return cur;
	}
	free(buffer);
	return -1;
}

// path == path of child directory
int addToParent(const char* path){
	printf("IN ADDTOPARENT\n");
	if(strcmp(path, "/") == 0){
		return 0;
	}
	char parent[250];
	strcpy(parent, path);
	int len = strlen(parent);
	len--;
	while(parent[len] != '/'){
		parent[len] = '\0';
		len--;
	}
	if(len != 0){
		parent[len] = '\0';
		len--; // AQ VIYAVI BOLOS
	}
	int curNum = increaseChildrensNum(parent);
	char newKey[264];
	sprintf(newKey, "%d", curNum);
	strcat(newKey, parent);
	char * buff = malloc(332);
	int cur_buf_real_len = sizeof(struct bucket_struct) + strlen(path);
	printf("pathlen: %d, path: %s\n", (int)strlen(path), path);
	((struct bucket_struct *)buff)->cur_size = cur_buf_real_len;
	strcpy(buff + sizeof(struct bucket_struct), path);
	cache_set(newKey, buff, cur_buf_real_len);
	printf("IN ADDDDDD TOOOOO PARENENNTTTT: %d %s\n",cur_buf_real_len, buff);
	free(buff);
	return curNum;
}

static int hello_mkdir(const char* path, mode_t mode) {
	printf("IN MKDIR\n");
	printf("mkdir path: %s\n", path);
	int parentIndex = addToParent(path);
	if(parentIndex >= 0){
		int cur_bucket_size = sizeof(struct bucket_struct) + sizeof(struct header_struct);
		char * buf = malloc(cur_bucket_size);
		((struct bucket_struct *)buf)->cur_size = cur_bucket_size;
		struct header_struct * cur_buf = (struct header_struct *)(buf + sizeof(struct bucket_struct));
		cur_buf->is_dir = 1;
		cur_buf->num_buckets = 0;
		cur_buf->data_len = 0;
		cur_buf->parent_index = parentIndex;
		cache_set(path, buf, cur_bucket_size);
		free(buf);
		return 0;
	}
	return -EFAULT;
}

int init_check(){
	char curBuf[128];
	if(cache_get("stats", curBuf, 128) == -1){
		printf("CAN NOT USE OLDS\n");
		return 0;
	}
	if(((struct stats_struct *)(curBuf + sizeof(struct bucket_struct)))->block_size != MAX_BUCKET_SIZE){
		printf("CAN NOT USE OLDS\n");
		return 0;
	}
	printf("CAN USE OLDS\n");
	return 1;
}

static void *hello_init(struct fuse_conn_info *conn,
			struct fuse_config *cfg)
{
	printf("initializing\n");
	cache_init();
	if(!init_check()){
		char * flush_all_msg = "flush_all\n";
		char * buffer = malloc(32);
		cache_msg(flush_all_msg, buffer, 32);
		printf("flush_all messaged\n");
		if(strcmp(buffer, "OK")){
			printf("flushed all\n");
		}else printf("not flushed :(\n");
		free(buffer);
		int cur_bucket_len = sizeof(struct bucket_struct) + sizeof(struct stats_struct);
		buffer = malloc(cur_bucket_len);
		((struct bucket_struct *)buffer)->cur_size = cur_bucket_len;
		void * curAddr = buffer + sizeof(struct bucket_struct);
		((struct stats_struct *)curAddr)->total_bytes = 0;
		((struct stats_struct *)curAddr)->block_size = MAX_BUCKET_SIZE;
		int ans = cache_set("stats", buffer, cur_bucket_len);
		free(buffer);
		hello_mkdir("/", 0);
	}
	
	//(void) conn;
	//cfg->kernel_cache = 1;
	
	return NULL;
}

int hello_opendir(const char* path, struct fuse_file_info* fi) {

	return 0;
}

int decreaseChildrensNum(const char* path){
	printf("in increaseChildrensNum\n");
	char * buffer = malloc(1280);
	if(cache_get(path, buffer, 1280) != -1){
		struct header_struct * cur_struct = (struct header_struct *)(buffer + sizeof(struct bucket_struct));
		int cur = --(cur_struct->num_buckets);
		printf("current num in parent: %d\n", cur);
		cache_set(path, buffer, sizeof(struct bucket_struct) + sizeof(struct header_struct));
		free(buffer);
		return cur;
	}
	free(buffer);
	return -1;
}

int decreaseXattrNum(const char* path){
	char * buffer = malloc(1280);
	if(cache_get(path, buffer, 1280) != -1){
		struct header_struct * cur_struct = (struct header_struct *)(buffer + sizeof(struct bucket_struct));
		int cur = --(cur_struct->xattr_num);
		cache_set(path, buffer, sizeof(struct bucket_struct) + sizeof(struct header_struct));
		free(buffer);
		return cur;
	}
	free(buffer);
	return -1;
}

// path == path of child directory
int hello_rmdir(const char* path) {
	printf("IN RMDIR\n");
	if(strcmp(path, "/") == 0){
		return -EPERM;
	}
	char parent[250];
	strcpy(parent, path);
	int len = strlen(parent);
	len--;
	while(parent[len] != '/'){
		parent[len] = '\0';
		len--;
	}
	if(len != 0){
		parent[len] = '\0';
		len--;
	}
	char current_element[1280];
	if(cache_get(path, current_element, 1280) == -1){
		printf("directory doesn't exist\n");
		return -ENOTDIR;
	}
	
	if(((struct header_struct *)(current_element + sizeof(struct bucket_struct)))->num_buckets > 0){
		printf("Removing directory is not empty\n");
		return -ENOTEMPTY;
	}
	char parent_element[1280];
	cache_get(parent, parent_element, 1280);
	int parentCurNum = ((struct header_struct *)(current_element + sizeof(struct bucket_struct)))->parent_index;
	char keyToChange[250];
	sprintf(keyToChange, "%d", parentCurNum);
	strcat(keyToChange, parent);
	
	int parentMaxIndex = decreaseChildrensNum(parent) + 1;
	char parentLastElemKey[250];
	sprintf(parentLastElemKey, "%d", parentMaxIndex);
	strcat(parentLastElemKey, parent);
	char lastElemOfParent[1280];

	cache_get(parentLastElemKey, lastElemOfParent, 1280);
	int elemLen = ((struct bucket_struct *)lastElemOfParent)->cur_size;
	cache_set(keyToChange, lastElemOfParent, elemLen);
	// change parent index in last elem
	char keyBuf[260];
	strncpy(keyBuf, lastElemOfParent + sizeof(struct bucket_struct), elemLen - sizeof(struct bucket_struct));
	keyBuf[elemLen - sizeof(struct bucket_struct)] = '\0';
	char tempBuf[1280];
	cache_get(keyBuf, tempBuf, 1280);
	((struct header_struct *)(tempBuf + sizeof(struct bucket_struct)))->parent_index = parentCurNum;
	int curLength = ((struct bucket_struct *)tempBuf)->cur_size;
	cache_set(keyBuf, tempBuf, curLength);

	char removeLastMsg[264] = "delete ";
	strcat(removeLastMsg, parentLastElemKey);
	strcat(removeLastMsg, "\r\n");
	char answer[128];
	cache_msg(removeLastMsg, answer, 128);
	printf("REMOVED in Parent DIR ANSWER: %s\n", answer);
	char curAnswer[128];
	char curRemoveMsg[264] = "delete ";
	strcat(curRemoveMsg, path);
	strcat(curRemoveMsg, "\r\n");
	cache_msg(curRemoveMsg, curAnswer, 128);
	printf("REMOVED Cur DIR ANSWER: %s\n", curAnswer);
	return 0;
}

// removing
////////////////////////////////////////////////////////////also need to implement removing all the xattrs
int hello_unlink(const char* path) {
	printf("IN unlink\n");
	if(strcmp(path, "/") == 0){
		return -EPERM;
	}
	char parent[250];
	strcpy(parent, path);
	int len = strlen(parent);
	len--;
	while(parent[len] != '/'){
		parent[len] = '\0';
		len--;
	}
	if(len != 0){
		parent[len] = '\0';
		len--;
	}
	char current_element[1280];
	if(cache_get(path, current_element, 1280) == -1){
		printf("file doesn't exist\n");
		return -ENOENT;
	}
	int bucksNum = ((struct header_struct *)(current_element + sizeof(struct bucket_struct)))->num_buckets;
	int xattrsNum = ((struct header_struct *)(current_element + sizeof(struct bucket_struct)))->xattr_num;
	char parent_element[1280];
	cache_get(parent, parent_element, 1280);
	int parentCurNum = ((struct header_struct *)(current_element + sizeof(struct bucket_struct)))->parent_index;
	char keyToChange[250];
	sprintf(keyToChange, "%d", parentCurNum);
	strcat(keyToChange, parent);
	
	int parentMaxIndex = decreaseChildrensNum(parent) + 1;
	char parentLastElemKey[250];
	sprintf(parentLastElemKey, "%d", parentMaxIndex);
	strcat(parentLastElemKey, parent);
	char lastElemOfParent[1280];

	cache_get(parentLastElemKey, lastElemOfParent, 1280);
	int elemLen = ((struct bucket_struct *)lastElemOfParent)->cur_size;
	cache_set(keyToChange, lastElemOfParent, elemLen);
	// change parent index in last elem
	char keyBuf[260];
	strncpy(keyBuf, lastElemOfParent + sizeof(struct bucket_struct), elemLen - sizeof(struct bucket_struct));
	keyBuf[elemLen - sizeof(struct bucket_struct)] = '\0';
	char tempBuf[1280];
	cache_get(keyBuf, tempBuf, 1280);
	((struct header_struct *)(tempBuf + sizeof(struct bucket_struct)))->parent_index = parentCurNum;
	int curLength = ((struct bucket_struct *)tempBuf)->cur_size;
	cache_set(keyBuf, tempBuf, curLength);

	char removeLastMsg[264] = "delete ";
	strcat(removeLastMsg, parentLastElemKey);
	strcat(removeLastMsg, "\r\n");
	char answer[128];
	cache_msg(removeLastMsg, answer, 128);
	printf("REMOVED in Parent DIR ANSWER: %s\n", answer);
	char curAnswer[128];
	char curRemoveMsg[264] = "delete ";
	strcat(curRemoveMsg, path);
	strcat(curRemoveMsg, "\r\n");
	cache_msg(curRemoveMsg, curAnswer, 128);
	printf("REMOVED Cur DIR ANSWER: %s\n", curAnswer);

	for(int i = 1; i <= xattrsNum; i++){
		char rmChildMsg[264] = "delete $";
		char ragaca[22];
		sprintf(ragaca, "%d", i);
		strcat(rmChildMsg, ragaca);
		strcat(rmChildMsg, path);
		strcat(rmChildMsg, "\r\n");
		char buffer[1280];
		cache_msg(rmChildMsg, buffer, 1280);
		printf("REMOVED Child DIR ANSWER: %s\n", buffer);

	}

	for(int i = 1; i <= bucksNum; i++){
		char rmChildMsg[264] = "delete &";
		char ragaca[22];
		sprintf(ragaca, "%d", i);
		strcat(rmChildMsg, ragaca);
		strcat(rmChildMsg, path);
		strcat(rmChildMsg, "\r\n");
		char buffer[1280];
		cache_msg(rmChildMsg, buffer, 1280);
		printf("REMOVED Child DIR ANSWER: %s\n", buffer);

	}
	return 0;
}

int hello_create(const char * path, mode_t mode, struct fuse_file_info * fi){
	printf("IN CREATE\n");
	int parentIndex = addToParent(path);
	if(parentIndex >= 0){
		int cur_bucket_size = sizeof(struct bucket_struct) + sizeof(struct header_struct);
		char * buffer = malloc(cur_bucket_size);
		((struct bucket_struct *)buffer)->cur_size = cur_bucket_size;
		struct header_struct * curFile = (struct header_struct *)(buffer + sizeof(struct bucket_struct));
		curFile->is_dir = 0;
		curFile->data_len = 0;
		curFile->num_buckets = 0;
		curFile->parent_index = parentIndex;
		curFile->xattr_num = 0;
		cache_set(path, buffer, cur_bucket_size);
		free(buffer);
		return 0;
	}
}

int hello_read(const char* path, char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	if(offset < 0){
		return 0;
	}
	printf("IN READ\n");
	int toReturn = size;
	char * headers = malloc(1280);
	cache_get(path, headers, 1280);
	struct header_struct * curFile = (struct header_struct *)(headers + sizeof(struct bucket_struct));
	int curLength = curFile->data_len;
	if(offset >= curLength){
		free(headers);
		return 0;
	}
	if(offset + size > curLength){
		size = curLength - offset;
		toReturn = size;
		//return 0;
	}
	int firstBucketIndex = offset/MAX_BUCKET_SIZE + 1;
	int written_len = 0;
	while(size > 0){
		char curKey[250];
		curKey[0] = '&';
		sprintf(curKey + 1, "%d", firstBucketIndex);
		strcat(curKey, path);
		char data[SPACE_FOR_STRUCTS + MAX_BUCKET_SIZE];
		cache_get(curKey, data, SPACE_FOR_STRUCTS + MAX_BUCKET_SIZE);
		int bucketDataLen = ((struct bucket_struct*)data)->cur_size - sizeof(struct bucket_struct);// - sizeof(struct header_struct);
		int startingOffsetInData = offset % MAX_BUCKET_SIZE;
		int maxAvailableSpace = MAX_BUCKET_SIZE - startingOffsetInData;
		int realDataToReadLen = min(size, maxAvailableSpace);
		realDataToReadLen = min(realDataToReadLen, bucketDataLen);
		memcpy(buf + written_len, data + sizeof(struct bucket_struct) + startingOffsetInData, realDataToReadLen);
		written_len+=realDataToReadLen;
		offset+=realDataToReadLen;
		size-=realDataToReadLen;
		firstBucketIndex++;
	}
	free(headers);

	return toReturn;
}

static int hello_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi){
	printf("IN WRITE\n");
	int toReturn = size;
	char * headers = malloc(1280);
	cache_get(path, headers, 1280);
	struct header_struct * curFile = (struct header_struct *)(headers + sizeof(struct bucket_struct));
	int curLength = curFile->data_len;
	if(offset > curLength){
		char * zeros = malloc(offset - curLength);
		memset(zeros, 0, offset - curLength);
		hello_write(path, zeros, offset - curLength, curLength, fi);
		free(zeros);
		curLength = offset;
		curFile->data_len = curLength;
	}
	int firstBucketIndex = 1 + offset/MAX_BUCKET_SIZE;
	int written_len = 0;
	while(size > 0){
		if(curFile->num_buckets < firstBucketIndex){
			char curKey[250];
			curKey[0] = '&';
			sprintf(curKey + 1, "%d", firstBucketIndex);
			strcat(curKey, path);
			int realDataToWriteLength = min(size, MAX_BUCKET_SIZE);
			char data[SPACE_FOR_STRUCTS + MAX_BUCKET_SIZE];
			((struct bucket_struct *)data)->cur_size = sizeof(struct bucket_struct) + realDataToWriteLength;
			memcpy(data + sizeof(struct bucket_struct), buf + written_len, realDataToWriteLength);
			written_len+=realDataToWriteLength;
			cache_set(curKey, data, realDataToWriteLength + sizeof(struct bucket_struct));
			curFile->num_buckets++;
			curFile->data_len+=realDataToWriteLength;
			curFile->data_len+=sizeof(struct bucket_struct);
			offset+=realDataToWriteLength;
			size-=realDataToWriteLength;
		} else {
			char curKey[250];
			curKey[0] = '&';
			sprintf(curKey + 1, "%d", firstBucketIndex);
			strcat(curKey, path);
			char data[SPACE_FOR_STRUCTS + MAX_BUCKET_SIZE];
			cache_get(curKey, data, SPACE_FOR_STRUCTS + MAX_BUCKET_SIZE);
			int oldLen = ((struct bucket_struct*)data)->cur_size - sizeof(struct bucket_struct);
			int startingOffsetInData = offset % MAX_BUCKET_SIZE;
			int maxAvailableSpace = MAX_BUCKET_SIZE - startingOffsetInData;
			int realDataToWriteLen = min(size, maxAvailableSpace);
			memcpy(data + sizeof(struct bucket_struct) + startingOffsetInData, buf + written_len, realDataToWriteLen);
			written_len+=realDataToWriteLen;
			int newLen = max(oldLen, startingOffsetInData + realDataToWriteLen);
			((struct bucket_struct*)data)->cur_size = newLen + sizeof(struct bucket_struct);
			curFile->data_len+=(newLen-oldLen);
			offset+=realDataToWriteLen;
			size-=realDataToWriteLen;
			
			cache_set(curKey,  data, newLen+sizeof(struct bucket_struct));
		}

		firstBucketIndex++;
	}

	cache_set(path, headers, sizeof(struct header_struct) + sizeof(struct bucket_struct));
	free(headers);
	return toReturn;
	//cant return 0
}

int hello_release(const char* path, struct fuse_file_info *fi){
	
	return 0;
}

int hello_access(const char* path, int mask){
	// return -ENOENT if the path doesn't exist;
	char buf[1280];
	if(cache_get(path, buf, 1280) == -1){
		return -ENOENT;
	}
	// return -EACCESS; if the requested permission isn't available
	return 0; //success
}

/*
       On success, zero is returned.  On failure, -1 is returned and errno
       is set appropriately.
	   see man setfattr error numebers for more info.
 */
int hello_setxattr(const char* path, const char* name, const char* value, size_t size, int flags){
	char buf[MAX_BUCKET_SIZE];
	cache_get(path, buf, MAX_BUCKET_SIZE);
	int newI = ++((struct header_struct*)(buf + sizeof(struct bucket_struct)))->xattr_num;
	cache_set(path, buf, ((struct bucket_struct *)buf)->cur_size);
	char newKey[250];
	newKey[0] = '$';
	sprintf(newKey + 1, "%d", newI);
	strcat(newKey, path);
	int len = sizeof(struct bucket_struct) + 2*sizeof(int) + sizeof(struct header_struct) + strlen(name) + size;
	char * data = malloc(len);
	((struct bucket_struct *)data)->cur_size = len;
	((struct header_struct *)(data + sizeof(struct bucket_struct)))->data_len=size;
	((struct header_struct *)(data + sizeof(struct bucket_struct)))->is_dir = 2;
	((struct header_struct *)(data + sizeof(struct bucket_struct)))->parent_index = newI;
	*((int *)(data + sizeof(struct header_struct) + sizeof(struct bucket_struct))) = strlen(name); // name len
	strcpy(data + sizeof(int) + sizeof(struct header_struct) + sizeof(struct bucket_struct), name); //name
	*((int *)(data + sizeof(struct header_struct) + sizeof(struct bucket_struct) + sizeof(int) + strlen(name))) = size; // data len
	memcpy(data + sizeof(struct header_struct) + sizeof(struct bucket_struct) + 2*sizeof(int) + strlen(name), value, size);
	cache_set(newKey, data, len);

	free(data);
	return 0;
}

int hello_getxattr(const char* path, const char* name, char* value, size_t size){
	char* buf = malloc(MAX_BUCKET_SIZE);
	cache_get(path, buf, MAX_BUCKET_SIZE);
	int newI = ((struct header_struct*)(buf + sizeof(struct bucket_struct)))->xattr_num;
	free(buf);
	for(int i = 1; i <= newI; i++){
		char curKey[250];
		curKey[0] = '$';
		sprintf(curKey + 1, "%d", i);
		strcat(curKey, path);
		char * curData = malloc(66000);
		cache_get(curKey, curData, 66000);
		int curLen = *((int *)(curData + sizeof(struct header_struct) + sizeof(struct bucket_struct)));
		char curName[250];
		strncpy(curName, curData + sizeof(int) + sizeof(struct header_struct) + sizeof(struct bucket_struct), curLen);
		curName[curLen] = '\0';
		if(strcmp(name, curName) == 0){
			printf("SHEMOVIDAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA\n");
			int dataLen = *((int *)(curData + sizeof(struct header_struct) + sizeof(struct bucket_struct) + sizeof(int) + strlen(name)));
			char * ptr = curData + sizeof(struct header_struct) + sizeof(struct bucket_struct) + 2*sizeof(int) + strlen(name);
			size = min(size, dataLen);
			memcpy(value, ptr, size);
			printf("VALUEEEEEEEEEEE BEACHHHHH: %s\n", ptr);
			printf("VALUE SIZE BEACHHHHHH: %d\n", dataLen);
			free(curData);
			return dataLen;

		}
		

		free(curData);

	}

	return -1;
}

int hello_listxattr(const char* path, char* list, size_t size){
	printf("BUFFER SIZE IN LISTXATTR: %d\n", (int)size);
	char buf[MAX_BUCKET_SIZE];
	cache_get(path, buf, MAX_BUCKET_SIZE);
	int newI = ((struct header_struct*)(buf + sizeof(struct bucket_struct)))->xattr_num;
	if(size == 0){
		return newI;
	}
	int pos = 0;
	for(int i = 1; i <= newI; i++){
		char curKey[250];
		curKey[0] = '$';
		sprintf(curKey + 1, "%d", i);
		strcat(curKey, path);
		char * curData = malloc(66000);
		cache_get(curKey, curData, 66000);
		int curLen = *((int *)(curData + sizeof(struct header_struct) + sizeof(struct bucket_struct)));
		char curName[250];
		strncpy(curName, curData + sizeof(int) + sizeof(struct header_struct) + sizeof(struct bucket_struct), curLen);
		curName[curLen] = '\0';
		printf("CUUUUUUUUUUUR NAMEEEEEEEEEEEEEEEEEEEEEEE: %s\n", curName);
		printf("CUUUUUUUUUUUUUUUUUURRRRRRRRRR LEEEEEEEEEEENNNNNNNN: %d\n", curLen);
		if(size < pos + curLen + 1){
			free(curData);
			return newI;
		}
		strcpy(list + pos, curName);
		pos+=curLen;
		
		pos++;
		free(curData);
	}
	printf("CUUUUUUUUREEEENNNTT ANSS BUFFF: %s\n", list);
	return pos;
}

int hello_removexattr(const char *path, const char *name){
	char buf[MAX_BUCKET_SIZE];
	cache_get(path, buf, MAX_BUCKET_SIZE);
	int newI = (((struct header_struct*)(buf + sizeof(struct bucket_struct)))->xattr_num)--;
	int removingI = -1;
	char * curKey = malloc(250);
	for(int i = 1; i <= newI; i++){
		curKey[0] = '$';
		sprintf(curKey + 1, "%d", i);
		strcat(curKey, path);
		char * curData = malloc(66000);
		cache_get(curKey, curData, 66000);
		int curLen = *((int *)(curData + sizeof(struct header_struct) + sizeof(struct bucket_struct)));
		char curName[250];
		strncpy(curName, curData + sizeof(int) + sizeof(struct header_struct) + sizeof(struct bucket_struct), curLen);
		curName[curLen] = '\0';
		if(strcmp(name, curName) == 0){
			removingI = i;
			free(curData);
			cache_set(path, buf, ((struct bucket_struct*)buf)->cur_size);
			break;
		}
		free(curKey);
		curKey = malloc(250);
		free(curData);
	}
	if(removingI == -1){
		free(curKey);
		return -1;// couldnt find attribute
	}

	printf("REMOVEXATTRRRETJAOTJOAHRofQHOFIAZHWOFHQOIAHGO\n");
	char lastKey[250];
	lastKey[0] = '$';
	sprintf(lastKey + 1, "%d", newI);
	strcat(lastKey, path);
	char * lastData = malloc(66000);
	cache_get(lastKey, lastData, 66000);
	printf("VIS EJAXEBI\n");
	((struct header_struct*)(lastData + sizeof(struct bucket_struct)))->parent_index = removingI;
	cache_set(curKey, lastData, ((struct bucket_struct*)lastData)->cur_size);
	free(lastData);
	char removeLastMsg[264] = "delete ";
	strcat(removeLastMsg, lastKey);
	strcat(removeLastMsg, "\r\n");
	char answer[1024];
	printf("RAS EJAXEBI\n");
	cache_msg(removeLastMsg, answer, 1024);

	free(curKey);
	printf("gawlgagmsehjog rdoh jpoeoj hpoorsj hposjph jdprjh pojh osjoh jsiodh joisegj osdrj sd\n");
	return 0;
}

static struct fuse_operations hello_oper = {
	.init       = hello_init,
	.getattr	= hello_getattr,
	.readdir	= hello_readdir,
	.read		= hello_read,
	.mkdir		= hello_mkdir,
	.write		= hello_write,
	.rmdir		= hello_rmdir,
	.opendir	= hello_opendir,
	.unlink		= hello_unlink,
	.create		= hello_create,
	.open		= hello_open,
	.release	= hello_release,
	.access		= hello_access,
	.setxattr	= hello_setxattr,
	.getxattr	= hello_getxattr,
	.listxattr	= hello_listxattr,
	.removexattr= hello_removexattr,
};

static void show_help(const char *progname)
{
	printf("usage: %s [options] <mountpoint>\n\n", progname);
	printf("File-system specific options:\n"
	       "    --name=<s>          Name of the \"hello\" file\n"
	       "                        (default: \"hello\")\n"
	       "    --contents=<s>      Contents \"hello\" file\n"
	       "                        (default \"Hello, World!\\n\")\n"
	       "\n");
}

int main(int argc, char *argv[])
{
	printf("started main\n");
	int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	/* Set defaults -- we have to use strdup so that
	   fuse_opt_parse can free the defaults if other
	   values are specified */
	//options.filename = strdup("hello");
	//options.contents = strdup("Hello World!\n");

	/* Parse options */
	if (fuse_opt_parse(&args, &options, option_spec, NULL) == -1)
		return 1;

	/* When --help is specified, first print our own file-system
	   specific help text, then signal fuse_main to show
	   additional help (by adding `--help` to the options again)
	   without usage: line (by setting argv[0] to the empty
	   string) */
	if (options.show_help) {
		show_help(argv[0]);
		assert(fuse_opt_add_arg(&args, "--help") == 0);
		args.argv[0][0] = '\0';
	}
	
	ret = fuse_main(args.argc, args.argv, &hello_oper, NULL);
	printf("midel\n");
	fuse_opt_free_args(&args);
	printf("finish\n");
	return ret;
}
