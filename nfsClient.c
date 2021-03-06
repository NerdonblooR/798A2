#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>


#define SERV_PORT 9876
#define INITIAL_SIZE 100
#define BUFFER_SIZE 4096


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


void serialize_data(const char *data, my_buffer *b, size_t size) {
    reserve_space(b, size);
    memcpy(b->data + b->next, data, size);
    //printf("%s\n", b->data + b->next);
    b->next += size;
}

//deserialize str from next
void deserialize_int(int *x, char *b, size_t *offset) {
    int tmp;
    memcpy(&tmp, b + *offset, sizeof(int));
    *offset += sizeof(int);
    *x = ntohl(tmp);
}

//deserialize str from next
char *deserialize_str(char *b, size_t *offset) {
    printf("buffer offset: %d\n", *offset);
    char *ret = malloc(strlen(b + *offset) + 1);//str will be null terminated
    strcpy(ret, b + *offset);
    *offset += (strlen(ret) + 1);
    printf("ret size: %d\n", strlen(ret));
    return ret;
}


void get_tcp_response(int sockfd, char *recvbuffer) {
    char buffer[BUFFER_SIZE];
    size_t n = recv(sockfd, recvbuffer, BUFFER_SIZE, 0); //initial read
    if (n < 1) {
        //error: server terminated prematurely
        printf("The server terminated prematurely\n");
        exit(4);
    }
    printf("received %s\n", recvbuffer);
    printf("byte recevied %d\n", n);
}


void *get_readdir_response(int sockfd, int *num_dir) {
    char buffer[BUFFER_SIZE];
    char *data_buffer = malloc(1);
    size_t n = recv(sockfd, buffer, BUFFER_SIZE, 0); //initial read
    if (n == 0) {
        //error: server terminated prematurely
        printf("The server terminated prematurely\n");
        exit(4);
    }
    size_t offset = 0;
    deserialize_int(num_dir, buffer, &offset);
    int total_bytes;
    int read_bytes;
    deserialize_int(&total_bytes, buffer, &offset);
    if (total_bytes > 0) {
        data_buffer = realloc(data_buffer, total_bytes);
        memset(data_buffer, 0, total_bytes);
        read_bytes = n - offset;
        memcpy(data_buffer, buffer + offset, read_bytes);
        int rest_bytes = total_bytes - read_bytes;
        offset = read_bytes;
        printf("read: %d\n", read_bytes);
        printf("rest: %d\n", rest_bytes);
        while (rest_bytes > 0) {//buffer is full
            printf("WOW4\n");
            //deadlock when server failed
            read_bytes = recv(sockfd, data_buffer + offset, rest_bytes, 0);
            offset += read_bytes;
            rest_bytes -= read_bytes;
            //printf("data_buffer: %s\n", data_buffer);
            printf("read: %d\n", read_bytes);
            printf("rest: %d\n", rest_bytes);
        }
        if (rest_bytes == 0) read_bytes = recv(sockfd, buffer, BUFFER_SIZE, 0); // read final newline
    }

    return data_buffer;
}


char *get_read_response(int sockfd, file_handler *fh, int *total_bytes) {
    char buffer[BUFFER_SIZE];
    char *data_buffer = malloc(1);
    size_t n = recv(sockfd, buffer, BUFFER_SIZE, 0); //initial read
    if (n == 0) {
        //error: server terminated prematurely
        printf("The server terminated prematurely\n");
        exit(4);
    }
    printf("WOW\n");
    int response_code;
    size_t offset = 0;
    deserialize_int(&response_code, buffer, &offset);
    printf("WOW5\n");
    int read_bytes;
    fh->wc = deserialize_str(buffer, &offset);
    if (response_code == 0) {
        printf("WOW2\n");
        deserialize_int(total_bytes, buffer, &offset);
        printf("taotal_bytes: %d\n", *total_bytes);
        data_buffer = realloc(data_buffer, *total_bytes);
        memset(data_buffer, 0, *total_bytes);
        read_bytes = n - offset;
        memcpy(data_buffer, buffer + offset, read_bytes);
        printf("WOW3\n");
        int rest_bytes = *total_bytes - read_bytes;
        offset = read_bytes;
        while (rest_bytes > 0) {//buffer is full
            printf("keep reading\n");
            //deadlock when server failed
            read_bytes = recv(sockfd, data_buffer + offset, rest_bytes, 0);
            offset += read_bytes;
            rest_bytes -= read_bytes;
        }
        if (rest_bytes == 0) read_bytes = recv(sockfd, buffer, BUFFER_SIZE, 0); // read final newline
    }

    return data_buffer;
}

