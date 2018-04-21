#ifndef SCHEDULER_HPP
#define SCHEDULER_HPP

#include "ssfs.hpp"

using namespace std;

void CREATE(char* filename);

void IMPORT(char* ssfsFile, char* unixFilename);

void CAT(char* fileName);

void DELETE(char* fileName);

void WRITE(char* fileName, char c, uint start, uint num);

void READ(char* fileName, uint start, uint num);

int getEmptyInode();

int getInode(char* file);

inode* getInodeFromIndex(int ind);

int getStartOfDataBlocks();

void SHUTDOWN();

void* SCH_run(void* vec);

#endif
