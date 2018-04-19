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

<<<<<<< HEAD
enum tags {
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
	tags tag; //Maps to disk operation
	string fname1; //<SSFS File Name>
	string fname2; //<Unix File Name>
	char c; //<char>
	uint start_byte; //<start byte>
	uint num_bytes; //<num bytes>
} request;

=======
void CREATE(string filename, void* buffer);

void IMPORT(string ssfs, string unix, void* buffer);

void CAT(string ssfs, void* buffer);

void DELETE(string fileName, void* buffer);

void WRITE(string fileName, char c, uint start, uint num, void* buffer);

void READ(string fileName, uint start, uint num, void* buffer);

void SHUTDOWN(void* buffer);
>>>>>>> refs/remotes/origin/master

#endif
