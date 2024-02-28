/*
Görkem Kadir Solun 22003214
Murat Çağrı Kara 22102505
*/

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

// Global variables
int childrenID[MAX_CLIENT_SIZE] = { [0 ... MAX_CLIENT_SIZE - 1] = -1 };
int clientsID[MAX_CLIENT_SIZE] = { [0 ... MAX_CLIENT_SIZE - 1] = -1 };
int currentChildrenCount = 0;
int mq;
int serverID;

// trim the string from the end
void trimString(char* str) {
     int end = strlen(str) - 1;
     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }
     str[end + 1] = '\0';
}

void signal_handler(int sig) {
     if (sig == SIGTERM) {
          // remove the message queue and the files of the clients and children if the server is terminated
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

               remove(CLIENT_REMOVE_FILE_NAME);
          }

          printf("Process %d received termination signal.\n", getpid());
          exit(0);
     }
}

// add header to the message
void addHeader2Message(char* message, int length, int type) {
     memmove(message + 8, message, length); // shift the message to the right by 8
     memset(message, 0, 8);
     message[3] = length >> 24;
     message[2] = length >> 16;
     message[1] = length >> 8;
     message[0] = length;
     message[4] = type; // last 3 bytes are padding
}

// check for overflow in the header values, char can hold -128 to 127
int overflowCheck4Header(int value) {
     if (value < 0) {
          value += 256;
     }
     return value;
}

// remove the header from the message
void removeHeaderFromMessage(char* message, int* length, int* type) {
     *length = overflowCheck4Header(message[0]) + (overflowCheck4Header(message[1]) << 8)
          + (overflowCheck4Header(message[2]) << 16) + (overflowCheck4Header(message[3]) << 24);
     *type = message[4];

     memmove(message, message + 8, *length);
     message[*length] = '\0';
}

