/* csci4061 S2016 Assignment 4 
* section: one_digit_number 
* date: mm/dd/yy 
* names: Name of each member of the team (for partners)
* UMN Internet ID, Student ID (xxxxxxxx, 4444444), (for partners)
*/

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "util.h"

#define MAX_THREADS 100
#define MAX_QUEUE_SIZE 100
#define MAX_REQUEST_LENGTH 64

//Structure for queue.
typedef struct request_queue
{
        int             m_socket;
        char    m_szRequest[MAX_REQUEST_LENGTH];
} request_queue_t;

static int input_index = 0;
static int output_index = 0;
static int total_requests;
static int request_count = 0;

static request_queue_t* requests;

static pthread_t dispatchers[MAX_THREADS];
static pthread_t workers[MAX_THREADS];

static pthread_mutex_t queue_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dispatch_CV = PTHREAD_COND_INITIALIZER;
static pthread_cond_t worker_CV = PTHREAD_COND_INITIALIZER;

void insert_request(request_queue_t req)
{
	pthread_mutex_lock(&queue_access);

	while(request_count == total_requests)
		pthread_cond_wait(&dispatch_CV, &queue_access);
	
	requests[input_index] = req;

	input_index = (input_index + 1) % total_requests;
	request_count++;
	
	pthread_cond_signal(&worker_CV);
	pthread_mutex_unlock(&queue_access);
}

request_queue_t extract_request()
{
	request_queue_t req;
	
	pthread_mutex_lock(&queue_access);

	while(request_count == 0)
		pthread_cond_wait(&worker_CV, &queue_access);

	req.m_socket = requests[output_index].m_socket;
	strcpy(req.m_szRequest, requests[output_index].m_szRequest);

	output_index = (output_index + 1) % total_requests;
	request_count--;

	pthread_cond_signal(&dispatch_CV);
	pthread_mutex_unlock(&queue_access);

	return req;
}

void * dispatch(void * arg)
{
	int fd;
	char filename[1024];

	while(1)
	{
		if((fd = accept_connection()) != 0)
			continue;

		pthread_mutex_lock(&queue_access);

		if(get_request(fd, filename) != 0)
		{
			pthread_mutex_unlock(&queue_access);
			continue;
		}

		pthread_mutex_unlock(&queue_access);

		request_queue_t req;

		req.m_socket = fd;
		strcpy(req.m_szRequest, filename);

		insert_request(req);
		
	}	

	return NULL;
}

void * worker(void * arg)
{
	while(1)
	{
		//read from queue
		request_queue_t req;	
		req = extract_request();

		//Parse file type
		char* filepath = req.m_szRequest;
		int type = 0;
		if(strstr(filepath,".html") != NULL){
			type += 1;
		}	
		if(strstr(filepath,".jpg") != NULL){
			type += 2;
		}
		if(strstr(filepath,".gif") != NULL){
			type += 4;
		}
		if(strstr(filepath,".txt") != NULL){
			type += 8;
		}

		char* cont_type;
		switch(type){
			case 1:
				cont_type = "text/html";
			case 2:
				cont_type = "text/jpeg";
			case 4:
				cont_type = "image/gif";
			default:
				cont_type = "text/plain";
		}
	
		//Put contents of file into buffer
		int length;
		char* buffer = 0;
		FILE * fp = fopen(filepath, "r");
		if(fp)
		{
			fseek(fp,0,SEEK_END);
			length = ftell(fp);
			fseek(fp,0,SEEK_SET);
			buffer = malloc(length);

			if(buffer)
			{
				fread(buffer,1,length,fp);
			}
			fclose(fp);
		}

		//Return result
		if(return_result(req.m_socket,cont_type,buffer,sizeof(char)*length) != 0){
			perror("failed to return result \n");
		}
		
		

	}
	return NULL;
}

int main(int argc, char **argv)
{
        //Error check first.
        if(argc != 6 && argc != 7)
        {
			printf("usage: %s port path num_dispatcher num_workers queue_length [cache_size]\n", argv[0]);
			return -1;
        }

        printf("Call init() first and make a dispather and worker threads\n");

		init(atoi(argv[1]));

		total_requests = atoi(argv[5]);

		requests = (request_queue_t*)malloc(total_requests*sizeof(request_queue_t));

		int i;
		for(i = 0; i < atoi(argv[3]); i++)
		{
			if(pthread_create(&dispatchers[i], NULL, dispatch, NULL) != 0);
		}
		
		int j;
		for(j = 0; j < atoi(argv[4]); j++)
		{
			if(pthread_create(&workers[j], NULL, worker, NULL) != 0);
		}

		int k;
		for(k = 0; k < atoi(argv[3]); k++)
		{
			if(pthread_join(dispatchers[k], NULL) != 0);
		}

		int l;
		for(l = 0; l < atoi(argv[4]); l++)
		{
			if(pthread_join(workers[l], NULL) != 0);
		}
		
        return 0;
}
