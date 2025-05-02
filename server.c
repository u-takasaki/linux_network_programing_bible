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
	char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct addrinfo hints, *res0;
	int soc, opt, errcode;
	socklen_t opt_len;

	(void)memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((errcode = getaddrinfo(NULL, portnm, &hints, &res0)) != 0){
		(void)fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(errcode));
		return (-1);
	}
	if((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen, nbuf, sizeof(nbuf), sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)) != 0){
		(void)fprintf(stderr, "getnameinfo(): %s\n", gai_strerror(errcode));
		freeaddrinfo(res0);
		return (-1);
	}
	(void)fprintf(stderr, "port=%s\n", sbuf);
	/*create socket*/
	if((soc = socket(res0->ai_family, res0->ai_socktype, res0->ai_protocol)) == 1){
		perror("socket");
		freeaddrinfo(res0);
		return (-1);
	}
	/*socket option(reused flag on)setting*/
	opt = 1;
	opt_len = sizeof(opt);
	if(setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) == -1){
		perror("setsockopt");
		(void)close(soc);
		freeaddrinfo(res0);
		return (-1);
	}
	/*specify address in socket*/
	if(bind(soc, res0->ai_addr, res0->ai_addrlen) == -1){
		perror("bind");
		(void)close(soc);
		freeaddrinfo(res0);
		return (-1);
	}
	/*specify access back log*/
	if(listen(soc, SOMAXCONN) == -1){
		perror("listen");
		(void)close(soc);
		freeaddrinfo(res0);
		return (-1);
	}
	freeaddrinfo(res0);
	return (soc);
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