void handle_send(int sockfd, my_buffer *arg_buffer) {
    //need to break large packet into several small one
    char sendbuffer[arg_buffer->next];
    memset(sendbuffer, 0, sizeof(sendbuffer));
    memcpy(sendbuffer, arg_buffer->data, arg_buffer->next);
    if (arg_buffer->size <= BUFFER_SIZE) {
        send(sockfd, sendbuffer, arg_buffer->next, 0);
    } else {
        //not tested
        size_t offset = 0;
        size_t to_send = arg_buffer->next;
        while (offset < to_send) {
            offset = send(sockfd, sendbuffer + offset, to_send, 0);
            to_send - offset;
        }
    }
}

void print_buffer(my_buffer *arg_buffer) {
    printf("size: %d\n", arg_buffer->size);
    printf("next: %d\n", arg_buffer->next);
    printf("data: %s\n", arg_buffer->data + 1);
}


int nfs_getattr(int sockfd, const char *path, struct stat *stbuf) {
    int cid = 1;
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    print_buffer(arg_buffer);
    serialize_str("GETATTR", arg_buffer);
    serialize_str(path, arg_buffer);
    serialize_char('\n', arg_buffer);

    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler
    memset(recvbuffer, 0, sizeof(recvbuffer));

    printf("HERE1\n");
    handle_send(sockfd, arg_buffer);
    printf("HERE2\n");
    get_tcp_response(sockfd, recvbuffer);
    printf("HERE3\n");


    int type, file_size;
    size_t offset = 0;
    deserialize_int(&type, recvbuffer, &offset);
    deserialize_int(&file_size, recvbuffer, &offset);

    memset(stbuf, 0, sizeof(struct stat));
    if (type == 0) {
        //path
        stbuf->st_mode = 0;
        stbuf->st_size = file_size;
        printf("dir size: %d\n", file_size);
    } else {
        //file
        stbuf->st_mode = 1; //change this when copy to fuse
        stbuf->st_size = file_size;
        printf("file size: %d\n", file_size);
    }

    free(arg_buffer);
    return 0;
}


int nfs_open_internal(int sockfd, const char *path, file_handler **fh, const char *command) {
    //sed a path to server, get a file handler
    int cid = 0;
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
    deserialize_int(&f_id, recvbuffer, &offset);
    if (f_id < 0) return -1;

    my_fh->file_id = f_id;
    printf("file_id: %d\n", my_fh->file_id);
    my_fh->offset = 0;
    printf("offset: %d\n", my_fh->offset);
    my_fh->wc = deserialize_str(recvbuffer, &offset);
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
    size_t offset = 0;
    deserialize_int(&response_code, recvbuffer, &offset);
    if (response_code < 0) {
        printf("delete failed\n");
        return -1;
    } else {
        printf("delete success\n");

    }

    free(arg_buffer);
    return 0;
}


int nfs_read(int sockfd, file_handler *nfsfh, size_t offset, size_t size, char *buf) {
    int cid = 2;
    printf("===========start read=========\n");
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    serialize_str("READ", arg_buffer);
    print_buffer(arg_buffer);
    serialize_int(nfsfh->file_id, arg_buffer);
    serialize_int(offset, arg_buffer);
    serialize_int(size, arg_buffer);
    serialize_str(nfsfh->wc, arg_buffer);
    print_buffer(arg_buffer);
    serialize_char('\n', arg_buffer);
    print_buffer(arg_buffer);

    handle_send(sockfd, arg_buffer);
    int total_byte;
    char *data = get_read_response(sockfd, nfsfh, &total_byte);
    printf("HERE3: %d\n", total_byte);
    memcpy(buf, data, total_byte);

    //printf("data: %s\n", buf);


    free(data);
    free(arg_buffer);
    return total_byte;
}


