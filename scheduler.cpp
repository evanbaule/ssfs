#include "scheduler.hpp"

void CREATE(string filename){}

void IMPORT(string ssfsFile, string unixFilename){}

void CAT(string fileName){}

void DELETE(string fileName){}

void WRITE(string fileName, char c, uint start, uint num){}

void READ(string fileName, uint start, uint num){}

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
