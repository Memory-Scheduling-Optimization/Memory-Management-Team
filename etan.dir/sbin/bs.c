#include "libc.h"

#define UNSAFE_SYSCALL(N, OUT)					\
    __asm volatile ("int $48" : "=a" (OUT) : "a" (N))

int main(int argc, char** argv) {

    const char* tn = argv[1];
    
    printf("*** [%s] argc = %d\n", tn, argc);
    for (int i = 0; i < argc; i++)
        printf("*** [%s] argv[%d] = %s\n", tn, i, argv[i]);    

    int test = atoi(argv[2]);
    
    if (test == 0) {    
	int out;
    
	printf("*** [%s] syscall %d\n", tn, 20);
	UNSAFE_SYSCALL(20, out);
	if (out >= 0)
	    printf("*** [%s] syscall %d didn't fail!\n", tn, 20);

	printf("*** [%s] syscall 0x%x\n", tn, 0xffffffff);
	UNSAFE_SYSCALL(0xffffffff, out);
	if (out >= 0)
	    printf("*** [%s] syscall 0x%x didn't fail!\n", tn, 0xffffffff);

	printf("*** [%s] syscall 0x%x\n", tn, 0xdeadbeef);
	UNSAFE_SYSCALL(0xdeadbeef, out);
	if (out >= 0)
	    printf("*** [%s] syscall 0x%x didn't fail!\n", tn, 0xdeadbeef);
    
	printf("*** [%s] syscall %d\n", tn, 14);
	UNSAFE_SYSCALL(14, out);
	if (out >= 0)
	    printf("*** [%s] syscall %d didn't fail!\n", tn, 14);

	return 0;
	
    } else if (test == 1) {

	int val = atoi(argv[3]);
	int id[val];
	int sd = sem(1);

	for (int i = 0; i < val; i++) {
	    if ((id[i] = fork()) < 0)
		printf("*** [%s] fork failed\n", tn);
	}
    
	down(sd);
	printf("*** [%s] a ... ", tn);
	int id0 = fork();
	if (id0 < 0) {
	    printf("fork failed\n");
	} else if (id0 == 0) {
	    exit(0);
	} else {
	    uint32_t status = 0xffffffff;
	    wait(id0, &status);
	}
	printf("b ... ");
	int id1 = fork();
	if (id1 < 0) {
	    printf("fork failed\n");
	} else if (id1 == 0) {
	    exit(0);
	} else {
	    uint32_t status = 0xffffffff;
	    wait(id1, &status);
	}
	printf("c\n");
	up(sd);

	uint32_t sum = 1;
	for (int i = 0; i < val; i++) {
	    uint32_t status = 0;
	    if (id[i] > 0)
		wait(id[i], &status);
	    sum += status;
	}
	return sum;	
    }
    return -1;
}
