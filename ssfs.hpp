#ifndef SSFS_HPP
#define SSFS_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <pthread.h>
#include <sstream>

#include "scheduler.hpp"

using namespace std;

typedef unsigned int uint;

/*byte array of the disk in memory*/
char* DISK;

/*Any access to the DISK array should be locked by this*/
pthread_mutex_t DISK_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t REQUESTS_LOCK = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
  string fileName; // Max 32 chars
  uint fileSize;
  uint direct[12];
  uint indirect;
  uint doubleIndirect;
} inode;

enum Tag {
	create_tag,
	import_tag,
	cat_tag,
	delete_tag,
	write_tag,
	read_tag,
	list_tag,
	shutdown_tag,
};

typedef struct {
	Tag tag; //Maps to disk operation
	string fname1; //<SSFS File Name>
	string fname2; //<Unix File Name>
	char c; //<char>
	uint start_byte; //<start byte>
	uint num_bytes; //<num bytes>
} request;

vector<request>* requests;


#endif
