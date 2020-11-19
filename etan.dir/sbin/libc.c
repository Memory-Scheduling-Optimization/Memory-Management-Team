#include "libc.h"

int putchar(int c) {
    char t = (char)c;
    return write(1,&t,1);
}

int puts(const char* p) {
    char c;
    int count = 0;
    while ((c = *p++) != 0) {
        int n = putchar(c); 
        if (n < 0) return n;
        count ++;
    }
    putchar('\n');
    
    return count+1;
}

void cp(int from, int to) {
    while (1) {
        char buf[100];
        ssize_t n = read(from,buf,100);
        if (n == 0) break;
        if (n < 0) {
            printf("%s:%d read error\n",__FILE__,__LINE__);
            break;
        }
        char *ptr = buf;
        while (n > 0) {
            ssize_t m = write(to,ptr,n);
            if (m < 0) {
                printf("%s:%d write error\n",__FILE__,__LINE__);
                break;
            }
            n -= m;
            ptr += m;
        }
    }
}

int atoi(const char* str) {
    int neg = 0;
    int i = 0;
    if (str[i] == '-') {
	neg = 1;
	i++;
    }
    int val = 0;
    while (str[i] != '\0') {
	int digit = str[i++] - '0';
	val *= 10;
	val += digit;
    }
    return val * (neg ? -1 : 1);
}
