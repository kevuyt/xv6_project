#include "param.h"
#include "types.h"
#include "buf.h"


struct file {
  enum { FD_NONE, FD_PIPE, FD_INODE } type;
  int ref; // reference count
  char readable;
  char writable;
  struct pipe *pipe;
  struct inode *ip;
  uint off;
};


// in-memory copy of an inode
struct inode {
  uint dev;           // Device number
  uint inum;          // Inode number
  int ref;            // Reference count
  struct sleeplock lock; // protects everything below here
  int valid;          // inode has been read from disk?
  struct extent extents[100];
  short type;         // copy of disk inode
  short major;
  short minor;
  short nlink;
  uint size;
  uint addrs[NDIRECT+1];
  int off_t;             // Current file offset
  struct pipe *pipe;     // Pointer to pipe (for pipes)
  // Add a field to store the target path of the symbolic link
  char symlink_target[PATH_MAX]; // Adjust the size as necessary
};

// table mapping major device number to
// device functions
struct devsw {
  int (*read)(struct inode*, char*, int);
  int (*write)(struct inode*, char*, int);
};

extern struct devsw devsw[];

#define CONSOLE 1
#define FD_SYMLINK 3 // Define symbolic link file descriptor type
