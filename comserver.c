#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <signal.h>
#include <sys/msg.h> 
#include "constant.h"

int childrenID[MAX_CLIENT_SIZE] = { [0 ... MAX_CLIENT_SIZE - 1] = -1 };
int clientsID[MAX_CLIENT_SIZE] = { [0 ... MAX_CLIENT_SIZE - 1] = -1 };
int currentChildrenCount = 0;
int mq;
int serverID;

void trimString(char* str) {
     int end = strlen(str) - 1;

     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }

     str[end + 1] = '\0';
}

void signal_handler(int sig) {
     if (sig == SIGTERM) {
          if (getpid() == serverID) {
               msgctl(mq, IPC_RMID, NULL);

               for (int i = 0; clientsID[i] != -1 && i < MAX_CLIENT_SIZE; i++) {
                    kill(clientsID[i], SIGTERM);
                    char clientfileName[NAME_SIZE];
                    sprintf(clientfileName, "%d.txt", clientsID[i]);
                    remove(clientfileName);
               }

               for (int i = 0; childrenID[i] != -1 && i < MAX_CLIENT_SIZE; i++) {
                    kill(childrenID[i], SIGTERM);

               }
          }

          printf("Process %d received termination signal.\n", getpid());
          exit(0);
     }
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

     mq = mq_open(mqName, O_CREAT | O_RDWR, 0666, &attr);

     serverID = getpid();
     signal(SIGTERM, signal_handler);

     printf("Comserver is started. Connect to it via clients.\n");

     while (1) {
          // connection should be refused if children count > 0;
          if (currentChildrenCount == MAX_CLIENT_SIZE) {
               sleep(30);
               printf("Server is full, clients cannot connect.\n");
               continue;
          }

          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          printf("%s\n", msgBuffer);

          sprintf(msgBuffer, "Connection established with %d.", serverID);
          mq_send(mq, msgBuffer, MSG_SIZE, 0);

          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          printf("Received pipe names, clients ID, and wsize.\n");

          char* token = strtok(msgBuffer, " ");
          int i = 0;
          int clientID, wsize;
          char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];

          while (token != NULL) {
               if (i == 0) {
                    clientID = atoi(token);
               } else if (i == 1) {
                    strcpy(csPipeName, token);
               } else if (i == 2) {
                    strcpy(scPipeName, token);
               } else {
                    wsize = atoi(token);
               }
               i++;
               token = strtok(NULL, " ");
          }

          int n = fork();

          // id registration
          int childIndex = -1;
          for (int i = 0; i < MAX_CLIENT_SIZE; i++) {
               if (childrenID[i] == -1) {
                    childIndex = i;
                    break;
               }
          }

          if (childIndex != -1) {
               childrenID[childIndex] = n;
               clientsID[childIndex] = clientID;
          }

          if (n == 0) { // child
               int sc = open(scPipeName, O_WRONLY);
               int cs = open(csPipeName, O_RDONLY);

               char fileName[NAME_SIZE];
               sprintf(fileName, "%d.txt", clientID);

               char pipeBuffer[BUFFER_SIZE];
               sprintf(pipeBuffer, "%d", clientID);
              for(int i = sizeof(pipeBuffer); i > 0; i -= wsize)
                write(sc, pipeBuffer, wsize); // TODO WSIZE LOOP NEEDED

               while (1) {
                    int file = open(fileName, O_RDWR | O_CREAT, 0666);
                    read(cs, pipeBuffer, BUFFER_SIZE);
                    trimString(pipeBuffer);
                    printf("Command given by %d is %s\n", clientID, pipeBuffer);

                    if (strcmp(pipeBuffer, "quitall") == 0) { // killing all
                         kill(serverID, SIGTERM);
                         exit(0);
                    }

                    if (strcmp(pipeBuffer, "quit") == 0) { // ending this child
                         close(file);
                         remove(fileName);
                         printf("Client %d quits.\n", clientID);
                         break;
                    }

                    char* isCompoundCommand = strchr(pipeBuffer, '|');

                    if (isCompoundCommand == NULL) { // single command
                         char* args[MAX_ARGS_LENGTH];
                         int i = 0;

                         char* token = strtok(pipeBuffer, " ");
                         while (token != NULL) {
                              args[i++] = token;
                              token = strtok(NULL, " ");
                         }
                         args[i] = NULL;

                         int nn = fork();

                         if (nn == 0) { // grandchild
                              dup2(file, STDOUT_FILENO); // redirect output
                              execvp(args[0], args);
                         }

                         waitpid(nn, NULL, 0);
                    } else { // compound command
                         char c1[BUFFER_SIZE], c2[BUFFER_SIZE];
                         char* token = strtok(pipeBuffer, "|");
                         int i = 0;
                         while (token != NULL) {
                              if (i == 0) {
                                   strcpy(c1, token);
                              } else {
                                   strcpy(c2, token);
                              }
                              i++;
                              token = strtok(NULL, "|");
                         }

                         int unnamedPipes[2];
                         pipe(unnamedPipes);

                         int n1 = fork();

                         if (n1 == 0) {
                              dup2(unnamedPipes[1], STDOUT_FILENO);
                              close(unnamedPipes[0]);
                              close(unnamedPipes[1]);

                              char* args1[BUFFER_SIZE];
                              char* token = strtok(c1, " ");
                              int i = 0;
                              while (token != NULL) {
                                   args1[i++] = token;
                                   token = strtok(NULL, " ");
                              }
                              args1[i] = NULL;

                              execvp(args1[0], args1);
                         } else {

                              int n2 = fork();

                              if (n2 == 0) {
                                   char* args2[BUFFER_SIZE];
                                   char* token = strtok(c2, " ");
                                   int i = 0;
                                   while (token != NULL) {
                                        args2[i++] = token;
                                        token = strtok(NULL, " ");
                                   }
                                   args2[i] = NULL;

                                   dup2(unnamedPipes[0], STDIN_FILENO);
                                   dup2(file, STDOUT_FILENO);
                                   close(unnamedPipes[0]);
                                   close(unnamedPipes[1]);

                                   execvp(args2[0], args2);
                              } else {
                                   close(unnamedPipes[0]);
                                   close(unnamedPipes[1]);

                                   waitpid(n1, NULL, 0);
                                   waitpid(n2, NULL, 0);
                              }
                         }
                    }

                    file = open(fileName, O_RDONLY);
                    memset(pipeBuffer, 0, BUFFER_SIZE);
                    read(file, pipeBuffer, BUFFER_SIZE);
                    close(file);
                    remove(fileName);
                    write(sc, pipeBuffer, wsize);
               }

               close(cs);
               close(sc);
               unlink(scPipeName);
               unlink(csPipeName);
               return 0; // end the child
          }

          /* TODO THERE WiLL BE A FiLE OR PiPE TO COMMUNiCATE WiTH parent to make this possible
          childrenID[childIndex] = -1;
          clientsID[childIndex] = -1;
          currentChildrenCount--;
          */

     }

     mq_close(mq);
     return 0;
}
