#define FUSE_USE_VERSION 30
#define BUFFER_SIZE 4096
#define SERV_PORT 3000



#include <config.h>
#include <fuse.h>
#include <fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#in


struct nfs_context *nfs = NULL;
char *server_ip;



static int nfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	struct nfsdir *nfsdir;
	struct nfsdirent *nfsdirent;

	int ret;
	ret = nfs_opendir(nfs, path, &nfsdir);
	if (ret < 0) {
		return ret;
	}

	char *name_buffer;
	while (nfs_readdir(nfsdir)) != NULL) {
		filler(buf, nfsdirent->name, NULL, 0);
	}
	nfs_closedir(nfs, nfsdir);
	return 0;
}

static int nfs_open(const char *path, struct fuse_file_info *fi)
{
	int ret = 0;
	file_handler *nfsfh;

	fi->fh = 0;
	ret = nfs_open(server_ip, path, &nfsfh);
	if (ret < 0) {
		return ret;
	}

	fi->fh = (uint64_t)nfsfh; //file handler returned by the server to identify call
	return ret;
}


static int nfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	int ret = 0;
	struct nfsfh *nfsfh;

	ret = nfs_creat(nfs, path, mode, &nfsfh);
	if (ret < 0) {
		return ret;
	}

	fi->fh = (uint64_t)nfsfh;

	return ret;
}

static int nfs_read(const char *path, char *buf, size_t size,
       off_t offset, struct fuse_file_info *fi)
{
	file_handler *nfsfh = (struct nfsfh *)fi->fh;


	return nfs_read(server_ip, nfsfh, offset, size, buf);
}


static int nfs_write(const char *path, const char *buf, size_t size,
       off_t offset, struct fuse_file_info *fi)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;

        update_rpc_credentials();

	return nfs_pwrite(nfs, nfsfh, offset, size, discard_const(buf));
}


static int nfs_mkdir(const char *path, mode_t mode)
{
	int ret = 0;

	ret = nfs_mkdir(nfs, path);
	if (ret < 0) {
		return ret;
	}
	ret = nfs_chmod(path);
	if (ret < 0) {
		return ret;
	}

	return ret;
}

static int nfs_rmdir(const char *path)
{
	update_rpc_credentials();

	return nfs_rmdir(nfs, path);
}

 static int nfs_rename(const char *from, const char *to)
{

	return nfs_rename(nfs, from, to);
}

static int nfs_fsync(const char *path, int isdatasync,
			  struct fuse_file_info *fi)
{
	struct nfsfh *nfsfh = (struct nfsfh *)fi->fh;

	update_rpc_credentials();

	return nfs_fsync(nfs, nfsfh);
}


static struct fuse_operations nfs_oper = {
	.create		= fuse_nfs_create,
	.fsync		= fuse_nfs_fsync,
	.mkdir		= fuse_nfs_mkdir,
	.open		= fuse_nfs_open,
	.read		= fuse_nfs_read,
	.readdir	= fuse_nfs_readdir,
	.rmdir		= fuse_nfs_rmdir,
	.rename		= fuse_nfs_rename,
	.write		= fuse_nfs_write,
};

 int main(int argc, char *argv[])
 {
 	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
 	struct fuse_cmdline_opts opts;
 	struct stat stbuf;

 	if (fuse_parse_cmdline(&args, &opts) != 0)
 		return 1;

 	if (!opts.mountpoint) {
 		fprintf(stderr, "missing mountpoint parameter\n");
 		return 1;
 	}

 	//communicate with server to set u_id
 	u_id = 0;
 	//initialize server ip
 	strcpy(server_ip, argv[1]);

 	return fuse_main(argc, argv, &nfs_oper, NULL);
 }
