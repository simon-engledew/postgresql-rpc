/* PostgreSQL */
#include <postgres.h>
#include <funcapi.h>
#include <utils/guc.h>
#include <utils/builtins.h>

#include <sys/un.h>
#include <unistd.h>

PG_MODULE_MAGIC;

void _PG_init(void);
void _PG_fini(void);

char * g_path;
int g_timeout;

void _PG_init(void)
{
    DefineCustomStringVariable("rpc.path", "Hostname", NULL, &g_path, "/var/run/postgresql/rpc.sock", PGC_USERSET, GUC_NOT_IN_SAMPLE, NULL, NULL, NULL);
    DefineCustomIntVariable("rpc.timeout", "Timeout (Seconds)", NULL, &g_timeout, 5, 0, 60, PGC_USERSET, GUC_NOT_IN_SAMPLE, NULL, NULL, NULL);
}

void _PG_fini(void)
{
}

#ifndef MSG_MORE
# define MSG_MORE 0
#endif

ssize_t send_cstring(int socket, const char *buffer);
ssize_t send_cstring(int sockfd, const char *buffer)
{
    ssize_t total_bytes = 0;
    ssize_t bytes = 0;
    uint32_t len = strlen(buffer);
    uint32_t netlen = htonl(len);

    if (send(sockfd, &netlen, sizeof(uint32_t), MSG_MORE) == -1) {
        return -1;
    }

    while (len > 0) {
        bytes = send(sockfd, buffer + total_bytes, len, 0);
        if (bytes == -1) {
            break;
        }
        total_bytes += bytes;
        len -= bytes;
    }

    return total_bytes;
}

char * recv_cstring(int sockfd);
char * recv_cstring(int sockfd)
{
    ssize_t total_bytes = 0;
    ssize_t bytes = 0;
    uint32_t len;
    uint32_t netlen;
    char * response;

    if (recv(sockfd, &netlen, sizeof(uint32_t), 0)  == -1) {
        return NULL;
    }

    len = ntohl(netlen);

    response = malloc(len + 1);

    if (response == NULL) {
        return NULL;
    }

    memset(response, 0, len + 1);

    while (len > 0) {
        bytes = recv(sockfd, response + total_bytes, len, 0);
        if (bytes == -1) {
            free(response);
            return NULL;
        }
        total_bytes += bytes;
        len -= bytes;
    }

    return response;
}

Datum rpc_request(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(rpc_request);
Datum rpc_request(PG_FUNCTION_ARGS)
{
    text * request_text;
    text * response_text;
    char * request;
    char * response;
    char * error;

    struct timeval timeout;

    int sockfd;
    struct sockaddr_un addr;

    if (PG_ARGISNULL(0)) {
        elog(ERROR, "JSON document cannot be NULL");
        PG_RETURN_NULL();
    }

    request_text = PG_GETARG_TEXT_P(0);
    request = text_to_cstring(request_text);

    sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd == -1) {
        PG_RETURN_NULL();
    }

    timeout.tv_sec  = g_timeout;
    timeout.tv_usec = 0;

    setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    memset(&addr, 0, sizeof(addr));

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
        error = strerror(errno);
        elog(ERROR, "Could not connect: %s", error);
        PG_RETURN_NULL();
    }

    if (send_cstring(sockfd, request) == -1) {
        error = strerror(errno);
        elog(ERROR, "Data did not send: %s", error);
        PG_RETURN_NULL();
    }

    if (shutdown(sockfd, SHUT_WR) == -1) {
        error = strerror(errno);
        elog(ERROR, "Could not shutdown write: %s", error);
        PG_RETURN_NULL();
    }

    response = recv_cstring(sockfd);

    if (response  == NULL) {
        error = strerror(errno);
        elog(ERROR, "Could not recv data: %s", error);
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
