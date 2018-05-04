#include "ssfs.hpp"

using namespace std;

pthread_t op_thread[4];
pthread_t SCH_thread;

char* SUPER;
char* FREE_MAP;
char* INODE_MAP = new char[MAX_INODES];

/*Lock to protect output to console to ensure clean printing */
pthread_mutex_t CONSOLE_OUT_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t REQUESTS_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t BMAP_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t IMAP_LOCK = PTHREAD_MUTEX_INITIALIZER;

/* Workload queue that will be serviced by the disk scheduler*/
queue<disk_io_request*>* requests = new queue<disk_io_request*>;

bool shut=0;
bool isShutdown() {return shut;}

//Just an alias that I'm too scared to consolidate
int
getBitmapSize()
{
  return getFreeMapSize();
}


/* Return the block number of a block that is not currently assigned to a file
 * @return if successful, return block# of free block, otherwise return -1 (no free blocks)
 */
int getUnusedBlock()
{
  for(int i = getUserDataStart(); i < getNumBlocks(); i++)
  {
      pthread_mutex_init(&BMAP_LOCK, NULL);
      pthread_mutex_lock(&BMAP_LOCK);
    // Maybe need to do i % blockSize() here or something to index into individual bytes
  if(FREE_MAP[i] == 0)
      {
        FREE_MAP[i] = 1;
        writeToBlock(i, new char[getBlockSize()]());
        return i; //returns the block # of the unassigned block
      }
  }
  pthread_mutex_unlock(&BMAP_LOCK);
  return -1;
  /* DEPRECATED 
  for(int i=257;i<getNumBlocks();i++)
    {
      int asdf = getFreeByteMapInBlock(i);
      if(asdf != -1) return asdf;
    }
  return -1;
  */
}

//Push a request onto the disk scheduler workload queue
void addRequest(disk_io_request* req)
{
  pthread_mutex_lock(&REQUESTS_LOCK);
  requests->push(req);
  pthread_mutex_unlock(&REQUESTS_LOCK);
}

/* Return the block number of an inode that is not currently assigned to a file
 * @return block number of found empty inode, otherwise return -1 (no free blocks)
 */
int getEmptyInode()
{
  pthread_mutex_init(&IMAP_LOCK,NULL);
  pthread_mutex_lock(&IMAP_LOCK);
  for(int i = 0; i < MAX_INODES; i++)
  {
    if(INODE_MAP[i] == 0)
    {
      INODE_MAP[i] = 1;
      return i+getInodesStart();
    }
  }
 pthread_mutex_unlock(&IMAP_LOCK);
 cerr << "Disk cannot fit any more files" << endl;
 return -1; // DISK IS FULL
}

/*
Retrieve the index of an inode that contains argument file name
@param file : name of file associated with the inode
@return the block number of the inode if it exists, -1 if it doesn't exist
*/
int getInode(const char* file)
{
  bool found=0;
  int i;
  pthread_mutex_init(&IMAP_LOCK,NULL);
  pthread_mutex_lock(&IMAP_LOCK);
  for(i=0;i<MAX_INODES && !found;i++)
    {
      if(!INODE_MAP[i])
      {
        //I think this will skip requests to unallocated blocks but im autistic so it could be completely wrong
        continue;
      }
      char* data = readFromBlock(i+getInodesStart());
      if(strncmp(data, file, MAX_FILENAME_SIZE) == 0)
        found = 1;
      delete[](data);
    }
  pthread_mutex_unlock(&IMAP_LOCK);
  if(found) return i-1+getInodesStart();
  else return -1;
}

/*
Retrieve inode data from index number given by passing filename into getInode() func
@param ind : index in inode table of the inode bytes
@return a pointer to the inode constructed by the casting of data in block \ind
 */
inode* getInodeFromBlockNumber(int ind)
{
  return (inode*) readFromBlock(ind);
}

/*
Adds an io_WRITE request to the disk scheduler workload buffer
@param
  - block : block number to write to (offset write() syscall by block*blocksize)
  - data  : char[blocksize] containing the data that will be written by the syscall
@return void
*/
void writeToBlock(int block, char* data)
{
  disk_io_request* req = new disk_io_request;
  req->op = io_WRITE;
  req->data = data;
  req->block_number = block;
  req->done = 0;

  pthread_mutex_init( &req->lock, NULL);
  pthread_cond_init( &req->waitFor, NULL);

  addRequest(req);

  pthread_mutex_lock(&req->lock);
  while(!req->done)
    pthread_cond_wait(&req->waitFor, &req->lock);
  pthread_mutex_unlock(&req->lock);

}

