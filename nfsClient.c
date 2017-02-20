#include "nfsClient.h"

#define BUFFER_SIZE 4096
#define SERV_PORT 3000


#define CRT_DIR_FLAG   1
#define CRT_FILE_FLAG  2
#define OPEN_DIR_FLAG  3
#define OPEN_FILE_FLAG 4


#define READ_CALL_ID 5
#define WRITE_CALL_ID 6
#define FILE_SYNC_CALL_ID 7
#define READ_DIR_CALL_ID 8
#define RM_DIR_CALL_ID 9

#define htonll(x) ((1==htonl(1)) ? (x) : ((uint64_t)htonl((x) & 0xFFFFFFFF) << 32) | htonl((x) >> 32))
#define ntohll(x) ((1==ntohl(1)) ? (x) : ((uint64_t)ntohl((x) & 0xFFFFFFFF) << 32) | ntohl((x) >> 32))

typedef struct my_buffer {
    size_t next;
    size_t size;
    char *data;
} my_buffer;

my_buffer *new_buffer() {
    my_buffer *b = malloc(sizeof(my_buffer));
    b->data = malloc(INITIAL_SIZE);
    b->size = INITIAL_SIZE;
    b->next = 0;
    return b;
}

void reserve_space(my_buffer *b, size_t bytes) {
    if ((b->next + bytes) > b->size) {
        /* double size to enforce O(lg N) reallocs */
        b->data = realloc(b->data, b->size * 2);
        b->size *= 2;
    }
}

void serialize_size_t(size_t x, my_buffer *b) {
    /* assume int == long; how can this be done better? */
    x = htonll(x);
    reserve_space(b, sizeof(x));
    memcpy(b->data + b->next, &x, sizeof(x));
    b->next += sizeof(x);
}

void serialize_str(char *x, my_buffer *b) {
    reserve_space(b, strlen(x) + 1);
    strcpy(b->data + b->next, x);
    b->next += strlen(x) + 1;
}

//deserialize str from next
void deserialize_size_t(size_t *x, char *b, size_t offset) {
    memcpy(&tmp, b + offset, sizeof(size_t));
    *x = ntohll(tmp);
}


//deserialize str from next
void deserialize_str(char *x, char *b, size_t offset, size_t len) {
    memcpy(x, b + offset, len);
}




typedef struct nfs_context {
    /* data */
    //the cache associate with each filre descriptor for commit

} nfs_context;


int get_udp_socket(char *server_ip, struct sockaddr_in *servaddr) {
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        //fail to create socket
        return -1;
    }

    //Creation of the socket
    memset(servaddr,
           0, sizeof(servaddr));
    servaddr->sin_family = AF_INET;
    servaddr->sin_addr.s_addr = inet_addr(server_ip);
    servaddr->sin_port = htons(SERV_PORT); //convert to big-endian order

    int slen = sizeof(servaddr);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 8000;

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        //fail to set timeout
        return -1;
    }
    return sockfd;
}

void get_response(int sockfd, char *sendbuffer,  struct sockaddr_in *servaddr, int slen, char *recvbuffer){
    //send the cd data to server
    sendto(sockfd, sendbuffer, strlen(sendbuffer), 0, (struct sockaddr *) servaddr, slen);

    //busy waiting for response
    while (recvfrom(sockfd, recvbuffer, strlen(recvline), 0, (struct sockaddr *) servaddr, &slen) < 0) {
        //resend
        sendto(sockfd, sendbuffer, strlen(sendbuffer), 0, (struct sockaddr *) servaddr, slen);
    }
}


int nfs_open_internal(const char *nfs_server_ip, const char *path, file_handler **fh,  size_t flag) {
    //sed a path to server, get a file handler
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    if (sockfd < 0) return -1;

    my_buffer *arg_buffer = new_buffer();


    serialize_size_t(flag, arg_buffer);
    serialize_size_t(strlen(path) + 1, arg_buffer);
    serialize_str(path, arg_buffer);

    int slen = sizeof(servaddr);
    char sendbuffer[arg_buffer.next];
    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler
    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);

    get_response(sockfd, sendbuffer,  &servaddr, slen, recvbuffer);
    //deserialize the fh received from server
    //set approporiate field in file_handler

    if (flag == OPEN_FILE_FLAG || flag == CRT_CALL_ID){
        file_handler *my_fh = malloc(sizeof(file_handler));
        *fh = my_fh;
    }
    
    free(arg_buffer);
    close(sockfd);
}

int nfs_open(const char *nfs_server_ip, const char *path, file_handler **fh){
    return nfs_open_internal(nfs_server_ip, path, fh, OPEN_FILE_FLAG);
}

int nfs_create(const char *nfs_server_ip, const char *path, file_handler **fh){
    return nfs_open_internal(nfs_server_ip, path, fh, CRT_FILE_FLAG);
}

int nfs_open_dir(const char *nfs_server_ip, const char *path){
    return nfs_open_internal(nfs_server_ip, path, NULL, OPEN_DIR_FLAG);
}

