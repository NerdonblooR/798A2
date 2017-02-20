#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

typedef struct file_handler {
    /* data */
    char *file_id;
    size_t offset;
    size_t data_size;
    char *wc;
    char *data; //contain all the file data?

} file_handler;


int get_udp_socket(char *server_ip, struct sockaddr_in *servaddr);

void get_response(int sockfd, char *sendbuffer,  struct sockaddr_in *servaddr, int slen, char *recvbuffer);

int nfs_open(const char *nfs_server_ip, const char *path, file_handler **fh);

int nfs_create(const char *nfs_server_ip, const char *path, file_handler **fh);

int nfs_open_dir(const char *nfs_server_ip, const char *path);

int nfs_mkdir(const char *nfs_server_ip, const char *path);

int nfs_read(const char *nfs_server_ip, file_handler *nfsfh, size_t offset, size_t size, char *buf);

int nfs_write(const char *nfs_server_ip, file_handler *nfsfh, size_t offset, size_t size, char *buf);

int nfs_fsync(const char *nfs_server_ip, file_handler *nfsfh);

int nfs_read_dir(const char *nfs_server_ip, const char *path, char ***dirs, int *num_dir);

int nfs_rmdir(const char *nfs_server_ip, const char *path);