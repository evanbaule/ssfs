#include "scheduler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

typedef struct disk_io_request disk_io_request;

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<disk_io_request*>* requests = str->requests;
  pthread_mutex_t lock = str->lock;
  int fd = str->fd;

  while(1)
    {
      if(isShutdown() && requests->size()==0) break;
      if(requests->size()==0)continue;
      pthread_mutex_lock(&lock);
      disk_io_request* req = requests->front();
      requests->pop();
      pthread_mutex_unlock(&lock);

      if(req->op == io_READ)
        {
          pthread_mutex_lock(&req->lock);
          lseek(fd, req->block_number*getBlockSize(), SEEK_SET);
          read(fd, req->data, getBlockSize());
          req->done = 1;
          pthread_cond_signal(&req->waitFor);
          pthread_mutex_unlock(&req->lock);
        }
      else if (req->op == io_WRITE)
        {
          printf("Writing data to file: %s to block: %d\n", req->data, req->block_number);
          lseek(fd, req->block_number*getBlockSize(), SEEK_SET);
          write(fd, req->data, getBlockSize());
          delete[] req->data;
        }
    }
  
  //Writeback byte maps
  lseek(fd, getFreeMapStart()*getBlockSize(), SEEK_SET);
  for(int i = 0; i < getFreeMapSize(); i++)
  {
    write(fd, FREE_MAP + (i * getBlockSize()), getBlockSize());
  }

  lseek(fd, getInodeMapStart()*getBlockSize(), SEEK_SET);
  for(int i = 0; i < getInodeMapSize(); i++)
  {
    write(fd, INODE_MAP + (i * getBlockSize()), getBlockSize());
  }

  return NULL;
}
