#Evan M. Baule, Daniel Hintz, & Ari Schild
#C350 - Operating Systems
#Programming Assignment 5 - Super Simple File System SSFS

all:	link_all

#Links *.o into driver executable
link_all: comp_all ssfs.o scheduler.o 
	g++  ssfs.o scheduler.o -o ssfs
	

#Compiles src files into object in build/*.o
comp_all: ssfs.cpp scheduler.cpp
	g++ -c -std=c++11 -Wall ssfs.cpp -o ssfs.o -pthread
	g++ -c -std=c++11 -Wall scheduler.cpp -o scheduler.o -pthread
	
#Removes object files build/*.o and executable bin/drive(.exe), build afterwards to replace those files
clean:
	rm -f *.o *.orig ssfs

#Runs the bin/drive(.exe) executable
run: all
	./ssfs DISK thread1ops.txt thread2ops.txt thread3ops.txt

	