int nfs_mkdir(const char *nfs_server_ip, const char *path){
    return nfs_open_internal(nfs_server_ip, path, NULL, CRT_DIR_FLAG);
}


int nfs_read(const char *nfs_server_ip, file_handler *nfsfh, size_t offset, size_t size, char *buf) {
    //read content from offset, and maximum size fron file specified by the nfsfh
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    //serialize cd into send sendbuffer;
    if (sockfd < 0) return -1;

    my_buffer * arg_buffer= new_buffer();
    serialize_size_t(READ_CALL_ID, arg_buffer);
    serialize_str(nfsfh->file_id, arg_buffer);
    serialize_size_t(offset, arg_buffer);
    serialize_size_t(size, arg_buffer);


    int slen = sizeof(servaddr);
    char sendbuffer[cdbuffer.next];
    char recvbuffer[BUFFER_SIZE];

    memcpy(sendbuffer, arg_buffer->data, arg_buffer->data);

    get_response(sockfd, sendbuffer,  &servaddr, slen, recvbuffer);
    //copy data in recvbuffer to buf
    free(arg_buffer);
    close(sockfd);
    return 0;
}

int nfs_write(const char *nfs_server_ip, file_handler *nfsfh, size_t offset, size_t size, char *buf){
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    //serialize cd into send sendbuffer;
    if (sockfd < 0) return -1;

    my_buffer * arg_buffer= new_buffer();
    serialize_size_t(WRITE_CALL_ID, arg_buffer);
    serialize_str(nfsfh->file_id, arg_buffer);
    serialize_str(nfsfh->wc, arg_buffer);
    serialize_size_t(offset, arg_buffer);
    serialize_size_t(size, arg_buffer);
    serialize_size_t(strlen(buf) + 1, arg_buffer);
    serialize_str(buf, arg_buffer);


    int slen = sizeof(servaddr);
    char sendbuffer[cdbuffer.next];
    char recvbuffer[BUFFER_SIZE];

    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);

    get_response(sockfd, sendbuffer,  &servaddr, slen, recvbuffer);
    //get ack, set wc from sever


    free(arg_buffer);
    close(sockfd);
    return 0;

}


int nfs_fsync(const char *nfs_server_ip, file_handler *nfsfh){
    //send out call id and file descriptor
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    //serialize cd into send sendbuffer;
    if (sockfd < 0) return -1;

    my_buffer * arg_buffer= new_buffer();
    serialize_size_t(FILE_SYNC_CALL_ID, arg_buffer);
    serialize_str(nfsfh->file_id, arg_buffer);
    serialize_str(nfsfh->wc, arg_buffer);

    int slen = sizeof(servaddr);
    char sendbuffer[cdbuffer.next];
    char recvbuffer[BUFFER_SIZE];

    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);

    get_response(sockfd, sendbuffer,  &servaddr, slen, recvbuffer);
    //get response

    free(arg_buffer);
    close(sockfd);
    return 0;


    return 0;
}


int nfs_read_dir(const char *nfs_server_ip, const char *path, char ***dirs, int *num_dir){
    //send out call id and file descriptor
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    //serialize cd into send sendbuffer;
    if (sockfd < 0) return -1;

    my_buffer * arg_buffer= new_buffer();
    serialize_size_t(READ_DIR_CALL_ID, arg_buffer);
    serialize_size_t(strlen(path) + 1, arg_buffer);
    serialize_str(path, arg_buffer);

    int slen = sizeof(servaddr);
    char sendbuffer[cdbuffer.next];
    char recvbuffer[BUFFER_SIZE];

    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);
    get_response(sockfd, sendbuffer,  &servaddrdeserialize_str, slen, recvbuffer);
    //get response

    int rcv_num_dir = 0;
    char ** all_dirs = malloc(sizeof(*allDirs) * rcv_num_dir);

    int i = 0;
    size_t offset =0;

    for (i = 0; i < rcv_num_dir; ++i){
        int path_len = 0;
        all_dirs[i] = malloc(sizeof(char) * path_len);
        deserialize_str(all_dirs[i], recvbuffer, offset, path_len);
    }

    *dirs = all_dirs;

    free(arg_buffer);
    close(sockfd);
    return 0;
}

int nfs_rmdir(const char *nfs_server_ip, const char *path){
    struct sockaddr_in servaddr;
    int sockfd = get_udp_socket(nfs_server_ip, &servaddr);
    if (sockfd < 0) return -1;

    my_buffer *arg_buffer = new_buffer();

    serialize_size_t(RM_DIR_CALL_ID, arg_buffer);
    serialize_size_t(strlen(path) + 1, arg_buffer);
    serialize_str(path, arg_buffer);

    int slen = sizeof(servaddr);
    char sendbuffer[arg_buffer.next];
    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler
    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);

    get_response(sockfd, sendbuffer,  &servaddr, slen, recvbuffer);
    //deserialize the fh received from server
    free(arg_buffer);
    close(sockfd);
}