/*
Adds an io_READ request to the disk scheduler workload buffer 
@param
  - block : block number to read from (offset read() syscall by block*blocksize)
@return char[blocksize] pointer to buffer containing data read from disk
*/
char* readFromBlock(int block)
{
  disk_io_request* req = new disk_io_request;
  req->op = io_READ;

  req->data = new char[getBlockSize()]();
  req->block_number = block;
  req->done = 0;

  pthread_mutex_init( &req->lock, NULL);
  pthread_cond_init(& req->waitFor, NULL);

  addRequest(req);

  pthread_mutex_lock(&req->lock);
  while(!req->done)
    pthread_cond_wait(&req->waitFor, &req->lock);
  pthread_mutex_unlock(&req->lock);

  return req->data;
}

/*
-- SPEC --
Causes  SSFS  to  create  a new file  named <SSFS file name>. If  <SSFS file name> exists,  the command should  fail. SSFS  does  not support directories/folders,  so  all file  names should  be  unique.
----------

@param filename: name of file to be created (inode->filename)
@return void
*/
void CREATE(const char* filename)
{
  if(filename[0] == 0)
  {
    cerr << "Invalid filename entered into CREATE argument " << endl;
  }
  //  if(getInode(filename) != -1) return;
  int inod = getEmptyInode();
  if(inod != -1)
    {
      char* data = new char[getBlockSize()]();
      memcpy(data, filename, strlen(filename));
      writeToBlock(inod, data);
    }
}

/*
-- SPEC --
This  command causes  the contents  of  <SSFS file name>, if  any,  to  be  overwritten by  the contents  of  the linux file  (in the CS  file  system) named <unix file name>. If  a file  named <SSFS file name> does not yet exist in SSFS, then  a new one should  be  created to  contain the contents  of  <unix file name>.
----------

Reads from unixFileName in blocksize chunks and pushes io_WRITE requests to the scheduler buffer to write the blocks to ssfsFileName
@param
  - ssfsFile : name of file on disk to be overwritten | created
  - unixFileName : name of file to be imported into SSFS
@return void
*/
void IMPORT(const char* ssfsFile, const char* unixFilename){
  /* Initializing */
  int block_size = getBlockSize();
  int num_blocks = getNumBlocks();
  int byteOffs =  block_size + MAX_INODES*block_size + (num_blocks/block_size);
  int max_file_size = block_size * (1 + NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int)))); //max number of blocks we can hold in a single file * num of bytes in a block

  int fd;
  if((fd = open(unixFilename, O_RDONLY)) < 0)
  {
    cerr << "Error opening file stream from " << unixFilename << endl;
    return;
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

  DELETE(ssfsFile);
  CREATE(ssfsFile);

  int inodeBlock = getInode(ssfsFile);
  inode* inode = getInodeFromBlockNumber(inodeBlock);
  inode->fileSize = filesize;

  int* indirect = 0;
  int* doubleIndirect = 0;

  int addrsPerBlock = getBlockSize() / 4;
  //i represents the # of blocks read at point in loop
  for(int i = 0; i < (filesize + block_size)/block_size; i++) //add block size to filesize to avoid truncating
    {
      char* read_buffer = new char[block_size]();
      int bytes_read;
      if((bytes_read = read(fd, read_buffer, block_size)) <= 0)
        {
          if(bytes_read == -1)
            cerr << "Error reading file" << endl;
          break;
        }
      if(i<NUM_DIRECT_BLOCKS)
        {
          int block = inode->direct[i];
          if(block == 0) block = (inode->direct[i] = getUnusedBlock());
          if(block == -1) break;

          writeToBlock(block, read_buffer);
        }
      else if (i < NUM_DIRECT_BLOCKS + getBlockSize()/4)
        {
          if(inode->indirect == 0) if((inode->indirect = getUnusedBlock()) == -1) break;

          if(indirect == 0) indirect = (int*)readFromBlock(inode->indirect);
          if(indirect[i-NUM_DIRECT_BLOCKS] == 0) if((indirect[i-NUM_DIRECT_BLOCKS] = getUnusedBlock()) == -1) break;

          writeToBlock(indirect[i-NUM_DIRECT_BLOCKS], read_buffer);
        }
      else
        {
          if(inode->doubleIndirect == 0) inode->doubleIndirect = getUnusedBlock();
          if(doubleIndirect == 0) doubleIndirect = (int*) readFromBlock(inode->doubleIndirect);

          int doubIndex = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)/addrsPerBlock;
          int doubIndexIntoBlock = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)%addrsPerBlock;

          if(doubleIndirect[doubIndex] == 0) doubleIndirect[doubIndex] = getUnusedBlock();
          int* currentBlock = (int*)readFromBlock(doubleIndirect[doubIndex]);
          if(currentBlock[doubIndexIntoBlock] == 0) currentBlock[doubIndexIntoBlock] = getUnusedBlock();

          writeToBlock(currentBlock[doubIndexIntoBlock], read_buffer);
          writeToBlock(doubleIndirect[doubIndex], (char*)currentBlock);
        }
    }
  if(indirect)writeToBlock(inode->indirect, (char*)indirect);
  if(doubleIndirect) writeToBlock(inode->doubleIndirect, (char*)doubleIndirect);

  writeToBlock(inodeBlock, (char*)inode);
}

