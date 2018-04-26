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
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

// Identifies the disk io operation to performed by a disk_io_request within scheduler workload buffer
enum Operation {
	io_READ,
	io_WRITE,
  io_WRONG
};

/* Flow: Each thread will *instead of adding a reqeust 
   to the buffer* call the corresponding function which will 
   pass n disk_io_requests to the scheduler depending on how many 
   blocks to read or write, which the scheduler will service one at a time */
typedef struct
{
	uint block_number; // Target block
	Operation op = io_WRONG; // Indicated whether we are reading or writing data to/from block_number
	char* data; // Will either be a pointer to the SOURCE LOCATION to write FROM ||OR|| the DESTINATION LOCATION to read TO
  pthread_cond_t waitFor;
  pthread_mutex_t lock;
  bool done;
} disk_io_request;


#include "scheduler.hpp"

/* Constants */
#define MAX_FILENAME_SIZE 32
#define NUM_DIRECT_BLOCKS 12

/* File Meta - Data */
typedef struct {
  char fileName[MAX_FILENAME_SIZE]; // Max 32 chars
  uint fileSize;
  uint direct[NUM_DIRECT_BLOCKS];
  uint indirect;
  uint doubleIndirect;
} inode;

int getUnusedBlock();

int getFreeByteMapInBlock(int block);

bool getByteMap(int block);

void setByteMap(int block, bool flag);

void CREATE(const char* filename);

void IMPORT(const char* ssfsFile, const char* unixFilename);

void CAT(const char* fileName);

void DELETE(const char* fileName);

void WRITE(const char* fileName, char c, uint start, uint num);

void READ(const char* fileName, uint start, uint num);

int getEmptyInode();

int getInode(const char* file);

inode* getInodeFromIndex(int ind);

int getStartOfDataBlocks();

void SHUTDOWN();

#define MIN_BLOCK_SIZE 128
#define MAX_BLOCK_SIZE 512

#define MIN_DISK_SIZE 1024
#define MAX_DISK_SIZE 128000 //technically 131,072 but thats a nitpick

#define MAX_INODES 256

using namespace std;

typedef unsigned int uint;

typedef struct
{
  queue<disk_io_request*>* requests;
  pthread_mutex_t lock;
  pthread_mutex_t diskLock;
} SCH_struct;

char* getDisk(); 
//METADATA INFORMATION KEPT IN MEMORY
int getNumBlocks(); 
int getBlockSize(); 
int getFreeMapStart(); 
int getFreeMapSize(); 
int getInodeMapStart();
int getInodeMapSize();
int getInodesStart();
int getDataStart(); 


void shutdown();
bool isShutdown();


#endif
