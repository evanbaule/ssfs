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
    // Maybe need to do i % blockSize() here or something to index into individual bytes
    if(FREE_MAP[i] == 0)
      {
        FREE_MAP[i] = 1;
        return i; //returns the block # of the unassigned block
      }
  }
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
  //this should dramatically speed up empty inode allocation
  for(int i = 0; i < MAX_INODES; i++)
  {
    if(INODE_MAP[i] == 0)
    {
      INODE_MAP[i] = 1;
      return i+getInodesStart();
    }
  }
  return -1; // DISK IS FULL
  /* DEPRECATED
  bool found=0;
  //Indexing is all off for this
  int i;
  for(i=0;i<MAX_INODES && !found;i++)
    {
      //If we find
      if(INODE_MAP[i] == 0)
      {
        return i+1;
      }
      disk_io_request req;
      req.op = io_READ;
      char* data = new char[getBlockSize()];
      uint blockNumber = i+1;
      req.data = data;
      req.block_number = blockNumber;
      req.done = 0;

      pthread_mutex_init( &req.lock, NULL);
      pthread_cond_init( &req.waitFor, NULL);

      addRequest(&req);

      cout << "getEmptyInode() Fetching block " << blockNumber << endl;

      pthread_mutex_lock(&req.lock);
      while(!req.done)
        pthread_cond_wait(&req.waitFor, &req.lock);
      pthread_mutex_unlock(&req.lock);

      if(data[0] == 0)
        found = 1;
      delete[](data);
      if(found) break;
    }
  if(found) return i+1;
  else return -1;
  */
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
  addRequest(req);
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
  pthread_cond_init( &req->waitFor, NULL);

  addRequest(req);

  pthread_mutex_lock(&req->lock);
  while(!req->done)
    pthread_cond_wait(&req->waitFor, &req->lock);
  pthread_mutex_unlock(&req->lock);

  return req->data;
}

/*
Flips the bytemap respective to a blog to indicate that the block has been either assigned or released
@param
  - block : block number being changed (index in map)
  - flag  : whether we are indicating that the block is now taken or free
@return void
*/
void setByteMap(int block, bool flag)
{
  if(block < 1+256+getBitmapSize()) return;

  int blockByteLoc = 4*block/getBlockSize();
  blockByteLoc+=(1+256);
  block%=(getBlockSize()/4);

  int* data = (int*) readFromBlock(blockByteLoc);
  data[block] = flag;

  writeToBlock(blockByteLoc, (char*) data);

  delete data;
}

/*
Look to see if a particular block is free or taken
@param block : block number to check availability of
@return if(block is taken)
*/
bool getByteMap(int block)
{
  if(block < 1+256+getBitmapSize()) return 1;

  int blockByteLoc = block/getBlockSize();
  blockByteLoc+=(1+256);
  block%=(getBlockSize());

  char* data = (char*) readFromBlock(blockByteLoc);

  bool tmp = data[block];
  delete[] (char*)data;
  return tmp;
}

