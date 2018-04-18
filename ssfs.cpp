#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <pthread.h>
using namespace std;

/* convince me not to do this */
pthread_t op_thread1, op_thread2, op_thread3, op_thread4;

void*
process_ops(void* file_arg)
{
	//string op_file_name = "thread1ops.txt";
  char* op_file_name = *(char**)file_arg;
	cout << "Thread created:\t" << op_file_name << endl;

	/* Parsing */
	ifstream input_stream(op_file_name);
	string linebuff;
	while(getline(input_stream, linebuff))
	{
		cout << linebuff << endl;
	}
	return NULL;
}

int main(int argc, char const *argv[])
{
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
		/* Creates n pthreads and passes in the appropriate file names to the worker func */
		switch(argc)
		{
			//this is ugly sorry
			case 3:
			{
				int rc1 = pthread_create(&op_thread1, NULL, process_ops, (void*)argv[2]);
				/* For testing
				ops.push_back(argv[2]);
				*/
				break;
			}
			case 4:
			{
				int rc1 = pthread_create(&op_thread1, NULL, process_ops, (void*)argv[2]);
				int rc2 = pthread_create(&op_thread2, NULL, process_ops, (void*)argv[3]);

				/* For testing 
				ops.push_back(argv[2]);
				ops.push_back(argv[3]);
				*/
				break;
			}
			case 5:
			{
				int rc1 = pthread_create(&op_thread1, NULL, process_ops, (void*)argv[2]);
				int rc2 = pthread_create(&op_thread2, NULL, process_ops, (void*)argv[3]);
				int rc3 = pthread_create(&op_thread3, NULL, process_ops, (void*)argv[4]);

				/* For testing 
				ops.push_back(argv[2]);
				ops.push_back(argv[3]);
				ops.push_back(argv[4]);
				*/
				break;
			}
			case 6:
			{
				int rc1 = pthread_create(&op_thread1, NULL, process_ops, (void*)argv[2]);
				int rc2 = pthread_create(&op_thread2, NULL, process_ops, (void*)argv[3]);
				int rc3 = pthread_create(&op_thread3, NULL, process_ops, (void*)argv[4]); 
				int rc4 = pthread_create(&op_thread4, NULL, process_ops, (void*)argv[5]); 

				/* For testing 
				ops.push_back(argv[2]); 
				ops.push_back(argv[3]);
				ops.push_back(argv[4]);
				ops.push_back(argv[5]);
				*/
				break;
			}
				
		}
		/*
		for(int i = 0; i < ops.size(); i++)
		{
			cout << "ARG:\t" << ops[i] << endl;
		}
		*/
	}
}


