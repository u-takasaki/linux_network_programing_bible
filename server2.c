#include <sys/param.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

int server_socket(const char *portnm);
void accept_loop(int soc);
size_t mystrlcat(char *dst, const char *src, size_t size);
void send_recv_loop(int acc);

int main(int argc, char *argv[])
{
	int soc;
	/*checking whether a port number is specified in the arguments*/
	if(argc <= 1){
		(void)fprintf(stderr, "server port\n");
		return (EX_USAGE);
	}
	/*preparing server socket*/
	if((soc = server_socket(argv[1])) == -1){
		(void)fprintf(stderr, "server_socket(%s): error\n", argv[1]);
		return (EX_UNAVAILABLE);
	}
	(void)fprintf(stderr, "ready for accept\n");
	/*loop accept*/
	accept_loop(soc);
	/*close socket*/
	(void)close(soc);
	return (EX_OK);
}

int server_socket(const char *portnm)
{
	struct sockaddr_in my;
	int soc, portno, opt;
	socklen_t opt_len;
	struct servernt *se;

	/*setting zero address information*/
	(void)memset(&my, 0, sizeof(my));
	my.sin_family = AF_INET;
	my.sin_addr.s_addr = htonl(INADDR_ANY);
	/*decision port number*/
	if(isdigit(portnm[0])){
		/*head is number*/
		if((portno = atoi(portnm)) <= 0){
			/*when converted to a number, it is less than or equal to zero*/
			(void)fprintf(stderr, "bad port no\n");
			return -1;
		}
		my.sin_port = htons(portno);
	} else{
		if((se = getservbyname(portnm, "tcp")) == NULL){
			/*not find service*/
			(void)fprintf(stderr, "getservbyname(): error\n");
			return -1;
		} else{
			/*found service: aplicable port number*/
			my.sin_port = se->s_port;
		}
	}
	(void)fprintf(stderr, "port=%d\n", ntohs(my.sin_port));
	/*create socket*/
	if((soc = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		perror("socket");
		return -1;
	}
	/*setting socket option(reuse flag)*/
	opt = 1;
	opt_len = sizeof(opt);
	if(setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) == -1){
		perror("setsockopt");
		(void)close(soc);
		return -1;
	}
	/*specify an address for the socket*/
	if(bind(soc, (struct sockaddr *)&my, sizeof(my)) == -1){
		perror("bind");
		(void)close(soc);
		return -1;
	}
	/*specifying the listen backlog*/
	if(listen(soc, SOMAXCONN) == -1){
		perror("listen");
		(void)close(soc);
		return -1;
	}
	return soc;
}
}

void accept_loop(int soc)
{
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct sockaddr_storage from;
	int acc;
	socklen_t len;
	for(;;){
		len = (socklen_t)sizeof(from);
		/*connect reception*/
		if((acc = accept(soc, (struct sockaddr *)&from, &len)) == -1){
			if(errno != EINTR){
				perror("accept");
			}
		} else{
			(void) getnameinfo((struct sockaddr *) &from, len,
													hbuf, sizeof(hbuf),
													sbuf, sizeof(sbuf),
													NI_NUMERICHOST | NI_NUMERICSERV);
			(void) fprintf(stderr, "accept: %s:%s\n", hbuf, sbuf);
			/*loop for sending and receiving*/
			send_recv_loop(acc);
			/*accept socket close*/
			(void) close(acc);
			acc = 0;
		}
	}
}

/*size-bounded string concatenation*/
size_t mystrlcat(char *dst, const char *src, size_t size)
{
	const char *ps;
	char *pd, *pde;
	size_t dlen, lest;

	for(pd = dst, lest = size; *pd != '\0' && lest != 0; pd++, lest--);
	dlen = pd - dst;
	if(size - dlen == 0){
		return (dlen + strlen(src));
	}
	pde = dst + size - 1;
	for(ps = src; *ps != '\0' && pd < pde; pd++, ps++){
		*pd = *ps;
	}
	for(; pd <= pde; pd++){
		*pd = '\0';
	}
	while(*ps++);
	return (dlen + (ps - src - 1));
}

/*loop for sending and receiving*/
void send_recv_loop(int acc)
{
	char buf[512], *ptr;
	ssize_t len;
	for(;;){
		/*receive*/
		if((len = recv(acc, buf, sizeof(buf), 0)) == -1){
			/*error*/
			perror("recv");
			break;
		}
		if(len == 0){
			/*end of file*/
			(void)fprintf(stderr, "recv:EOF\n");
			break;
		}
		/*stringification and display*/
		buf[len] = '\0';
		if((ptr = strpbrk(buf, "\r\n")) != NULL){
			*ptr = '\0';
		}
		(void)fprintf(stderr, "[client]%s\n", buf);
		/*create text response*/
		(void)mystrlcat(buf, ":OK\r\n", sizeof(buf));
		len = (ssize_t)strlen(buf);
		/*response*/
		if((len = send(acc, buf, (size_t)len, 0)) == -1){
			/*error*/
			perror("send");
			break;
		}
	}
}
