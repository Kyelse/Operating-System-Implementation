/*
	FUSE: Filesystem in Userspace


	gcc -Wall `pkg-config fuse --cflags --libs` csc452fuse.c -o csc452
 * File: csc452fuse.c 
 * Author: Quan Nguyen
 * Project:  Syscalls and IPC
 * Class: CSC452 
 * Purpose: This is the main function using fuse in order to implement a two 
 * level file system. The file system is managed via a single file that represents
 * the disk device (.disk). Through FUSE and this program, 
 * it will be possible to interact with our newly created file system using standard
 * UNIX/Linux programs in a transparent way (mkdir, vim, touch, echo, ls, etc. )


*/

#define	FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>


//size of a disk block
#define	BLOCK_SIZE 512

//we'll use 8.3 filenames
#define	MAX_FILENAME 8
#define	MAX_EXTENSION 3

//How many files can there be in one directory?
#define MAX_FILES_IN_DIR (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + (MAX_EXTENSION + 1) + sizeof(size_t) + sizeof(long))

//The attribute packed means to not align these things
struct csc452_directory_entry
{
	int nFiles;	//How many files are in this directory.
				//Needs to be less than MAX_FILES_IN_DIR

	struct csc452_file_directory
	{
		char fname[MAX_FILENAME + 1];	//filename (plus space for nul)
		char fext[MAX_EXTENSION + 1];	//extension (plus space for nul)
		size_t fsize;					//file size
		long nStartBlock;				//where the first block is on disk
	} __attribute__((packed)) files[MAX_FILES_IN_DIR];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_FILES_IN_DIR * sizeof(struct csc452_file_directory) - sizeof(int)];
} ;

typedef struct csc452_root_directory csc452_root_directory;

#define MAX_DIRS_IN_ROOT (BLOCK_SIZE - sizeof(int)) / ((MAX_FILENAME + 1) + sizeof(long))

struct csc452_root_directory
{
	int nDirectories;	//How many subdirectories are in the root
						//Needs to be less than MAX_DIRS_IN_ROOT
	struct csc452_directory
	{
		char dname[MAX_FILENAME + 1];	//directory name (plus space for nul)
		long nStartBlock;				//where the directory block is on disk
	} __attribute__((packed)) directories[MAX_DIRS_IN_ROOT];	//There is an array of these

	//This is some space to get this to be exactly the size of the disk block.
	//Don't use it for anything.  
	char padding[BLOCK_SIZE - MAX_DIRS_IN_ROOT * sizeof(struct csc452_directory) - sizeof(int)];
} ;

typedef struct csc452_directory_entry csc452_directory_entry;

//How much data can one block hold?
#define	MAX_DATA_IN_BLOCK (BLOCK_SIZE- sizeof(long))

struct csc452_disk_block
{
	//Space in the block can be used for actual data storage.
	char data[MAX_DATA_IN_BLOCK];
	//Link to the next block in the chain
	long nNextBlock;
};

typedef struct csc452_disk_block csc452_disk_block;
static void get_root(csc452_root_directory *root); 
static int get_dir(csc452_directory_entry *entry, char *directory);
static int file_exist(char *directory, char *filename, char *extension, int path_comp); 
static int dir_exists(char *directory);
static long next_free_block(); 
/**
 * @brief Get the root of the file. Root is directly at the first block of the file. 
 * This will populate the content of the csc452_root_directory structure based on the 
 * pointer of the struct passed in.
 * @param root the pointer of the struct representing the root
 */
static void get_root(csc452_root_directory *root) {
	FILE *fp = fopen(".disk", "rb");
	if (fp != NULL) {
		fread(root, sizeof(csc452_root_directory), 1, fp);	
	}
	fclose(fp);

}
/**
 * @brief Get the directory object. Populate them based on the directory entry pointer passed in. 
 * The directory is identical to the character. Return 0 if success and -1 if failed (no directory).
 * @param entry
 * @param directory 
 * @return int 
 */
