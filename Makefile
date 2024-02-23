all: comserver comcli

comcli: comcli.c
	gcc -Wall -o comcli comcli.c -lrt
comserver: comserver.c
	gcc -Wall -o comserver comserver.c -lrt
clean: 
	rm -fr *~ comserver comcli