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


int pivot_root(void);

static char stack[STACKSIZE];

struct param {
    char **argv;
    int * pipefd;
};

void print_err(char const * const reason)
{
    fprintf(stderr, "Error %s: %s\n", reason, strerror(errno));
}

//define the mappings between the parent and the child user and group IDs of the processes
static void define_mapping(pid_t pid, uid_t uid, gid_t gid) {
  char path[PATH_MAX];
  int fd;
  
  snprintf(path, PATH_MAX, "/proc/%d/uid_map", pid);
  if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
    print_err("Can't open uid_map");

  dprintf(fd, "0 %d 1", uid);
  close(fd);

  snprintf(path, PATH_MAX, "/proc/%d/setgroups", pid);
  if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
    print_err("Can't open gid_map");

  dprintf(fd, "deny");
  close(fd);

  snprintf(path, PATH_MAX, "/proc/%d/gid_map", pid);
  if ((fd = open(path, O_WRONLY | O_TRUNC)) == -1)
    print_err("Can't open gid_map");

  dprintf(fd, "0 %d 1", gid);
  close(fd); 
}

int exec(void * args)
{
    // Read in information sent by parent
    struct param * p = ((struct param *)args);
    int * pipefd = (int *) p->pipefd;
    char c;
    prctl(PR_SET_PDEATHSIG, SIGKILL);
    close(pipefd[1]);
    // Wait until parent creates the mapping
    if (read(pipefd[0], &c, 1) != 0)
        print_err("Read from pipe in child returned != 0");

    printf("Command:");
    char ** ptr = p->argv;
    while (*ptr) {
      printf(" %s", *ptr++);
    }
    printf("\n");

    printf("Execute pivot_root\n");
    pivot_root();

    // Remount proc
    // Use print_err to print error if mount fails
    if (mount("proc", "/proc", "proc", 0, NULL) == -1) {
        print_err("mount fails");
        return 1;
    }
    printf("Mounting process at /proc\n");

    char const * const hostname = "CSC595";
    // Set a new hostname
    // Use print_err to print error if sethostname fails
    if (sethostname(hostname, strlen(hostname)) == -1) {
        print_err("set hostname fails");
        return 1;
    }
    printf("Setting hostname\n");

    // Create a message queue
    // Use print_err to print error if creating message queue fails
    int msgid;
    if((msgid = msgget(0, IPC_CREAT | 0660)) == -1) {
        print_err("creat message queue fails");
        return 1;
    }
    printf("Creating a message queue\n");
    
    char ** const argv = p->argv;
    // Execute the given command
    // Use print_err to print error if execvp fails
    // printf(": %s\n", *argv);
    if (execvp(*argv, argv) < 0) {
        print_err("execute command fails");
        return 1;
    }

    return 0;
}


int main(int argc, char ** argv)
{

    // Create an instance of struct param
    int    pipefd[2];
    struct param p = {NULL, NULL};

    // Provide some feedback about the usage
    if (argc < 2) {
        fprintf(stderr, "No command specified\n");
        return 1;
    } else {
        p.argv = &argv[1];
        p.pipefd = ((int *)pipefd);
    }

    if (pipe(pipefd)) {
        print_err("Could not create pipe");
        return 1;
    }
    // Namespace flags
    // ADD more flags
    const int flags = CLONE_NEWNET | CLONE_NEWUTS | CLONE_NEWIPC | 
        CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWUSER | SIGCHLD;
    // Create a new child process
    pid_t pid = clone(exec, stack + STACKSIZE, flags, &p);
    if (pid < 0) {
        print_err("calling clone");
        return 1;
    }

    //define the mappings 
    uid_t uid = getuid();
    gid_t tid = getegid();
    define_mapping(pid, uid, tid);

    // we hang up both ends of the pipe to let the child
    //know that we've written the appropriate files.
    close(pipefd[0]); 
    close(pipefd[1]);

    // Wait for the process to finish
    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        print_err("waiting for pid");
        return 1;
    }
    // Return the exit code
    return WEXITSTATUS(status);
}