void fillData(char* data, char c, int start, int num, int i)
{
  int startBlock = start/getBlockSize();
  int endBlock = (start+num)/getBlockSize();

  if(i != startBlock && i != endBlock) for(int j=0;j<getBlockSize();j++)data[j]=c;
  else if (i==startBlock&&i!=endBlock)for(int j = start%getBlockSize();j<getBlockSize();j++)data[j]=c;
  else if (i != startBlock && i==endBlock)for(int j=0;j<(start+num)%getBlockSize();j++)data[j]=c;
  else if (i == startBlock && i==endBlock)for(int j =start%getBlockSize();j<(start+num)%getBlockSize();j++)data[j]=c;
}

void WRITE(const char* fileName, char c, int start, int num)
{
  int inodeBlock = getInode(fileName);

  if(inodeBlock == -1) CREATE(fileName);

  inodeBlock = getInode(fileName);

  if(inodeBlock == -1) return;

  inode* inode = getInodeFromBlockNumber(inodeBlock);

  int startBlock = start/getBlockSize();
  int endBlock = (start+num)/getBlockSize();


  inode->fileSize = max(inode->fileSize, start+num);

  int* indirect = 0;
  int* doubleIndirect = 0;

  int addrsPerBlock = getBlockSize()/4;
  for(int i = startBlock;i<=endBlock;i++)
    {
      if(i<NUM_DIRECT_BLOCKS)
        {
          int block = inode->direct[i];
          if(block == 0) block = (inode->direct[i] = getUnusedBlock());
          if(block == -1) break;
          char* data = readFromBlock(block);
          fillData(data, c, start, num, i);

          writeToBlock(block, data);
        }
      else if (i < NUM_DIRECT_BLOCKS + getBlockSize()/4)
        {
          if(inode->indirect == 0) if((inode->indirect = getUnusedBlock()) == -1) break;

          if(indirect == 0) indirect = (int*)readFromBlock(inode->indirect);
          if(indirect[i-NUM_DIRECT_BLOCKS] == 0) if((indirect[i-NUM_DIRECT_BLOCKS] = getUnusedBlock()) == -1) break;

          char* data = readFromBlock(indirect[i-NUM_DIRECT_BLOCKS]);
          fillData(data,c,start,num,i);

          writeToBlock(indirect[i-NUM_DIRECT_BLOCKS], data);
        }
      else if ( i < 12+addrsPerBlock+addrsPerBlock*addrsPerBlock)
        {
          if(inode->doubleIndirect == 0) inode->doubleIndirect = getUnusedBlock();
          if(doubleIndirect == 0) doubleIndirect = (int*) readFromBlock(inode->doubleIndirect);

          int doubIndex = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)/addrsPerBlock;
          int doubIndexIntoBlock = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)%addrsPerBlock;

          if(doubleIndirect[doubIndex] == 0) doubleIndirect[doubIndex] = getUnusedBlock();
          int* currentBlock = (int*)readFromBlock(doubleIndirect[doubIndex]);
          if(currentBlock[doubIndexIntoBlock] == 0) currentBlock[doubIndexIntoBlock] = getUnusedBlock();

          char* data = readFromBlock(currentBlock[doubIndexIntoBlock]);
          fillData(data, c, start, num, i);

          writeToBlock(currentBlock[doubIndexIntoBlock], data);
          writeToBlock(doubleIndirect[doubIndex], (char*)currentBlock);
        }
      else
        {
          cerr << "Attempted to write past max file size, truncated" << endl;
          inode->fileSize = (12+addrsPerBlock+addrsPerBlock*addrsPerBlock)*getBlockSize();
          break;
        }
    }

  if(indirect)writeToBlock(inode->indirect, (char*)indirect);
  if(doubleIndirect) writeToBlock(inode->doubleIndirect, (char*)doubleIndirect);

  writeToBlock(inodeBlock, (char*)inode);
}

