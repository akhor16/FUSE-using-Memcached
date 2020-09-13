// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#include "memcache.h"
#include <stdlib.h>
#define PORT 11211 
#define MB 1048576
int sockfd = 0;


void cache_close_sock(){
    close(sockfd);
}

//returns -1 on error
int cache_get(const char * key, char * buffer, int size){
	printf("get cache key: %s\n", key);
	char data[264] = "get ";
	strcat(data, key);
	strcat(data, "\r\n");
	if(send(sockfd, data, strlen(data), 0) == -1){
		printf("failed cache_get send\n");
		return -1;
	}
	int bufferSize = 66000;
	char * ans = malloc(bufferSize);
	read( sockfd , ans, bufferSize);
	printf("received msg in get: %s\n", ans);
	if(strncmp(ans, "VALUE", 5) == 0){
		int pos = 5;
		while(ans[pos] != '\n'){
			pos++;
		}
		pos++;
		int len = ((struct bucket_struct *)(ans + pos))->cur_size;
		memcpy(buffer, ans + pos, len);
		printf("answer in cache get\n");
		free(ans);
		return 0;
	}
	free(ans);
	return -1;
}

int cache_msg(char * data, char * buffer, int size){
	if(send(sockfd, data, strlen(data), 0) == -1){
		return -1;
	}
	int valread = read( sockfd , buffer, size);
	return 0;
}

int cache_set(const char * key, char * val, int val_length){
	printf("IN CACHE SET, KEY ISSSS: %s\n", key);
	printf("IN CACHE SET, VALUE ISSSS: %s\n", val + sizeof(struct bucket_struct));
	char * data = malloc(66000);
	strcpy(data, "set ");
	strcat(data, key);
	strcat(data, " 0 0 ");
	char char_len[20];
	sprintf(char_len, "%d", val_length);
	strcat(data, char_len);
	strcat(data, "\r\n");

	int msgLen = strlen(data) + val_length + 2;
	char * startPos = data + strlen(data);
	memcpy(startPos, val, val_length);
	strcpy(startPos + val_length, "\r\n");
	if(send(sockfd, data, msgLen, 0) == -1){
		printf("couldn't send msg ;(\n");
		free(data);
		return -1;
	}
	if(FAST_MODE){
		free(data);
		return 0;
	}
	char * buffer = malloc(128);
	int valread = read( sockfd , buffer, 128);
	printf("RECEIVED ANSWER IN CACHE_SET: %s\n", buffer);
	if(strcmp(buffer, "NOT_STORED\r\n") == 0 || strcmp(buffer, "ERROR\r\n") == 0){
		printf("%s not stored", key);
		free(data);
		free(buffer);
		return -1;
	}
	free(buffer);
	free(data);
	return 0;
}

void cache_init() 
{
	printf("Initializing memcached\n");
	struct sockaddr_in serv_addr; 

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) 
	{ 
		printf("\n Socket creation error \n"); 
		return; 
	} 
	bzero(&serv_addr, sizeof(serv_addr)); 
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	serv_addr.sin_port = htons(PORT); 
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) != 0)
	{ 
		printf("\nConnection Failed \n");
	} else {
		printf("connected to the server\n");
	}
}