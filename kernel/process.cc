#include "atomic.h"
#include "shared.h"
#include "pcb.h"
#include "process.h"
#include "descriptor.h"

using namespace Descriptor;

Atomic<uint32_t> PCB::next_id{0};
Shared<PCB> kProc{Shared<PCB>::make((uint32_t) VMM::kpd)};

/* Descriptors */

int Process::close(uint32_t des) {
    uint32_t index = des & NUM_MASK;
    switch (des & TYPE_MASK) {
    case TYPE_FD:
	if (index >= FDT_SIZE || fdt[index] == FD::empty) break;
	fdt[index] = FD::empty;
	return 0;
    case TYPE_PD:
	if (index >= PDT_SIZE || pdt[index] == PD::empty) break;
	pdt[index] = PD::empty;
	return 0;
    case TYPE_SD:
	if (index >= SDT_SIZE || sdt[index] == SD::empty) break;
	sdt[index] = SD::empty;
	return 0;
    default:
	break;
    }
    return -1;
}

int Process::wait(uint32_t des, uint32_t* status) {
    if ((des & TYPE_MASK) != TYPE_PD)
	return -1;
    uint32_t index = des & NUM_MASK;
    if (index >= PDT_SIZE)
	return -1;
    if (pdt[index]->wait(status) < 0)
	return -1;
    pdt[index] = PD::empty;
    return 0;
}

/* Process cloning for fork */

void Process::reset() {
    VMM::clear_pd((uint32_t*) cr3);
    for (uint32_t i = 0; i < SDT_SIZE; i++)
	sdt[i] = SD::empty;
}

Shared<PCB> Process::clone() {
    ASSERT(cr3 == getCR3());
    VMM::write_protect((uint32_t*) cr3);
    return Shared<PCB>{new Process{*this}};
}
