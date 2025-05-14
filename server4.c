#include <sys/epoll.h>
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

/*prepare server socket*/

int
server_socket(const char *portnm);


/*size-specified string concatenation*/

size_t
mystrlcat(char *dst, const char *src, size_t size);


/*loop for accept*/

void
accept_loop(int soc);


/*sending and recieving*/
int 
send_recv(int acc, int child_no);


int 
main(int argc, char *argv[])
{
	/*check specify port number*/
	if(argc <= 1){
		(void)fprintf(stderr, "server2 port\n");
		return EX_USAGE;
	}

	int soc;

	/*perpare server socket*/
	if((soc = server_socket(argv[1])) == -1){
		(void)fprintf(stderr, "server_socket(%s): error\n", argv[1]);
		return EX_UNAVAILABLE;
	}

	(void)fprintf(stderr, "ready for accept\n");
	/*accept loop*/
	accept_loop(soc);
	/*close socket*/
	(void)close(soc);

	return EX_OK;
}

int
server_socket(const char *portnm)
{
	char nbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct addrinfo hints, *res0, *tmp;
	int soc, opt, errcode;
	socklen_t opt_len;

	/*clear addrinfo by zero for hints valiable*/
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/*descision address info*/
	if((errcode = getaddrinfo(NULL, portnm, &hints, &res0)) != 0){
		(void)fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(errcode));
		return -1;
	}
	if((errcode = getnameinfo(res0->ai_addr, res0->ai_addrlen,
														nbuf, sizeof(nbuf),
														sbuf, sizeof(sbuf),
														NI_NUMERICHOST | NI_NUMERICSERV)) != 0){
		(void)fprintf(stderr, "getnaminfo(): %s\n", gai_strerror(errcode));
		freeaddrinfo(res0);
		return -1;
	}
	(void)fprintf(stderr, "port=%s\n", sbuf);

	/*create socket*/
	tmp = res0;
	while(tmp != NULL){
		if((soc = socket(tmp->ai_family, tmp->ai_socktype, 0)) == -1){
			perror("socket");
			continue;
		} else{
			break;
		}

		tmp = tmp->ai_next;
	}

	if(tmp == NULL){
		fprintf(stderr, "failed to create socket all\n");
		return -1;
	}

	opt = 1;
	opt_len = sizeof(opt);
	if(setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &opt, opt_len) == -1){
		perror("setsockopt");
		(void)close(soc);
		freeaddrinfo(res0);
		return -1;
	}

	/*specify address for socket*/
	if(bind(soc, tmp->ai_addr, tmp->ai_addrlen) == -1){
		perror("bind");
		(void)close(soc);
		freeaddrinfo(res0);
		return -1;
	}

	/*specify access back log*/
	if(listen(soc, SOMAXCONN) == -1){
		perror("listen");
		(void)close(soc);
		return -1;
	}

	freeaddrinfo(res0);
	return soc;
}

/*size-specified string concatenation*/

size_t
mystrlcat(char *dst, const char *src, size_t size)
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

/*sending and recieving*/
int 
send_recv(int acc, int child_no)
{
	char buf[512], *ptr;
	ssize_t len;

	/*recieve*/
	if((len = recv(acc, buf, sizeof(buf), 0)) == -1){
		/*error*/
		perror("recv");
		return -1;
	}

	/*stringification and display*/
	buf[len] = '\0';
	if((ptr = strpbrk(buf, "\r\n")) != NULL){
		*ptr = '\0';
	}
	(void)fprintf(stderr, "[child%d]%s\n", child_no, buf);
	
	/*create response string*/
	(void)mystrlcat(buf, ":OK\r\n", sizeof(buf));
	len = strlen(buf);

	/*response*/
	if((len = send(acc, buf, len, 0)) == -1){
		/*error*/
		perror("send");
		return -1;
	}

	return 0;
}

/*max number of concurrent processes*/

#define MAX_CHILD (20)

/*loop for accept*/
void
accept_loop(int soc)
{
	char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
	struct sockaddr_storage from;
	int acc, count, i, epollfd, nfds, ret;
	socklen_t len;
	struct epoll_event ev, events[MAX_CHILD];

	if((epollfd = epoll_create(MAX_CHILD + 1)) == -1){
		perror("epoll_create");
		return;
	}

	/*create data for epoll*/

	ev.data.fd = soc;
	ev.events = EPOLLIN;
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, soc, &ev) == -1){
		perror("epoll_ctl");
		(void)close(epollfd);
		return;
	}

	count = 0;
	for(;;){
		(void)fprintf(stderr, "<<child count: %d>>\n", count);
		switch ((nfds = epoll_wait(epollfd, events, MAX_CHILD, 10 * 1000))){
			case -1:
				/*error*/
				perror("epoll_wait");
				break;
			case 0:
				/*timed out*/
				break;
			default:
				/*socket is ready*/
				for(i = 0; i < nfds; i++){
					if(events[i].data.fd == soc){
						/*ready server socket*/
						len = (socklen_t)sizeof(from);
						/*recept to connect*/
						if((acc = accept(soc, (struct sockaddr *)&from, &len)) == -1){
							if(errno != EINTR){
								perror("accept");
							}
						} else{
							(void)getnameinfo((struct sockaddr *)&from, len,
																hbuf, sizeof(hbuf),
																sbuf, sizeof(sbuf),
																NI_NUMERICHOST | NI_NUMERICSERV);
							(void)fprintf(stderr, "accept:%s:%s\n", hbuf, sbuf);
							/*not empty*/
							if(count + 1 >= MAX_CHILD){
								/*cannot accept any more connections*/
								(void)fprintf(stderr, 
															"connection is full : cannot accept\n");
								/*close*/
								(void)close(acc);
							} else{
								ev.data.fd = acc;
								ev.events = EPOLLIN;
								if(epoll_ctl(epollfd, EPOLL_CTL_ADD, acc, &ev) == -1){
									perror("epoll_ctl");
									(void)close(acc);
									(void)close(epollfd);
									return;
								}
								count++;
							}
						}
					} else{
						/*sending and recieving*/
						if((ret = send_recv(events[i].data.fd, events[i].data.fd)) == -1){
							/*error or disconnect*/
							if(epoll_ctl(epollfd,
													 EPOLL_CTL_DEL,
													 events[i].data.fd,
													 &ev) == -1){
								perror("epoll_ctl");
								(void)close(events[i].data.fd);
								(void)close(epollfd);
								return;
							}
							/*close*/
							(void)close(events[i].data.fd);
							count--;
						}
					}
				}
				break;
		}
	}
	(void)close(epollfd);
}
				
