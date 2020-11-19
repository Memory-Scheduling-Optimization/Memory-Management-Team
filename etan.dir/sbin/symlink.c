#include "libc.h"

int main(int argc, char** argv) {

    const char* tn = argv[1];
    
    printf("*** [%s] argc = %d\n", tn, argc);
    for (int i = 0; i < argc; i++)
        printf("*** [%s] argv[%d] = %s\n", tn, i, argv[i]);
    if (argv[argc] != 0)
	printf("*** [%s] failed to null-terminate, argv[%d] = %s\n",
	       tn, argc, argv[argc]);
    
    for (int i = 0; i < 5; i++)
	if ("exec"[i] != argv[2][i]) goto test;
    printf("*** [%s] successfully executed from symbolic link\n", tn);    
    return 0;
    
test: {
	int fd = open("/files/link1", 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, "/files/link1");
	printf("*** [%s] ", tn);
	cp(fd, 1);

	fd = open("/files/link2", 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, "/files/link2");
	printf("*** [%s] ", tn);
	cp(fd, 1);

	fd = open("/files/link3", 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, "/files/link3");
	printf("*** [%s] ", tn);
	cp(fd, 1);

	fd = open("/files/link4", 0);
	if (fd < 0)
	    printf("*** [%s] failed to open %s\n", tn, "/files/link4");
	printf("*** [%s] ", tn);
	cp(fd, 1);

	int id = fork();
	if (id < 0) {
	    printf("*** [%s] fork failed\n", tn);
	} else if (id == 0) {
	    int rc = execl("/files/link5", "link5", argv[1], "exec", 0);
	    printf("*** [%s] execl failed, rc = %d\n", tn, rc);
	    return -1;
	} else {
	    uint32_t status = 0xffffffff;
	    wait(id, &status);
	    printf("*** [%s] result = %ld\n", tn, status);
	}

	id = fork();
	if (id < 0) {
	    printf("*** [%s] fork failed\n", tn);
	} else if (id == 0) {
	    int rc = execl("/files/link2", "link2", argv[1], "exec", 0);
	    printf("*** [%s] failed to execute %s\n", tn, "files/link2");
	    printf("    rc = %d\n", rc);
	    return -1;
	} else {
	    uint32_t status = 0xffffffff;
	    wait(id, &status);
	    printf("*** [%s] result = %ld\n", tn, status);
	}
    }
    return 0;
}
