CC=g++
CCFLAGS= -g -I

%.o : %.cpp $(HEADERS) Makefile
	$(CC) $(CCFLAGS) -Wall -c $<
		
all: configuration.o 
	echo $(PWD)
	$(CC) $(CCFLAGS) -Wall *.o synchronizer.cpp -o synchronizer

clean:
	rm *.o synchronizer
		
	
%.o : %.cpp $(HEADERS) Makefile
	$(CC) $(CCFLAGS) -Wall -c $<
	

