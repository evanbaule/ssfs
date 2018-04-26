#include "scheduler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<disk_io_request*>* requests = str->requests;
  pthread_mutex_t lock = str->lock;
  int fd = str->fd;

  while(1)
    {
      if(isShutdown()) break;
      if(requests->size()==0)continue;
      pthread_mutex_lock(&lock);
      disk_io_request* req = requests->front();
      requests->pop();
      pthread_mutex_unlock(&lock);

      if(req->op == io_READ)
        {
          printf("Waiting for lock in sched\n", req->op);
          pthread_mutex_lock(&req->lock);
          printf("Acquired lock in sched\n", req->op);
          lseek(fd, req->block_number*getBlockSize(), SEEK_SET);
          read(fd, req->data, getBlockSize());
          req->done = 1;
          pthread_cond_signal(&req->waitFor);
          pthread_mutex_unlock(&req->lock);
        }
      else if (req->op == io_WRITE)
        {
          printf("Writing data to file: %s\n", req->data);
          lseek(fd, req->block_number*getBlockSize(), SEEK_SET);
          write(fd, req->data, getBlockSize());
        }
    }
  return NULL;
}
