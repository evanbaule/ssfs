#include "scheduler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<disk_io_request>* requests = str->requests;
  pthread_mutex_t lock = str->lock;
  pthread_mutex_t diskLock = str->diskLock;

  while(1)
    {
      if(requests->size()==0)break;

      pthread_mutex_lock(&lock);
      disk_io_request req = requests->front();
      requests->pop();
      pthread_mutex_unlock(&lock);

      pthread_mutex_lock(&diskLock);
      if(req.op == io_READ)
        {
          pthread_mutex_lock(&req->lock);
          memcpy(req.data, getDisk()+req.block_number*getBlockSize(), getBlockSize());
          pthread_cond_signal(&req->waitFor);
          pthread_mutex_unlock(&req->lock);
        }
      else if (req.op == io_WRITE)
        {
          memcpy(getDisk()+req.block_number*getBlockSize(), req.data, getBlockSize());
          delete(req.data);
        }
      pthread_mutex_unlock(&diskLock);
    }
}
