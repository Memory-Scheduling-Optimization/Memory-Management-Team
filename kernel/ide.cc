#include "ide.h"
#include "stdint.h"
#include "debug.h"
#include "machine.h"
#include "threads.h"

// The drive number encodes the controller in bit 1 and the channel in bit 0

static inline int controller(uint32_t drive) {
    return (drive >> 1) & 1;
}

static inline int channel(uint32_t drive) {
    return drive & 1;
}

// the base I/O port for each controller

static int ports[2] = { 0x1f0, 0x170 };

static inline int port(uint32_t drive) {
    return ports[controller(drive)];
}

//////////////////
// drive status //
//////////////////

static inline long getStatus(uint32_t drive) {
    return inb(port(drive) + 7);
}

// Status bits
#define ERR     0x01
#define DRQ     0x08
#define SRV     0x10
#define DF      0x20
#define DRDY	0x40
#define BSY	0x80
    
/* Simple polling PIO interface
   Need to use interrupts and DMA
 */

static void waitForDrive(uint32_t drive) {
    uint8_t status = getStatus(drive);
    if ((status & (ERR | DF)) != 0) {
        Debug::panic("drive error, device:%x, status:%x",drive,status);
    }
    if ((status & DRDY) == 0) {
        Debug::panic("drive %x is not ready, status:%x",drive,status);
    }
    while ((getStatus(drive) & BSY) != 0) {
        yield();
    }
}

static uint32_t nRead = 0;
static uint32_t nWrite = 0;

void Ide::read_block(uint32_t sector, char* buffer) {
//    Debug::printf("reading sector %d\n", sector);    
    uint32_t* ptr = (uint32_t*) buffer;

    nRead += 1;
    int base = port(drive);
    int ch = channel(drive);

    waitForDrive(drive);

    outb(base + 2, 1);			// sector count
    outb(base + 3, sector >> 0);	// bits 7 .. 0
    outb(base + 4, sector >> 8);	// bits 15 .. 8
    outb(base + 5, sector >> 16);	// bits 23 .. 16
    outb(base + 6, 0xE0 | (ch << 4) | ((sector >> 24) & 0xf));
    outb(base + 7, 0x20);		// read with retry

    waitForDrive(drive);

    while ((getStatus(drive) & DRQ) == 0) {
        yield();
    }

    for (uint32_t i=0; i<block_size/sizeof(uint32_t); i++) {
        ptr[i] = inl(base);
    }
}

/*
void Ide::writeSector(uint32_t sector, const void* buffer) {
    const uint32_t* ptr = (const uint32_t*) buffer;

    nWrite += 1;
    int base = port(drive);
    int ch = channel(drive);

    waitForDrive(drive);

    outb(base + 2, 1);			// sector count
    outb(base + 3, sector >> 0);	// bits 7 .. 0
    outb(base + 4, sector >> 8);	// bits 15 .. 8
    outb(base + 5, sector >> 16);	// bits 23 .. 16
    outb(base + 6, 0xE0 | (ch << 4) | ((sector >> 24) & 0xf));
    outb(base + 7, 0x30);		// write

    waitForDrive(drive);

    while ((getStatus(drive) & DRQ) == 0) {
        yield();
    }

    for (uint32_t i=0; i<SectorSize/sizeof(uint32_t); i++) {
        outl(base,ptr[i]);
    }

    waitForDrive(drive);

    //outb(base + 7, 0xE7); // flush buffers

    //waitForDrive(drive);

}
*/

/*

int32_t Ide::write(uint32_t offset, const void* buffer, uint32_t n) {
    uint32_t sector = offset / SectorSize;
    uint32_t start = offset % SectorSize;

    uint32_t end = start + n;
    if (end > SectorSize) end = SectorSize;

    uint32_t count = end - start;
    
    if (count == SectorSize) {
        // whole sector
        writeSector(sector,buffer);
    } else if (count != 0) {
        char data[SectorSize];
        readSector(sector,data);
        memcpy(&data[start],buffer,count);
        writeSector(sector,data);
    }
    return count;
}
*/


void ideStats(void) {
    Debug::printf("nRead %d\n",nRead);
    Debug::printf("nWrite %d\n",nWrite);
}
