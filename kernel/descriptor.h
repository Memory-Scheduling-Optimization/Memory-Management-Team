#ifndef _descriptor_h_
#define _descriptor_h_

#include "pcb.h"
#include "shared.h"
#include "io.h"

namespace Descriptor {

    constexpr uint32_t TYPE_MASK = 0xf0000000;
    constexpr uint32_t NUM_MASK = 0x0fffffff;
    constexpr uint32_t TYPE_FD = 0x00000000;
    constexpr uint32_t TYPE_PD = 0x10000000;
    constexpr uint32_t TYPE_SD = 0x20000000;
    
    struct FD : public Sharable<FD> {
	static const Shared<FD> empty;
	virtual ~FD() {}
	virtual int write(char*, int)	{ return -1; }
	virtual int len()      		{ return -1; }
	virtual int read(char*, int)	{ return -1; }
	virtual int seek(int)		{ return -1; }
    };

    struct PD : public Sharable<PD> {
	static const Shared<PD> empty;
	virtual ~PD() {}
 	virtual int wait(uint32_t*) { return -1; }
    };

    struct SD : public Sharable<SD> {
	static const Shared<SD> empty;
	virtual ~SD() {}
	virtual int up()	{ return -1; }
	virtual int down()	{ return -1; }
    };

    struct StdIn : public FD {
    };

    struct StdOut : public FD {
	Shared<OutputStream<char>> io;
	StdOut(Shared<OutputStream<char>> io) : io{io} {}
	int write(char*, int) override;
    };

    struct StdErr : public FD {	
	Shared<OutputStream<char>> io;
	StdErr(Shared<OutputStream<char>> io) : io{io} {}
	int write(char*, int) override;
    };
}

#endif
