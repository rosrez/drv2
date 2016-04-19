#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/netlink.h>

#define BUF_SIZE 4096

#define MSG_TEXT (NLMSG_MIN_TYPE + 1)

struct nlmsg {
    struct nlmsghdr hdr;
    char data[BUF_SIZE];
};

void netlink_populate(char *msg, char *data, size_t len)
{
    /* set up header */
    struct nlmsghdr *nlh = (struct nlmsghdr *) msg;
    nlh->nlmsg_len = len;
    nlh->nlmsg_type = MSG_TEXT;
    nlh->nlmsg_flags |= NLM_F_REQUEST;

    /* set up payload */
    char *payload = (char *) NLMSG_DATA(nlh);
    memcpy(payload, data, len);
}

char *netlink_msgdata(char *msg)
{
    struct nlmsghdr *nlh = (struct nlmsghdr *) msg;
    return (char *) NLMSG_DATA(nlh);
}

int *netlink_msglen(char *msg)
{
    struct nlmsghdr *nlh = (struct nlmsghdr *) msg;
    return NLMSG_PAYLOAD(nlh,nlh->nlmsg_len); 
}

int netlink_open(int dobind)
{
   struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_pid = getpid(),
    };

    int sd = socket(AF_NETLINK, SOCK_RAW, NETLINK_USERSOCK);
    if (sd < 0) {
        perror("socket() failed");
        return -1;
    }

    if (dobind) {
        if (bind(sd, (struct sockaddr *) &sa, sizeof(sa)) < 0) {
            perror("bind() failed");
            return -1;
        }
    }

    return sd;
}

int netlink_send(int sd, pid_t pid, char *buf)
{
    struct sockaddr_nl sa = {
        .nl_family = AF_NETLINK,
        .nl_pid = pid,
    };

    struct nlmsghdr *nlh = (struct nlmsghdr *) buf;

    struct iovec iov;
    struct msghdr msg;

    printf("Sending %d bytes to peer %d\n", nlh->nlmsg_len, sa.nl_pid);

    iov.iov_base = (void *) nlh;
    iov.iov_len = nlh->nlmsg_len;

    memset(&msg, 0, sizeof(msg));
    msg.msg_name = (void *) &sa;
    msg.msg_namelen = sizeof(sa);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

#if 0
    struct iovec iov = { nlmsg, nlmsg->hdr.nlmsg_len };
    struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
#endif

    if (sendmsg(sd, &msg, 0) < 0) {
        perror("sendmsg() failed");
        return -1;
    }

    return 0;
}

int netlink_recv(int sd, char *buf, size_t size)
{
    int len;
    struct iovec iov = { buf, size };
    struct sockaddr_nl sa;
    struct msghdr msg = { &sa, sizeof(sa), &iov, 1, NULL, 0, 0 };
    
    len = recvmsg(sd, &msg, 0);
    if (len < 0) {
        perror("recvmsg() failed");
        return -1;
    }

    return len;
}

int main(int argc, char *argv[])
{
    int sendmode;
    int sd;
    int peer;
    char out[BUF_SIZE];
    char in[BUF_SIZE];
    char str[256];

    sendmode = argc > 1;
    
    /* we are to send a message to a peer */
    if (sendmode) {
        peer = atoi(argv[1]);      
        if (argc > 2)
            strncpy(str, argv[2], sizeof(str));
        else
            strcpy(str, "Hello world");

    /* we are just to listen for incoming messages; show our PID */
    } else {
        printf("My PID is %d\n", getpid());
    }

    /* create and bind netlink socket */
    if ((sd = netlink_open(!sendmode)) < 0) {
        fprintf(stderr, "netlink_open() failed\n");
        return 1;
    }

    if (sendmode) {
        /* prepare data for sending */
        netlink_populate(out, str, strlen(str));

        /* send netlink message */
        if (netlink_send(sd, peer, &out[0]) < 0) {
            fprintf(stderr, "netlink_send() failed\n");
            return 2;
        }
    } else {
        printf("Listening\n");

        /* receive netlink message */
        if (netlink_recv(sd, in, sizeof(in)) < 0) {
            fprintf(stderr, "netlink_recv() failed\n");
            return 3;
        }
        printf("Received message: %s\n", (char *) NLMSG_DATA((struct nlmsghdr *) &in)); 
    }



#if 0
    if (sendto(sd, &buf[0], strlen(buf), 0, (void *) &dst, sizeof(dst)) < 0) {
        perror("sendto() failed");
        return 3;
    }

    ret = recv(sd, buf, sizeof(buf), 0);
    if (ret < 0) {
        perror("recv() failed");
        return 4;
    }

    printf("%.*s\n", (int) ret, buf);
#endif

    return EXIT_SUCCESS;
}