// ???
int getFreeByteMapInBlock(int block)
{
  if(block < 1+256+getBitmapSize()) return -1;

  int blockByteLoc = block/getBlockSize();
  blockByteLoc+=(1+256);

  char* data = (char*) readFromBlock(blockByteLoc);
  int ret = -1;
  for(int i =0;i<getBlockSize();i++)
    {
      if(data[i] == 0)
        {
          ret = i;
          break;
        }
    }
  return ret;
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

  DELETE(ssfsFile);
  CREATE(ssfsFile);

  int inodeBlock = getInode(ssfsFile);
  inode* ino = getInodeFromBlockNumber(inodeBlock);
  ino->fileSize = filesize;
  int* indirs = new int[getBlockSize()/4]();

  int* doubleIndirs1 = new int[getBlockSize()/4]();

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
      if(i < NUM_DIRECT_BLOCKS)
        {
          ino->direct[i] = getUnusedBlock();
          writeToBlock(ino->direct[i], read_buffer);
        }
      else if (i < NUM_DIRECT_BLOCKS + getBlockSize()/4)
        {
          if(ino->indirect == 0)
            ino->indirect = getUnusedBlock();
          indirs[i-NUM_DIRECT_BLOCKS] = getUnusedBlock();
          writeToBlock(indirs[i-NUM_DIRECT_BLOCKS], read_buffer);
        }
      else
        {
          if(ino->doubleIndirect == 0)
            ino->doubleIndirect = getUnusedBlock();

          int index = i-(NUM_DIRECT_BLOCKS + getBlockSize()/4);
          int doubIndex =  index / (getBlockSize()/4);
          int indirIndex = index % (getBlockSize()/4);

          int* doubInt = (int*) readFromBlock(ino->doubleIndirect);
          if(doubInt[doubIndex] == 0)
            doubInt[doubIndex] = getUnusedBlock();

          int* indirectBlock = (int*) readFromBlock(doubInt[doubIndex]);
          indirectBlock[indirIndex] = getUnusedBlock();

          writeToBlock(indirectBlock[indirIndex], read_buffer);

          writeToBlock(doubInt[doubIndex],  (char*) indirectBlock);

          writeToBlock(ino->doubleIndirect, (char*) doubInt);
        }
    }

  if(ino->indirect!=0)
    writeToBlock(ino->indirect, (char*) indirs);
  writeToBlock(inodeBlock, (char*) ino);
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
  inode* inode = getInodeFromBlockNumber(inodeBlock);

  int startBlock = start/getBlockSize();
  int endBlock = (start+num)/getBlockSize();

  int* indirect = 0;
  for(int i = startBlock;i<=endBlock;i++)
    {
      if(i<NUM_DIRECT_BLOCKS)
        {
          int block = inode->direct[i];
          if(block == 0) block = (inode->direct[i] = getUnusedBlock());
          char* data = readFromBlock(block);
          fillData(data, c, start, num, i);

          writeToBlock(block, data);
        }
      else if (i < NUM_DIRECT_BLOCKS + getBlockSize()/4)
        {
          if(inode->indirect == 0) inode->indirect = getUnusedBlock();
          if(indirect == 0) indirect = (int*)readFromBlock(inode->indirect);
          if(indirect[i-NUM_DIRECT_BLOCKS] == 0) indirect[i-NUM_DIRECT_BLOCKS] = getUnusedBlock();

          char* data = readFromBlock(indirect[i-NUM_DIRECT_BLOCKS]);
          fillData(data,c,start,num,i);

          writeToBlock(indirect[i-NUM_DIRECT_BLOCKS], data);
        }
      else
        {
          //lol double indirect
        }
    }

  if(indirect)writeToBlock(inode->indirect, (char*)indirect);
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

  int* indirect_block = (int*) readFromBlock(inod->indirect);
  //  int* double_indirect_block = (int*) readFromBlock(inod->doubleIndirect);

  int file_blocks = (int) ceil((double)inod->fileSize / getBlockSize());
  int fs = inod->fileSize;
  char* cat_buffer = new char[fs](); //will read blocks into this buffer

  for(int i = 0; i < file_blocks; i++)
  {
    if(i < NUM_DIRECT_BLOCKS)
    {
      char* block_buffer = readFromBlock(inod->direct[i]);
      memcpy(cat_buffer + (i*getBlockSize()), block_buffer, getBlockSize());
      delete block_buffer;
    }
    else if(i >= NUM_DIRECT_BLOCKS && i < indirect_max_size)
    {
      char* block_buffer = readFromBlock(indirect_block[i - NUM_DIRECT_BLOCKS]);
      memcpy(cat_buffer + (i*getBlockSize()), block_buffer, getBlockSize());
      delete block_buffer;
    }
    else if(i >= indirect_max_size && i < double_indirect_max_size)
    {
      cout << "Fuck double indirect rn" << endl;
    }
  }

  //mutex to console to ensure contig. output
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  for(int i = 0; i < fs; i++)
  {
    printf("%c", cat_buffer[i]);
  }
  printf("\n");
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);

  //  delete[] double_indirect_block;
  delete[] indirect_block;
  delete[] inod;
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

  int* indirect_block = (int*) readFromBlock(inod->indirect);
  if(inod->indirect)
  for(int i =0;i<getBlockSize()/4;i++)
    {
      FREE_MAP[indirect_block[i]] = 0;
    }

  delete[] (char*) indirect_block;

  int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
  if(inod->doubleIndirect)
  for(int i =0;i<getBlockSize()/4;i++)
    {
      int* indir_block = (int*) readFromBlock(dindirect_block[i]);
      for(int j=0;j<getBlockSize()/4;j++)
        {
          FREE_MAP[indir_block[j]] = 0;
        }
      delete[] (char*) indir_block;
    }
  delete[] (char*) dindirect_block;

  delete[] (char*) inod;

  char* asdf = new char[getBlockSize()]();
  writeToBlock(ino, asdf);

  INODE_MAP[ino-getInodesStart()] = 0;
}

