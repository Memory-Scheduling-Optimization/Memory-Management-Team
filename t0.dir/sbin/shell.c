#include "libc.h"

int main(int argc, char** argv) {
    printf("*** argc = %d\n",argc);
    for (int i=0; i<argc; i++) {
        printf("*** argv[%d]=%s\n",i,argv[i]);
    }

    cp(3,1);

    return 666;
}
