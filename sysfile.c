//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"
#include "stat.h"
#include "proc.h"


#define MAXPATH PATH_MAX
// Function declarations
static struct inode* create(char *path, short type, short major, short minor);


int resolve_symlink(struct inode **ip, const char *path, int flags) {
    // Check if the inode pointer is valid
    if (*ip == 0) {
        return -1; // Invalid inode
    }

    // Allocate a temporary buffer for storing target paths
    char target[MAXPATH]; // Define MAXPATH as the maximum path length
    int n = readi(*ip, target, 0, sizeof(target) - 1);
    if (n <= 0) {
        return -1; // Failed to read the target path
    }
    target[n] = '\0'; // Ensure the target path is null-terminated

    // Release the previous inode
    iunlockput(*ip);

    // If O_NOFOLLOW flag is specified, do not recursively resolve
    if (flags & O_NOFOLLOW) {
        *ip = namei(target);
        if (*ip == 0) {
            return -1; // Failed to resolve the target path of the symlink
        }
        return 0; // Success without recursion
    }

    // Recursively resolve the target path until it's not a symlink
    int depth = 0; // Depth counter for recursion
    while (depth < 10) { // Limit the depth to prevent infinite loops
        *ip = namei(target);
        if (*ip == 0) {
            return -1; // Failed to resolve the target path of the symlink
        }

        if ((*ip)->type != T_SYMLINK) {
            break; // Exit loop if it's not a symlink
        }

        // Read the new target path from the new symlink
        n = readi(*ip, target, 0, sizeof(target) - 1);
        if (n <= 0) {
            iput(*ip); // Release the new inode
            return -1; // Failed to read the target path
        }
        target[n] = '\0'; // Ensure the target path is null-terminated
        depth++; // Increment recursion depth
    }

    if (depth >= 10) {
        iput(*ip); // Clean up in case of too deep recursion
        return -1; // Return an error if recursion depth is exceeded
    }

    return 0; // Success
}

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *curproc = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd] == 0){
      curproc->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

int
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

int
sys_read(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return fileread(f, p, n);
}

int
sys_write(void)
{
  struct file *f;
  int n;
  char *p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argptr(1, &p, n) < 0)
    return -1;
  return filewrite(f, p, n);
}

int
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

int
sys_fstat(void)
{
  struct file *f;
  struct stat *st;

  if(argfd(0, 0, &f) < 0 || argptr(1, (void*)&st, sizeof(*st)) < 0)
    return -1;
  return filestat(f, st);
}


// Create a symbolic link to a file.
int
sys_symlink(void)
{
  char *target = 0, *path = 0;  // Initialize pointers to 0
  struct inode *ip;
  
  // Fetch the target and path from the user space
  if (argstr(0, &target) < 0 || argstr(1, &path) < 0)
    return -1;

  // Begin a file system operation
  begin_op();

  // Attempt to create an inode for the symlink
  if ((ip = create(path, T_SYMLINK, 0, 0)) == 0) {
    end_op();  // End the file system operation if creation failed
    return -1;
  }

  // Lock the inode for writing
  ilock(ip);

  // Compute the length of the target path, including the null terminator
  int length = strlen(target) + 1;
  int written = writei(ip, target, 0, length);

  // Check if the write operation wrote the full length of the target
  if (written != length) {
    iunlockput(ip);  // Unlock and put the inode back if writing failed
    end_op();
    return -1;
  }

  // Unlock and release the inode
  iunlockput(ip);
  end_op();  // End the file system operation
  
  return 0;  // Return success
}

// Create the path new as a link to the same inode as old.
int
sys_link(void) {
    char name[DIRSIZ], *new, *old;
    struct inode *dp, *ip;

    if(argstr(0, &old) < 0 || argstr(1, &new) < 0)
        return -1;

    begin_op();
    if((ip = namei(old)) == 0) {
        end_op();
        return -1;
    }

    ilock(ip);
    if(ip->type == T_SYMLINK) {
        // If it's a symlink, we need to link to the symlink itself, not the target
        iunlock(ip); // Unlock the symlink inode without following it
    } else if(ip->type == T_DIR) {
        iunlockput(ip);
        end_op();
        return -1; // Cannot link directories for safety reasons
    } else {
        ip->nlink++;
        iupdate(ip);
        iunlock(ip);
    }

    if((dp = nameiparent(new, name)) == 0) {
        iput(ip);
        end_op();
        return -1;
    }
    ilock(dp);
    if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0) {
        iunlockput(dp);
        iunlockput(ip);
        end_op();
        return -1;
    }
    iunlockput(dp);
    iput(ip);

    end_op();
    return 0;
}


// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, (char*)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

//PAGEBREAK!
int
sys_unlink(void) {
    struct inode *ip, *dp;
    struct dirent de;
    char name[DIRSIZ], *path;
    uint off;

    if(argstr(0, &path) < 0)
        return -1;

    begin_op();
    if((dp = nameiparent(path, name)) == 0) {
        end_op();
        return -1;
    }
    ilock(dp);

    if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0) {
        iunlockput(dp);
        end_op();
        return -1; // Cannot unlink "." or ".."
    }

    if((ip = dirlookup(dp, name, &off)) == 0) {
        iunlockput(dp);
        end_op();
        return -1;
    }
    ilock(ip);

    if(ip->type == T_DIR && !isdirempty(ip)) {
        iunlockput(ip);
        iunlockput(dp);
        end_op();
        return -1;
    }

    if(ip->type != T_SYMLINK && ip->nlink < 1)
        panic("unlink: nlink < 1");

    memset(&de, 0, sizeof(de));
    if(writei(dp, (char*)&de, off, sizeof(de)) != sizeof(de)) {
        panic("unlink: writei");
    }

    if(ip->type == T_DIR) {
        dp->nlink--;
        iupdate(dp);
    }
    iunlockput(dp);

    ip->nlink--;
    iupdate(ip);
    iunlockput(ip);

    end_op();
    return 0;
}

static struct inode* 
create(char *path, short type, short major, short minor) {
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;
  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && ip->type == T_FILE)
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}


int sys_open(void)
{
  char *path;
  int fd, omode;
  struct file *f;
  struct inode *ip;

  if(argstr(0, &path) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    int fileType = (omode & O_EXTENT) ? T_EXTENTS : T_FILE;
    ip = create(path, fileType, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_SYMLINK && !(omode & O_NOFOLLOW)){
      // Handle symbolic link resolution
    }
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  end_op();

  f->type = FD_INODE;
  f->ip = ip;
  f->off = 0;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
  return fd;
}

int
sys_mkdir(void)
{
  char *path;
  struct inode *ip;

  begin_op();
  if(argstr(0, &path) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_mknod(void)
{
  struct inode *ip;
  char *path;
  int major, minor;

  begin_op();
  if((argstr(0, &path)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEV, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

int
sys_chdir(void)
{
  char *path;
  struct inode *ip;
  struct proc *curproc = myproc();
  
  begin_op();
  if(argstr(0, &path) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(curproc->cwd);
  end_op();
  curproc->cwd = ip;
  return 0;
}

int
sys_exec(void)
{
  char *path, *argv[MAXARG];
  int i;
  uint uargv, uarg;

  if(argstr(0, &path) < 0 || argint(1, (int*)&uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv))
      return -1;
    if(fetchint(uargv+4*i, (int*)&uarg) < 0)
      return -1;
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    if(fetchstr(uarg, &argv[i]) < 0)
      return -1;
  }
  return exec(path, argv);
}

int
sys_pipe(void)
{
  int *fd;
  struct file *rf, *wf;
  int fd0, fd1;

  if(argptr(0, (void*)&fd, 2*sizeof(fd[0])) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      myproc()->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  fd[0] = fd0;
  fd[1] = fd1;
  return 0;
}

int
sys_find(void){
return 0;
}
int sys_lseek(void) {
    int fd, offset;

    if(argint(0, &fd) < 0 || argint(1, &offset) < 0)
        return -1;

    struct file *f;
    if((f = myproc()->ofile[fd]) == 0)
        return -1;

    f->off = offset;

    if(f->off < 0)
        f->off = 0;

    return f->off;
}
