#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/socket.h>
#include <linux/netlink.h>
#include <linux/connector.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/time.h>

#include "iwl_nl.h"
#include "iwl_connector.h"

static char netlink_buffer[IWL_NL_BUFFER_SIZE];
static uint32_t seq = 0;
int TCPSERVERPORT=8088;
#define TCPSERVERIP "127.0.0.1"
#define UDPIP "127.0.0.1"

int open_tcp_socket()
{
	int sockfd;
	struct sockaddr_in addr;
	if((sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0){
		return -1;
	}
	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TCPSERVERPORT);
	addr.sin_addr.s_addr = inet_addr(TCPSERVERIP);
	if(connect(sockfd, (struct sockaddr* )&addr, sizeof(addr)) < 0){
		return -1;
	}else{
		printf("success connect\n");
		return sockfd;
	}
}

int open_iwl_netlink_socket()
{
	/* Local variables */
	struct sockaddr_nl proc_addr, kern_addr; // addrs for recv, send, bind
	int sock_fd;

	/* Setup the socket */
	sock_fd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (sock_fd == -1) 
	{
		perror("socket");
		return -1;
	}

	/* Initialize the address structs */
	memset(&proc_addr, 0, sizeof(struct sockaddr_nl));
	proc_addr.nl_family = AF_NETLINK;
	proc_addr.nl_pid = getpid();			// this process' PID
	proc_addr.nl_groups = CN_IDX_IWLAGN;
	memset(&kern_addr, 0, sizeof(struct sockaddr_nl));
	kern_addr.nl_family = AF_NETLINK;
	kern_addr.nl_pid = 0;					// kernel
	kern_addr.nl_groups = CN_IDX_IWLAGN;

	/* Now bind the socket */
	if (bind(sock_fd, (struct sockaddr *)&proc_addr, sizeof(struct
					sockaddr_nl)) == -1) {
		close(sock_fd);
		perror("bind");
		return -1;
	}

	/* And subscribe to netlink group */
	int on = proc_addr.nl_groups;
	if (setsockopt(sock_fd, 270, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on))) {
		close(sock_fd);
		perror("setsockopt");
		return -1;
	}
	/* this could be the reason of resource temporarily unavailable, be not confirmed yet
	struct timeval tv;
	tv.tv_sec = 1;
	tv.tv_usec = 200000;
	if(setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0){
		close(sock_fd);
		perror("setsockopt");
		return -1;
	}*/
	return sock_fd;
}

int iwl_netlink_recv(int sock_fd, u_char **buf, int *len)
{
	int ret = recv(sock_fd, netlink_buffer, sizeof(netlink_buffer), 0);
	if (ret == -1) {
		printf("error number is: %d\n", errno);
		perror("netlink recv");
	    // ret = recv(sock_fd, netlink_buffer, sizeof(netlink_buffer), 0);
		return 100;
	}

	/* Pull out the message portion and print some stats */
	struct cn_msg *cmsg = NLMSG_DATA(netlink_buffer);
/* 	printf("received %d bytes: id: %u val: %u seq: %u clen: %d\n", */
/* 			cmsg->len, cmsg->id.idx, cmsg->id.val, */
/* 			cmsg->seq, cmsg->len); */
	*buf = cmsg->data;
	*len = cmsg->len;

	return ret;
}

int iwl_netlink_send(int sock_fd, const u_char *buf, int len)
{
	struct nlmsghdr *nlh;
	uint32_t size;
	int ret;
	u_char local_buf[IWL_NL_BUFFER_SIZE];
	struct cn_msg *m;

	/* Set up outer netlink message */
	size = NLMSG_SPACE(sizeof(struct cn_msg) + len);
	nlh = (struct nlmsghdr *)local_buf;
	nlh->nlmsg_seq = seq;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_len = NLMSG_LENGTH(size - sizeof(*nlh));
	nlh->nlmsg_flags = 0;

	/* Set up inner connector message */
	m = NLMSG_DATA(nlh);
	m->id.idx = CN_IDX_IWLAGN;
	m->id.val = CN_VAL_IWLAGN;
	m->seq = seq;
	m->ack = 0;
	m->len = len;
	memcpy(m->data, buf, len);

	/* Increment sequence number */
	++seq;

	/* Send message */
	ret = send(sock_fd, nlh, size, 0);
	return ret;
}
