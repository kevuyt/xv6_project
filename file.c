//
// File descriptors
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;

void
fileinit(void)
{
  initlock(&ftable.lock, "ftable");
}

// Allocate a file structure.
struct file*
filealloc(void)
{
  struct file *f;

  acquire(&ftable.lock);
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  release(&ftable.lock);
  return 0;
}

// Increment ref count for file f.
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("filedup");
  f->ref++;
  release(&ftable.lock);
  return f;
}

// Close file f.  (Decrement ref count, close when reaches 0.)
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  if(f->ref < 1)
    panic("fileclose");
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  ff = *f;
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  if(ff.type == FD_PIPE)
    pipeclose(ff.pipe, ff.writable);
  else if(ff.type == FD_INODE){
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

// Get metadata about file f.
int
filestat(struct file *f, struct stat *st)
{
  if(f->type == FD_INODE){
    ilock(f->ip);
    stati(f->ip, st);
    iunlock(f->ip);
    return 0;
  }
  return -1;
}

// Read from file f.
// Assuming `fileread` should really handle `struct file*` instead of `struct inode*`
int
fileread(struct file *f, char *addr, int n) {
    if (!f || !f->ip) return -1; // Error handling for null pointers

    struct inode *ip = f->ip; // Use the inode from the file structure

    if (ip->type == T_SYMLINK) {
        memmove(addr, ip->symlink_target, n);
        return strlen(ip->symlink_target);
    }

    if (ip->type == T_FILE) {
        ilock(ip);
        int r = readi(ip, addr, f->off, n); // Use f->off instead of ip->off
        if (r > 0) {
            f->off += r;
        }
        iunlock(ip);
        return r;
    } else if (ip->type == T_PIPE) {
        return piperead(ip->pipe, addr, n);
    }

    return -1; // Error: Unsupported file type
}


//PAGEBREAK!
// Write to file f.
int
filewrite(struct file *f, char *addr, int n)
{
  if(f->writable == 0)
    return -1;
  if(f->type == FD_PIPE){
    return pipewrite(f->pipe, addr, n);
  }
  if(f->type == FD_INODE){
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;
    int i = 0;
    int r;
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      begin_op();
      ilock(f->ip);
      
      if(f->off > f->ip->size){
        int gap = f->off - f->ip->size;
        char zeroes[4096];
        memset(zeroes, 0, sizeof(zeroes));
        while(gap > 0){
          int towrite = gap < sizeof(zeroes) ? gap : sizeof(zeroes);
          int written = writei(f->ip, zeroes, f->ip->size, towrite);
          if(written < 0){
            iunlock(f->ip);
            end_op();
            return -1;
          }
          gap -= written;
          f->off += written;
        }
        f->ip->size = f->off;
      }
      
      if ((r = writei(f->ip, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      end_op();

      if(r < 0)
        break;
      if(r != n1)
        panic("short filewrite");
      i += r;
    }
    return i == n ? n : -1;
  }
  panic("filewrite");
}


