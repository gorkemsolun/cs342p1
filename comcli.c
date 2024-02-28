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
#include "constant.h"

char csPipeName[NAME_SIZE], scPipeName[NAME_SIZE];
int mq, sc, cs;

void trimString(char* str) {
     int end = strlen(str) - 1;

     while (str[end] == ' ' || str[end] == '\t' || str[end] == '\n') {
          end--;
     }

     str[end + 1] = '\0';
}

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

void addHeader2Message(char* message, int length, int type) {
     memmove(message + 8, message, length); // shift the message to the right by 8
     memset(message, 0, 8); // fill the first 8 bytes with 0
     message[3] = length >> 24;
     message[2] = length >> 16;
     message[1] = length >> 8;
     message[0] = length;
     message[4] = type; // last 3 bytes are padding
}

int overflowCheck4Header(int value) {
     if (value < 0) {
          value += 256;
     }
     return value;
}

void removeHeaderFromMessage(char* message, int* length, int* type) {
     *length = overflowCheck4Header(message[0]) + overflowCheck4Header(message[1] << 8)
          + overflowCheck4Header(message[2] << 16) + overflowCheck4Header(message[3] << 24);
     *type = message[4];

     memmove(message, message + 8, *length);
     message[*length] = '\0';
}

int main(int argc, char* argv[]) {
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

     if (wsize < 1) {
          printf("Wsize should be at least 1.\n");
          return 0;
     }

     char mqName[NAME_SIZE];
     strcpy(mqName, argv[1]);
     trimString(mqName);
     mq = mq_open(mqName, O_RDWR);

     struct mq_attr attr;
     mq_getattr(mq, &attr);
     int MSG_SIZE = attr.mq_msgsize;
     char msgBuffer[MSG_SIZE];
     int bufferLength, bufferType;

     int clientID = getpid();

     sprintf(msgBuffer, "Connection request from %d.", clientID);
     addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REQUEST);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     mq_receive(mq, msgBuffer, MSG_SIZE, NULL);
     removeHeaderFromMessage(msgBuffer, &bufferLength, &bufferType);
     if (bufferType == CONNECTION_REPLY_FAIL) {
          printf("Connection is not succesfully established. Server is full.\n");
          printf("%s\n", msgBuffer);
          return 0;
     }
     printf("Connection is succesfully established.\n%s\n", msgBuffer);

     //creation of pipes
     char pipeBuffer[BUFFER_SIZE], command[BUFFER_SIZE];

     sprintf(csPipeName, "cs%d", clientID);
     sprintf(scPipeName, "sc%d", clientID);
     cs = mkfifo(csPipeName, 0666);
     sc = mkfifo(scPipeName, 0666);

     sprintf(msgBuffer, "%d %s %s %d", clientID, csPipeName, scPipeName, wsize);
     addHeader2Message(msgBuffer, strlen(msgBuffer), CONNECTION_REGISTER);
     mq_send(mq, msgBuffer, MSG_SIZE, 0);

     sc = open(scPipeName, O_RDONLY);
     cs = open(csPipeName, O_WRONLY);
     for (int i = sizeof(pipeBuffer); i > 0; i -= wsize) {
          read(sc, pipeBuffer, wsize);
     }
     removeHeaderFromMessage(pipeBuffer, &bufferLength, &bufferType);

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

     //signal specification
     signal(SIGTERM, signal_handler);

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
          addHeader2Message(pipeBuffer, strlen(pipeBuffer), COMMAND_LINE_REQUEST);
          write(cs, pipeBuffer, BUFFER_SIZE);
          trimString(pipeBuffer);

          if (strcmp(pipeBuffer, "quit") == 0) {
               break;
          }
          for (int i = sizeof(pipeBuffer); i > 0; i -= wsize) {
               read(sc, pipeBuffer, wsize);
          }
          removeHeaderFromMessage(pipeBuffer, &bufferLength, &bufferType);

          trimString(pipeBuffer);
          printf("Following command is executed: %s\n", command);
          printf("%s\n", pipeBuffer);
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
