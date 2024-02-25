#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
#include <string.h>
#include "Message.h"

void trimString(char* str) {
     int end = strlen(str) - 1;

     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }

     str[end + 1] = '\0';
}

int main(int argc, char* argv[]) {
     if (argc != 2) {
          printf("Please provide proper amount of arguments.");
          return 0;
     }

     int i, readSize, wsize, cs, sc, file, rd, wr, n, nn, clientID, mq;
     struct mq_attr attr;
     char mq_name[256], csPipeName[256], scPipeName[256], buffer[1024], * token, * args[] = { NULL }, fileName[256];

     attr.mq_flags = 0;
     attr.mq_maxmsg = 10;
     attr.mq_msgsize = 1024;
     attr.mq_curmsgs = 0;

     strcpy(mq_name, "/");
     strcat(mq_name, argv[1]);

     mq = mq_open(mq_name, O_CREAT | O_RDWR, 0666, &attr);

     while (1) {
          mq_receive(mq, buffer, 1024, NULL);
          printf("Received connection: %s\n", buffer);

          strcpy(buffer, "You are connected");
          mq_send(mq, buffer, strlen(buffer), 0);

          mq_receive(mq, buffer, 1024, NULL);
          printf("Received cs, sc pipes: %s\n", buffer);

          token = strtok(buffer, " ");
          i = 0;

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

          n = fork();

          if (n == 0) { // child
               sc = open(scPipeName, O_RDONLY);
               cs = open(csPipeName, O_WRONLY);

               sprintf(fileName, "%d.txt", clientID);
               file = open(fileName, O_RDWR | O_CREAT, 0666);

               sprintf(buffer, "server is connected to pipes of %s", buffer);
               write(cs, buffer, strlen(buffer) + 1);

               while (1) {
                    readSize = read(sc, buffer, 1024); // parsing of the commands should be changed to accomadate arguments
                    buffer[readSize] = '\0';
                    printf("Command given by %d is: %s\n", clientID, buffer);
                    trimString(buffer);

                    if (strcmp(buffer, "quit") == 0) {
                         printf("Client %d quits.\n", clientID);
                         break;
                    }

                    i = 0;
                    token = strtok(buffer, " ");
                    while (token != NULL) {
                         trimString(token);
                         args[i++] = token;
                         token = strtok(NULL, " ");
                    }
                    args[i] = NULL;

                    nn = fork();

                    if (nn == 0) { // grandchild
                         dup2(file, 1); // redirect output
                         dup2(file, 2);

                         execvp(args[0], args);
                    }

                    wait(NULL);

                    file = open(fileName, O_RDWR, 0666);
                    rd = read(file, buffer, sizeof(buffer));
                    wr = write(cs, buffer, sizeof(buffer));
               }

               close(file);
               close(cs);
               close(sc);
               unlink(scPipeName);
               unlink(csPipeName);
               return 0;
          }
     }


     if (mq_close(mq) == -1) {
          perror("mq_close");
          exit(1);
     }

     return 0;
}