#include "scheduler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

int getBitmapSize()
{
  return (getNumBlocks()/getBlockSize());
}

int getFreeBlock()
{
  char* disk = getDisk();
  uint byteOffs =  getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
  for(unsigned int i = byteOffs; i < byteOffs + getBitmapSize(); i++)
  {
    if(disk[i] == 0)
      disk[byteOffs + i] = 1;
      return i;
  }
  cerr << "disk is fucking full af" << endl;
  return -1;
}

//TODO: shouldn't use the first some bit maps because they are reserved for meta data and inode map and bit map
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

int getDataStart()
{
  return (getBlockSize() + MAX_INODES*getBlockSize() + getBitmapSize());
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

      if(strcmp(disk + byteOffs, file) == 0)
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

int getStartOfDataBlocks()
{
  return getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
}

void CREATE(char* filename)
{
  if(filename[0] == 0)
  {
    cerr << "Invalid filename entered into CREATE argument " << endl;
  }
  int i = getEmptyInode();
  if(i==-1){
    cerr << "Error generating empty inode in func CREATE" << endl;
  }
  int inodeStart = getBlockSize() + i*getBlockSize();
  inode* inod = ((inode*)(getDisk()+inodeStart));
  strcpy(inod->fileName, filename);
}

void IMPORT(char* ssfsFile, char* unixFilename){
  /* Initializing */
  char* disk = getDisk();
  uint block_size = getBlockSize();
  uint num_blocks = getNumBlocks();
  uint byteOffs =  block_size + MAX_INODES*block_size + (num_blocks/block_size);
  uint max_file_size = block_size * (1 + NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int)))); //max number of blocks we can hold in a single file * num of bytes in a block
  
  int fd;
  if((fd = open(unixFilename, O_RDONLY)) == -1)
  {
    cerr << "Error opening file stream from " << unixFilename << endl;
  }

  /* Gets & checks the total bytes of unixFilename file */
  int filesize;
  struct stat sb;
  if(fstat(fd, &sb) == -1)
  {
    cerr << "Failed to read file size from fstat(fd, &sb) in func IMPORT" << endl;
    return;
  }
  else 
  {
    filesize = sb.st_size;
  }

  /* Checks if we have room in the file structure to store all the data */
  if(filesize > max_file_size - 1) // - 1 to make room for trailing bytes
  {
    cerr << "Unable to read unix file: file is too large in func IMPORT" << endl;
    return;
  }

  /* Begins to read from unix file and inject blocks into the disk */

  int ino = getInode(ssfsFile); //retrieve inode of file to write to OR empty file to begin writing to
  if(ino == -1) //this condition may be off at this point (4/20 @13:42)
  {
    ino = getEmptyInode();
  }

  inode* inod = getInodeFromIndex(ino);

  //i represents the # of blocks read at point in loop
  char* read_buffer[block_size];
  for(int i = 0; i < (filesize + block_size)/block_size; i++) //add block size to filesize to avoid truncating
  {
    int bytes_read;
    if((bytes_read = read(fd, &read_buffer, block_size)) != block_size)
    {
      cerr << "Read the wrong number of bytes into read_buffer OR maybe reached end of the file. Not sure tbh - EMB" << endl;
      break;
    }
    
  }
}

void CAT(char* fileName){}

