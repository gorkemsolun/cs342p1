#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include "Message.h"
#include <string.h>

int main(int argc, char* argv[]) {
     // TODO this should be changed after implementaion of sc,cs pipes
     if (/* argc != 6 && argc != 4*/ argc != 2) {
          printf("Please provide proper amount of arguments.");
          return 0;
     }

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

     strcpy(buffer, "Let me connect to the server!!");
     if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
          perror("mq_send");
          exit(1);
     }

     if (mq_receive(mq, buffer, 1024, NULL) == -1) {
          perror("mq_receive");
          exit(1);
     }
     printf("Received: %s\n", buffer);

     // create cs,sc pipes, send the names, 

     strcpy(buffer, "Sent you the sc,cs pipe names");
     if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
          perror("mq_send");
          exit(1);
     }

     if (mq_receive(mq, buffer, 1024, NULL) == -1) {
          perror("mq_receive");
          exit(1);
     }
     printf("Received: %s\n", buffer);

     if (mq_close(mq) == -1) {
          perror("mq_close");
          exit(1);
     }

     if (mq_unlink(mq_name) == -1) {
          perror("mq_unlink");
          exit(1);
     }
     return 0;
}