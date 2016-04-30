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

static int total_requests;
static int request_num = 0;

static request_queue_t* requests;

static pthread_t dispatchers[MAX_THREADS];
static pthread_t workers[MAX_THREADS];

static pthread_mutex_t queue_access = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t dispatch_CV = PTHREAD_COND_INITIALIZER;
static pthread_cond_t worker_CV = PTHREAD_COND_INITIALIZER;

/*void insert_request(request_queue_t req)
{
	pthread_mutex
}*/

void * dispatch(void * arg)
{
	while(1)
	{
		if(accept_connection() != 0);

		
	}	

	return NULL;
}

void * worker(void * arg)
{
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
