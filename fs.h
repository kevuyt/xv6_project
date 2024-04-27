#ifndef FS_H
#define FS_H

#include "types.h"
#include "sleeplock.h"


// Constants
#define ROOTINO 1  // root i-number
#define BSIZE 512  // block size
#define PATH_MAX 4096

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]

struct superblock {
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define NDIRECT 12
#define NINDIRECT (BSIZE / sizeof(uint))
#define MAXFILE (NDIRECT + NINDIRECT)
#define N_EXTENTS 6  // Define the number of extents in the inode

// Extent structure to represent consecutive blocks
struct extent {
    uint start_block; // Start block of the extent
    uint length;    
};
struct dinode {
    uint dev;            
    uint inum;            
    int ref;              
    struct sleeplock lock;
    int valid;            


    short type;        
    short major;        
    short minor;          
    short nlink;    
    uint size; 
    uint addrs[NDIRECT+1];  // Direct and indirect block addresses        
    struct extent extents[N_EXTENTS]; 

}

// Inodes per block.
#define IPB (BSIZE / sizeof(struct dinode))

// Block containing inode i
#define IBLOCK(i, sb) ((i) / IPB + sb.inodestart)

// Bitmap bits per block
#define BPB (BSIZE * 8)

// Block of free map containing bit for block b
#define BBLOCK(b, sb) (b / BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
#define DIRSIZ 14

#include <sys/types.h>
struct dirent {
  ushort inum;
  char name[DIRSIZ];
};
#endif  // FS_H
