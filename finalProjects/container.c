#define _GNU_SOURCE

#include <alloca.h>
#include <errno.h>
#include <dirent.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#define STACKSIZE (1024 * 1024)


int pivot_root();

static char stack[STACKSIZE];


struct param{

  char * cmd;
  char ** argv;
  int argc;
  int cpu_pct;
  int mem_limit;
  int num_levels;
   int * pipefd;
};



void print_err(char const * const reason)
{
    fprintf(stderr, "Error %s: %s\n", reason, strerror(errno));
}

//define the mappings between the parent and the child user and group IDs of the processes


int exec(void * args)
{
    
    struct param * p = ((struct param *)args);
    int * pipefd = (int *) p->pipefd;
    char c;
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    close(pipefd[1]);

    if (read(pipefd[0], &c, 1) != 0){
        print_err("Read from pipe in child returned != 0");
      }

    printf("cmd is :");
    char ** ptr = p->argv;
    while (*ptr) {
      printf(" %s", *ptr++);
    }
    printf("\n");

    pivot_root();

    // Remount proc
    // Use print_err to print error if mount fails
    int result;
    result = mount("proc", "/proc", "proc", 0, NULL);
    if (result == -1){
      print_err("mount fail");
    }


    char const * const hostname = "CSC595";
    // Set a new hostname
    // Use print_err to print error if sethostname fails
      result = sethostname(hostname, strlen(hostname));

    if (result == -1 ){
      print_err("sethostname fails");
    }

    // Create a message queue
    // Use print_err to print error if creating message queue fails
    result = msgget(0, IPC_CREAT| S_IRUSR | S_IWUSR | 0666);
    if (result < 0){
      print_err("creating message queue fails");
    }
    
    char ** const argv = p->argv;
    // Execute the given command
    // Use print_err to print error if execvp fails
    // printf(": %s\n", *argv);
    result = execvp(*argv, argv);
    if (result  == -1 ){
      print_err("execvp fails");
    }
    return 0;
}


int main(int argc, char ** argv)
{

    // Create an instance of struct param
    int    pipefd[2];
  
    struct param  paras = {NULL, NULL};

    // Provide some feedback about the usage
   
     
    pid_t pid;


    if (argc >= 2) {
       paras.argv = &argv[1];
        paras.pipefd = ((int *)pipefd);

      const int flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWUSER;

      int result = pipe(pipefd);
      if (result != 0) {
          print_err("pipe fail");
          return 1;
      }
      // Namespace flags
      // ADD more flags
      

      // Create a new child process
      pid = clone(exec, stack + STACKSIZE, flags, &paras);
      if (pid < 0) {
          print_err("calling clone");
          return 1;
      }

      //define the mappings 
      uid_t uid = getuid();
      gid_t gid = getegid();
      char path[PATH_MAX];
      int fd;
      
      snprintf(path, PATH_MAX, "/proc/%d/uid_map", pid);
      if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
        print_err("uid_map fail");

      close(fd);

      snprintf(path, PATH_MAX, "/proc/%d/setgroups", pid);
      if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
        print_err("gid_map fail");

      close(fd);

      snprintf(path, PATH_MAX, "/proc/%d/gid_map", pid);
      if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
        print_err("gid_map fail");

      close(fd); 

      // we hang up both ends of the pipe to let the child
      //know that we've written the appropriate files.
      close(pipefd[0]); 
      close(pipefd[1]);

    }

    // Wait for the process to finish
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        print_err("waiting for pid");
        return 1;
    }
    // Return the exit code
    return WEXITSTATUS(status);
}
