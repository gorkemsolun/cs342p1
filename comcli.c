/*
Görkem Kadir Solun 22003214
Murat Çağrı Kara 22102505
*/

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/msg.h>
#include <time.h>
#include "constant.h"

// Global variables
char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];
int mq, sc, cs;

// trim the string from the end
void trimString(char* str) {
     int end = strlen(str) - 1;
     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }
     str[end + 1] = '\0';
}

// signal handler
void signal_handler(int sig) {
     if (sig == SIGTERM) {
          printf("Process %d received termination signal.\n", getpid());
          close(sc);
          close(cs);
          unlink(scPipeName);
          unlink(csPipeName);
          remove(scPipeName);
          remove(csPipeName);
          mq_close(mq);
          exit(0);
     }
}

// add header to the message
void addHeader2Message(char* message, int length, int type) {
     memmove(message + 8, message, length); // shift the message to the right by 8
     memset(message, 0, 8); // fill the first 8 bytes with 0
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

// remove header from the message
void removeHeaderFromMessage(char* message, int* length, int* type) {
     *length = overflowCheck4Header(message[0]) + (overflowCheck4Header(message[1]) << 8)
          + (overflowCheck4Header(message[2]) << 16) + (overflowCheck4Header(message[3]) << 24);
     *type = message[4];

     memmove(message, message + 8, *length);
     message[*length] = '\0';
}

int main(int argc, char* argv[]) {
     if (argc != 6 && argc != 4 && argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     // parsing the arguments
     int wsize;
     int comfileIndex = -1;
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
     // check for the wsize
     if (wsize < 1) {
          printf("Wsize should be at least 1.\n");
          return 0;
     }

     // open the message queue
     char mqName[NAME_SIZE];
     strcpy(mqName, argv[1]);
     trimString(mqName);
     mq = mq_open(mqName, O_RDWR);

     // prepare the message queue
     struct mq_attr attr;
     mq_getattr(mq, &attr);
     int MSG_SIZE = attr.mq_msgsize;
     char msgBuffer[MSG_SIZE];
     int bufferLength, bufferType;

     int clientID = getpid();

     // send connection request
     sprintf(msgBuffer, "Connection request from %d.", clientID);
     addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REQUEST);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     // receive connection reply
     mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
     removeHeaderFromMessage(msgBuffer, &bufferLength, &bufferType);
     if (bufferType == CONNECTION_REPLY_FAIL) {
          printf("Connection is not succesfully established. Server is full (code %d).\n", bufferType);
          printf("%s\n", msgBuffer);
          return 0;
     }
     printf("Connection is succesfully established (code %d).\n%s\n", bufferType, msgBuffer);

     //creation of pipes
     char pipeBuffer[BUFFER_SIZE], command[BUFFER_SIZE];
     sprintf(csPipeName, "cs%d", clientID);
     sprintf(scPipeName, "sc%d", clientID);
     cs = mkfifo(csPipeName, 0666);
     sc = mkfifo(scPipeName, 0666);

     // send connection register
     sprintf(msgBuffer, "%d %s %s %d", clientID, csPipeName, scPipeName, wsize);
     addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REGISTER);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     // open the pipes
     sc = open(scPipeName, O_RDONLY);
     cs = open(csPipeName, O_WRONLY);

     // receive connection reply from the server, check if the connection is established
     // note that this is only the succes or fail message, not the actual command line result
     read(sc, pipeBuffer, MSG_SIZE); // this is just to check if the connection is successful, we read the client id which server got to confirm the connection
     removeHeaderFromMessage(pipeBuffer, &bufferLength, &bufferType);
     if (atoi(pipeBuffer) == clientID) {
          printf("Connection is succesfully established through pipes. (code %d)\n", bufferType);
     } else {
          printf("Connection is not succesfully established. Server got wrong id. Servers client id: %s, clients id: %d\n", pipeBuffer, clientID);
          return 0;
     }

     // start the timer for the batch file
     struct timespec start, end;
     clock_gettime(CLOCK_MONOTONIC, &start);

     // if batch option is specified
     int batchFile;
     char* batchLine;
     if (comfileIndex != -1) {
          batchFile = open(comfile, O_RDWR, 0666);
          char batchFileBuffer[FILE_SIZE];
          read(batchFile, batchFileBuffer, BUFFER_SIZE);
          batchLine = strtok(batchFileBuffer, "\n");
     }

     //signal specification
     signal(SIGTERM, signal_handler);

     // client loop
     while (1) {
          // get the command
          if (comfileIndex == -1) {
               printf("Please give a line of command to execute: \n");
               fgets(pipeBuffer, BUFFER_SIZE, stdin);
          } // if batch file is specified, read from the file
          else {
               if (batchLine != NULL) {
                    strcpy(pipeBuffer, batchLine);
                    batchLine = strtok(NULL, "\n");
               } else {
                    printf("Batch file is finished.\n");
                    strcpy(pipeBuffer, "quit"); // if the batch file is finished, quit the client with quit command
               }
          }
          trimString(pipeBuffer);
          strcpy(command, pipeBuffer);

          // send the command to the server 
          if (strcmp(command, "quit") == 0) {
               addHeader2Message(pipeBuffer, strlen(pipeBuffer), QUIT_REQUEST);
          } else if (strcmp(command, "quitall") == 0) {
               addHeader2Message(pipeBuffer, strlen(pipeBuffer), QUIT_ALL_REQUEST);
          } else {
               addHeader2Message(pipeBuffer, strlen(pipeBuffer), COMMAND_LINE);
          }

          write(cs, pipeBuffer, BUFFER_SIZE); // send the command to the server
          printf("Following command is going to be executed: %s\n", command);

          if (strcmp(command, "quit") == 0) { // if the command is quit, break the loop
               printf("Following command is executed: %s\n", command);
               break;
          }

          // receive the result from the server
          read(sc, pipeBuffer, 8); // read the header of the message to get the length and type of the message
          printf("Following command is executed: %s\n", command);
          bufferLength = overflowCheck4Header(pipeBuffer[0]) + (overflowCheck4Header(pipeBuffer[1]) << 8)
               + (overflowCheck4Header(pipeBuffer[2]) << 16) + (overflowCheck4Header(pipeBuffer[3]) << 24);
          bufferType = pipeBuffer[4];

          // check if the server returned the correct type of message, this may be redundant
          if (bufferType != COMMAND_LINE_RESULT) { // this may be redundant
               printf("Server returned wrong type of message. Expected: %d, got: %d\n", COMMAND_LINE_RESULT, bufferType);
               return 0;
          }

          // controlling the buffer length to prevent buffer overflow
          if (bufferLength >= BUFFER_SIZE - 8 || bufferLength < 0) {
               printf("Server returned too long message. Expected: %d, got: %d\n", BUFFER_SIZE - 8, bufferLength);
               printf("Length: %d\n", bufferLength);
               printf("This may break the program.\n");
               printf("This may break the program.\n");
               printf("This may break the program.\n");
          }

          // read the result from the server
          while (bufferLength > 0) {
               int wsize1 = bufferLength > wsize ? wsize : bufferLength;
               if (wsize1 == 0) {
                    break;
               }
               char temp[wsize1];
               read(sc, temp, wsize1);
               temp[wsize1] = '\0';
               printf("%s", temp);
               bufferLength -= wsize1;
          }
          printf("\n"); // print a new line after the result
     } // end of client loop

     // end the timer for the batch file and print the elapsed time
     clock_gettime(CLOCK_MONOTONIC, &end);
     float elapsedTime = (float)(end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1000000000.0);
     if (elapsedTime > 0) {
          printf("Elapsed time: %f\n", elapsedTime);
     }

     close(sc);
     close(cs);
     unlink(scPipeName);
     unlink(csPipeName);
     remove(scPipeName);
     remove(csPipeName);
     mq_close(mq);
     return 0;
}
