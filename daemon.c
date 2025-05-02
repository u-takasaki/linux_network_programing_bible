#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

int daemonize(int nochdir, int noclose);

#ifdef UNIT_TEST
#include <syslog.h>

int main(int argc, char *argv[])
{
	char buf[256];
	/*be daemon*/
	(void)daemonize(0, 0);
	/*check closed file descriptor*/
	(void)fprintf(stderr, "stderr\n");
	/*desplay current directory*/
	syslog(LOG_USER|LOG_NOTICE, "daemon:cwd=%s\n", getcwd(buf, sizeof(buf)));
	return (EX_OK);
}
#endif

/*closed max value of file descriptor*/
#define MAXFD 64
/*be daemon*/
int daemonize(int nochdir, int noclose)
{
	int i, fd;
	pid_t pid;
	if((pid = fork()) == -1){
		return -1;
	} else if(pid != 0){
		_exit(0);
	}
	/*first child process*/
	/*be session leader*/
	(void)setsid();
	/*ignore HUP signal*/
	(void)signal(SIGHUP, SIG_IGN);
	if((pid = fork()) != 0){
		/*close the first child process*/
		_exit(0);
	}
	/*daemon process*/
	if(nochdir == 0){
		/*move root directory*/
		(void)chdir("/");
	}
	if(noclose == 0){
		/*close all file descriptor*/
		for(i = 0; i < MAXFD; i++){
			(void)close(i);
		}
		/*open stdin, stdout and stderr redirect /dev/null*/
		if((fd = open("/dev/null", O_RDWR, 0)) != -1){
			(void)dup2(fd,0);
			(void)dup2(fd, 1);
			(void)dup2(fd, 2);
			if(fd > 2){
				(void)close(fd);
			}
		}
	}
	return 0;
}

