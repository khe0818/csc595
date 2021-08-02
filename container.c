#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/msg.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#define STACKSIZE (1024 * 1024)

struct params{

  char * cmd;
  char ** argv;
  int argc;
  int cpu_pct;
  int mem_limit;
  int num_levels;
  int numNamespaces;

};


static char stack[STACKSIZE];
void print_err(char const * const reason)
{
    fprintf(stderr, "Error %s: %s\n", reason, strerror(errno));
}
int exec(void * args)
{

  // Validate passing of struct
  struct params * paras = ((struct params *)args);

      printf("cmd: %s\n", paras->cmd);
      printf("cpu_pct: %d\n", paras->cpu_pct);
      printf("mem_limit: %d\n", paras->mem_limit);
      printf("num_levels: %d\n", paras->num_levels);
      printf("argv number: %d\n", paras->argc);


      for (int i = 0; i < paras->argc; i++){
        printf("Arg : %d: %s\n", i, paras->argv[i]);
      }

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
    // USe print_err to print error if creating message queue fails
    result = msgget(0, IPC_CREAT| S_IRUSR | S_IWUSR | 0666);
    if (result < 0){
      print_err("creating message queue fails");
    }


    // Execute the given command
    // Use print_err to print error if execvp fails
    result = execvp(paras->cmd, paras->argv);
    if (result  == -1 ){
      print_err("execvp fails");
    }


    return 0;
}
int main(int argc, char ** argv)
{
    // Provide some feedback about the usage
    if (argc < 2) {
        fprintf(stderr, "No command specified\n");
        return 1;
    }

    const int flags = SIGCHLD | CLONE_NEWNS | CLONE_NEWPID | CLONE_NEWNET | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWUSER;

     
    if(atoi(argv[1]) == 0){
       
        // Create a new child process
        pid_t pid = clone(exec, stack + STACKSIZE, flags, &argv[1]);
        if (pid < 0) {
            print_err("calling clone");
            return 1;
        }
         int status = 0;
        if (waitpid(pid, &status, 0) == -1) {
            print_err("waiting for pid");
            return 1;
        }
        return WEXITSTATUS(status);
    //}
        
    }
    else{
        
        struct params * paras = (struct params *)malloc(sizeof(struct params));
        paras->numNamespaces = atoi(argv[1]);
        int numOfArgs = argc - 5;
        paras->cmd = (char *)malloc(strlen(argv[5]) + 1);
        strcpy(paras->cmd, argv[5]);


        paras->cpu_pct = atoi(argv[2]);
        paras->mem_limit = atoi(argv[3]);
        paras->num_levels = atoi(argv[4]);

        paras->argv = (char **)malloc((sizeof(char *) * numOfArgs) + 1);
        paras->argc = numOfArgs;
        
        printf("numNameSpaces %d numArgs %d, argc %d \n", paras->numNamespaces, numOfArgs, argc);

         if (numOfArgs > 0){
          paras->argv = (char **)malloc((sizeof(char *) * numOfArgs) + 1);
          paras->argc = argc - 5;
        }
  
            for (int i = 5; i < argc; i++){
              paras->argv[i - 5] = (char *)malloc(strlen(argv[i]) + 1);
              strcpy(paras->argv[i- 5], argv[i]);
            }


        pid_t pid;
        int status = 0;
        for(int i = 0; i < paras->numNamespaces; i++){

          // Create a new child process
          pid = clone(exec, stack + STACKSIZE, flags, (void *)paras);
          if (pid < 0) {
            print_err("calling clone");
            return 1;
          }

           
        if (waitpid(pid, &status, 0) == -1) {
            print_err("waiting for pid");
            return 1;
        }

        }

        // Wait for the process to finish
       
        // Return the exit code
        return WEXITSTATUS(status);

   

        
    }
 


   
    // Namespace flags
    // ADD more flags

    // Wait for the process to finish
   
    // Return the exit code
    
}
