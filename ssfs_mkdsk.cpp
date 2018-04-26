#include <iostream>
#include <fstream>
#include <string>

using namespace std;

int main(int argc, char** argv)
{
  int numBlocks = atoi(argv[1]);
  int blockSize = atoi(argv[2]);
  string diskName = "DISK";
  if(argc == 4) diskName = argv[3];

  /*Calculate starting block locations */
  int loc_freemap = 1; // location of first block of the free list bytemap
  int free_map_size = numBlocks / blockSize;

  int loc_imap = loc_freemap + (free_map_size);
  int imap_size = (256) / blockSize; //we need to map 256 inodes (256 integer block numbers (4b each))

  int loc_inodes_start = loc_imap + imap_size;

  int loc_data_start = loc_inodes_start + 256;

  ofstream fout;
  fout.open(diskName, ios::binary | ios::out);
  char a = 0;
  fout.write((char*)&numBlocks, 4);
  fout.write((char*)&blockSize, 4);
  fout.write((char*)&loc_freemap, 4);
  fout.write((char*)&free_map_size, 4);
  fout.write((char*)&loc_imap, 4);
  fout.write((char*)&imap_size, 4);
  fout.write((char*)&loc_inodes_start, 4);
  fout.write((char*)&loc_data_start, 4);
  for(int i = 32; i < numBlocks * blockSize;i++)
    fout.write(&a, 1);

  fout.close();

  return 0;
}


