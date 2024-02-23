#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "Message.h"

int main(int argc, char* argv[]) {
     if (argc != 2) {
          printf("Please provide proper amount of arguments.");
          return 0;
     }

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
          if (mq_receive(mq, buffer, 1024, NULL) == -1) {
               perror("mq_receive");
               exit(1);
          }
          printf("Received: %s\n", buffer);

          strcpy(buffer, "You are connected");
          if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
               perror("mq_send");
               exit(1);
          }

          if (mq_receive(mq, buffer, 1024, NULL) == -1) {
               perror("mq_receive");
               exit(1);
          }
          printf("Received: %s\n", buffer);

          //parse the message, ID of client, name of the pipes created by client, wsize

          // this will be sent by child actually TODO
          strcpy(buffer, "got the sc,cs pipes, please send some commands");
          if (mq_send(mq, buffer, strlen(buffer), 0) == -1) {
               perror("mq_send");
               exit(1);
          }

          // create server child with fork()

          sleep(1);
     }

     if (mq_close(mq) == -1) {
          perror("mq_close");
          exit(1);
     }

     return 0;
}