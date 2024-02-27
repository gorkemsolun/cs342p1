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

void removeConnections(int mq, int cs, int sc, char* csPipeName, char* scPipeName) {
     close(sc);
     close(cs);
     unlink(scPipeName);
     unlink(csPipeName);
     remove(scPipeName);
     remove(csPipeName);
     mq_close(mq);
}

int main(int argc, char* argv[]) {
     /*
     printf("%d\n", argc);
     for (int i = 0; i < argc; i++) {
          printf("argv[%d]: %s\n", i, argv[i]);
     }
     */

     if (argc != 6 && argc != 4 && argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     int wsize;
     int comfileIndex = -1;// TODO
     int wsizeIndex = -1;
     char comfile[NAME_SIZE];

     for (int i = 1; i < argc; i++) {
          if (strcmp(argv[i], "-b") == 0 && i + 1 < argc) {
               comfileIndex = i + 1;
          } else if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
               wsizeIndex = i + 1;
          }
     }

     if (comfileIndex != -1) {
          strcpy(comfile, argv[comfileIndex]);
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
     int clientID = getpid();

     sprintf(msgBuffer, "Connection request from %d.", clientID);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
     printf("%s\n", msgBuffer);

     //creation of pipes
     char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];
     char pipeBuffer[BUFFER_SIZE], command[BUFFER_SIZE];

     sprintf(csPipeName, "cs%d", clientID);
     sprintf(scPipeName, "sc%d", clientID);
     int cs = mkfifo(csPipeName, 0666);
     int sc = mkfifo(scPipeName, 0666);

     sprintf(msgBuffer, "%d %s %s %d", clientID, csPipeName, scPipeName, wsize);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     sc = open(scPipeName, O_RDONLY);
     cs = open(csPipeName, O_WRONLY);

     for(int i = sizeof(pipeBuffer); i > 0; i -= wsize)
        read(sc, pipeBuffer, wsize); // TODO we need a loop here
     if (atoi(pipeBuffer) == clientID) {
          printf("Connection is succesfully established through pipes.\n");
     } else {
          printf("Connection is not succesfully established. Server got wrong id. Servers client id: %s, clients id: %d\n", pipeBuffer, clientID);
          return 0;
     }

     // if batch option is specified
     int batchFile;
     char* batchLine;
     if (comfileIndex != -1) {
          batchFile = open(comfile, O_RDWR, 0666);
          char batchFileBuffer[FILE_SIZE];
          read(batchFile, batchFileBuffer, BUFFER_SIZE);
          batchLine = strtok(batchFileBuffer, "\n");
     }

     while (1) {
          if (comfileIndex == -1) {
               printf("Please give a line of command to execute: \n");
               fgets(pipeBuffer, BUFFER_SIZE, stdin);
          } else {
               if (batchLine != NULL) {
                    strcpy(pipeBuffer, batchLine);
                    batchLine = strtok(NULL, "\n");
               } else {
                    printf("Batch file is finished.\n");
                    break;
               }
          }

          strcpy(command, pipeBuffer);
          write(cs, pipeBuffer, BUFFER_SIZE);
          trimString(pipeBuffer);

          if (strcmp(pipeBuffer, "quit") == 0) {
               break;
          }

          read(sc, pipeBuffer, wsize); // TODO wsize loop needed
          trimString(pipeBuffer);
          printf("Following command is executed: %s\n", command);
          printf("%s\n", pipeBuffer);
     }

     removeConnections(mq, cs, sc, csPipeName, scPipeName);
     return 0;
}
