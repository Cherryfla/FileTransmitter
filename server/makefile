cc = g++ 
CFLAGS = -std=c++11 -g -Wall

server:server.o
	$(cc) $(CFLAGS) -o server server.o

%.o:%.cpp
	$(cc) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm *.o server

