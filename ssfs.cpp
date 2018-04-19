#include "ssfs.hpp"

using namespace std;

pthread_t op_thread[4];
pthread_t SCH_thread;

/*byte array of the disk in memory*/
char* DISK;

/*Any access to the DISK array should be locked by this*/
pthread_mutex_t DISK_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t REQUESTS_LOCK = PTHREAD_MUTEX_INITIALIZER;

queue<request>* requests;

bool stop = 0;

void shutdown() {stop = 1;}

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

    request r;
    if(command == "CREATE")
      {
        Tag tag = create_tag;
        string fileName;
        ss >> fileName;

        r.tag = tag;
        r.fname1 = fileName;
      }
    else if (command == "IMPORT")
      {
        Tag tag = import_tag;
        string file1;
        string file2;
        ss >> file1;
        ss >> file2;

        r.tag = tag;
        r.fname1 = file1;
        r.fname2 = file2;
      }
    else if (command == "CAT")
      {
        Tag tag = cat_tag;
        string file1;
        ss >> file1;

        r.tag = tag;
        r.fname1 = file1;
      }
    else if (command == "DELETE")
      {
        Tag tag = delete_tag;
        string file1;
        ss >> file1;

        r.tag = tag;
        r.fname1 = file1;
      }
    else if (command == "WRITE")
      {
        Tag tag = write_tag;
        string file1;
        char c;
        uint start;
        uint num;
        ss >> file1;
        ss >> c;
        ss >> start;
        ss >> num;

        r.tag = tag;
        r.fname1 = file1;
        r.c = c;
        r.start_byte = start;
        r.num_bytes = num;
      }
    else if (command == "READ")
      {
        Tag tag = read_tag;
        string file1;
        uint start;
        uint num;

        ss >> file1;
        ss >> start;
        ss >> num;

        r.tag = tag;
        r.fname1 = file1;
        r.start_byte = start;
        r.num_bytes = num;
      }
    else if (command == "LIST")
      {
        Tag tag = list_tag;

        r.tag = tag;
      }
    else if (command == "SHUTDOWN")
      {
        Tag tag = shutdown_tag;

        r.tag = tag;
      }

    pthread_mutex_lock(&REQUESTS_LOCK);
    requests->push(r);
    pthread_mutex_unlock(&REQUESTS_LOCK);
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
  /*Read all bytes of the DISK file into the DISK variable*/
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

