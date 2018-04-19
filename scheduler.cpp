#include "scheduler.hpp"

int GETEMPTYINODE()
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

int GETINODE(char* file)
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

void CREATE(char* filename)
{
  if(filename[0] == 0); //ERROR
  int i = GETINODE();
  if(i==-1);//ERRORS
  int inodeStart = getBlockSize() + i*getBlockSize();
  inode* inod = ((inode*)(getDisk()+inodeStart));
  strcpy(inod->fileName, filename);
}


void IMPORT(char* ssfsFile, char* unixFilename){
  char* DISK = getDisk();
  uint block_size = getBlockSize();
  uint num_blocks = getNumBlocks();

  uint byteOffs =  block_size + 256*block_size + i + (num_blocks/block_size);
  uint max_file_size = 1 + 12 + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int)));
  char* buffer[max_file_size];
  int fd = open((const char*)unixFileName, O_RDONLY);
  ssize_t bytes_read = read(fd, buffer, max_file_size);

  //Get size of unix file to check return from read got whole file
  //idec if this works or not
  struct stat sb; 
  if (stat(unixFilename, &sb) == -1) {
    perror("stat fucked");
    return;
  }
  if(bytes_read != fs.st_size)
  {
    cerr << "read() syscall is fucked up and didnt read the correct amt of bits" << endl;
    return;
  }

  //


}

void CAT(char* fileName){}

void DELETE(char* fileName)
{
  
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