static int get_dir(csc452_directory_entry *entry, char *directory) {
	csc452_root_directory root;
	get_root(&root);
	int dir_order = dir_exists(directory); 
	if (dir_order == -1) {
		return -1; 
	}
	FILE *fp = fopen(".disk", "rb");
	if (fp == NULL) { // something extremely wrong happened
		printf("Cannot open root file. Something very wrong happen");
		return -1;
	}
	fseek(fp, root.directories[dir_order].nStartBlock, SEEK_SET);
	fread(entry, sizeof(csc452_directory_entry), 1, fp);
	fclose(fp);
	// dir found, return where the dir in the directory
	return dir_order; 
}
/**
 * @brief Check if a file name is exist or not under the directory. Return -1 if the file does not exist, 
 * or the size of the file in the directory is returned. File is checked with both name and extension if there are 
 * any
 * 
 * @param directory 
 * @param filename 
 * @param extension
 * @return int the size of the file if exist or -1 if not
 */
static int file_exist(char *directory, char *filename, char *extension, int path_comp) {
	csc452_directory_entry entry;  
	int dir_order = get_dir(&entry, directory); 
	if (dir_order == -1) { // directory simply not exist in the first place
		return -1; 
	} 
	// for some reason, ls passed in information without any extension. 
	// so, /testmount/test/1.txt becomes /testmount/test/1 in path, so we have no choice 
	// but add this in 
	for (int i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (path_comp == 3) { // the path does have extension, we do one more compare
				if (strcmp(extension, entry.files[i].fext) == 0) {
					return entry.files[i].fsize; 
				} 
			} else { // if it is only name, we return the size right away
				return entry.files[i].fsize; 
			}
		}
	}
	return -1; 
}

/**
 * @brief Check if a directory name is exist or not. Return -1 if the directory does not exist, 
 * or the order of the directory in the root is returned. 
 * 
 * 
 * @param directory 
 * @return int 
 */
static int dir_exists(char *directory) {
	csc452_root_directory root;
	get_root(&root);
	for (int i = 0; i < root.nDirectories; i++) {
		if (strcmp(directory, root.directories[i].dname) == 0) {
			return i; 
		}
	}
	return -1;
}
/**
 * @brief Get the location of the next free block available. The free tracking space 
 * will be at the end of the file. This will dynamically determine  the size of the disk file, 
 * thus, decide that when the free space tracking (or the next free) should be, and when is 
 * the disk full. 
 * @return long the start bytes of the next free block, -1 if there are none 
 */
static long next_free_block() {
	FILE *fp = fopen(".disk", "rb+");
	fseek(fp, 0, SEEK_END); 
	long file_max = ftell(fp) - BLOCK_SIZE;  // if the pointer reach this, the disk is full
	// move to the start of the free tracking space, which is exactly one block before the end 
	fseek(fp, (-1)*BLOCK_SIZE, SEEK_END); 
	// temp will be start the most recently occupied block
	long tmp;
	fread(&tmp, sizeof(long), 1, fp); 
	// so, free will be the next one
	long next_free = tmp + BLOCK_SIZE; 
	if (next_free >= file_max) {
		fclose(fp); 
		return -1; 
	}
	// move to the start of the free tracking space, which is exactly one block before the end 
	fseek(fp, (-1)*BLOCK_SIZE, SEEK_END); 
	// write back the recently just being occupied block
	fwrite(&next_free, sizeof(long), 1, fp); 
	fclose(fp); 
	return next_free; 

}
/*
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * man -s 2 stat will show the fields of a stat structure
 */
static int csc452_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else  {
		int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
		if (path_comp == 1) { //If the path does exist and is a directory
			if (dir_exists(directory) != -1) {
				stbuf->st_mode = S_IFDIR | 0755;
				stbuf->st_nlink = 2;
				return res;
			} 
		} else if (path_comp == 2 || path_comp == 3) { // since only filename
			int fsize = file_exist(directory, filename, extension, path_comp);
			if (fsize != -1) { // file does exist
				stbuf->st_mode = S_IFREG | 0666;
				stbuf->st_nlink = 2;
				stbuf->st_size = (size_t) fsize;
				return res;
			}
		}
		printf("Invalid path or path does not exist\n");
		res = -ENOENT;
	}

	return res;
}


