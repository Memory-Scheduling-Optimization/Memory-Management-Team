#ifndef _block_io_h_
#define _block_io_h_

#include "stdint.h"
#include "debug.h"

//
// Base class for things that support block IO (disks, files, directories, etc)
//
// Assumptions:
//    - block size is a power of 2
//    - block size is fixed
//    - the caller is responsible for serializaing access (locking, etc)
//    - the caller is responsible for ensuring that buffers assed in
//      as arguments are large enough for the required operation
//    - the caller is also responsibile for ensuring that all offsets
//      and block numbers are in bounds:
//           block_number < number_of_blocks
//           byte offset < size_in_bytes
//           
//
class BlockIO {
public:
    const uint32_t block_size;
    BlockIO(uint32_t block_size): block_size(block_size) {}

    // get number of bytes
    virtual uint32_t size_in_bytes() = 0;

    // get number of blocks
    uint32_t size_in_blocks() {
        return ((size_in_bytes() + block_size - 1) / block_size) * block_size;
    }


    // Read a block and put its bytes in the given buffer
    virtual void read_block(uint32_t block_number, char* buffer) = 0;

    // Read up to "n" bytes starting at "offset" and put the restuls in "buffer".
    // returns:
    //   > 0  actual number of bytes read
    //   = 0  end (offset == size_in_bytes)
    //   -1   error (offset > size_in_bytes)
    virtual int64_t read(uint32_t offset, uint32_t n, char* buffer);

    // Read min(n,size_in_bytes - offset) bytes starting at "offset" and
    //      put the results in "buffer".
    // returns:
    //    > 0 actual number of bytes read
    //    = 0 end (offset == size_in_bytes)
    //    -1 error (offset > size_in_bytes)
    //
    virtual int64_t read_all(uint32_t offset, uint32_t n, char* buffer);

    template <typename T>
    void read(uint32_t offset, T& thing) {
        auto cnt = read_all(offset,sizeof(T),(char*)&thing);
        ASSERT(cnt == sizeof(T));
    }
};


#endif