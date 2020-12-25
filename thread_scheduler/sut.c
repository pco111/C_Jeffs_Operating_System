#include <stdio.h>
#include <stdlib.h>
#include <ucontext.h>
#include "queue.h"
#include "sut.h"
#include <pthread.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#define MEM 64000
struct queue taskReadyQueue;
struct queue IOwaitQueue;
ucontext_t iExec_idleContext;
ucontext_t cExec_idleContext;
ucontext_t dump;
int current_socketfd;
int IO_occupied=0; //boolean value
int computation_occupied=0; //boolean value
int threadJoin_signal=0; //boolean value
pthread_t t_cExec_handle;
pthread_t t_iExec_handle;
char readBuffer[1024];
ucontext_t contextStorage[819200]; //global storage for all contexts in TRQ
int cs_count=0;
struct queue_entry* nodeStorage[819200]; //global storage for all nodes in TRQ
int ns_count=0;
pthread_mutex_t trqLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t iowqLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t csLock=PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t nsLock=PTHREAD_MUTEX_INITIALIZER;

//connect to socket from assignment 1
void connect_to_server(const char *host, uint16_t port, int *sockfd) {
  struct sockaddr_in server_address = { 0 };
  // create a new socket
  *sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (*sockfd < 0) {
    perror("Failed to create a new socket\n");
  }
  // connect to server
  server_address.sin_family = AF_INET;
  inet_pton(AF_INET, host, &(server_address.sin_addr.s_addr));
  server_address.sin_port = htons(port);
  if (connect(*sockfd, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
    perror("Failed to connect to server\n");
  }
}

void* cExec() {
	getcontext(&cExec_idleContext);
	while(1) {
		if(threadJoin_signal) {
			break;
		}
		if(queue_peek_front(&taskReadyQueue)) {
			pthread_mutex_lock(&trqLock);
			computation_occupied=1;
			struct queue_entry *ptr=queue_pop_head(&taskReadyQueue);
			pthread_mutex_unlock(&trqLock);
			usleep(100);
			swapcontext(&dump, (ucontext_t*)ptr->data);
		} else {
			computation_occupied=0;
		}
		usleep(100);
	}
}

void* iExec() {
	getcontext(&iExec_idleContext);
	while(1) {
		if(threadJoin_signal) {
			break;
		}
		if(queue_peek_front(&IOwaitQueue)) {
			pthread_mutex_lock(&iowqLock);
			IO_occupied=1;
			struct queue_entry *ptr= queue_pop_head(&IOwaitQueue);
			pthread_mutex_unlock(&iowqLock);
			usleep(100);
			swapcontext(&dump,(ucontext_t*)ptr->data);
		} else {
			IO_occupied=0;
		}
		usleep(100);
	}
}

void sut_init() {
	getcontext(&dump);
	taskReadyQueue=queue_create();
	queue_init(&taskReadyQueue);
        IOwaitQueue=queue_create();
        queue_init(&IOwaitQueue);
	pthread_create(&t_cExec_handle,NULL,cExec,NULL);
	pthread_create(&t_iExec_handle,NULL,iExec,NULL);
}

bool sut_create(sut_task_f fn) {
	pthread_mutex_lock(&csLock);
	int cc_cs=cs_count++; //current count for context storage
	pthread_mutex_unlock(&csLock);
	getcontext(&contextStorage[cc_cs]); 
	contextStorage[cc_cs].uc_link=&cExec_idleContext;
	contextStorage[cc_cs].uc_stack.ss_sp=malloc(MEM);
 	contextStorage[cc_cs].uc_stack.ss_size=MEM;
 	makecontext(&contextStorage[cc_cs],fn,0);
	pthread_mutex_lock(&nsLock);
	int cc_ns=ns_count++;
	pthread_mutex_unlock(&nsLock);
	nodeStorage[cc_ns]=queue_new_node(&contextStorage[cc_cs]);
	pthread_mutex_lock(&trqLock);
	queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
	pthread_mutex_unlock(&trqLock);
	return true;
}

void sut_yield() {
	pthread_mutex_lock(&csLock);
	int cc_cs=cs_count++;
	pthread_mutex_unlock(&csLock);
	pthread_mutex_lock(&nsLock);
	int cc_ns=ns_count++;
	pthread_mutex_unlock(&nsLock);
	nodeStorage[cc_ns]=queue_new_node(&contextStorage[cc_cs]);
	pthread_mutex_lock(&trqLock);
	queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
	struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
	pthread_mutex_unlock(&trqLock);
        if(ptr) {
                swapcontext(&contextStorage[cc_cs],(ucontext_t*)ptr->data);
        } else {
                swapcontext(&contextStorage[cc_cs],&cExec_idleContext);
        }
}

void sut_exit() {
	pthread_mutex_lock(&trqLock);
	struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
	pthread_mutex_unlock(&trqLock);
	if(ptr) {
		swapcontext(&dump,(ucontext_t*)ptr->data);
	} else {
		swapcontext(&dump,&cExec_idleContext);
	}
}

void sut_open(char *dest, int port) {
        pthread_mutex_lock(&trqLock);
	struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
	pthread_mutex_unlock(&trqLock);
        pthread_mutex_lock(&csLock);
        int cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        int cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&iowqLock);
        queue_insert_tail(&IOwaitQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&iowqLock);
	if(ptr) {
		swapcontext(&contextStorage[cc_cs],(ucontext_t*)ptr->data); //todo: mutex-up IOwaitSpot too?
	} else {
		swapcontext(&contextStorage[cc_cs],&cExec_idleContext);
	}
	connect_to_server(dest,port,&current_socketfd); //test configuration: change later
	pthread_mutex_lock(&csLock);
	cc_cs=cs_count++;
	pthread_mutex_unlock(&csLock);
	pthread_mutex_lock(&nsLock);
	cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
	pthread_mutex_unlock(&nsLock);
	nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
	pthread_mutex_lock(&trqLock);
	queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
	pthread_mutex_unlock(&trqLock);
	swapcontext(&contextStorage[cc_cs],&iExec_idleContext); 	
}


