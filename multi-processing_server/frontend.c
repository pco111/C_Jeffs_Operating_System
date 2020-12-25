#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include "a1_lib.h"

#define BUFSIZE   1024
struct MessageStruct {
   char  functionName[50];
   char  param1[50];
   char  param2[100];
};

int main(int argc, char *argv[]) {
  int sockfd;
  char user_input[BUFSIZE] = { 0 };
  char server_msg[BUFSIZE] = { 0 };
  int arg2Parsed=atoi(argv[2]);
  if (connect_to_server(argv[1],arg2Parsed, &sockfd) < 0) {
    fprintf(stderr, "oh no\n");
    return -1;
  }
  while (strcmp(user_input, "quit\n") || strcmp(user_input, "shutdown\n") || strcmp(user_input, "exit\n")) {
    memset(user_input, 0, sizeof(user_input));
    memset(server_msg, 0, sizeof(server_msg));
    //prompt user
    printf(">> ");
    // read user input from command line
    void* keeper=fgets(user_input, BUFSIZE, stdin);
    //parsing user input
    char* functionName;
    char* param1;
    char* param2;
    functionName=strtok(user_input," \n");
    param1=strtok(NULL," ");
    param2=strtok(NULL," ");
    MessageStruct oneMessage;
    strcpy( oneMessage.functionName, functionName);
    if(param1) strcpy( oneMessage.param1, param1);
    if(param2) strcpy( oneMessage.param2, param2);
    // send the input to server
    send_message(sockfd,(char *)&oneMessage, sizeof(oneMessage));
    // receive a msg from the server
    ssize_t byte_count = recv_message(sockfd, server_msg, sizeof(server_msg));
    if (byte_count <= 0) {
      break;
    }
    printf("Server: %s\n", server_msg);
    fflush(stdout);
  }
  close(sockfd);
  exit(0);
}

