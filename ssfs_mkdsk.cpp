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

  ofstream fout;
  fout.open(diskName, ios::binary | ios::out);
  char a = 0;
  fout.write((char*)&numBlocks, 4);
  fout.write((char*)&blockSize, 4);
  for(int i = sizeof(numBlocks)+sizeof(blockSize); i < numBlocks * blockSize;i++)
    fout.write(&a, 1);

  fout.close();

  return 0;
}


