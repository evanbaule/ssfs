#ifndef SSFS_HPP
#def SSFS_HPP

#def uint unsigned int

#include <iostream>
#include <string>

typedef struct {
  string fileName; // Max 32 chars
  uint fileSize;
  uint direct[12];
  uint indirect;
  uint doubleIndirect;
} inode;

void CREATE(string filename, void* buffer);

void IMPORT(string ssfs, string unix, void* buffer);

void CAT(string ssfs, void* buffer);

void DELETE(string fileName, void* buffer);

void WRITE(string fileName, char c, uint start, uint num, void* buffer);

void READ(string fileName, uint start, uint num, void* buffer);

void SHUTDOWN(void* buffer);

#endif