int nfs_write(int sockfd, file_handler *nfsfh, size_t offset, size_t size, char *buf) {
    int cid = 2;
    printf("===========start write=========\n");
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    serialize_str("WRITE", arg_buffer);
    serialize_int(nfsfh->file_id, arg_buffer);
    serialize_int(offset, arg_buffer);
    serialize_int(size, arg_buffer);
    serialize_str(nfsfh->wc, arg_buffer);
    serialize_data(buf, arg_buffer, size);
    serialize_char('\n', arg_buffer);


    char recvbuffer[BUFFER_SIZE]; //need to receive a file handler

    printf("HERE1\n");

    handle_send(sockfd, arg_buffer);

    printf("HERE2\n");

    get_tcp_response(sockfd, recvbuffer);

    printf("HERE3\n");


    int response_code;
    size_t my_offset = 0;
    deserialize_int(&response_code, recvbuffer, &my_offset);
    nfsfh->wc = deserialize_str(recvbuffer, &my_offset);
    printf("wc: %s\n", nfsfh->wc);

    if (response_code < 0) {
        printf("write failed\n");
        return -1;
    } else {
        printf("write success\n");

    }
    free(arg_buffer);
    return 0;
}

int nfs_fsync(int sockfd, file_handler *nfsfh) {

    int cid = 2;
    printf("===========start fsync=========\n");
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    serialize_str("COMMIT", arg_buffer);
    print_buffer(arg_buffer);
    serialize_int(nfsfh->file_id, arg_buffer);
    serialize_str(nfsfh->wc, arg_buffer);
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
    size_t my_offset = 0;
    deserialize_int(&response_code, recvbuffer, &my_offset);
    nfsfh->wc = deserialize_str(recvbuffer, &my_offset);
    printf("wc: %s\n", nfsfh->wc);

    if (response_code < 0) {
        printf("fsync failed\n");
        return -1;
    } else {
        printf("fsync success\n");

    }

    free(arg_buffer);
    return 0;
}


int nfs_read_dir(int sockfd, const char *path, char ***dirs) {
    int cid = 4;
    my_buffer *arg_buffer = new_buffer();
    serialize_int(cid, arg_buffer);
    serialize_str("READDIR", arg_buffer);
    serialize_str(path, arg_buffer);
    serialize_char('\n', arg_buffer);


    printf("HERE1\n");
    handle_send(sockfd, arg_buffer);
    printf("HERE2\n");
    int num_dir;
    char *all_dirs_data = get_readdir_response(sockfd, &num_dir);
    printf("HERE3\n");

    size_t offset = 0;
    char **all_dirs = malloc(sizeof(*all_dirs) * num_dir);
    int i;
    for (i = 0; i < num_dir; ++i) {
        int path_len = 0;
        all_dirs[i] = deserialize_str(all_dirs_data, &offset);
        printf("%s\n", all_dirs[i]);
    }

    *dirs = all_dirs;

    printf("HERE4\n");
    free(all_dirs_data);
    free(arg_buffer);

    return num_dir;
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

    file_handler *fh;

    struct stat stbuf;

    char **dirs;
    nfs_read_dir(sockfd, argv[2], &dirs);
    //nfs_getattr(sockfd, argv[2], &stbuf);
//    nfs_create(sockfd, argv[2], &fh);
//    nfs_mkdir(sockfd, argv[2]);
//    nfs_rmdir(sockfd, argv[2]);
//    char buf[BUFFER_SIZE * 2];
//    nfs_open(sockfd, argv[2], &fh);
//    //nfs_read(sockfd, fh, 0, BUFFER_SIZE, buf);
//    int i = 0;
//    size_t offset = 0;
//    for (i = 0; i < 10; i++) {
//        offset += nfs_read(sockfd, fh, offset, BUFFER_SIZE, buf);
//    }
//
//
//    char *to_write = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";
//
//    int i = 0;
//    size_t offset = 0;
//    for (i = 0; i < 10000; i++) {
//        printf(".");
//        nfs_write(sockfd, fh, offset, strlen(to_write), to_write);
//        offset += strlen(to_write);
//    }
//
//    nfs_write(sockfd, fh, 0, strlen(to_write), to_write);
//    nfs_fsync(sockfd, fh);
//
//    char **dirs;
//    nfs_read_dir(sockfd, argv[2], &dirs);
    return 0;
}










