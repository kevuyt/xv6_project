#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H
#include "spinlock.h" 


// Make sure to include the header defining `struct spinlock`

// Long-term locks for processes
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};

#endif