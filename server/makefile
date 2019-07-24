cc = g++ 
CFLAGS = -std=c++11 -g -Wall
tar = server
obj = server.o main.o

$(tar):$(obj) 
	$(cc) $(CFLAGS) $(obj) -o $(tar) 

%.o:%.cpp
	$(cc) $(CFLAGS) -c $< -o $@

.PHONY:clean
clean:
	rm $(tar) $(obj)

