cc = g++ 
CFLAGS = -std=c++11 -g -Wall

all:server client
server:server.o
	$(cc) $(CFLAGS) -o server server.o
client:client.o
	$(cc) $(CFLAGS) -o client client.o

%.o:%.cpp
	$(cc) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm *.o server client
