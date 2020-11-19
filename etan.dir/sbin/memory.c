#include "libc.h"

#define MAGIC 0xdecafbad

void memtest(const char* tn, uint32_t addr, char* str, const char* msg) {
    int id = fork();
    if (id < 0) {
	printf("*** [%s] fork failed\n", tn);
    } else if (id == 0) {
	*((char**) addr) = str;
	printf("*** [%s] %s at 0x%lx\n", tn, msg, addr);
	exit(MAGIC);
    } else {
	uint32_t status = MAGIC;
	wait(id, &status);
	printf("*** [%s] process exited %s\n", tn,
	       (status == MAGIC) ? "normally" : "abnormally");
    }

    id = fork();
    if (id < 0) {
	printf("*** [%s] fork failed\n", tn);
    } else if (id == 0) {
	printf("*** [%s] %d\n", tn, *((int*) addr));
	printf("*** [%s] %s at 0x%lx\n", tn, msg, addr);
	exit(MAGIC);
    } else {
	uint32_t status = MAGIC;
	wait(id, &status);
	printf("*** [%s] process exited %s\n", tn,
	       (status == MAGIC) ? "normally" : "abnormally");
    }
}

void prot_check(const char* tn, int r, const char* str) {
    printf("*** [%s] %s %s\n", tn, (r < 0) ? "protected" : "failed to protect", str);
}

void iota(uint32_t n, char* buf) {
    buf[10] = 0;
    buf[9] = '0' + ((n / 1) % 10);
    buf[8] = '0' + ((n / 10) % 10);
    buf[7] = '0' + ((n / 100) % 10);
    buf[6] = '0' + ((n / 1000) % 10);
    buf[5] = '0' + ((n / 10000) % 10);
    buf[4] = '0' + ((n / 100000) % 10);
    buf[3] = '0' + ((n / 1000000) % 10);
    buf[2] = '0' + ((n / 10000000) % 10);
    buf[1] = '0' + ((n / 100000000) % 10);
    buf[0] = '0' + ((n / 1000000000) % 10);
}

int main(int argc, char** argv) {
    
    const char* tn = argv[1];
    
    printf("*** [%s] argc = %d\n", tn, argc);
    for (int i = 0; i < 2; i++)
        printf("*** [%s] argv[%d] = %s\n", tn, i, argv[i]);
    if (argv[argc] != 0)
	printf("*** [%s] failed to null-terminate, argv[%d] = %s\n",
	       tn, argc, argv[argc]);
    
    if (argc == 4) {
	uint32_t addr = (uint32_t) atoi(argv[2]);
	if (*((uint32_t*) addr) == MAGIC)
	    printf("*** [%s] I can read data from previous programs\n", tn);
	else
	    printf("*** [%s] protected data from previous programs\n", tn);

	int fd = atoi(argv[3]);
	char buffer[6];
	buffer[4] = '\n';
	buffer[5] = 0;
	read(fd, buffer, 4);
	printf("*** [%s] ", tn);
	write(1, buffer, 5);
	return 0;
    }
    
    memtest(tn, 0, "null pointer", "failed to prevent null pointer dereference");
    memtest(tn, 0x70000000, "kernel", "failed to protect kernel memory");
    memtest(tn, 0xfec00fac, "ioAPIC", "failed to protect IO APIC");
    memtest(tn, 0xfee00000, "localAPIC", "failed to protect local APIC");
    
    /* Malicious writes */
    prot_check(tn, write(1, (void*) 0x00600000, 24), "kernel memory");
    prot_check(tn, write(1, (void*) 0xfec00100, 24), "IO APIC");
    prot_check(tn, write(1, (void*) 0xfee00ff8, 24), "local APIC");
    
    /* Malicious reads */
    int fd = open("/files/msg.txt", 0);
    int sz = len(fd);
    prot_check(tn, read(fd, (void*) 0x00620000, sz), "kernel memory");
    prot_check(tn, read(fd, (void*) 0xfec00ff0, sz), "IO APIC");
    prot_check(tn, read(fd, (void*) 0xfee00100, sz), "local APIC");

    /* Memory separation after execl */
    seek(fd, 6);    
    uint32_t unused[0x600000];
    uint32_t num = MAGIC;
    uint32_t addr = (uint32_t) &num;

    int id = fork();
    if (id < 0) {
	printf("*** [%s] fork failed\n", tn);
    } else if (id == 0) {	    
	char abuf[11];
	iota(addr, abuf);
	
	char fbuf[11];
	iota((uint32_t) (unused[0] = fd), fbuf);
    
	int rc = execl("/sbin/memory", "/sbin/memory", tn, abuf, fbuf, 0);
	printf("*** [%s] execl failed, rc = %d\n", tn, rc);
	return -1;
	
    } else {
	uint32_t status = 0xffffffff;
	wait(id, &status);
	printf("*** [%s] result = %ld\n", tn, status);
	
	char buffer[6];
	buffer[4] = '\n';
	buffer[5] = 0;
	read(fd, buffer, 4);
	printf("*** [%s] ", tn);
	write(1, buffer, 5);
    }
    return 0;
}
