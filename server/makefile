cc = g++ 
CFLAGS = -std=c++11 -g -Wall
tar = server
obj = server.o ProcessPool.o MyRedis.o main.o 
lib = -lhiredis

$(tar):$(obj) 
	$(cc) $(CFLAGS) $(obj) -o $(tar) $(lib)

MyRedis.o:MyRedis.cpp
	$(cc) $(CFLAGS) -c $< -o $@

%.o:%.cpp
	$(cc) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm $(tar) $(obj)

