#define FUSE_USE_VERSION 30
#define SERV_PORT 9876

#include <config.h>
#include <fuse.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

#include "nfsClient.c"

typedef struct nfs_context {
    char *server_ip;
    int port_num;
    int sock_fd;
    int is_up; //1 is up, 0 is down.
} nfs_context;


int sockfd;

static int fuse_nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi) {
    strcpy(buf, "DUMMY DIR");
    return 0;
}

static int fuse_nfs_open(const char *path, struct fuse_file_info *fi) {
    int ret = 0;
    file_handler *nfsfh;

    fi->fh = 0;
    ret =  nfs_open(sockfd, path, &nfsfh);
    if (ret < 0) {
        return ret;
    }

    fi->fh = (uint64_t) nfsfh; //file handler returned by the server to identify call
    return ret;
}


static int fuse_nfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    int ret = 0;
    file_handler *nfsfh;

    ret = nfs_create(sockfd, path, &fh);
    if (ret < 0) {
        return ret;
    }

    fi->fh = (uint64_t) nfsfh;

    return ret;
}

static int fuse_nfs_read(const char *path, char *buf, size_t size,
                    off_t offset, struct fuse_file_info *fi) {
    file_handler *nfsfh = (file_handler *) fi->fh;


    return nfs_read(sockfd,  nfsfh,  offset,  size,  buf);

}


static int fuse_nfs_write(const char *path, const char *buf, size_t size,
                     off_t offset, struct fuse_file_info *fi) {
    file_handler *nfsfh = (file_handler *) fi->fh;


    return nfs_write(sockfd, nfsfh,  offset,  size, discard_const(buf));

}


static int fuse_nfs_mkdir(const char *path, mode_t mode) {
    int ret = 0;
    ret = nfs_mkdir(sockfd, path);
    return ret;
}

static int nfs_rmdir(const char *path) {

    nfs_rmdir(sockfd, path);
}


static int fuse_nfs_fsync(const char *path, int isdatasync,
                     struct fuse_file_info *fi) {
    file_handler *nfsfh = (file_handler *) fi->fh;

    return nfs_fsync(sockfd, nfsfh);

}


static struct fuse_operations nfs_oper = {
        .create        = fuse_nfs_create,
        .fsync        = fuse_nfs_fsync,
        .mkdir        = fuse_nfs_mkdir,
        .open        = fuse_nfs_open,
        .read        = fuse_nfs_read,
        .readdir    = fuse_nfs_readdir,
        .rmdir        = fuse_nfs_rmdir,
        .write        = fuse_nfs_write,
};

int main(int argc, char *argv[]) {
    struct sockaddr_in servaddr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Problem in creating the socket\n");
        exit(2);
    }

    //Creation of the socket
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(argv[2]);
    servaddr.sin_port = htons(SERV_PORT); //convert to big-endian order

    //Connection of the client to the socket
    if (connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
        printf("Problem in connecting to the server\n");
        exit(3);
    }

    return fuse_main(argc, argv, &nfs_oper, NULL);
}
