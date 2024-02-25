#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unistd.h>
#include <string.h>
#include "Message.h"

int main(int argc, char* argv[]) {
     if (argc != 2) {
          printf("Please provide proper amount of arguments.");
          return 0;
     }

     int i, readSize;
     mqd_t mq;
     struct mq_attr attr;
     char buffer[1024];

     attr.mq_flags = 0;
     attr.mq_maxmsg = 10;
     attr.mq_msgsize = 1024;
     attr.mq_curmsgs = 0;

     char mq_name[256];
     strcpy(mq_name, "/");
     strcat(mq_name, argv[1]);

     mq = mq_open(mq_name, O_CREAT | O_RDWR, 0666, &attr);
     if (mq == -1) {
          perror("mq_open");
          exit(1);
     }

     while (1) {

          while (strcmp(buffer, "Let me connect") != 0) {
               if (mq_receive(mq, buffer, 1024, NULL) == -1) {
                    perror("mq_receive");
                    exit(1);
               }
               sleep(1);
          }
          printf("Received connection: %s\n", buffer);

          strcpy(buffer, "You are connected");
          if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
               perror("mq_send");
               exit(1);
          }

          while (strncmp(buffer, mq_name, strlen(mq_name)) != 0) {
               if (mq_receive(mq, buffer, 1024, NULL) == -1) {
                    perror("mq_receive");
                    exit(1);
               }
          }
          printf("Received cs, sc pipes: %s\n", buffer);

          //parse the message, ID of client, name of the pipes created by client, wsize

          char* token;
          token = strtok(buffer, " ");
          pid_t clientID;
          char csPipeName[256], scPipeName[256];
          int wsize, cs, sc;
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
               printf("%s\n", token);
               token = strtok(NULL, " ");
          }

          // create server child with fork() TODO


          pid_t n;
          n = fork();

          if (n < 0) {
               fprintf(stderr, "fork Failed");
               exit(-1);
          } else if (n == 0) {
               // TODO there may be a while loop to restrain the child
               printf("child exec\n");

               while (1) {
                    sc = open(scPipeName, O_RDONLY);
                    if (sc == -1) {
                         perror("Error opening sc pipe for reading");
                    }

                    readSize = read(sc, buffer, 1024);
                    buffer[readSize] = '\0';
                    printf("received over sc pipe: %s\n", buffer);

                    strcpy(buffer, "result");
                    cs = open(csPipeName, O_WRONLY);
                    if (cs == -1) {
                         perror("Error opening cs pipe for writing");
                    }
                    
                    write(cs, buffer, 1024);

                    return 0;// TODO make loop end
               }
               return 0;

          }
     }


     if (mq_close(mq) == -1) {
          perror("mq_close");
          exit(1);
     }

     return 0;
}