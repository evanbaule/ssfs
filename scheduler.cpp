#include "scheduler.hpp"

int getEmptyInode()
{
  char found = 0;
  int i;
  for(i = 0; i < getNumBlocks()/getBlockSize(); i++)
    {
      /*              metadata        num of inodes        */
      uint byteOffs =  getBlockSize() + i*getBlockSize();
      char* disk = getDisk();

      if(disk[byteOffs] == 0)
        {
          found = 1;
          break;
        }
    }
  if(!found) return -1;
  return i;
}

int getInode(char* file)
{
  char found = 0;
  int i;
  for(i = 0; i < getNumBlocks()/getBlockSize(); i++)
    {
      /*              metadata        num of inodes        */
      uint byteOffs =  getBlockSize() + i*getBlockSize();
      char* disk = getDisk();

      if(strcmp(disk[byteOffs], file) == 0)
        {
          found = 1;
          break;
        }
    }
  if(!found) return -1;
  return i;
}

inode* getIndexFromInode(int ind)
{
  int inodeStart = getBlockSize() + ind*getBlockSize();
  inode* inod = ((inode*)(getDisk()+inodeStart));
  return inod;
}

void CREATE(char* filename)
{
  if(filename[0] == 0); //ERROR
  int i = GETINODE();
  if(i==-1);//ERRORS
  int inodeStart = getBlockSize() + i*getBlockSize();
  inode* inod = ((inode*)(getDisk()+inodeStart));
  strcpy(inod->fileName, filename);
}

void IMPORT(char* ssfsFile, char* unixFilename){}

void CAT(char* fileName){}

void DELETE(char* fileName)
{
  int id = getInode(fileName);
  if(id == -1);//ERRORS
  inode* inod = getInodeFromIndex(id);
  inod->fileName[0] = 0;
  inod->fileSize = 0;

  for(int i =0;i<12;i++)
    {
      
    }
}

void WRITE(char* fileName, char c, uint start, uint num){}

void READ(char* fileName, uint start, uint num){}

bool shut=0;

void SHUTDOWN()
{
  shut = 1;

  /*from ssfs to tell the threads to stop adding to the requests*/
  shutdown();
}

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<request>* requests = str->requests;
  pthread_mutex_t lock = str->lock;

  while(1)
    {
      if(shut && requests->size()==0)break;

      pthread_mutex_lock(&lock);
      request req = requests->front();
      requests->pop();
      pthread_mutex_unlock(&lock);

    }
}