/*
 * Called whenever the contents of a directory are desired. Could be from an 'ls'
 * or could even be when a user hits TAB to do autocompletion
 */
static int csc452_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	//Since we're building with -Wall (all warnings reported) we need
	//to "use" every parameter, so let's just cast them to void to
	//satisfy the compiler
	(void) offset;
	(void) fi;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	//A directory holds two entries, one that represents itself (.) 
	//and one that represents the directory above us (..)
	if (strcmp(path, "/") == 0) {
		filler(buf, ".", NULL,0);
		filler(buf, "..", NULL, 0);
		csc452_root_directory root;
		get_root(&root);
		for(int i = 0; i < root.nDirectories; i++) {
			filler(buf, root.directories[i].dname, NULL, 0);
		}
	}
	else {
		int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
		// All we have _right now_ is root (/), so any other path must
		// not exist. 
		if (path_comp == 1) { //If the path does exist and is a directory form 
			csc452_directory_entry entry;  
			int dir_order = get_dir(&entry, directory); 
			if (dir_order == -1) { // directory simply not exist in the first place
				return -ENOENT; 
			} 
			for (int i = 0; i < entry.nFiles; i++) {
				filler(buf, entry.files[i].fname, NULL, 0);
			}
		} else {
			return -ENOENT;
		}
		
	}

	return 0;
}

/*
 * Creates a directory. We can ignore mode since we're not dealing with
 * permissions, as long as getattr returns appropriate ones for us.
 * Since this is a two level file system, we can be sure that the directory 
 * will be always below the root one level. 
 */
static int csc452_mkdir(const char *path, mode_t mode)
{
	(void) mode;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	if (path_comp != 1) {
		return -EPERM; 
	} 
	if (strlen(directory) > 8) {
		return -ENAMETOOLONG; 
	}
	if (dir_exists(directory) != -1) {
		return -EEXIST; 
	}
	csc452_root_directory root;
	get_root(&root);
	if (root.nDirectories >= MAX_DIRS_IN_ROOT) {
		printf("Maximum directory in root reached. Directory creation fail\n");
		return -ENOSPC;
	} else { // make a new directory here
		long address = next_free_block(); 
		if (address == -1) { 
			printf("Out of disk space");
			return -EDQUOT; 
		}
		strcpy(root.directories[root.nDirectories].dname, directory);
		root.directories[root.nDirectories].nStartBlock = address; 
		root.nDirectories += 1; 
		FILE *fp = fopen(".disk", "rb+");
		// write the root content back into the disk
		fwrite(&root, sizeof(csc452_root_directory), 1, fp); 
		fclose(fp);
	}
	return 0;
}

/*
 * Does the actual creation of a file. Mode and dev can be ignored.
 *
 * Note that the mknod shell command is not the one to test this.
 * mknod at the shell is used to create "special" files and we are
 * only supporting regular files.
 *
 */
static int csc452_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void) path;
	(void) mode;
    (void) dev;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);	
	// Since this is a two level file system, any thing not in the form of /dir/file.txt would be 
	// considered not good. We do not support file with no extension here
	if (path_comp != 3) { 
		return -EPERM; 
	} 
	if (strlen(filename) > 8 || strlen(extension) > 3) {
		return -ENAMETOOLONG; 
	}
	if (file_exist(directory, filename, extension, 3) != -1) {
		return -EEXIST;
	}
	// assuming that getatt already handle if a path is a directory
	csc452_directory_entry entry;  
	int dir_order = get_dir(&entry, directory); 
	csc452_root_directory root; 
	get_root(&root); 
	if (entry.nFiles >= MAX_FILES_IN_DIR) {
		printf("Maximum file in a directory reached. File creation fail\n");
		return -ENOSPC; 
	} else {
		long address = next_free_block(); 
		if (address == -1) { 
			printf("Out of disk space");
			return -EDQUOT; 
		}
		strcpy(entry.files[entry.nFiles].fname, filename);
		strcpy(entry.files[entry.nFiles].fext, extension);
		entry.files[entry.nFiles].fsize = 0; 
		entry.files[entry.nFiles].nStartBlock = address; 
		entry.nFiles += 1; 
		// write the updated directory into the disk. 
		FILE *fp = fopen(".disk", "rb+");
		fseek(fp, root.directories[dir_order].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, fp);
		fclose(fp); 
	}
	return 0;
}

