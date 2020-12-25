#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

#include "a1_lib.h"

#define BUFSIZE   1024

//dedicated to checking any exit requests from the childProcesses and handling these possible requests
void exitManager(pid_t childPids[20],int numChildren,int sockfd) {
  int exitRequested=0; //boolean value representing whether exit has been requested
  for(int i=0;i<numChildren;i++) {
        int rval;
        waitpid(childPids[i], &rval, WNOHANG);
        if(WEXITSTATUS(rval)==10) {
	  exitRequested=1;
        }
  }
  if(exitRequested==0) return;
  while(1) {
	int allRequested=1; //boolean representing if all child processes are ready to exit
	for(int i=0;i<numChildren;i++) {
        	int rval;
        	waitpid(childPids[i], &rval, WNOHANG);
        	if(WEXITSTATUS(rval)!=10) {
          		allRequested=0;
        	}
	}
	if(allRequested) {
		printf("The main server is about to exit \n");
		close(sockfd);
		exit(0);
	}
  }
}

void serveClient(int clientfd) {
  while(1){
    char msg[BUFSIZE];
    char response[80]="Action not found \n";
    MessageStruct recvStruct;
    memset(msg, 0, sizeof(msg));
    ssize_t byte_count = recv_message(clientfd,(void*)&recvStruct, sizeof(MessageStruct));
    if (byte_count <= 0) continue;
    //converts received string back to MessageStruct and parses it
    char* functionName;
    char* param1;
    char* param2;
    functionName=recvStruct.functionName;
    param1=recvStruct.param1;
    param2=recvStruct.param2;
    float param1F=atof(param1);
    float param2F=atof(param2);
    if (strcmp(functionName, "add")==0) {
	int result=(int) (param1F+param2F);
	sprintf(response, "%d",result);
    }
    if (strcmp(functionName, "multiply")==0) {
        int result=(int) (param1F*param2F);
        sprintf(response, "%d",result);
    }
    if (strcmp(functionName, "divide")==0) {
	if(param2F==0.00) {
		sprintf(response, "Error: Division by zero");
	} else {
        	float result=param1F/param2F;
        	sprintf(response, "%.6f",result);
	}
    }
    if (strcmp(functionName, "sleep")==0) {
        sleep((int)param1F);
	sprintf(response, "The child process slept for some time");
    }
    if (strcmp(functionName, "factorial")==0) {
        int result=1;
	for(int i=2;i<=(int)param1F;i++) result*=i;
	sprintf(response, "%d",result);
    }
    if (strcmp(functionName, "quit")==0 || strcmp(functionName, "shutdown")==0) {
	send_message(clientfd,"Bye \n", strlen("Bye \n"));
	close(clientfd);
	return;
    } 
    printf("Client: %s\n", functionName);
    if (strcmp(response, "Action not found \n")==0) sprintf(response, "Error: Command \"%s\" not found",functionName);
    send_message(clientfd, response, strlen(response));
  }
}

int main(int argc, char *argv[]) {
  int sockfd;
  int arg2Parsed=atoi(argv[2]);
  if (create_server(argv[1],arg2Parsed, &sockfd) < 0) {
    fprintf(stderr, "oh no\n");
    return -1;
  } else {
    printf("Server listening on %s : %d \n",argv[1],arg2Parsed);
  }
  int numChildren=0;
  pid_t childPids[20]; /*A list of all child PIDs */
  while (1) {
    exitManager(childPids,numChildren,sockfd); //checking and handling exit requests from child processes
    int clientfd;
    int pid;
    accept_connection(sockfd, &clientfd);
    if ((pid=fork())==0) {
	serveClient(clientfd);
	return 10;
    }
    childPids[numChildren]=pid;
    numChildren++;
  }
  return 0;
}