/*
-- SPEC --
This  command displays  the contents  of  <SSFS file name> on the screen, just  like  the unix  cat command.
----------

Issues read requests for blocks of fileName and pushes them into a buffer, then outputs each character in the buffer one at a time to the output stream
@param fileName : name of file on disk to be read from
@return void
*/
void CAT(const char* fileName){
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int ino = getInode(fileName);
  if(ino == -1)
    {
      printf("File doesnt exist\n");
      return;
    }
  inode* inod = getInodeFromBlockNumber(ino);

  READ(fileName, 0, inod->fileSize);
}

/*
-- SPEC --
Remove  the file  named <SSFS file name> from SSFS, and free  all of  its blocks, including the inode,  all data blocks, and all indirect  blocks associated with  the file.
----------

Removes inode associated with fileName from the tables that mark it as being in use, allowing it to be readily overwritten
  - We DO NOT wipe the data
@param
  - fileName : name of file on disk to be deleted
@return void
*/
void DELETE(const char* fileName)
{
  int ino = getInode(fileName);
  if(ino == -1) return;

  inode* inod = getInodeFromBlockNumber(ino);

  for(int i =0;i<NUM_DIRECT_BLOCKS;i++)
    {
      FREE_MAP[inod->direct[i]]=0;
    }

  if(inod->indirect)
    {
      int* indirect_block = (int*) readFromBlock(inod->indirect);
      FREE_MAP[inod->indirect] = 0;
      for(int i =0;i<getBlockSize()/4;i++)
        {
          FREE_MAP[indirect_block[i]] = 0;
        }
      delete[] (char*) indirect_block;
    }

  if(inod->doubleIndirect)
    {
      int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
      for(int i =0;i<getBlockSize()/4;i++)
        {
          int* indir_block = (int*) readFromBlock(dindirect_block[i]);
          for(int j=0;j<getBlockSize()/4;j++)
            {
              FREE_MAP[indir_block[j]] = 0;
            }
          FREE_MAP[dindirect_block[i]] = 0;
          delete[]  indir_block;
        }
      delete[]  dindirect_block;
    }
  FREE_MAP[inod->doubleIndirect] = 0;

  delete[]  inod;

  char* asdf = new char[getBlockSize()]();
  writeToBlock(ino, asdf);

  INODE_MAP[ino-getInodesStart()] = 0;
}

