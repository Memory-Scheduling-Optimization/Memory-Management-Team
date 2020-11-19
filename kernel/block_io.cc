#include "block_io.h"
#include "machine.h"
#include "libk.h"
#include "debug.h"

int64_t BlockIO::read(uint32_t offset, uint32_t desired_n, char* buffer) {
    auto sz = size_in_bytes();
    if (offset > sz) return -1;
    if (offset == sz) return 0;

    auto n = K::min(desired_n,sz - offset);
    //auto end_offset = K::min(offset+n,sz);
    auto block_number = offset / block_size;
    auto offset_in_block = offset % block_size;
    auto actual_n = K::min(block_size - offset_in_block, n);
    ASSERT(actual_n <= n);
    ASSERT(offset + actual_n <= sz);
    if (actual_n == block_size) {
//        Debug::printf("reading whole block %d\n",block_number);
        ASSERT(offset_in_block == 0);
        // we can read in-place
        read_block(block_number,buffer);
    } else {
        ASSERT(offset_in_block + actual_n <= block_size);
        char* temp = new char[block_size];
//        Debug::printf("reading partial block %d\n",block_number);
        read_block(block_number,temp);
//        Debug::printf("getting %d bytes starting at block offset %d\n",actual_n,offset_in_block);
        ::memcpy(buffer,&temp[offset_in_block],actual_n);
        delete []temp;
    }
    return actual_n;
}

int64_t BlockIO::read_all(uint32_t offset, uint32_t n, char* buffer) {
    int64_t total_count = 0;
    while (n > 0) {
        int64_t cnt = read(offset,n,buffer);
        if (cnt < 0) return cnt;
        if (cnt == 0) return total_count;
        total_count += cnt;
        offset += cnt;
        n -= cnt;
        buffer += cnt;
    }
    return total_count;
}
