CC = g++
CFLAGS = -g -Wall

default: client

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp

clean: 
	rm client 
	rm -r *.dSYM