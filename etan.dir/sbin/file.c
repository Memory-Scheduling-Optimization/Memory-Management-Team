#include "libc.h"

int main(int argc, char** argv) {

    const char* tn = argv[1];
    
    printf("*** [%s] argc = %d\n", tn, argc);
    for (int i = 0; i < argc; i++)
        printf("*** [%s] argv[%d] = %s\n", tn, i, argv[i]);
    if (argv[argc] != 0)
	printf("*** [%s] failed to null-terminate, argv[%d] = %s\n",
	       tn, argc, argv[argc]);

    {
	// write multiple bytes
	int sz = atoi(argv[2]);
	char buffer[sz+1];
	for (int i = 0; i < sz; i++)
	    buffer[i] = 'a' + i;
	buffer[sz] = '\n';
	printf("*** [%s] ", tn);
	int wr = write(1, buffer, sz+1);
	if (wr < 0)
	    printf("*** [%s] write to %d failed\n", tn, 1);
	printf("*** [%s] wrote %d bytes\n", tn, wr);
    }

    {
	// open a file
	int fd = open(argv[3], 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, argv[3]);

	// find the size of an open file
	int sz = len(fd);
	printf("*** [%s] size of %s is %d\n", tn, argv[3], sz);	
	int strsz = -1;
	while (argv[1][++strsz] != 0);	
	char buffer[sz+5+strsz+20];
	memcpy(&buffer[sz], "*** [", 5);
	memcpy(&buffer[sz+5], argv[1], strsz);
	memcpy(&buffer[sz+5+strsz], "] did not overwrite\n", 20);

	// read from an open file, past the size
	printf("*** [%s] reading %s\n", tn, argv[3]);
	int rd = read(fd, buffer, 100+sz);
	if (rd < 0)
	    printf("*** [%s] read from %d failed\n", tn, fd);
	printf("*** [%s] read %d bytes\n", tn, rd);

	// write multiple bytes
	printf("*** [%s] ", tn);
	int wr = write(1, buffer, sz+5+strsz+20);
	if (wr < 0)
	    printf("*** [%s] write to %d failed\n", tn, 1);
	printf("*** [%s] wrote %d bytes\n", tn, wr);

	// seek past the end of an open file
	int sk = seek(fd, sz+5);
	printf("*** [%s] seeked to %d\n", tn, sk);

	// read past the end of an open file
	rd = read(fd, buffer, sz);
	if (rd < 0)
	    printf("*** [%s] failed to read from offset %d\n", tn, sk);
	else
	    printf("*** [%s] read past end of file, size = %d, offset = %d\n", tn, sz, sk);
    }

    {
	// use all file descriptors
	int fd5 = -1;
	for (int i = 4; i < 10; i++) {
	    int fd = open(argv[3], 0);
	    if (fd < 0)
		printf("*** [%s] failed to open %s\n", tn, argv[3]);
	    if (i == 5)
		fd5 = fd;
	}
	
	// try to open with no available descriptors
	int fd = open(argv[3], 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, argv[3]);
	else
	    printf("*** [%s] opened too many files, fd = %d\n", tn, fd);

	// close an open file
	if (close(fd5) < 0)
	    printf("*** [%s] failed to close %d\n", tn, fd5);

	// try to call len, seek, and read on a closed file
	int sz = len(fd5);
	int sk = seek(fd5, sz / 2);
	char buffer[10];
	int rd = read(fd5, buffer, 10);
	if (sz >= 0)
	    printf("*** [%s] length of closed file = %d\n", tn, sz);
	if (sk >= 0)
	    printf("*** [%s] seeked to offset %d in closed file\n", tn, sk);
	if (rd >= 0)
	    printf("*** [%s] read %d bytes from closed file\n", tn, rd);

	// open a nonexistent file
	fd = open(argv[4], 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, argv[4]);
	else
	    printf("*** [%s] opened nonexistent file, fd = %d\n", tn, fd);

	// open a file
	fd = open(argv[3], 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, argv[3]);
	else
	    printf("*** [%s] opened %s\n", tn, argv[3]);
	if (fd != fd5)
	    printf("*** [%s] expected fd = %d; actual fd = %d\n", tn, fd5, fd);
	if (close(fd5) < 0)
	    printf("*** [%s] failed to close %d\n", tn, fd5);
	
	fd = open(argv[5], 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, argv[5]);
	else
	    printf("*** [%s] opened a directory, fd = %d\n", tn, fd);
    }
    
    return 0;
}
