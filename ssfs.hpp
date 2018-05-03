#ifndef SSFS_HPP_G
#define SSFS_HPP_G

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
#include <math.h>
#include "scheduler.hpp"

using namespace std;

/* Constants */
#define MAX_FILENAME_SIZE 32
#define NUM_DIRECT_BLOCKS 12
#define MIN_BLOCK_SIZE 128
#define MAX_BLOCK_SIZE 512
#define MIN_DISK_SIZE 1024
#define MAX_DISK_SIZE 128000 //technically 131,072 but thats a nitpick
#define MAX_INODES 256

//Identifies the io operation in a disk_io_request
enum Operation {
	io_READ,
	io_WRITE,
  io_WRONG
};

//"Low level" request that will be serviced by the disk scheduler
typedef struct disk_io_request
{
	int block_number; // Target block
	Operation op = io_WRONG; // Indicated whether we are reading or writing data to/from block_number
	char* data; // Will either be a pointer to the SOURCE LOCATION to write FROM ||OR|| the DESTINATION LOCATION to read TO
  pthread_cond_t waitFor;
  pthread_mutex_t lock;
  bool done;
} disk_io_request;

//Struct to contain file metadata (1 block in size)
typedef struct {
  char fileName[MAX_FILENAME_SIZE]; // Max 32 chars
  int fileSize;
  int direct[NUM_DIRECT_BLOCKS];
  int indirect;
  int doubleIndirect;
} inode;

typedef struct
{
  queue<disk_io_request*>* requests;
  pthread_mutex_t lock;
  int fd;
} SCH_struct;

//Request abstraction functions
char* readFromBlock(int i);
void writeToBlock(int i, char* data);

//Access to memory storage metadata
int getNumBlocks(); 
int getBlockSize(); 
int getFreeMapStart(); 
int getFreeMapSize(); 
int getInodeMapStart();
int getInodeMapSize();
int getInodesStart();
int getUserDataStart();

char* getSUPER();
char* getFREE_MAP();
char* getINODE_MAP();

int getUnusedBlock();
int getFreeByteMapInBlock(int block);
bool getByteMap(int block);
void setByteMap(int block, bool flag);
int getEmptyInode();
int getInode(const char* file);
inode* getInodeFromIndex(int ind);
int getStartOfDataBlocks();

// SSFS Disk operations
void CREATE(const char* filename);
void IMPORT(const char* ssfsFile, const char* unixFilename);
void CAT(const char* fileName);
void DELETE(const char* fileName);
void WRITE(const char* fileName, char c, int start, int num);
void READ(const char* fileName, int start, int num);
void LIST();
void SHUTDOWN();

void shutdown();
bool isShutdown();

#endif