/*
 * Read size bytes from file into buf starting from offset.
 *
 */
static int csc452_read(const char *path, char *buf, size_t size, off_t offset,
			  struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1]; 
	int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);
	//check to make sure path exists. This will be only in the directory form. 
	if (path_comp == 1) {
		if (dir_exists(directory) != -1) {
			return -EISDIR; 
		}	
	} 
	// only form of /dir/file.ext allowed. If it is not, it will be a not valid error. 
	if (path_comp != 3) {
		return -ENOENT; 
	}
	// check if the file is truly exist or not
	size_t fsize = file_exist(directory, filename, extension, 3);  
	if (fsize == -1) {
		return -ENOENT; 
	}
	//check that size is > 0
	if (size == 0) {
		return size; 
	}
	csc452_directory_entry entry; 
	get_dir(&entry, directory);  
	int i; 
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (strcmp(extension, entry.files[i].fext) == 0) {
				break; 
			}
		}
	}
	//check that offset is <= to the file size
	if (offset > fsize) {
		return -EINVAL; 
	}
	//read in data
	FILE *fp = fopen(".disk", "rb"); 
	csc452_disk_block file_block; 
	fseek(fp, entry.files[i].nStartBlock, SEEK_SET); 
	fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
	// find the begin index of the data block of the disk_block to read
	int begin_index = offset; 
	while (begin_index > MAX_DATA_IN_BLOCK) {
		fseek(fp, file_block.nNextBlock, SEEK_SET); 
		fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
		begin_index = begin_index - MAX_DATA_IN_BLOCK; 
	}
	if (size > fsize) {
		size = fsize; 
	}
	size_t left_to_read = size; 
	int buf_index = 0; 
	int file_block_left = MAX_DATA_IN_BLOCK - begin_index; 
	while (left_to_read > 0) {
		// move to the next one when data in this block is read.
		if (file_block_left == 0) {
			fseek(fp, file_block.nNextBlock, SEEK_SET); 
			fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
			file_block_left = MAX_DATA_IN_BLOCK; 
			begin_index = 0; 
		} 
		buf[buf_index] = file_block.data[begin_index]; 
		left_to_read--; 
		file_block_left--; 
		begin_index++; 
		buf_index++; 
	}
	//return success, or error
	fclose(fp);
	return size;
}

/*
 * Write size bytes from buf into file starting from offset
 *
 */
