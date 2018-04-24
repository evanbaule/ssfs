#include "ssfs.hpp"

using namespace std;

pthread_t op_thread[4];
pthread_t SCH_thread;

/*byte array of the disk in memory*/
char* DISK;

/*Any access to the DISK array should be locked by this*/
pthread_mutex_t DISK_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t REQUESTS_LOCK = PTHREAD_MUTEX_INITIALIZER;

queue<disk_io_request>* requests;

bool stop = 0;

void shutdown() {stop = 1;}

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

      req.waitFor = PTHREAD_COND_INITIALIZER;
      req.lock = PTHREAD_MUTEX_INITIALIZER;

      addRequest(&req);

      while(!req.done)
        pthread_cond_wait(&req.waitFor, &req.lock);

      if(data[0] != 0)
        found = 1;
      delete(data);
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

int getInode(char* file)
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

      req.waitFor = PTHREAD_COND_INITIALIZER;
      req.lock = PTHREAD_MUTEX_INITIALIZER;

      addRequest(&req);

      while(!req.done)
        pthread_cond_wait(&req.waitFor, &req.lock);

      if(strncmp(data, file, MAX_FILE_NAME) == 0)
        found = 1;
      delete(data);
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

/*
int getStartOfDataBlocks()
{
  return getBlockSize() + MAX_INODES*getBlockSize() + ((getBitmapSize()*4)/getBlockSize() + ((getBitmapSize()*4)%getBlockSize()!=0))*getBlockSize();
}
*/
void addRequest(disk_io_request* req)
{
  pthread_mutex_lock(&REQUESTS_LOCK);
  requests->push(req);
  pthread_mutex_unlock(&REQUESTS_LOCK);
}

void CREATE(char* filename)
{
  if(filename[0] == 0)
  {
    cerr << "Invalid filename entered into CREATE argument " << endl;
  }
  int inod = getEmptyInode();
  if(inod != -1)
    {
      disk_io_request req;
      req.op = io_WRITE;
      char* data = new char[getBlockSize()]();
      memcpy(data, filename, strlen(filename));
      req.data = data;
      req.blockNumber = inod;
      addRequest(&req);
    }
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

void CAT(char* fileName){
  char* 
}

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
      delete indirect_block;
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

      delete double_indirect_block;
      delete indirect_block;
      */
    }
  }
  delete inod;
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
  bool found = 0;
  for(i = 0; i < getNumBlocks()/getBlockSize(); i++)
    {
      /*              metadata        num of inodes        */
      uint byteOffs =  getBlockSize() + i*getBlockSize();
      char* disk = getDisk();
    }
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
    if(stop) break;
    stringstream ss(linebuff);
    string command;
    ss >> command;

    if(command == "CREATE")
      {
        char* fileName;
        ss >> fileName;

        CREATE(fileName);
      }
    else if (command == "IMPORT")
      {
        char* file1;
        char* file2;
        ss >> file1;
        ss >> file2;

        IMPORT(file1, file2);
      }
    else if (command == "CAT")
      {
        char* file1;
        ss >> file1;

        CAT(file1);
      }
    else if (command == "DELETE")
      {
        char* file1;
        ss >> file1;

        DELETE(file1);
      }
    else if (command == "WRITE")
      {
        char* file1;
        char c;
        uint start;
        uint num;
        ss >> file1;
        ss >> c;
        ss >> start;
        ss >> num;

        WRITE(file1, c, start, num);
      }
    else if (command == "READ")
      {
        char* file1;
        uint start;
        uint num;

        ss >> file1;
        ss >> start;
        ss >> num;

        READ(file1, start, num);
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

uint getBlockSize()
{
  return ((int*) DISK)[1];
}

uint getNumBlocks()
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
    pthread_create(&SCH_thread, NULL, SCH_run, (void*) str);
	}
}

