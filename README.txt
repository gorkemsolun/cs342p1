Görkem Kadir Solun 22003214
Murat Çağrı Kara 22102505

Operating Systems CS342 Project 1

A command server program.

The server will execute the commands (programs) whose names
are submitted by a client and return the results (outputs) to the client.

The server is able to communicate with more than one client concurrently. You can change the maximum client count in constant.h.

By typing "make", you should be able to produce executables.

You can see the message types in constant.h.

You need to provide the same name in order to connect the server and client.
Example:
Terminal 1                        Terminal 2
./comserver /msgqueue             ./comcli /msgqueue

For the client, we can specify [-b COMFILE] [-s WSIZE].
Example: ./comcli /msgqueue -b batch.txt -s 1024

You should not use a text file named "clientsToRemove.txt" as it will be used for removing clients. You can change this in constant.h.
