#include "scheduler.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>

typedef struct disk_io_request disk_io_request;

void* SCH_run(void* vec)
{
  SCH_struct* str = (SCH_struct*) vec;
  queue<disk_io_request*>* requests = str->requests;
  pthread_mutex_t lock = str->lock;
  int fd = str->fd;

  vector<char*> vecs;

  while(1)
    {
      pthread_mutex_lock(&lock);
      if(isShutdown() && requests->size()==0){
        pthread_mutex_unlock(&lock);
        break;
      }
      if(requests->size()==0)
        {
          pthread_mutex_unlock(&lock);
          continue;
        }
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
          pthread_mutex_lock(&req->lock);
          lseek(fd, req->block_number*getBlockSize(), SEEK_SET);
          write(fd, req->data, getBlockSize());
          req->done = 1;
          pthread_cond_signal(&req->waitFor);
          pthread_mutex_unlock(&req->lock);
          vecs.push_back(req->data);
          if(vecs.size() % 300 == 0)
            {
              delete[] vecs.front();
              vecs.erase(vecs.begin());
            }
        }
      delete req;
    }

  //Writeback byte maps
  lseek(fd, getFreeMapStart()*getBlockSize(), SEEK_SET);
  for(int i = 0; i < getFreeMapSize(); i++)
  {
    write(fd, getFREE_MAP() + (i * getBlockSize()), getBlockSize());
  }

  lseek(fd, getInodeMapStart()*getBlockSize(), SEEK_SET);
  for(int i = 0; i < getInodeMapSize(); i++)
  {
    write(fd, getINODE_MAP() + (i * getBlockSize()), getBlockSize());
  }

  exit(0);

  return NULL;
}
