#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
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
     // TODO this should be changed after implementaion of sc,cs pipes
     if (/* argc != 6 && argc != 4*/ argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     int readSize, i, mq, pid, cs, sc;
     char buffer[1024], mq_name[256], csPipeName[256], scPipeName[256];

     strcpy(mq_name, "/");
     strcat(mq_name, argv[1]);

     mq = mq_open(mq_name, O_RDWR);

     strcpy(buffer, "Let me connect");
     mq_send(mq, buffer, strlen(buffer), 0);

     mq_receive(mq, buffer, 1024, NULL);

     printf("Received success of connection: %s\n", buffer);

     //creation of pipes
     pid = getpid();
     sprintf(csPipeName, "cs%d", pid);
     sprintf(scPipeName, "sc%d", pid);
     cs = mkfifo(csPipeName, 0666);
     sc = mkfifo(scPipeName, 0666);


     sprintf(buffer, "%s %d %s %s 1024", mq_name, pid, csPipeName, scPipeName);
     mq_send(mq, buffer, strlen(buffer), 0);

     sc = open(scPipeName, O_WRONLY);
     cs = open(csPipeName, O_RDONLY);

     readSize = read(cs, buffer, 1024);
     buffer[readSize] = '\0';
     printf("%s\n", buffer);

     while (1) {
          printf("Please give a line of command to execute: \n");
          fgets(buffer, 1024, stdin);
          write(sc, buffer, 1024);
          trimString(buffer);

          if (strcmp(buffer, "quit") == 0) {
               break;
          }

          readSize = read(cs, buffer, 1024);
          buffer[readSize] = '\0';
          printf("%s\n", buffer);
     }

     close(sc);
     close(cs);
     unlink(scPipeName);
     unlink(csPipeName);

     if (mq_close(mq) == -1) {
          perror("mq_close");
          exit(1);
     }
     return 0;
}