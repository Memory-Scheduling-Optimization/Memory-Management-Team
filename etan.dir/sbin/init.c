#include "libc.h"

/* Run a test */
#define RUN_TEST(file, name, what, ...) do {			\
	printf("***\n");					\
	printf("*** ----- %s ----- \n", what);			\
	int id = fork();					\
	if (id < 0) {						\
	    printf("*** fork failed\n");			\
	} else if (id == 0) {					\
	    int rc = execl(file, file, name, __VA_ARGS__);	\
	    printf("*** execl failed, rc = %d\n", rc);		\
	} else {						\
	    uint32_t status = 0xffffffff;			\
	    wait(id, &status);					\
	    printf("*** [%s] result = %ld\n", name, status);	\
	}							\
    } while (0)

int main(int argc, char** argv) {    

    printf("*** [%s] argc = %d\n", argv[0], argc);
    for (int i=0; i<argc; i++)
        printf("*** [%s] argv[%d] = %s\n", argv[0], i, argv[i]);
    if (argv[argc] != 0)
	printf("*** [%s] failed to null-terminate, argv[%d] = %s\n",
	       argv[0], argc, argv[argc]);
    
    RUN_TEST("/sbin/bs", "test 1", "Bad System Calls", "0", 0);

    RUN_TEST("/sbin/bs", "test 2", "Semaphores", "1", "3", 0);

    RUN_TEST("/sbin/recurse", "test 3", "Recursive execl", "4", "0", 0);
    
    RUN_TEST("/sbin/file", "test 4", "Files",
	     "26", "/files/msg.txt", "/files/none.txt", "/files", 0);

    RUN_TEST("/sbin/symlink", "test 5", "Symlinks", "test", 0);

    RUN_TEST("/sbin/memory", "test 6", "Memory Protection", 0);

    printf("***\n");
    printf("*** [%s] last line\n", argv[0]);
    
    shutdown();

    printf("*** shouldn't print\n");
    
    return 0;
}
