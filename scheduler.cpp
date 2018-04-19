#include "scheduler.hpp"

int
getBitMapSize()
{
  return (getNumBlocks()/getBlockSize());
}

int getFreeBlock()
{
  char* disk = getDisk();
  uint byteOffs =  getBlockSize() + 256*getBlockSize();
  for(unsigned int i = byteOffs; i < byteOffs + getBitMapSize(); i++)
  {
    if(disk[i] == 0)
      return i;
  }

  disk[byteOffs + i] = 1;

  cerr << "disk is fucking full af" << endl;
}

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

void IMPORT(char* ssfsFile, char* unixFilename){
  char* DISK = getDisk();
  uint block_size = getBlockSize();
  uint num_blocks = getNumBlocks();

  uint byteOffs =  block_size + 256*block_size + i + (num_blocks/block_size);
  uint max_file_size = 1 + 12 + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int)));
  char* read_buffer[max_file_size];
  int fd = open((const char*)unixFileName, O_RDONLY);
  ssize_t bytes_read = read(fd, read_buffer, max_file_size);

  //Get size of unix file to check return from read got whole file
  /*idec if this works or not
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
  */

  inode* inod = GETINODE(ssfsFile);
  if(inod == -1)
  {
    inod = GETEMPTYINODE();
  }

  bool end_write;

  uint data_ptr = 0;
  for(int i = 0; i < 12 && !end_write; i++)
  {
    if(read_buffer[data_ptr] == 0)
    {
      end_write = true;
      continue;
    }

    int dblock;
    if(inod->direct[i] == 0) //block DNE
    {
      dblock = getFreeBlock(); //allocate a block
      inod->direct[i] = dblock; //assign block# to inode
    } else {
      dblock = inod->direct[i]; //set destination for memcpy to allocated dir block 
    }

    memcpy(&disk[dblock + (i*block_size)], &read_buffer[data_ptr], block_size); //write to the disk
    data_ptr += block_size; //move file stream pointer
  }

  if(end_write)
  {
    cout << "DANGER: FILE DOES NOT FIT INTO 12 BLOCKS NYI" << endl;
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

  for(int i =0;i<12;i++)
    {
      int dir = inod->direct[i];
      if(dir != -1);
      *(getDisk() + 256*getBlockSize() + (getBitmapSize()*4)*getBlockSize + dir*getBlockSize()) = 0;
    }
  
  for(int i =0;i<getBlockSize()/4;i++)
    {
      
      (getDisk()+getBlockSize()+256*getBlockSize)[i]=0;
    }
}

void WRITE(char* fileName, char c, uint start, uint num){
  inode* inod = GETINODE(fileName);
  if(inod == -1)
  {
    inod = GETEMPTYINODE();
  }

  int total_bytes = num;
  int i = 0;
  while(i < num)
  {
    int dblock;
    if(i < 12)
    {
      if(inod->direct[i] == 0) //block DNE
      {
        dblock = getFreeBlock(); //allocate a block
        inod->direct[i] = dblock; //assign block# to inode
      } else {
        dblock = inod->direct[i]; //set destination for memcpy to allocated dir block 
      }
      memcpy(&disk[dblock + (i*block_size) + start], &c, sizeof(char));
    }
  }
}

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
