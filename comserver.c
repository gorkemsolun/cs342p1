#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
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
     if (argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     char mqName[NAME_SIZE];
     strcpy(mqName, "/");
     strcat(mqName, argv[1]);

     char msgBuffer[MSG_SIZE];
     struct mq_attr attr;

     attr.mq_flags = 0;
     attr.mq_maxmsg = 10;
     attr.mq_msgsize = 1024;
     attr.mq_curmsgs = 0;

     int mq = mq_open(mqName, O_CREAT | O_RDWR, 0666, &attr);

     while (1) {
          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          printf("Received connection: %s\n", msgBuffer);

          strcpy(msgBuffer, "You are connected");
          mq_send(mq, msgBuffer, MSG_SIZE, 0);

          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          printf("Received cs, sc pipes: %s\n", msgBuffer);

          char* token = strtok(msgBuffer, " ");
          int i = 0;
          int clientID, wsize;
          char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];

          while (token != NULL) {
               if (i == 0) {
               } else if (i == 1) {
                    clientID = atoi(token);
               } else if (i == 2) {
                    strcpy(csPipeName, token);
               } else if (i == 3) {
                    strcpy(scPipeName, token);
               } else {
                    wsize = atoi(token);
               }
               i++;
               token = strtok(NULL, " ");
          }

          int n = fork();

          if (n == 0) { // child
               int sc = open(scPipeName, O_RDONLY);
               int cs = open(csPipeName, O_WRONLY);
               char fileName[NAME_SIZE];
               char pipeBuffer[wsize];

               sprintf(fileName, "%d.txt", clientID);
               int file = open(fileName, O_RDWR | O_CREAT, 0666);

               sprintf(pipeBuffer, "Comserver is connected to pipes of %d", clientID);
               write(cs, pipeBuffer, wsize);

               while (1) {
                    read(sc, pipeBuffer, wsize); // parsing of the commands should be changed to accomadate arguments
                    trimString(pipeBuffer);
                    printf("Command given by %d is: %s\n", clientID, pipeBuffer);

                    if (strcmp(pipeBuffer, "quit") == 0) {
                         printf("Client %d quits.\n", clientID);
                         break;
                    }

                    int i = 0;
                    char* token = strtok(pipeBuffer, " ");
                    char* args[] = { NULL };

                    while (token != NULL) {
                         trimString(token);
                         args[i++] = token;
                         token = strtok(NULL, " ");
                    }
                    args[i] = NULL;

                    int nn = fork();

                    if (nn == 0) { // grandchild
                         dup2(file, 1); // redirect output
                         dup2(file, 2);

                         execvp(args[0], args);
                    }

                    wait(NULL);

                    file = open(fileName, O_RDWR, 0666);
                    read(file, pipeBuffer, wsize);
                    write(cs, pipeBuffer, wsize);
               }

               close(file);
               close(cs);
               close(sc);
               unlink(scPipeName);
               unlink(csPipeName);
               return 0; // end the child
          }
     }

     mq_close(mq);
     return 0;
}