static int csc452_write(const char *path, const char *buf, size_t size,
			  off_t offset, struct fuse_file_info *fi)
{
	(void) buf;
	(void) offset;
	(void) fi;
	(void) path;
	char directory[MAX_FILENAME + 1];
    char filename[MAX_FILENAME + 1];
    char extension[MAX_EXTENSION + 1];
	int path_comp = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension); 
	//check to make sure path exists. This will be only in the directory form. 
	if (path_comp == 1) {
		if (dir_exists(directory) != -1) {
			return -EISDIR; 
		}	
	} 
	// only form of /dir/file.ext allowed. If it is not, it will be a not valid error. 
	if (path_comp != 3) {
		return -ENOENT; 
	}
	// check if the file is truly exist or not
	size_t fsize = file_exist(directory, filename, extension, 3);  
	if (fsize == -1) {
		return -ENOENT; 
	}
	//check that size is > 0
	if (size == 0) {
		return size; 
	}
	csc452_directory_entry entry; 
	int dir_order = get_dir(&entry, directory); 
	int i; 
	for (i = 0; i < entry.nFiles; i++) {
		if (strcmp(filename, entry.files[i].fname) == 0) {
			if (strcmp(extension, entry.files[i].fext) == 0) {
				break;
			}
		}
	}
	//check that offset is <= to the file size
	if (offset > fsize) {
		return -EFBIG; 
	}
	FILE *fp = fopen(".disk", "rb+"); 
	csc452_disk_block file_block; 
	long current_block_address = entry.files[i].nStartBlock; 
	fseek(fp, current_block_address, SEEK_SET); 
	fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
	// We have to figure out if the file size will increase or not.
	size_t file_size_increase = offset - fsize + size; 
	// if it is increasing, we do update the metadata first
	if (file_size_increase > 0) {
		entry.files[i].fsize += file_size_increase; 
		csc452_root_directory root;
		get_root(&root); 
		fseek(fp, root.directories[dir_order].nStartBlock, SEEK_SET); 
		fwrite(&entry, sizeof(csc452_directory_entry), 1, fp); 
	}
	// find the beginning of the place to write
	int begin_index = offset; 
	while (begin_index > MAX_DATA_IN_BLOCK) {
		current_block_address =  file_block.nNextBlock; 
		fseek(fp, current_block_address, SEEK_SET); 
		fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
		begin_index = begin_index - MAX_DATA_IN_BLOCK; 
	}

	size_t left_to_write = size; 
	int buf_index = 0; 
	int file_block_left = MAX_DATA_IN_BLOCK - begin_index; // keep track of how many character left for file
	while (left_to_write > 0) {
		if (file_block_left == 0) {
			// reached the end of the file, just create new blocks
			if (file_block.nNextBlock == 0) {
				long address = next_free_block(); 
				if (address == -1) { 
					printf("Out of disk space");
					return -EFBIG; 
				}
				file_block.nNextBlock = address; 

			}
			fseek(fp, current_block_address, SEEK_SET); 
			// save content of the current one
			fwrite(&file_block, sizeof(csc452_disk_block), 1, fp);
			fseek(fp, file_block.nNextBlock, SEEK_SET);  
			current_block_address =  file_block.nNextBlock;
			fread(&file_block, sizeof(csc452_disk_block), 1, fp); 
			file_block_left = MAX_DATA_IN_BLOCK; 
			begin_index = 0; 
		} 
		file_block.data[begin_index] = buf[buf_index]; 
		left_to_write--; 
		file_block_left--; 
		begin_index++; 
		buf_index++;
	}
	// write back the content of the current one
	fseek(fp, current_block_address, SEEK_SET); 
	fwrite(&file_block, sizeof(csc452_disk_block), 1, fp);
	fclose(fp);
	return size;
}

/*
 * Removes a directory (must be empty)
 *
 */
static int csc452_rmdir(const char *path)
{
	  (void) path;

	  return 0;
}

/*
 * Removes a file.
 *
 */
static int csc452_unlink(const char *path)
{
        (void) path;
        return 0;
}


/******************************************************************************
 *
 *  DO NOT MODIFY ANYTHING BELOW THIS LINE
 *
 *****************************************************************************/

/*
 * truncate is called when a new file is created (with a 0 size) or when an
 * existing file is made shorter. We're not handling deleting files or
 * truncating existing ones, so all we need to do here is to initialize
 * the appropriate directory entry.
 *
 */
static int csc452_truncate(const char *path, off_t size)
{
	(void) path;
	(void) size;

    return 0;
}

/*
 * Called when we open a file
 *
 */
static int csc452_open(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;
    /*
        //if we can't find the desired file, return an error
        return -ENOENT;
    */

    //It's not really necessary for this project to anything in open

    /* We're not going to worry about permissions for this project, but
	   if we were and we don't have them to the file we should return an error

        return -EACCES;
    */

    return 0; //success!
}

/*
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int csc452_flush (const char *path , struct fuse_file_info *fi)
{
	(void) path;
	(void) fi;

	return 0; //success!
}


//register our new functions as the implementations of the syscalls
static struct fuse_operations csc452_oper = {
    .getattr	= csc452_getattr,
    .readdir	= csc452_readdir,
    .mkdir		= csc452_mkdir,
    .read		= csc452_read,
    .write		= csc452_write,
    .mknod		= csc452_mknod,
    .truncate	= csc452_truncate,
    .flush		= csc452_flush,
    .open		= csc452_open,
    .unlink		= csc452_unlink,
    .rmdir		= csc452_rmdir
};

//Don't change this.
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &csc452_oper, NULL);
}
