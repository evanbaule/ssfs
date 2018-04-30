#Evan M. Baule, Daniel Hintz, & Ari Schild
#C350 - Operating Systems
#Programming Assignment 5 - Super Simple File System SSFS

all: link_all

#Links *.o into driver executable
link_all: comp_all ssfs.o scheduler.o
	g++ -lpthread ssfs.o scheduler.o -o ssfs


#Compiles src files into object in build/*.o
comp_all: ssfs.cpp scheduler.cpp
	g++ -c -g -std=c++11 ssfs.cpp -o ssfs.o
	g++ -c -g -std=c++11 scheduler.cpp -o scheduler.o

dsk: cdisk ssfs_mkdsk.o
	g++ ssfs_mkdsk.o -o ssfs_mkdsk

cdisk: ssfs_mkdsk.cpp
	g++ -c -g -Wall -std=c++11 ssfs_mkdsk.cpp -o ssfs_mkdsk.o

#Removes object files build/*.o and executable bin/drive(.exe), build afterwards to replace those files
clean:
	rm -f *.o *.orig ssfs ssfs_mkdsk

#Runs the bin/drive(.exe) executable
run: all
	./ssfs DISK thread1ops.txt
