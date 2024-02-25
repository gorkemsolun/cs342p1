#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/stat.h>
#include <unistd.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include "Message.h"
#include <string.h>

int main(int argc, char* argv[]) {
     // TODO this should be changed after implementaion of sc,cs pipes
     if (/* argc != 6 && argc != 4*/ argc != 2) {
          printf("Please provide proper amount of arguments.\n");
          return 0;
     }

     int readSize, i;
     mqd_t mq;
     char buffer[1024];
     char mq_name[256];
     strcpy(mq_name, "/");
     strcat(mq_name, argv[1]);

     mq = mq_open(mq_name, O_RDWR);
     if (mq == -1) {
          perror("mq_open");
          exit(1);
     }

     strcpy(buffer, "Let me connect");
     if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
          perror("mq_send");
          exit(1);
     }

     while (strcmp(buffer, "You are connected") != 0) {
          if (mq_receive(mq, buffer, 1024, NULL) == -1) {
               perror("mq_receive");
               exit(1);
          }
          sleep(1);
     }
     printf("Received success of connection: %s\n", buffer);

     pid_t pid = getpid();
     char csPipeName[256], scPipeName[256];
     sprintf(csPipeName, "cs%d", pid);
     sprintf(scPipeName, "sc%d", pid);
     int cs = mkfifo(csPipeName, 0666);
     if (cs == -1) {
          perror("Error creating cs pipe\n");
     }
     int sc = mkfifo(scPipeName, 0666);
     if (sc == -1) {
          perror("Error creating sc pipe\n");
     }

     sprintf(buffer, "%s %d %s %s 1024", mq_name, pid, csPipeName, scPipeName);
     if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
          perror("mq_send");
          exit(1);
     }

     while (1) {
          // here should send the commands to cs pipe
          printf("Please give a line of command to execute: \n");
          fgets(buffer, 1024, stdin);
          printf("%s", buffer);

          sc = open(scPipeName, O_WRONLY);
          if (sc == -1) {
               perror("Error opening sc pipe for writing");
          }
          write(sc, buffer, 1024);

          cs = open(csPipeName, O_RDONLY);
          if (cs == -1) {
               perror("Error opening cs pipe for writing");
          }
          readSize = read(cs, buffer, 1024);
          buffer[readSize] = '\0';
          printf("received over cs pipe: %s\n", buffer);

          if (mq_close(mq) == -1) {
               perror("mq_close");
               exit(1);
          }
          return 0;
     }

}