void sut_write(char *buf, int size) {
	pthread_mutex_lock(&trqLock);
        struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
        pthread_mutex_unlock(&trqLock);
        pthread_mutex_lock(&csLock);
        int cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        int cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&iowqLock);
        queue_insert_tail(&IOwaitQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&iowqLock);
        if(ptr) {
                swapcontext(&contextStorage[cc_cs],(ucontext_t*)ptr->data); //todo: mutex-up IOwaitSpot too?
        } else {
                swapcontext(&contextStorage[cc_cs],&cExec_idleContext);
        }
	send(current_socketfd,buf,size,0); //test configuration: change later
	pthread_mutex_lock(&csLock);
        cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&trqLock);
        queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&trqLock);
        swapcontext(&contextStorage[cc_cs],&iExec_idleContext); 
}

void sut_close() {
        pthread_mutex_lock(&trqLock);
        struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
        pthread_mutex_unlock(&trqLock);
        pthread_mutex_lock(&csLock);
        int cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        int cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&iowqLock);
        queue_insert_tail(&IOwaitQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&iowqLock);
        if(ptr) {
                swapcontext(&contextStorage[cc_cs],(ucontext_t*)ptr->data); //todo: mutex-up IOwaitSpot too?
        } else {
                swapcontext(&contextStorage[cc_cs],&cExec_idleContext);
        }
	close(current_socketfd);
	current_socketfd=0;
        pthread_mutex_lock(&csLock);
        cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&trqLock);
        queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&trqLock);
        swapcontext(&contextStorage[cc_cs],&iExec_idleContext);
}

char *sut_read() {
        pthread_mutex_lock(&trqLock);
        struct queue_entry *ptr= queue_pop_head(&taskReadyQueue);
        pthread_mutex_unlock(&trqLock);
        pthread_mutex_lock(&csLock);
        int cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        int cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&iowqLock);
        queue_insert_tail(&IOwaitQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&iowqLock);
        if(ptr) {
                swapcontext(&contextStorage[cc_cs],(ucontext_t*)ptr->data); //todo: mutex-up IOwaitSpot too?
        } else {
                swapcontext(&contextStorage[cc_cs],&cExec_idleContext);
        }	
	recv(current_socketfd,readBuffer,sizeof(readBuffer), 0);
        pthread_mutex_lock(&csLock);
        cc_cs=cs_count++;
        pthread_mutex_unlock(&csLock);
        pthread_mutex_lock(&nsLock);
        cc_ns=ns_count++; //cc stands for current count; ns for nodeStorage; current index in node storage
        pthread_mutex_unlock(&nsLock);
        nodeStorage[cc_ns]= queue_new_node(&contextStorage[cc_cs]);
        pthread_mutex_lock(&trqLock);
        queue_insert_tail(&taskReadyQueue,nodeStorage[cc_ns]);
        pthread_mutex_unlock(&trqLock);
        swapcontext(&contextStorage[cc_cs],&iExec_idleContext);	
	return readBuffer;
}

void sut_shutdown() {
	while(1) {
		if(!queue_peek_front(&taskReadyQueue) && !IO_occupied && !queue_peek_front(&IOwaitQueue) && !computation_occupied) {
			printf("program will soon shutdown \n");
			threadJoin_signal=1;
			pthread_join(t_cExec_handle,NULL);
			pthread_join(t_iExec_handle,NULL);
			return;
		}
		usleep(100);
	}
}