/* Feeds 12 + blocksize/sizeof(int) + (blocksize/sizeof(int))^2 requests into the scheduler buffer to 'zero-out' the entirety of a file */
void DELETE(char* fileName)
{
  int id = getInode(fileName);
  if(id == -1){
    cerr << "Error finding inode with fileName in func DELETE" << endl;
  }
  inode* inod = getInodeFromIndex(id);
  inod->fileName[0] = 0;
  inod->fileSize = 0;

  for(int i =0;i<NUM_DIRECT_BLOCKS;i++)
    {
      int dir = inod->direct[i];

      /*
      Operation op = io_WRITE;
      disk_io_request req;
      req.block_number = dir;
      req.op = op;
      req.data = 0;
      requests->push(req);
      */

      *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (dir)) = 0;
    }

  int* indirect = (int*) (getDisk() + inod->indirect*getBlockSize());
  for(int i =0;i<getBlockSize()/4;i++)
    {

      /*
      Operation op = io_WRITE;
      disk_io_request req;
      req.block_number = indirect[i];
      req.op = op;
      req.data = 0;
      requests->push(req);
      */

      *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (indirect[i])) = 0;
    }

  int* doubleindirect = (int*) (getDisk() + inod->doubleIndirect*getBlockSize());
  for(int i =0;i<getBlockSize()/4;i++)
    {
      int* indirect2 = (int*) (getDisk() + doubleindirect[i]*getBlockSize());
      for(int j=0;j<getBlockSize()/4;j++)
        {

          /*
          Operation op = io_WRITE;
          disk_io_request req;
          req.block_number = indirect[i];
          req.op = op;
          req.data = 0;
          requests->push(req);
          */

          *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (indirect2[i])) = 0;
        }
    }
}

void WRITE(char* fileName, char c, uint start, uint num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int id = getInode(fileName);
  if(id == -1){
    cerr << "Error finding inode with fileName in func WRITE" << endl;
  }
  inode* inod = getInodeFromIndex(id);
  for(int i = start; i < num; i++)
  {
    int curr_block = i/getBlockSize(); //for shorthand later on

    if(i/getBlockSize() < NUM_DIRECT_BLOCKS)
    {
      //WARNING: NOT CHECKING FOR UNALLOCATED BLOCKS AT THIS POINT
      int* loc = (int* ) (inod->direct[curr_block] + (i%getBlockSize()));
      memcpy(&loc, &c, sizeof(char));
    }
    else if((curr_block >= NUM_DIRECT_BLOCKS) && (curr_block < indirect_max_size)){
      int* loc = (int* ) inod->indirect + (curr_block - NUM_DIRECT_BLOCKS) + ( i % getBlockSize());
      memcpy(&loc, &c, sizeof(char));
    }
    else if(curr_block >= indirect_max_size && curr_block < double_indirect_max_size)
    {
      int* indir_block = (int*) inod->doubleIndirect + ((curr_block - (NUM_DIRECT_BLOCKS + (getBlockSize()/sizeof(int)))) / (getBlockSize() / sizeof(int)));
      int* loc = (int* ) indir_block + (curr_block - (NUM_DIRECT_BLOCKS) + (getBlockSize() / sizeof(int))) + (i%getBlockSize());
      memcpy(&loc, &c, sizeof(char));
    }
  }
}


void READ(char* fileName, uint start, uint num)
{
  char* bytes = new char[num];
  inode* inod = getInodeFromIndex(getInode(fileName));
}

bool shut=0;

void SHUTDOWN()
{
  shut = 1;

  /*from ssfs to tell the threads to stop adding to the requests*/
  shutdown();
}

void LIST()
{
  int i;
  boolean found = 0;
  for(i = 0; i < getNumBlocks()/getBlockSize(); i++)
    {
      /*              metadata        num of inodes        */
      uint byteOffs =  getBlockSize() + i*getBlockSize();
      char* disk = getDisk();

      if(strcmp(disk + byteOffs, file) == 0)
        {
          found = 1;
          break;
        }
    }
}

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<disk_io_request>* requests = str->requests;
  pthread_mutex_t lock = str->lock;
  pthread_mutex_t diskLock = str->diskLock;

  while(1)
    {
      if(shut && requests->size()==0)break;

      pthread_mutex_lock(&lock);
      disk_io_request req = requests->front();
      requests->pop();
      pthread_mutex_unlock(&lock);

      pthread_mutex_lock(&diskLock);
      if(req.op == io_READ)
        {
          memcpy(req.data, getDisk()+req.block_number*getBlockSize(), getBlockSize());
        }
      else if (req.op == io_WRITE)
        {
          memcpy(getDisk()+req.block_number*getBlockSize(), req.data, getBlockSize());
        }
      pthread_mutex_unlock(&diskLock);
    }
}
