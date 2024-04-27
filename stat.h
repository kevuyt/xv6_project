#ifndef STAT_H
#define STAT_H

#include "types.h"

// Definitions of file types
#define T_DIR      1   // Directory
#define T_FILE     2   // Regular file
#define T_DEV      3   // Device
#define T_EXTENTS   4   // Extent-based file
#define T_SYMLINK  5   // Symbolic link
#define T_PIPE     6   // Pipe

struct stat {
  short type;    // Type of file
  int dev;       // File system's disk device
  uint ino;      // Inode number
  short nlink;   // Number of links to file
  uint size;     // Size of file in bytes
  uint offset;   // Offset within file (for special file types)
};

#endif  // STAT_H
