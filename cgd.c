#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <sched.h>
#include <sys/wait.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#define errOut(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Run our cpu intensive process here
static int child_exec(void *arg) {
  int *p = (int *) arg;
  char c;
  prctl(PR_SET_PDEATHSIG, SIGKILL);

  close(p[1]);
  // WAIT HERE UNTIL PARENT CLOSES PIPE
  if (read(p[0], &c, 1) != 0)
    errOut("Read from pipe in child returned != 0");

  execlp("./cpu100", "cpu100", (char *) 0);
  exit(EXIT_FAILURE);
}

static pid_t child_pid;
static void death_handler (int sig) {
  char path[PATH_MAX];
  
  kill(child_pid, SIGKILL);
  /* if we don't wait for the child to
   * completely die here, cgroups won't let us remove
   * the subdirectories
   */
  waitpid(child_pid, NULL, 0);

  snprintf(path, PATH_MAX, "/sys/fs/cgroup/cpuacct/cgdemo/%d", child_pid);
  if (!access(path, F_OK))
    return;

  if (rmdir(path))
    errOut("Could not remove cgroup dir");
}

/* /sys/fs/cgroup/cpuacct is the directory where our cpu limit will go
 * make a directory cgdemo/<PID> to write our cgroups files into
 * <PID> will be the pid of our child container
 */ 
static void make_cpu_cgroup_dir(pid_t pid) {
  char path[PATH_MAX];

  snprintf(path, PATH_MAX, "/sys/fs/cgroup/cpuacct/cgdemo/%d", pid);

  if (!access(path, F_OK))
    return;

  if (mkdir(path, S_IRWXU | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH))
    errOut("Could not create cgroup dir");
}

/* We must write two files
 * 1. cpu.cfs_quota_us -- this is the number of microseconds that that we will permit in a given time period.
 *                        the time length of the time period is set in cpu.cfs_period_us and defaults to
 *                        100000 microseconds.
 * 2. tasks            -- write the pid of the child process to be constrained in this file
 */
static void write_cpu_cgroup_files(pid_t pid, int pct_limit) {
  char path[PATH_MAX];
  int fd;
  
  snprintf(path, PATH_MAX, "/sys/fs/cgroup/cpuacct/cgdemo/%d/cpu.cfs_quota_us", pid);

  if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
    errOut("Can't open cpu.cfs_quota_us");

  dprintf(fd, "%d", pct_limit * 1000);
  close(fd);
    
  snprintf(path, PATH_MAX, "/sys/fs/cgroup/cpuacct/cgdemo/%d/tasks", pid);

  if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
    errOut("Can't open tasks");

  dprintf(fd, "%d", pid);
  close(fd);  
}
/* Pass one optional arg
 * The arg should be an int from 0-100 which specifies the max cpu pct permitted for the child
 * default is 100 
 */
int main(int argc, char *argv[]) {

  unsigned stk_sz = 1024 * 1024;
  pid_t    pid;
  int      pipefd[2];
  
  if (argc > 2) {
    fprintf(stderr, "Usage:  [cpu pct]\n");
    exit(EXIT_SUCCESS);
  }
  
  int cpu_pct       = (argc == 2) ? atoi(argv[1]) : 100;

  /* set up a pid and user namespace for our child to run in
   * mount namespace would be fun, but its a bit more work to 
   * shuttle all needed binaries into the new namespace
   */ 
  int clone_flags   = SIGCHLD | CLONE_NEWPID | CLONE_NEWUSER;
  void *child_stack = mmap(0, stk_sz, PROT_EXEC | PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (child_stack == MAP_FAILED) 
    errOut("mmap for child stack failed: %s\n");

  /* we'll use this pipe for communicating with the child
   * we want to handle our cgroup writes before letting the child exec
   * so child will wait for us to close the pipe
   */
  if (pipe(pipefd))
    errOut("Could not create pipe");

  if ((pid = clone(child_exec, child_stack + stk_sz, clone_flags, &pipefd)) < 0)
    errOut("Clone failed");

  // Look in this function for the cgroup directory setup
  make_cpu_cgroup_dir(pid);
  
  // we really should  clean these cgroup dirs up if the process exits,
  // so we make sure here that we'll catch user interrupts (^C)
  child_pid = pid; // put this in a static so sighandler can find it
  signal(SIGINT, death_handler);

  // Its in this function (below) where we use cgroups to limit cpu usage
  write_cpu_cgroup_files(pid, cpu_pct);

  // we hang up both ends of the pipe to let the child
  //know that we've written the appropriate files.
  close(pipefd[0]); 
  close(pipefd[1]); 

  // wait on child to exit
  waitpid(pid, NULL, 0);
  exit(EXIT_SUCCESS);
}
