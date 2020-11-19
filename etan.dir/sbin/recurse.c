#include "libc.h"

int main(int argc, char** argv) {

    const char* tn = argv[1];

    int a = atoi(argv[2]);
    int b = atoi(argv[3]);
    
    printf("*** [%s] a = %d, b = %d\n", tn, a, b);

    if (a < 2)
	return a + b + a*b;
    
    --argv[2][0];
    
    uint32_t status = 0xffffffff;
    
    int id = fork();
    if (id < 0) {
	printf("*** [%s] fork failed\n", tn);	
    } else if (id == 0) {
	int rc = execl(argv[0], argv[0], argv[1], argv[2], argv[3], 0);
	printf("*** [%s] execl failed, rc = %d\n", tn, rc);
    } else {
	wait(id, &status);
    }

    int result = (int) status + a + b;
    
    char buffer[5];
    buffer[4] = 0;
    buffer[3] = '0' + ((result / 1) % 10);
    buffer[2] = '0' + ((result / 10) % 10);
    buffer[1] = '0' + ((result / 100) % 10);
    buffer[0] = '0' + ((result / 1000) % 10);

    int rc = execl(argv[0], argv[0], argv[1], argv[2], buffer, 0);
    printf("*** [%s] execl failed, rc = %d\n", tn, rc);
    return -1;
}
