#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>

int main(int argc, char *argv[]) {
  struct passwd *pw = getpwuid(getuid());
  if (!pw) {
    fprintf(stderr, "Can't get passwd!\n");
    exit(EXIT_FAILURE);
  }
  printf("Hello! I am pid %d run by user %s\n", getpid(), pw->pw_name);
  printf("My parent is %d\n", getppid());
  printf("I will now consume all cpu!\n");
  int x = 1;
  while (1) {
    for (int i = 0; i < 10000000; i++) {
      x = i;
    }
  }
}
