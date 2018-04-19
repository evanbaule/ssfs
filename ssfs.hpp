#ifndef SSFS_HPP
#define SSFS_HPP

#include <stdio.h>
#include <iostream>
#include <string>
#include <cstring>
#include <queue>
#include <fstream>
#include <pthread.h>
#include <sstream>

#include "scheduler.hpp"

using namespace std;

typedef unsigned int uint;

typedef struct {
  char fileName[32]; // Max 32 chars
  uint fileSize;
  int direct[12];
  int indirect;
  int doubleIndirect;
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

typedef struct
{
  queue<request>* requests;
  pthread_mutex_t lock;
} SCH_struct;

uint getBlockSize();
uint getNumBlocks();
char* getDisk();

void shutdown();


#endif