int main(int argc, char* argv[]) {
     // check for the proper amount of arguments
     if (argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }
     if (argv[1][0] != '/') {
          printf("Please provide a proper message queue name.\n");
          return 0;
     }

     // open the message queue
     char mqName[NAME_SIZE];
     strcpy(mqName, argv[1]);
     trimString(mqName);
     mq = mq_open(mqName, O_CREAT | O_RDWR, 0666, NULL);

     // get the message queue attributes
     struct mq_attr attr;
     mq_getattr(mq, &attr);
     int MSG_SIZE = attr.mq_msgsize;
     char msgBuffer[MSG_SIZE];
     int bufferLength, bufferType;

     serverID = getpid();
     printf("Comserver(%d) is started. Connect to it via clients. Message queue name is %s.\n", serverID, mqName);

     signal(SIGTERM, signal_handler); // signal handler for termination

     // server loop
     while (1) {

          // receiving the connection request
          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          removeHeaderFromMessage(msgBuffer, &bufferLength, &bufferType);
          if (bufferType != CONNECTION_REQUEST) {
               printf("Received message is not a connection request(code %d).\n", bufferType);
               continue;
          }
          printf("Received connection request(code %d).\n", bufferType);
          printf("%s\n", msgBuffer);

          // remove clients from the list of clients to be removed
          int clientRemoveFile = open(CLIENT_REMOVE_FILE_NAME, O_RDONLY);
          if (clientRemoveFile != -1) {
               char clientToRemove[NAME_SIZE];
               int clientIDToRemove;
               while (read(clientRemoveFile, clientToRemove, NAME_SIZE) > 0) {
                    clientIDToRemove = atoi(clientToRemove);
                    for (int i = 0; i < MAX_CLIENT_SIZE; i++) {
                         if (clientsID[i] == clientIDToRemove) {
                              childrenID[i] = -1;
                              clientsID[i] = -1;
                              currentChildrenCount--;
                         }
                    }
               }
               remove(CLIENT_REMOVE_FILE_NAME);
          }

          // Connection request handling
          if (currentChildrenCount == MAX_CLIENT_SIZE) {
               sprintf(msgBuffer, "Server(%d) is full, clients cannot connect.", serverID);
               addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REPLY_FAIL);
               mq_send(mq, msgBuffer, MSG_SIZE, 0);
               printf("Server is full, clients cannot connect. Try 30 seconds later.\n");
               sleep(30);
               continue;
          } else {
               sprintf(msgBuffer, "Connection established with %d (code %d).", serverID, CONNECTION_REPLY_SUCCESS);
               addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REPLY_SUCCESS);
               mq_send(mq, msgBuffer, MSG_SIZE, 0);
          }

          // receiving the registration message
          mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
          removeHeaderFromMessage(msgBuffer, &bufferLength, &bufferType);
          if (bufferType != CONNECTION_REGISTER) {
               printf("Received message is not a connection register (code %d).\n", bufferType);
               continue;
          }

          // parsing the registration message to get the clientID, csPipeName, scPipeName, and wsize
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

          printf("Received pipe names, clients ID(%d), and wsize(%d) (code %d).\n", clientID, wsize, bufferType);
          printf("Please be careful while entering commands, \nit may break the system if they are WRONG.\n");
          printf("Please be careful while entering commands long output, \nit may break the system if the buffer SIZE is NOT ENOUGH.\n");

          int n = fork();

          // id registration of children
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

          // child process
          if (n == 0) {
               // opening the pipes
               int sc = open(scPipeName, O_WRONLY);
               int cs = open(csPipeName, O_RDONLY);

               //prepare the file name
               char fileName[NAME_SIZE];
               sprintf(fileName, "%d.txt", clientID);

               // sending the connection success message to the client, note that this is only the succes message not the actual command line result
               char pipeBuffer[BUFFER_SIZE];
               sprintf(pipeBuffer, "%d", clientID); // this is just to check if the connection is successful, we send the client id to confirm the connection
               addHeader2Message(pipeBuffer, strlen(pipeBuffer), CONNECTION_REPLY_SUCCESS);
               write(sc, pipeBuffer, MSG_SIZE);

               // command handling, child process loop
               while (1) {
                    // prepare the result file
                    int file = open(fileName, O_RDWR | O_CREAT, 0666);

                    // read the command from the client
                    read(cs, pipeBuffer, BUFFER_SIZE);
                    removeHeaderFromMessage(pipeBuffer, &bufferLength, &bufferType);
                    trimString(pipeBuffer);
                    if (atoi(pipeBuffer) != clientID) { // This is for the case that the client receives quitall command from another client
                         printf("Command given by %d is %s(code %d)\n", clientID, pipeBuffer, bufferType);
                    }


                    // quit handling
                    if (bufferType == QUIT_REQUEST || bufferType == QUIT_ALL_REQUEST) {
                         if (bufferType == QUIT_ALL_REQUEST) { // killing all
                              kill(serverID, SIGTERM);
                              exit(0);
                         }

                         if (bufferType == QUIT_REQUEST) { // ending this child
                              close(file);
                              remove(fileName);
                              printf("Client %d quits.\n", clientID);
                              break;
                         }
                    }

                    // compound command handling
                    char* isCompoundCommand = strchr(pipeBuffer, '|');

                    // single command
                    if (isCompoundCommand == NULL) {
                         // split the command into arguments
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
                              dup2(file, STDOUT_FILENO); // write to the file
                              execvp(args[0], args); // execute the command
                         }

                         waitpid(nn, NULL, 0);
                    }
                    // compound command
                    else {
                         // split the compound command into two
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

                         if (n1 == 0) { // grandchild 1
                              dup2(unnamedPipes[1], STDOUT_FILENO); // write to the pipe
                              close(unnamedPipes[0]);
                              close(unnamedPipes[1]);

                              char* args1[MAX_ARGS_LENGTH];
                              char* token = strtok(c1, " ");
                              int i = 0;
                              while (token != NULL) {
                                   args1[i++] = token;
                                   token = strtok(NULL, " ");
                              }
                              args1[i] = NULL;

                              execvp(args1[0], args1); // execute the first command
                         } else {

                              int n2 = fork();

                              if (n2 == 0) { // grandchild 2
                                   char* args2[MAX_ARGS_LENGTH];
                                   char* token = strtok(c2, " ");
                                   int i = 0;
                                   while (token != NULL) {
                                        args2[i++] = token;
                                        token = strtok(NULL, " ");
                                   }
                                   args2[i] = NULL;

                                   dup2(unnamedPipes[0], STDIN_FILENO); // read from the pipe
                                   dup2(file, STDOUT_FILENO);        // write to the file
                                   close(unnamedPipes[0]);
                                   close(unnamedPipes[1]);

                                   execvp(args2[0], args2); // execute the second command
                              } else {
                                   close(unnamedPipes[0]);
                                   close(unnamedPipes[1]);

                                   waitpid(n1, NULL, 0);
                                   waitpid(n2, NULL, 0);
                              }
                         }
                    } // end of command handling

                    // read the result from the file
                    file = open(fileName, O_RDONLY);
                    read(file, pipeBuffer, BUFFER_SIZE);
                    bufferLength = strlen(pipeBuffer);
                    pipeBuffer[bufferLength] = '\0';
                    close(file);
                    remove(fileName);

                    // send the result to the client
                    addHeader2Message(pipeBuffer, strlen(pipeBuffer), COMMAND_LINE_RESULT);
                    write(sc, pipeBuffer, 8);// send the header first, to inform the client about the length of the message
                    int sent = 8;
                    while (bufferLength > 0) {
                         int wsize1 = bufferLength > wsize ? wsize : bufferLength;
                         bufferLength -= wsize1;
                         char temp[wsize1];
                         memccpy(temp, pipeBuffer + sent, wsize1, wsize1);
                         temp[wsize1] = '\0';
                         write(sc, temp, wsize1); // send the message in chunks of wsize
                         sent += wsize1;
                    } // end of sending the result to the client
               } // end of child process loop

               // add here clients to be removed
               int file = open(CLIENT_REMOVE_FILE_NAME, O_RDWR | O_CREAT | O_APPEND, 0666);
               lseek(file, 0, SEEK_END);
               dprintf(file, "%d\n", clientsID[childIndex]);
               close(file);

               // close the pipes
               close(cs);
               close(sc);
               unlink(scPipeName);
               unlink(csPipeName);
               return 0; // end the child
          } // end of child process
     } // end of server loop

     mq_close(mq);
     return 0;
}
