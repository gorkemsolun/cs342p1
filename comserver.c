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

int main(int argc, char* argv[]) {
     if (argc != 2) {
          printf("Please provide proper amount of arguments.");
          return 0;
     }

     int i, readSize, wsize, cs, sc, file, rd, wr;
     mqd_t mq;
     pid_t n, nn, clientID;
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

          // parse the message, ID of client, name of the pipes created by client, wsize

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

          // TODO create server child with fork()

          n = fork();

          if (n == 0) { // child
               sc = open(scPipeName, O_RDONLY);
               cs = open(csPipeName, O_WRONLY);

               sprintf(fileName, "%d.txt", clientID);
               file = open(fileName, O_RDWR | O_CREAT, 0666);

               while (1) {
                    strcpy(buffer, "server is connected to pipes");
                    write(cs, buffer, 1024);

                    readSize = read(sc, buffer, 1024); // parsing of the commands should be changed to accomadate arguments
                    buffer[readSize] = '\0';
                    printf("%s\n", buffer);

                    i = 0;
                    token = strtok(buffer, " ");
                    while (token != NULL) {
                         if (token[strlen(token) - 1] == '\n') {
                              token[strlen(token) - 1] = '\0';
                         }
                         args[i++] = token;
                         token = strtok(NULL, " ");
                    }
                    args[i] = NULL;

                    nn = fork();

                    if (nn == 0) { // grandchild
                         dup2(file, 1); // redirect output
                         dup2(file, 2);

                         if (execvp(args[0], args) == -1) {
                              perror("execvp");
                              exit(1);
                         }

                    }
                    wait(NULL);

                    file = open(fileName, O_RDWR, 0666);
                    rd = read(file, buffer, sizeof(buffer));
                    wr = write(cs, buffer, sizeof(buffer));

                    /*
                    // Read from the file and write to the pipe
                    while ((rd = read(file, buffer, sizeof(buffer))) > 0) {
                         if (write(cs, buffer, sizeof(buffer)) != rd) {
                              perror("write");
                              exit(1);
                         }
                         printf("%s", buffer);
                    }
                    */


                    break;
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