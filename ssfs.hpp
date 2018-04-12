#ifndef SSFS_HPP
#def SSFS_HPP

#include <iostream>
#include <string>

typedef struct {
  string fileName; // Max 32 chars
  unsigned int fileSize;
  unsigned int direct[12];
  unsigned int indirect;
  unsigned int doubleIndirect;
} inode;


#endif
