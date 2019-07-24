cc = g++
CFLAGS = -std=c++11 -g -Wall

all:server client

.PHONY:server
server:
	cd server && make && cd ..

.PHONY:client
client:client.o
	$(cc) $(CFLAGS) -o client client.o

%.o:%.cpp
	$(cc) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm *.o client && cd ./server && make clean
