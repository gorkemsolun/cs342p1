#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "constant.h"

void trimString(char* str) {
     int end = strlen(str) - 1;

     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }

     str[end + 1] = '\0';
}

int main(int argc, char* argv[]) {
     /*
     printf("%d\n", argc);
     for (int i = 0; i < argc; i++) {
          printf("argv[%d]: %s\n", i, argv[i]);
     }
     */

     if (argc != 6 && argc != 4) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     int wsize;
     int comfileIndex = -1;// TODO
     int wsizeIndex = -1;

     for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
               comfileIndex = i + 1;
          } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
               wsizeIndex = i + 1;
          }
     }

     if (comfileIndex != -1) {
          // Handle the COMFILE option
          // char* comfile = argv[comfileIndex];
          // Do something with comfile...
     }

     if (wsizeIndex != -1) {
          wsize = atoi(argv[wsizeIndex]);
     } else {
          wsize = WSIZE_DEFAULT;
     }

     char msgBuffer[MSG_SIZE], mqName[NAME_SIZE];

     strcpy(mqName, "/");
     strcat(mqName, argv[1]);

     int mq = mq_open(mqName, O_RDWR);

     strcpy(msgBuffer, "Let me connect");
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     mq_receive(mq, msgBuffer, MSG_SIZE, NULL);

     printf("Received success of connection: %s\n", msgBuffer);

     //creation of pipes
     int clientID = getpid();
     char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];
     char pipeBuffer[wsize];

     sprintf(csPipeName, "cs%d", clientID);
     sprintf(scPipeName, "sc%d", clientID);
     int cs = mkfifo(csPipeName, 0666);
     int sc = mkfifo(scPipeName, 0666);


     sprintf(msgBuffer, "%s %d %s %s %d", mqName, clientID, csPipeName, scPipeName, wsize);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     sc = open(scPipeName, O_WRONLY);
     cs = open(csPipeName, O_RDONLY);

     read(cs, pipeBuffer, wsize);

     trimString(pipeBuffer);

     printf("%s\n", pipeBuffer);

     while (1) {
          printf("Please give a line of command to execute: \n");
          fgets(pipeBuffer, wsize, stdin);
          write(sc, pipeBuffer, wsize);
          trimString(pipeBuffer);

          if (strcmp(pipeBuffer, "quit") == 0) {
               break;
          }

          read(cs, pipeBuffer, wsize);

          trimString(pipeBuffer);

          printf("%s\n", pipeBuffer);
     }

     close(sc);
     close(cs);
     unlink(scPipeName);
     unlink(csPipeName);

     mq_close(mq);
     return 0;
}