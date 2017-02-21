#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>


#define SERV_PORT 9876

#define INITIAL_SIZE 100
#define BUFFER_SIZE 4096

#define CRT_DIR_FLAG   1
#define CRT_FILE_FLAG  2
#define OPEN_DIR_FLAG  3
#define OPEN_FILE_FLAG 4
#define READ_CALL_ID 5
#define WRITE_CALL_ID 6
#define FILE_SYNC_CALL_ID 7
#define READ_DIR_CALL_ID 8
#define RM_DIR_CALL_ID 9


typedef struct file_handler {
    /* data */
    int file_id;
    size_t offset;
    char *wc;
} file_handler;

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
    while ((b->next + bytes) > b->size) {
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


void serialize_int(int x, my_buffer *b) {
    x = htonl(x);
    reserve_space(b, sizeof(x));
    memcpy(b->data + b->next, &x, sizeof(x));
    b->next += sizeof(x);
}

void serialize_char(char x, my_buffer *b) {
    reserve_space(b, sizeof(x));
    memcpy(b->data + b->next, &x, sizeof(x));
    printf("%s\n", b->data + b->next);
    b->next += sizeof(x);
}


void serialize_str(const char *x, my_buffer *b) {
    reserve_space(b, strlen(x) + 1);
    strcpy(b->data + b->next, x);

    printf("%s\n", b->data + b->next);
    b->next += strlen(x) + 1;
}

//deserialize str from next
void deserialize_int(int *x, char *b, size_t offset) {
    int tmp;
    memcpy(&tmp, b + offset, sizeof(int));
    *x = ntohl(tmp);
}

//deserialize str from next
char *deserialize_str(char *b, size_t offset) {
    char *ret = malloc(strlen(b + offset) + 1);//str will be null terminated
    strcpy(ret, b + offset);
    printf("ret size: %d\n", strlen(ret));
    return ret;
}


void get_tcp_response(int sockfd, char *recvbuffer) {
    char buffer[BUFFER_SIZE];
    size_t n = recv(sockfd, recvbuffer, BUFFER_SIZE, 0); //initial read
    if (n == 0) {
        //error: server terminated prematurely
        printf("The server terminated prematurely\n");
        exit(4);
    }

    printf("received %s\n", recvbuffer);
    printf("byte recevied %d\n", n);

}

//break large packet into several small one
void handle_send(int sockfd, my_buffer *arg_buffer) {
    char sendbuffer[BUFFER_SIZE];
    memset(sendbuffer, 0, sizeof(sendbuffer));
    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);
    if (arg_buffer->size <= BUFFER_SIZE) {
        send(sockfd, sendbuffer, arg_buffer->size, 0);
    } else {

    }

}

void print_buffer(my_buffer *arg_buffer) {
    printf("size: %d\n", arg_buffer->size);
    printf("next: %d\n", arg_buffer->next);
    printf("data: %s\n", arg_buffer->data + 1);
}


int nfs_open_internal(int sockfd, const char *path, file_handler **fh, const char *command) {
    //sed a path to server, get a file handler
    int cid = 1;
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    print_buffer(arg_buffer);
    serialize_str(command, arg_buffer);
    print_buffer(arg_buffer);
    serialize_str(path, arg_buffer);
    print_buffer(arg_buffer);
    serialize_char('\n', arg_buffer);
    print_buffer(arg_buffer);

    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler
    memset(recvbuffer, 0, sizeof(recvbuffer));


    printf("HERE1\n");

    handle_send(sockfd, arg_buffer);

    printf("HERE2\n");

    get_tcp_response(sockfd, recvbuffer);

    printf("HERE3\n");

    //deserialize the fh received from server
    //set approporiate field in file_handler
    file_handler *my_fh = malloc(sizeof(file_handler));
    int f_id;
    size_t offset = 0;
    deserialize_int(&f_id, recvbuffer, offset);
    offset += sizeof(f_id);

    if (f_id < 0) return -1;

    my_fh->file_id = f_id;
    printf("file_id: %d\n", my_fh->file_id);
    my_fh->offset = 0;
    printf("offset: %d\n", my_fh->offset);
    my_fh->wc = deserialize_str(recvbuffer,offset);
    printf("wc: %s\n", my_fh->wc);


    *fh = my_fh;
    free(arg_buffer);
    return 0;
}

int nfs_open(int sockfd, const char *path, file_handler **fh) {
    return nfs_open_internal(sockfd, path, fh, "LOOKUP");
}

int nfs_create(int sockfd, const char *path, file_handler **fh) {
    return nfs_open_internal(sockfd, path, fh, "CREATE");
}

int nfs_mkdir(int sockfd, const char *path) {
    file_handler **fh;
    return nfs_open_internal(sockfd, path, fh, "MKDIR");
}


int nfs_rmdir(int sockfd, const char *path) {
    int cid = 1;
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    print_buffer(arg_buffer);
    serialize_str("DELETE", arg_buffer);
    print_buffer(arg_buffer);
    serialize_str(path, arg_buffer);
    print_buffer(arg_buffer);
    serialize_char('\n', arg_buffer);
    print_buffer(arg_buffer);

    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler

    printf("HERE1\n");

    handle_send(sockfd, arg_buffer);

    printf("HERE2\n");

    get_tcp_response(sockfd, recvbuffer);

    printf("HERE3\n");


    int response_code;
    deserialize_int(&response_code, recvbuffer, 0);
    if (response_code < 0) {
        printf("delete failed\n");
        return -1;
    } else {
        printf("delete success\n");

    }
    free(arg_buffer);
    return 0;
}


int main(int argc, char *argv[]) {
    //communicate with server to set u_id

    if (argc < 2) {
        printf("No server ip specified\n");
        exit(1);
    }

    int sockfd;
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Problem in creating the socket\n");
        exit(2);
    }

    //Creation of the socket
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[1]);
    servaddr.sin_port = htons(SERV_PORT); //convert to big-endian order

    //Connection of the client to the socket
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        printf("Problem in connecting to the server\n");
        exit(3);
    }

    printf("HERE\n");

    file_handler *fh;
    //nfs_create(sockfd, argv[2], &fh);
    //nfs_open(sockfd, argv[2], &fh);
    // nfs_mkdir(sockfd, argv[2]);
    nfs_rmdir(sockfd, argv[2]);
    return 0;
}