/*
-- SPEC --
This  command should  display <num bytes> of  file  <SSFS file name>, starting  at  byte  <start byte>.
----------

Read n bytes from file starting at given start point and send to cout
@param
  - fileName : name of file on disk to be read
  - start    : byte number to start reading at
  - num      : number of bytes to read
@return void
*/
void READ(const char* fileName, int start, int num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int ino = getInode(fileName);
  if(ino == -1)
  {
    printf("File doesnt exist\n");
    return;
  }

  inode* inod = getInodeFromBlockNumber(ino);
  int* indirect_block = (int*) readFromBlock(inod->indirect);
  int* doubleIndirect = 0;

  int fs = inod->fileSize;

  int start_block_num = start/getBlockSize();
  int start_block_index = start % getBlockSize();

  int end_block_num = (start+num)/getBlockSize();
  int end_block_index = (start + num)/getBlockSize();

  int buffer_blocks = 1 + (end_block_num - start_block_num); //I swear there's a good reason for this seperation

  char* read_buffer = new char[buffer_blocks * getBlockSize()]();

  int addrsPerBlock = getBlockSize()/4;
  int b = 0;

  for(int i = start_block_num; i <= end_block_num; i++)
  {
    if(i < NUM_DIRECT_BLOCKS)
      {
        char* block_buffer = readFromBlock(inod->direct[i]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else if(i < indirect_max_size)
      {
        char* block_buffer = readFromBlock(indirect_block[i - NUM_DIRECT_BLOCKS]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else
      {
        int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
        for(int i =0;i<getBlockSize()/4;i++)
          {
            if(!dindirect_block[i])continue;
            int* indir_block = (int*) readFromBlock(dindirect_block[i]);
            for(int j=0;j<getBlockSize()/4;j++)
              {
                if(!indir_block[j]) continue;
                char* block_buffer = readFromBlock(indir_block[j]);
                memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
                delete[] block_buffer;
                b++;
              }
            delete[]  indir_block;
          }
        delete[]  dindirect_block;
        break;
      }
    b++;
  }

  //mutex to console to ensure contig. output
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  cout << start % getBlockSize() << endl;
  cout << (start + num) % getBlockSize() << endl;
  int x = 0;
  for(int i = start%getBlockSize(); i < (start%getBlockSize() + num); i++)
  {
    printf("%c", read_buffer[i]);
    x++;
  }
  printf("\nPrinted %d characters\n",x);
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);

  if(inod->indirect)
    delete[] indirect_block;
  delete[] inod;
}

char* READ_with_return(const char* fileName, int start, int num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int ino = getInode(fileName);
  if(ino == -1)
  {
    printf("File doesnt exist\n");
    return NULL;
  }

  inode* inod = getInodeFromBlockNumber(ino);
  int* indirect_block = (int*) readFromBlock(inod->indirect);
  int* doubleIndirect = 0;

  int fs = inod->fileSize;

  int start_block_num = start/getBlockSize();
  int start_block_index = start % getBlockSize();

  int end_block_num = (start+num)/getBlockSize();
  int end_block_index = (start + num)/getBlockSize();

  int buffer_blocks = 1 + (end_block_num - start_block_num); //I swear there's a good reason for this seperation

  char* read_buffer = new char[buffer_blocks * getBlockSize()]();

  int addrsPerBlock = getBlockSize()/4;
  int b = 0;

  for(int i = start_block_num; i <= end_block_num; i++)
  {
    if(i < NUM_DIRECT_BLOCKS)
      {
        char* block_buffer = readFromBlock(inod->direct[i]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else if(i < indirect_max_size)
      {
        char* block_buffer = readFromBlock(indirect_block[i - NUM_DIRECT_BLOCKS]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else
      {
        int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
        for(int i =0;i<getBlockSize()/4;i++)
          {
            if(!dindirect_block[i])continue;
            int* indir_block = (int*) readFromBlock(dindirect_block[i]);
            for(int j=0;j<getBlockSize()/4;j++)
              {
                if(!indir_block[j]) continue;
                char* block_buffer = readFromBlock(indir_block[j]);
                memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
                delete[] block_buffer;
                b++;
              }
            delete[]  indir_block;
          }
        delete[]  dindirect_block;
        break;
      }
    b++;
  }

  /*
  //mutex to console to ensure contig. output
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  cout << start % getBlockSize() << endl;
  cout << (start + num) % getBlockSize() << endl;
  int x = 0;
  for(int i = start%getBlockSize(); i < (start%getBlockSize() + num); i++)
  {
    printf("%c", read_buffer[i]);
    x++;
  }
  printf("\nPrinted %d characters\n",x);
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);
  */

  if(inod->indirect)
    delete[] indirect_block;
  delete[] inod;

  return read_buffer;
}

void READ_file_redirect(const char* fileName, int start, int num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int ino = getInode(fileName);
  if(ino == -1)
  {
    printf("File doesnt exist\n");
    return;
  }

  inode* inod = getInodeFromBlockNumber(ino);
  int* indirect_block = (int*) readFromBlock(inod->indirect);
  int* doubleIndirect = 0;

  int fs = inod->fileSize;

  int start_block_num = start/getBlockSize();
  int start_block_index = start % getBlockSize();

  int end_block_num = (start+num)/getBlockSize();
  int end_block_index = (start + num)/getBlockSize();

  int buffer_blocks = 1 + (end_block_num - start_block_num); //I swear there's a good reason for this seperation

  char* read_buffer = new char[buffer_blocks * getBlockSize()]();

  int addrsPerBlock = getBlockSize()/4;
  int b = 0;

  for(int i = start_block_num; i <= end_block_num; i++)
  {
    if(i < NUM_DIRECT_BLOCKS)
      {
        char* block_buffer = readFromBlock(inod->direct[i]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else if(i < indirect_max_size)
      {
        char* block_buffer = readFromBlock(indirect_block[i - NUM_DIRECT_BLOCKS]);
        memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
        delete[] block_buffer;
      }
    else
      {
        int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
        for(int i =0;i<getBlockSize()/4;i++)
          {
            if(!dindirect_block[i])continue;
            int* indir_block = (int*) readFromBlock(dindirect_block[i]);
            for(int j=0;j<getBlockSize()/4;j++)
              {
                if(!indir_block[j]) continue;
                char* block_buffer = readFromBlock(indir_block[j]);
                memcpy(read_buffer + (b*getBlockSize()), block_buffer, getBlockSize());
                delete[] block_buffer;
                b++;
              }
            delete[]  indir_block;
          }
        delete[]  dindirect_block;
        break;
      }
    b++;
  }

  /*
  //mutex to console to ensure contig. output
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  cout << start % getBlockSize() << endl;
  cout << (start + num) % getBlockSize() << endl;
  int x = 0;
  for(int i = start%getBlockSize(); i < (start%getBlockSize() + num); i++)
  {
    printf("%c", read_buffer[i]);
    x++;
  }
  printf("\nPrinted %d characters\n",x);
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);
  */

  if(inod->indirect)
    delete[] indirect_block;
  delete[] inod;
}


/*
-- SPEC --
Closes the  DISK  file  after the Disk  Scheduler thread  has finished  servicing all pending requests, and exits the ssfs
process.
----------

Signals the system to shutdown - which will block injections into the request queue and allow the system to finish servicing pending requests
@return void
*/
void SHUTDOWN()
{
  shut = 1;
}

/*
-- SPEC --
List  all the names and sizes of  the files in  SSFS.
----------

Read each valid inode and print out its filename and filesize members
@return void
*/
void LIST()
{
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  cout << "FILE CONTENTS " << endl;
  for(int i = 0; i < MAX_INODES; i++)
  {
    if(INODE_MAP[i] == 0) continue;
    inode* inod = getInodeFromBlockNumber(i+getInodesStart());
    cout << "FILE:\t" << inod->fileName << "\t" << inod->fileSize << endl;
  }
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);
}

int thread = 0;

/*
Thread worker function to process text files and build appropriate requests for the disk scheduler thread (max 4 threads)
@param file_arg as text file containing valid commands to input to the fs
@return void*
*/
void*
process_ops(void* file_arg)
{
	//string op_file_name = "thread1ops.txt";
  char* op_file_name = (char*)file_arg;
	cout << "Thread created:\t" << op_file_name << endl;

  int threadNum = thread++;

	/* Parsing */
	ifstream input_stream(op_file_name);
	string linebuff;
	while(getline(input_stream, linebuff))
	{
    if(shut) break;
    stringstream ss(linebuff);
    string command;
    ss >> command;
  
    if(command == "#")
    {
      continue;
    }
    else if(command == "EXPORT")
    {
      string file1;
      string file2;
      ss >> file1;
      ss >> file2;
      EXPORT(file1.c_str(), file2.c_str());
    }
    else if(command == "CREATE")
      {
        string fileName;
        ss >> fileName;

        CREATE(fileName.c_str());
      }
    else if (command == "IMPORT")
      {
        string file1;
        string file2;
        ss >> file1;
        ss >> file2;

        IMPORT(file1.c_str(), file2.c_str());
      }
    else if (command == "CAT")
      {
        string file1;
        ss >> file1;

        CAT(file1.c_str());
      }
    else if (command == "DELETE")
      {
        string file1;
        ss >> file1;

        DELETE(file1.c_str());
      }
    else if (command == "WRITE")
      {
        string file1;
        char c;
        int start;
        int num;
        ss >> file1;
        ss >> c;
        ss >> start;
        ss >> num;

        WRITE(file1.c_str(), c, start, num);
      }
    else if (command == "READ")
      {
        string file1;
        int start;
        int num;

        ss >> file1;
        ss >> start;
        ss >> num;

        READ(file1.c_str(), start, num);
      }
    else if (command == "LIST")
      {
        LIST();
      }
    else if (command == "SHUTDOWN")
      {
        SHUTDOWN();
      }
    else if(command == "MV")
      {
        string file1;
        string file2;
        ss >> file1;
        ss >> file2;

        MV(file1.c_str(), file2.c_str());
      }
    else if(command == "CP")
      {
       string file1;
       string file2;
       ss >> file1;
       ss >> file2;

       CP(file1.c_str(), file2.c_str());
      }
    else
      {
        printf("Unrecognized thread operation, skipping to the next operation\n");
      }
	}
	return NULL;
}


char* getFREE_MAP(){return FREE_MAP; }
char* getINODE_MAP(){return INODE_MAP; }
char* getSUPER(){return SUPER; }

int
getNumBlocks()
{
  return ((int*) SUPER)[0];
}

int
getBlockSize()
{
  return ((int*) SUPER)[1];
}

int
getFreeMapStart()
{
  return ((int*) SUPER)[2];
}

int
getFreeMapSize()
{
  return ((int*) SUPER)[3];
}

int
getInodeMapStart()
{
  return ((int*) SUPER)[4];
}

int
getInodeMapSize()
{
  return ((int*) SUPER)[5];
}

int
getInodesStart()
{
  return ((int*) SUPER)[6];
}

int
getUserDataStart()
{
  return ((int*) SUPER)[7];
}

/* AUXILIARY FUNCTIONS */
//Copies all metadata/data blocks to new locations effectively duplicating the file
//This does NOT just make a new inode that points to the same location
void CP(const char* fileName, const char* newFileName)
{
  int block_size = getBlockSize();
  int num_blocks = getNumBlocks();
  int byteOffs =  block_size + MAX_INODES*block_size + (num_blocks/block_size);
  int max_file_size = block_size * (1 + NUM_DIRECT_BLOCKS + (block_size/sizeof(int)) + ((block_size*block_size)/(sizeof(int)*sizeof(int)))); //max number of blocks we can hold in a single file * num of bytes in a block

  DELETE(newFileName);
  CREATE(newFileName);

  int orig_inode_block = getInode(fileName);
  inode* orig = getInodeFromBlockNumber(orig_inode_block);

  int inodeBlock = getInode(newFileName);
  inode* inode = getInodeFromBlockNumber(inodeBlock);
  inode->fileSize = orig->fileSize;
  int filesize = inode->fileSize;

  int* indirect = 0;
  int* doubleIndirect = 0;

  int addrsPerBlock = getBlockSize() / 4;
  //i represents the # of blocks read at point in loop
  for(int i = 0; i < (filesize + block_size)/block_size; i++) //add block size to filesize to avoid truncating
    {
      char* read_buffer = READ_with_return(fileName, (i*getBlockSize()), getBlockSize());
      if(i<NUM_DIRECT_BLOCKS)
        {
          int block = inode->direct[i];
          if(block == 0) block = (inode->direct[i] = getUnusedBlock());
          if(block == -1) break;
          writeToBlock(block, read_buffer);
        }
      else if (i < NUM_DIRECT_BLOCKS + getBlockSize()/4)
        {
          if(inode->indirect == 0) if((inode->indirect = getUnusedBlock()) == -1) break;
          if(indirect == 0) indirect = (int*)readFromBlock(inode->indirect);
          if(indirect[i-NUM_DIRECT_BLOCKS] == 0) if((indirect[i-NUM_DIRECT_BLOCKS] = getUnusedBlock()) == -1) break;
          writeToBlock(indirect[i-NUM_DIRECT_BLOCKS], read_buffer);
        }
      else
        {
          if(inode->doubleIndirect == 0) inode->doubleIndirect = getUnusedBlock();
          if(doubleIndirect == 0) doubleIndirect = (int*) readFromBlock(inode->doubleIndirect);

          int doubIndex = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)/addrsPerBlock;
          int doubIndexIntoBlock = (i-NUM_DIRECT_BLOCKS-addrsPerBlock)%addrsPerBlock;
          if(doubleIndirect[doubIndex] == 0) doubleIndirect[doubIndex] = getUnusedBlock();
          
          int* currentBlock = (int*)readFromBlock(doubleIndirect[doubIndex]);
          if(currentBlock[doubIndexIntoBlock] == 0) currentBlock[doubIndexIntoBlock] = getUnusedBlock();

          writeToBlock(currentBlock[doubIndexIntoBlock], read_buffer);
          writeToBlock(doubleIndirect[doubIndex], (char*)currentBlock);
        }
    }
  if(indirect)writeToBlock(inode->indirect, (char*)indirect);
  if(doubleIndirect) writeToBlock(inode->doubleIndirect, (char*)doubleIndirect);

  writeToBlock(inodeBlock, (char*)inode);
}

void EXPORT(const char* fileName, const char* unixFileName){
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));
  
  ofstream outfile;
  outfile.open(unixFileName);
  
  int ino = getInode(fileName);
  if(ino == -1)
    {
      cout << "Failed opening from inode : " << fileName << endl;
      printf("File doesnt exist ::: HELLO???\n");
      return;
    }
  inode* inod = getInodeFromBlockNumber(ino);
  char* rbuff = READ_with_return(fileName, 0, inod->fileSize);
    
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  for(int i = 0; i < inod->fileSize; i++)
  {
    outfile << rbuff[i];
  }
  outfile << endl;
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);

  outfile.close();
  DELETE(fileName);
}

//Copy the file and all of its metadata/contents to a new file location and delete the old file
//This really just copies the inode into a new inode since we aren't supporting directories
//I had just finished this when I realized that this is just renaming the file and I could have just changed the name
//If I have time later I'll change it to just do that instead of all this, regardless itll help with CP
void MV(const char* fileName, const char* newFileName)
{
  //Create new file inode, copy all original inode material into here */
  CREATE(newFileName);

  int orig_inode_block = getInode(fileName);
  inode* orig = getInodeFromBlockNumber(orig_inode_block);

  int new_inode_block = getInode(newFileName);
  inode* new_block = getInodeFromBlockNumber(new_inode_block);
  
  new_block->fileSize = orig->fileSize;
  for(int i = 0; i < NUM_DIRECT_BLOCKS; i++)
  {
    new_block->direct[i] = orig->direct[i];
  }
  new_block->indirect = orig->indirect;
  new_block->doubleIndirect = orig->doubleIndirect;
  
  //print_inode_contents(orig);
  //print_inode_contents(new_block);

  //Write changes to new file  
  writeToBlock(new_inode_block, (char*)new_block);

  //Remove old file
  int inode_location_in_map = orig_inode_block - getInodesStart();
  char* asf = new char[getBlockSize()];
  INODE_MAP[inode_location_in_map] = 0;
  writeToBlock(inode_location_in_map, asf);
 
}

void print_inode_contents(inode* i)
{
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  cout << "INODE CONTENTS:" << endl;
  cout << i->fileName << endl << i->fileSize << " bytes" << endl;
  cout << "Direct Blocks: " << endl;
  for(int j = 0; j < NUM_DIRECT_BLOCKS; j++)
  {
    cout << i->direct[j] << endl;
  }
  cout << "Indirect: " << i->indirect << endl;
  cout << "D-Indirect: " << i->doubleIndirect << endl;
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);
}

int main(int argc, char const *argv[])
{
  /* Parsing terminal inputs */
	if(argc > 2)
	{
		if(argc > 6)
		{
			cerr << "Invalid input: too many arguments - this application supports a maximum of 4 threads." << endl;
			cerr << "Usage: ssfs <disk file name> thread1ops.txt thread2ops.txt thread3ops.txt" << endl;
			exit(EXIT_FAILURE);
		}
		cout << "argc:\t" << argc << endl;

		/*
      Creates n pthreads and passes in the appropriate file names to the worker func
      changed ur giant shit fest to 3 lines u baboon
    */

    /*scheduler thread*/
    SCH_struct* str = new SCH_struct;
    str->requests = requests;
    str->lock = REQUESTS_LOCK;
    str->fd = open(argv[1], O_RDWR);

    SUPER = new char[32];
    read(str->fd, SUPER, 32);

    FREE_MAP = new char[getNumBlocks()];

    pthread_create(&SCH_thread, NULL, SCH_run, (void*) str);

    for(int i =0;i<getNumBlocks()/getBlockSize();i++)
      memcpy(FREE_MAP+i*getBlockSize(), readFromBlock(getFreeMapStart()+i), getBlockSize());

    for(int i =0;i<256/getBlockSize();i++)
      memcpy(INODE_MAP+i*getBlockSize(), readFromBlock(getInodeMapStart()+i), getBlockSize());

    for(int i = 2; i < argc;i++)
      {
        pthread_create(&op_thread[i-2], NULL, process_ops, (void*)argv[i]);
      }

	}
  while(!0);
}
