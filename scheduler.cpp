#include "scheduler.hpp"

int
getBitMapSize()
{
  return (getNumBlocks()/getBlockSize());
}

int getFreeBlock()
{
  char* disk = getDisk();
  uint byteOffs =  getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
  for(unsigned int i = byteOffs; i < byteOffs + getBitMapSize(); i++)
  {
    if(disk[i] == 0)
      return i;
  }

  disk[byteOffs + i] = 1;

  cerr << "disk is fucking full af" << endl;
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
  return (getBlcokSize() + MAX_INODES*getBlockSize + getBitMapSize());
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

int getStartOfDataBlocks()
{
  return getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
}

void CREATE(char* filename)
{
  if(filename[0] == 0); //ERROR
  int i = getEmptyInode();
  if(i==-1);//ERRORS
  int inodeStart = getBlockSize() + i*getBlockSize();
  inode* inod = ((inode*)(getDisk()+inodeStart));
  strcpy(inod->fileName, filename);
}

void IMPORT(char* ssfsFile, char* unixFilename){
  /* Initializing */
  char* disk = getDisk();
  uint block_size = getBlockSize();
  uint num_blocks = getNumBlocks();
  uint byteOffs =  block_size + MAX_INODES*block_size + i + (num_blocks/block_size);
  uint max_file_size = block_size * (1 + NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int))); //max number of blocks we can hold in a single file * num of bytes in a block
  
  int fd = open((const char*)unixFileName, O_RDONLY);

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

  inode* inod = GETINODE(ssfsFile); //retrieve inode of file to write to OR empty file to begin writing to
  if(inod == -1) //this condition may be off at this point (4/20 @13:42)
  {
    inod = GETEMPTYINODE();
  }

  //i represents the # of blocks read at point in loop
  char* read_buffer[block_size];
  for(int i = 0; i < (filesize + block_size)/block_size; i++) //add block size to filesize to avoid truncating
  {
    ssize_t bytes_read = read(fd, read_buffer, max_file_size);
    if(bytes_read != block_size)
    {
      cerr << "Read the wrong number of bytes into read_buffer OR maybe reached end of the file. Not sure tbh - EB" << endl;
      break;
    }
    int dblock; //destination block
   
    //Index into direct data blocks
    if(i < NUM_DIRECT_BLOCKS)
    {

      if(inod->direct[i] == 0) //block DNE
      {
        dblock = getFreeBlock(); //allocate a block
        inod->direct[i] = dblock; //assign block# to inode
      } else {
        dblock = inod->direct[i]; //set destination for memcpy to allocated dir block 
      }
      memcpy(&getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (i*block_size)], &read_buffer, block_size); //index into direct block
    
    }

    //Index into indirect data block
    else if (i >= NUM_DIRECT_BLOCKS && i < (NUM_DIRECT_BLOCKS + (block_size/sizeof(int))))
    {
      int indir_block = disk + inod->indirect[i-NUM_DIRECT_BLOCKS];
      if(disk + indir_block == 0) //block DNE
      {
        dblock = getFreeBlock();
      } 
      else 
      {
        dblock = indir_block + i*sizeof(int);
      }
      memcpy(&getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (getDisk() + i*block))

    }

    //Index into double indirect data block
    else if (i >= (NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) && i < (NUM_DIRECT_BLOCKS + (block_size/sizeof(int) + (block_size * block_size)/(sizeof(int) * sizeof(int)))))) //holy
    {

      

    }
  }
}

void CAT(char* fileName){}

void DELETE(char* fileName)
{
  int id = getInode(fileName);
  if(id == -1);//ERRORS
  inode* inod = getInodeFromIndex(id);
  inod->fileName[0] = 0;
  inod->fileSize = 0;

  for(int i =0;i<NUM_DIRECT_BLOCKS;i++)
    {
      int dir = inod->direct[i];
      *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (dir)) = 0;
    }

  int* indirect = (int*) (getDisk() + inod->indirect*getBlockSize());
  for(int i =0;i<getBlockSize()/4;i++)
    {
      *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (indirect[i])) = 0;
    }

  int* doubleindirect = (int*) (getDisk() + inod->doubleindirect*getBlockSize());
  for(int i =0;i<getBlockSize()/4;i++)
    {
      int* indirect2 = (int*) (getDisk() + doubleindirect[i]*getBlockSize());
      for(int j=0;j<getBlockSize()/4;j++)
        {
          *(getDisk() + getBlockSize() + MAX_INODES*getBlockSize() + (indirect2[i])) = 0;
        }
    }
}

void WRITE(char* fileName, char c, uint start, uint num){
  inode* inod = GETINODE(fileName);
  if(inod == -1)
  {
    inod = GETEMPTYINODE();
  }

  //i is the number of bytes we have written
  for(uint i = start; i < num; i++)
  {
    int current_block = i/getBlockSize(); //current block worth of data (i.e. if we write 300 characters and block size if 128 we are on block 3 of chars)
    int max_indirect_block = (NUM_DIRECT_BLOCKS + (block_size/sizeof(int)));
    int max_double_indirect_block = (NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) + (block_size*block_size)/(sizeof(int)*sizeof(int)));
    if(current_block < NUM_DIRECT_BLOCKS)
    {
      int dblock = inod->direct[current_block];
      memcpy(&getDataStart() + dblock + i, &c, sizeof(char));
    }
    else if(current_block >= NUM_DIRECT_BLOCKS && current_block < max_indirect_block)
    {
      int dblock = inod->indirect[(i/block_size)-NUM_DIRECT_BLOCKS];
      memcpy(&getDataStart() + dblock + i, &c, sizeof(char));
    }
    /* Double indirect blocks are fucking crazy
    else if(current_block >= max_indirect_block && current_block < max_double_indirect_block)
    {
      int dblock = inod->doubleindirect[(i-max_indirect_block)/block_size];
      int double_dblock = inod->
      memcpy(&getDataStart() + dblock + i, &c, sizeof(char));
    }
    */
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
