#include "u8250.h"
#include "machine.h"

/* 8250 */

void U8250::put(char c) {
    while (!(inb(0x3F8+5) & 0x20));
    outb(0x3F8,c);
}

char U8250::get() {
    while (!(inb(0x3F8+5) & 0x01)) {
    }
    char x = inb(0x3F8);
    return x;
}
