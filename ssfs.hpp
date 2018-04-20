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


/* Constants */
#define MAX_FILENAME_SIZE 32;
#define NUM_DIRECT_BLOCKS 12;

#define MIN_BLOCK_SIZE 128;
#define MAX_BLOCK_SIZE 512;

#define MIN_DISK_SIZE 1024;
#define MAX_DISK_SIZE 128000; //technically 131,072 but thats a nitpick

#define MAX_INODES 256;

using namespace std;

typedef unsigned int uint;

typedef struct {
  char fileName[MAX_FILENAME_SIZE]; // Max 32 chars
  uint fileSize;
  uint direct[NUM_DIRECT_BLOCKS];
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
