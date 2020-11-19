#include "shared.h"
#include "descriptor.h"

namespace Descriptor {
    const Shared<FD> FD::empty{Shared<FD>::make()};
    const Shared<PD> PD::empty{Shared<PD>::make()};
    const Shared<SD> SD::empty{Shared<SD>::make()};

    int StdOut::write(char* buffer, int len) {
	for (int i = 0; i < len; i++)
	    io->put(buffer[i]);
	return len;
    }

    int StdErr::write(char* buffer, int len) {
	for (int i = 0; i < len; i++)
	    io->put(buffer[i]);
	return len;
    }
}
