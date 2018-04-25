#include "ssfs.hpp"

using namespace std;

pthread_t op_thread[4];
pthread_t SCH_thread;

/*byte array of the disk in memory*/
char* DISK;

/*Any access to the DISK array should be locked by this*/
pthread_mutex_t DISK_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t CONSOLE_OUT_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t REQUESTS_LOCK = PTHREAD_MUTEX_INITIALIZER;

queue<disk_io_request*>* requests = new queue<disk_io_request*>;

bool shut=0;
bool isShutdown() {return shut;}

int getBitmapSize()
{
  return (getNumBlocks()/getBlockSize());
}

int getUnusedBlock()
{
  for(int i=257;i<getNumBlocks();i++)
    {
      int asdf = getFreeByteMapInBlock(i);
      if(asdf != -1) return asdf;
    }
  return -1;
}

void addRequest(disk_io_request* req)
{
  pthread_mutex_lock(&REQUESTS_LOCK);
  requests->push(req);
  pthread_mutex_unlock(&REQUESTS_LOCK);
}

int getEmptyInode()
{
  bool found=0;
  int i;
  for(i=0;i<MAX_INODES && !found;i++)
    {
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
}

/*
int getDataStart()
{
  return (getBlockSize() + MAX_INODES*getBlockSize() + getBitmapSize());
}
*/

int getInode(const char* file)
{
  bool found=0;
  int i;
  for(i=0;i<MAX_INODES && !found;i++)
    {
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

      //cout << "getInode() Fetching block " << blockNumber << endl;

      pthread_mutex_lock(&req.lock);
      while(!req.done){
        pthread_cond_wait(&req.waitFor, &req.lock);
      }
      pthread_mutex_unlock(&req.lock);

      if(strncmp(data, file, MAX_FILENAME_SIZE) == 0)
        found = 1;
      delete[](data);
    }
  if(found) return i+1;
  else return -1;
}

inode* getInodeFromBlockNumber(int ind)
{
  disk_io_request req;
  req.op = io_READ;
  char* data = new char[getBlockSize()];
  uint blockNumber = ind;
  req.data = data;
  req.block_number = blockNumber;

  req.waitFor = PTHREAD_COND_INITIALIZER;
  req.lock = PTHREAD_MUTEX_INITIALIZER;

  addRequest(&req);

  while(!req.done)
    pthread_cond_wait(&req.waitFor, &req.lock);

  return (inode*) data;
}

void writeToBlock(int block, char* data)
{
  disk_io_request req;
  req.op = io_WRITE;
  req.data = data;
  req.block_number = block;
  addRequest(&req);
}

char* readFromBlock(int block)
{
  disk_io_request req;
  req.op = io_READ;
  req.data = new char[getBlockSize()]();
  req.block_number = block;

  req.waitFor = PTHREAD_COND_INITIALIZER;
  req.lock = PTHREAD_MUTEX_INITIALIZER;

  while(!req.done)
    pthread_cond_wait(&req.waitFor, &req.lock);

  addRequest(&req);

  return req.data;
}

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
int getStartOfDataBlocks()
{
  return getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
}
*/


void CREATE(const char* filename)
{
  if(filename[0] == 0)
  {
    cerr << "Invalid filename entered into CREATE argument " << endl;
  }
  int inod = getEmptyInode();
  cout << "got node" << endl;
  if(inod != -1)
    {
      char* data = new char[getBlockSize()]();
      memcpy(data, filename, strlen(filename));

      writeToBlock(inod, data);
      cout << "wrote to block" << endl;
      delete[] data;
    }
}

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
  char* read_buffer = new char[block_size]();
  for(int i = 0; i < (filesize + block_size)/block_size; i++) //add block size to filesize to avoid truncating
  {
    int bytes_read;
    if((bytes_read = read(fd, &read_buffer, block_size)) != block_size)
    {
      cerr << "Read the wrong number of bytes into read_buffer OR maybe reached end of the file. Not sure tbh - EMB" << endl;
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

  writeToBlock(ino->indirect, (char*) indirs);

  writeToBlock(inodeBlock, (char*) ino);

  delete[] indirs;
  delete[] read_buffer;
  delete[] (char*) ino;
}

void CAT(const char* fileName){
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
    if(file_blocks < NUM_DIRECT_BLOCKS)
    {
      int blocknum_to_req = inod->direct[i];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(&cat_buffer + (i*getBlockSize()), &block_buffer, getBlockSize());
      delete block_buffer;
    }
    else if(i >= NUM_DIRECT_BLOCKS && i < indirect_max_size)
    {
      int blocknum_to_req = indirect_block[i - NUM_DIRECT_BLOCKS];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(&cat_buffer + (i*getBlockSize()), &block_buffer, getBlockSize());
      delete block_buffer;
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

void DELETE(const char* fileName)
{
  int ino = getInode(fileName);
  if(ino == -1) return;

  inode* inod = getInodeFromBlockNumber(ino);

  inod->fileSize = 0;

  for(int i =0;i<NUM_DIRECT_BLOCKS;i++)
    {
      setByteMap(inod->direct[i], false);
    }

  int* indirect_block = (int*) readFromBlock(inod->indirect);
  for(int i =0;i<getBlockSize()/4;i++)
    {
      setByteMap(indirect_block[i], false);
    }

  delete[] (char*) indirect_block;

  int* dindirect_block = (int*) readFromBlock(inod->doubleIndirect);
  for(int i =0;i<getBlockSize()/4;i++)
    {
      int* indir_block = (int*) readFromBlock(dindirect_block[i]);
      for(int j=0;j<getBlockSize()/4;j++)
        {
          setByteMap(indir_block[j], false);
        }
      delete[] (char*) indir_block;
    }
  delete[] (char*) dindirect_block;

  delete[] (char*) inod;

  char* asdf = new char[getBlockSize()]();
  writeToBlock(ino, asdf);
  delete[] asdf;
}

void WRITE(const char* fileName, char c, uint start, uint num)
{
  int indirect_max_size = NUM_DIRECT_BLOCKS + getBlockSize()/sizeof(int); // # of blocks that can be refferenced by an indirect block
  int double_indirect_max_size = indirect_max_size + (getBlockSize()/sizeof(int)) * (getBlockSize()/sizeof(int));
  
  int id = getInode(fileName);
  inode* inod = getInodeFromBlockNumber(id);

  char* start_block;
  for(int i = start; i < getBlockSize(); i++)
  {
    memcpy(&start_block + i, &c, sizeof(char));
    //Should cover corner case where (start + num) < blocksize so we only write 1 block */
    if(i == (start + num))
    {
      break;
    }
  }
  char* middle_block;
  for(int i = 0; i < getBlockSize(); i++)
  {
    memcpy(&middle_block + i, &c, sizeof(char));
  }
  char* end_block;
  for(int i = 0; i < (i+num) % getBlockSize(); i++)
  {
    memcpy(&end_block + i, &c, sizeof(char));
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
        writeToBlock(inod->direct[curr_block], start_block);
      }
      //If we only wrote one block
      if(last_block == 0)
      {
        break;
      }
      //If we're writing to the last block
      if(curr_block == last_block)
      {
        writeToBlock(inod->direct[curr_block], end_block);
        break;
      }
      //Writing to some block full of &c*blocksize in the middle
      else 
      {
        writeToBlock(inod->direct[curr_block], middle_block);
      }
    }
    else if((curr_block >= NUM_DIRECT_BLOCKS) && (curr_block < (indirect_max_size))){
      int* indirect_block = (int*) readFromBlock(inod->indirect);
      int destination_block_num = indirect_block[curr_block - NUM_DIRECT_BLOCKS];

      if(curr_block == last_block)
      {
        writeToBlock(destination_block_num, end_block);
        break;
      }
      else 
      {
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
      */
    }
  }
  delete inod;
}


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
    if(file_blocks < NUM_DIRECT_BLOCKS)
    {
      int blocknum_to_req = inod->direct[i];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(&cat_buffer + (i*getBlockSize()), &block_buffer, getBlockSize());
      delete block_buffer;
    }
    else if(i >= NUM_DIRECT_BLOCKS && i < indirect_max_size)
    {
      int blocknum_to_req = indirect_block[i - NUM_DIRECT_BLOCKS];
      char* block_buffer = readFromBlock(blocknum_to_req);
      memcpy(&cat_buffer + (i*getBlockSize()), &block_buffer, getBlockSize());
      delete block_buffer;
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

void SHUTDOWN()
{
  shut = 1;
}

void LIST()
{

}

void*
process_ops(void* file_arg)
{
	//string op_file_name = "thread1ops.txt";
  char* op_file_name = (char*)file_arg;
	cout << "Thread created:\t" << op_file_name << endl;

	/* Parsing */
	ifstream input_stream(op_file_name);
	string linebuff;
	while(getline(input_stream, linebuff))
	{
    if(shut) break;
    stringstream ss(linebuff);
    string command;
    ss >> command;

    cout << command << endl;

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

char*
getDisk()
{
  return DISK;
}

int getBlockSize()
{
  return ((int*) DISK)[1];
}

int getNumBlocks()
{
  return ((int*) DISK)[0];
}

int main(int argc, char const *argv[])
{
  /*Read all bytes of the DISK file into the DISK variable

  if(i/getBlockSize() < NUM_DIRECT_BLOCKS)
    {
      //WARNING: NOT CHECKING FOR UNALLOCATED BLOCKS AT THIS POINT
      memcpy(&inod->direct[curr_block] + (i%getBlockSize()), &c, sizeof(char)); 
    }

    else if(curr_block >= NUM_DIRECT_BLOCKS && curr_block < indirect_max_size) //write to first indir block
    {
      memcpy(&inod->indirect[curr_block - NUM_DIRECT_BLOCKS] + (i % getBlockSize(), &c, sizeof(char));
    }

    else if(curr_block >= max_indirect_size && curr_block < double_indirect_max_size)
    {
      int indir_block = inod->doubleIndirect[(curr_block - (NUM_DIRECT_BLOCKS + (getBlockSize()/sizeof(int)))) / (getBlockSize() / sizeof(int))];
      memcpy(&indir_block[curr_block - (NUM_DIRECT_BLOCKS + (getBlockSize() / sizeof(int))) + (i%getBlockSize())], &c, sizeof(char));
    }

  */
  {
    ifstream ifs("DISK", ios::binary|ios::ate);
    ifstream::pos_type pos = ifs.tellg();

    DISK = new char[pos];

    ifs.seekg(0, ios::beg);
    ifs.read(DISK, pos);
  }

  /* Parsing terminal inputs */
	vector<string> ops;
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

    for(int i = 2; i < argc;i++)
      {
        pthread_create(&op_thread[i-2], NULL, process_ops, (void*)argv[i]);
      }

    /*scheduler thread*/
    SCH_struct* str = new SCH_struct;
    str->requests = requests;
    str->lock = REQUESTS_LOCK;
    //    pthread_create(&SCH_thread, NULL, SCH_run, (void*) str);

    SCH_run((void*) str);
	}
  while(1);
}

