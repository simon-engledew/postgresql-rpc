/* PostgreSQL */
#include <postgres.h>
#include <funcapi.h>
#include <utils/guc.h>
#include <utils/builtins.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

PG_MODULE_MAGIC;


/* Function signatures */
void _PG_init(void);
void _PG_fini(void);


/* Global variables */
char * g_hostname;
int g_port;
int g_timeout;

/* Startup */
void _PG_init(void)
{
    DefineCustomStringVariable("rpc.hostname", "Hostname", NULL, &g_hostname, false, PGC_USERSET, GUC_NOT_IN_SAMPLE, NULL, NULL, NULL);
    DefineCustomIntVariable("rpc.port", "Port", NULL, &g_port, false, 1, 65535, PGC_USERSET, GUC_NOT_IN_SAMPLE, NULL, NULL, NULL);
    DefineCustomIntVariable("rpc.timeout", "Timeout (Seconds)", NULL, &g_timeout, 5, 0, 60, PGC_USERSET, GUC_NOT_IN_SAMPLE, NULL, NULL, NULL);
}


void _PG_fini(void)
{
}

#ifndef MSG_MORE
# define MSG_MORE 0
#endif

ssize_t sendall(int socket, const char *buffer, size_t length);
ssize_t sendall(int sockfd, const char *buffer, size_t len)
{
    ssize_t total_bytes = 0;
    ssize_t bytes = 0;

    while (len > 0) {
        bytes = send(sockfd, buffer + total_bytes, len, MSG_MORE);
        if (bytes == -1)
            break;
        total_bytes += bytes;
        len -= bytes;
    }

    return total_bytes;
}

ssize_t recvall(int sockfd, char *buffer, size_t len);
ssize_t recvall(int sockfd, char *buffer, size_t len)
{
    ssize_t total_bytes = 0;
    ssize_t bytes = 0;

    while (len > 0) {
        bytes = recv(sockfd, buffer + total_bytes, len, 0);
        if (bytes == -1)
            break;
        total_bytes += bytes;
        len -= bytes;
    }

    return total_bytes;
}

Datum rpc_request(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(rpc_request);
Datum rpc_request(PG_FUNCTION_ARGS)
{
    int flags;

    char protoname[] = "tcp";
    struct protoent *protoent;
    text * request_text;
    text * response_text;
    char * request;
    char * response;

    struct timeval timeout;

    uint32_t send_len;
    uint32_t send_netlen;

    uint32_t recv_len;
    uint32_t recv_netlen;

    int sockfd;
    in_addr_t in_addr;
    struct hostent *hostent;
    struct sockaddr_in sockaddr_in;

    fd_set w, x;

    if (PG_ARGISNULL(0)) {
        elog(ERROR, "An JSON document must be provided");
        PG_RETURN_NULL();
    }

    if (g_hostname == false) {
        elog(ERROR, "rpc.hostname must be set");
        PG_RETURN_NULL();
    }

    if (g_port == false) {
        elog(ERROR, "rpc.port must be set");
        PG_RETURN_NULL();
    }

    request_text = PG_GETARG_TEXT_P(0);

    request = text_to_cstring(request_text);

    send_len = strlen(request);

    send_netlen = htonl(send_len);

    protoent = getprotobyname(protoname);
    if (protoent == NULL) {
        PG_RETURN_NULL();
    }

    sockfd = socket(AF_INET, SOCK_STREAM, protoent->p_proto);
    if (sockfd == -1) {
        PG_RETURN_NULL();
    }

    timeout.tv_sec  = g_timeout;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    hostent = gethostbyname(g_hostname);
    if (hostent == NULL) {
        elog(ERROR, "Could not lookup host by name");
        PG_RETURN_NULL();
    }
    in_addr = inet_addr(inet_ntoa(*(struct in_addr*)*(hostent->h_addr_list)));
    if (in_addr == (in_addr_t)-1) {
        elog(ERROR, "Could not lookup address");
        PG_RETURN_NULL();
    }
    sockaddr_in.sin_addr.s_addr = in_addr;
    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port = htons(g_port);

    flags = fcntl(sockfd, F_GETFL);
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        elog(ERROR, "could not set socket nonblocking");
        PG_RETURN_NULL();
    }

    FD_ZERO(&w);
    FD_ZERO(&x);
    FD_SET(sockfd, &w);
    FD_SET(sockfd, &x);

    if (connect(sockfd, (struct sockaddr*)&sockaddr_in, sizeof(sockaddr_in)) != -1) {
        elog(ERROR, "not a non blocking connection");
        PG_RETURN_NULL();
    }

    if (errno != EINPROGRESS) {
        elog(ERROR, "not connecting");
        PG_RETURN_NULL();
    }

    select(FD_SETSIZE, NULL, &w, &x, &timeout);

    if (!FD_ISSET(sockfd, &w))
    {
        elog(ERROR, "connection timed out");
        PG_RETURN_NULL();
    }

    // opts &= ~O_NONBLOCK;

    if (fcntl(sockfd, F_SETFL, flags & (~O_NONBLOCK)) == -1) {
        elog(ERROR, "could not set socket blocking");
        PG_RETURN_NULL();
    }

    if (send(sockfd, &send_netlen, sizeof(uint32_t), MSG_MORE) == -1) {
        elog(ERROR, "Length did not send");
        PG_RETURN_NULL();
    }

    if (sendall(sockfd, request, send_len) == -1) {
        elog(ERROR, "Data did not send");
        PG_RETURN_NULL();
    }

    if (shutdown(sockfd, SHUT_WR) == -1) {
        elog(ERROR, "Could not shutdown write");
        PG_RETURN_NULL();
    }

    if (recv(sockfd, &recv_netlen, sizeof(uint32_t), 0)  == -1) {
        elog(ERROR, "Could not read len");
        PG_RETURN_NULL();
    }

    recv_len = ntohl(recv_netlen);

    response = (char *)malloc(recv_len + 1);
    memset(response, 0, recv_len + 1);

    if (recvall(sockfd, response, recv_len)  == -1) {
        free(response);
        elog(ERROR, "Could not read data");
        PG_RETURN_NULL();
    }

    response_text = cstring_to_text(response);

    free(response);

    if (close(sockfd) == -1) {
        elog(ERROR, "Could not close");
        PG_RETURN_NULL();
    }

    PG_RETURN_TEXT_P(response_text);
}
