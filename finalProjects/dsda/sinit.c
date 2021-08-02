#define _GNU_SOURCE
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wordexp.h>
#include <errno.h>
#include <sys/wait.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define CMD_SIZE 10000

static int verbose = 1;
static void
child_sighandler(int sig)
{
    pid_t pid;
    int status;

    /* WUNTRACED and WCONTINUED allow waitpid() to catch stopped and
       continued children (in addition to terminated children) */

    while ((pid = waitpid(-1, &status,
                          WNOHANG | WUNTRACED | WCONTINUED)) != 0) {
        if (pid == -1) {
            if (errno == ECHILD)        
                break;
            else
                perror("waitpid");      
        }

    }
}

int
main(int argc, char *argv[])
{   
    struct sigaction sa;
    char cmd[CMD_SIZE];
    pid_t pid;
    int opt;


    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = child_sighandler;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        errExit("sigaction");

    printf("\tinit: my PID is %ld\n", (long) getpid());
    signal(SIGTTOU, SIG_IGN);

    /* Become leader of a new process group and make that process
       group the foreground process group for the terminal */

    if (setpgid(0, 0) == -1)
        errExit("setpgid");;
    if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
        errExit("tcsetpgrp-child");

    while (1) {

	// CHANGE THE COMMAND PROMPT
        
       printf("type in your command$ ");

	/* Read a shell command; exit on end of file */
        if (fgets(cmd, CMD_SIZE, stdin) == NULL) {
            if (verbose)
                printf("\tinit: exiting");
            printf("\n");
            exit(EXIT_SUCCESS);
        }

        if (cmd[strlen(cmd) - 1] == '\n')
            cmd[strlen(cmd) - 1] = '\0';        /* Strip trailing '\n' */

        if (strlen(cmd) == 0)
            continue;           /* Ignore empty commands */
	
	//CREATE A NEW CHILD PROCESS	
        pid = fork();

        if (pid == -1)
            errExit("fork");

        if (pid == 0) {         /* Child */


            // Make child the leader of a new process group and
            //   make that process group the foreground process
            //   group for the terminal 

            if (setpgid(0, 0) == -1)
                errExit("setpgid");;
            if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
                errExit("tcsetpgrp-child");

           //MAKE THE CHILD RUN THE COMMAND
           char **args;
           execvp(args[0], args);


        }

        // Parent

        if (verbose)
            printf("\tinit: created child %ld\n", (long) pid);

        pause();                /* Will be interrupted by signal handler */

        /* Making the 'init' program the foreground process group for the terminal */

        if (tcsetpgrp(STDIN_FILENO, getpgrp()) == -1)
            errExit("tcsetpgrp-parent");
    }
}