/*
-- SPEC --
Write <num bytes> copies  of  character <char> into file  <SSFS file name>, beginning at byte <startbyte>. If there are fewer than  <start byte> bytes  in  the file, the command should  report  an  error.  If there are fewer than  <start byte> + <num bytes> bytes  in  the file  before  the command runs, the file  should  be appended  to  make  room  for the extra characters. If  not enough  free  file  blocks  exist to  complete  the WRITE command, you may either  write as  many  bytes as  you can and then  abort the command,  or  just  not carry out the command,  instead returning an  error message.  (You  may choose  whichever of  those two options you prefer.)
----------

Copies &c into fileName from start to (start + num)
@param
  - fileName : name of file on disk to be written to
  - c        : character to write into bytes
  - start    : byte number to start writing at
  - num      : number of copies of c to write
@return void

void WRITE(const char* fileName, char c, uint start, uint num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int id = getInode(fileName);
  inode* inod = getInodeFromBlockNumber(id);

  char* start_block = new char[getBlockSize()]();
  for(int i = start; i < getBlockSize(); i++)
  {
    memcpy(start_block + i, &c, sizeof(char));
    //Should cover corner case where (start + num) < blocksize so we only write 1 block 
    if(i == (start + num))
    {
      break;
    }
  }
  char* middle_block = new char[getBlockSize()]();
  for(int i = 0; i < getBlockSize(); i++)
  {
    memcpy(middle_block + i, &c, sizeof(char));
  }
  char* end_block = new char[getBlockSize()]();
  for(int i = 0; i < num % getBlockSize(); i++)
  {
    memcpy(end_block + i, &c, sizeof(char));
  }

  int last_block = (start + num)/getBlockSize();
  for(int i = 0; i < last_block; i++)
  {
    int curr_block = i/getBlockSize();
    if(curr_block < NUM_DIRECT_BLOCKS)
    {
      //If we're writing to the first block we start @ \start
      if(curr_block == 0)
      {
        char* data = new char[getBlockSize()]();
        memcpy(data, start_block, getBlockSize());
        writeToBlock(inod->direct[curr_block], data);
      }
      //If we only wrote one block
      if(last_block == 0)
      {
        break;
      }
      //If we're writing to the last block
      if(curr_block == last_block)
      {
        char* data = new char[getBlockSize()]();
        memcpy(data, end_block, getBlockSize());
        writeToBlock(inod->direct[curr_block], end_block);
        break;
      }
      //Writing to some block full of &c*blocksize in the middle
      else 
      {
        char* data = new char[getBlockSize()]();
        memcpy(data, middle_block, getBlockSize());
        writeToBlock(inod->direct[curr_block], middle_block);
      }
    }
    else if((curr_block >= NUM_DIRECT_BLOCKS) && (curr_block < (indirect_max_size))){
      int* indirect_block = (int*) readFromBlock(inod->indirect);
      int destination_block_num = indirect_block[curr_block - NUM_DIRECT_BLOCKS];

      if(curr_block == last_block)
      {
        char* data = new char[getBlockSize()]();
        memcpy(data, end_block, getBlockSize());
        writeToBlock(inod->direct[curr_block], end_block);
        writeToBlock(destination_block_num, end_block);
        break;
      }
      else 
      {
        char* data = new char[getBlockSize()]();
        memcpy(data, end_block, getBlockSize());
        writeToBlock(inod->direct[curr_block], end_block);
        writeToBlock(destination_block_num, middle_block);
      }
      delete[] indirect_block;
    }
    else if(curr_block >= indirect_max_size && curr_block < double_indirect_max_size)
    {
      /*
      int* double_indirect_block = (int*) readFromBlock(inod->doubleIndirect);
      int indir_block_num = double_indirect_block[curr_block - (NUM_DIRECT_BLOCKS + (getBlockSize()/sizeof(int)))) / (getBlockSize() / sizeof(int)))];
      int* indirect_block = (int*) readFromBlock(indir_block_num);
      int destination_block_num = indir_block + (curr_block - indirect_max_size);
      if(curr_block == last_block)
      {
        writeToBlock(destination_block_num, end_block);
        break;
      }
      else 
      {
        writeToBlock(destination_block_num, middle_block);
      }

      delete[] double_indirect_block;
      delete[] indirect_block;
      
    }
  }
  delete inod;
}
*/
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
void READ(const char* fileName, uint start, uint num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));

  int ino = getInode(fileName);
  if(ino == -1) return;

  inode* inod = getInodeFromBlockNumber(ino);

  int* indirect_block = (int*) readFromBlock(inod->indirect);
  int* double_indirect_block = (int*) readFromBlock(inod->doubleIndirect);

  int file_blocks = inod->fileSize / getBlockSize();
  int fs = inod->fileSize;
  char* cat_buffer = new char[fs](); //will reach blocks into this buffer

  for(int i = 0; i < file_blocks; i++)
  {
    if(i < NUM_DIRECT_BLOCKS)
    {
      int blocknum_to_req = inod->direct[i];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(cat_buffer + (i*getBlockSize()), block_buffer, getBlockSize());
      delete[] block_buffer;
    }
    else if(i >= NUM_DIRECT_BLOCKS && i < indirect_max_size)
    {
      int blocknum_to_req = indirect_block[i - NUM_DIRECT_BLOCKS];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(cat_buffer + (i*getBlockSize()), block_buffer, getBlockSize());
      delete[] block_buffer;
    }
    else if(i >= indirect_max_size && i < double_indirect_max_size)
    {
      cout << "Fuck double indirect rn" << endl;
    }
  }

  //mutex to console to ensure contig. output
  pthread_mutex_lock(&CONSOLE_OUT_LOCK);
  for(int i = 0; i < sizeof(cat_buffer); i++)
  {
    cout << cat_buffer[i];
  }
  pthread_mutex_unlock(&CONSOLE_OUT_LOCK);

  delete[] double_indirect_block;
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

    if(command == "CREATE")
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
        uint start;
        uint num;
        ss >> file1;
        ss >> c;
        ss >> start;
        ss >> num;

        WRITE(file1.c_str(), c, start, num);
      }
    else if (command == "READ")
      {
        string file1;
        uint start;
        uint num;

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

/* DEPRECATED
char*
getDisk()
{
  return DISK;
}

int getNumBlocks()
{
  return ((int*) DISK)[0];
}

int getBlockSize()
{
  return ((int*) DISK)[1];
}
//Returns the block number where the free map begins
int getFreeMapStart()
{
  return ((int*) DISK)[2];
}
//Returns the block number where the inode map begins
int getInodeMapStart()
{
  return ((int*) DISK)[3];
}
//Returns the block number where we begin to store inodes and user data blocks
int getDataStart()
{
  return ((int*) DISK)[4];
}
*/

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

    for(int i = 2; i < argc;i++)
      {
        pthread_create(&op_thread[i-2], NULL, process_ops, (void*)argv[i]);
      }

	}
  while(!shut);
}
