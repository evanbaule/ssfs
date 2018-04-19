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


#endif
