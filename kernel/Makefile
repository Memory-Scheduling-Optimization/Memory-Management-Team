B = build

IMAGES = $B/kernel.img

all : ${IMAGES}

UTCS_OPT ?= -O3

CFLAGS = -std=c99 -m32 -nostdlib -nostdinc -g ${UTCS_OPT} -Wall -Werror
CCFLAGS = -std=c++17 -fno-exceptions -fno-rtti -m32 -ffreestanding -nostdlib -g ${UTCS_OPT} -Wall -Werror

CFILES = $(wildcard *.c)
CCFILES = $(wildcard *.cc)
SFILES = $(wildcard *.S) $(wildcard *.s)

ALL_OFILES = $(subst .cc,.o,$(CCFILES)) $(subst .c,.o,$(CFILES)) $(subst .s,.o,$(subst .S,.o,$(SFILES)))
KERNEL_OFILES = ${filter-out crtn.o, ${filter-out crti.o, ${ALL_OFILES}}}

CRTBEGIN = ${shell gcc ${CFLAGS} -print-file-name=crtbegin.o}
CRTEND = ${shell gcc ${CFLAGS} -print-file-name=crtend.o}

OFILES = ${addprefix $B/,crti.o} ${CRTBEGIN} ${addprefix $B/,${KERNEL_OFILES}} ${CRTEND} ${addprefix $B/,crtn.o}

# keep all files
.SECONDARY :

$B/%.o :  Makefile %.c
	@echo "compiling  $*.c"
	@mkdir -p build
	gcc -I. -c -MD -MF $B/$*.d -o $B/$*.o $(CFLAGS) $*.c

$B/%.o :  Makefile %.cc
	@echo "compiling  $*.cc"
	@mkdir -p build
	g++ -I. -c -MD $ -MF $B/$*.d -o $B/$*.o $(CCFLAGS) $*.cc

$B/%.o :  Makefile %.S
	@echo "assembling $*.S"
	@mkdir -p build
	gcc -I. -MD -MF $B/$*.d -o $B/$*.o -m32 -c $*.S

$B/%.o :  Makefile %.s
	@echo "assembling $*.s"
	@mkdir -p build
	gcc -I. -MD -MF $B/$*.d -o $B/$*.o -m32 -c $*.s

$B/%.bin : Makefile $B/%.kernel
	@echo "producing  $B/$*.bin"
	@mkdir -p build
	objcopy -O binary $B/$*.kernel $B/$*.bin

$B/%.img : Makefile $B/%.bin
	@echo "producing  $B/$*.img"
	@mkdir -p build
	dd if=$B/$*.bin of=$B/$*.img bs=512 conv=sync > /dev/null 2>&1
	@echo "---------------------------------"

clean ::
	rm -rf build

-include $B/*.d

$B/kernel.kernel : script.ld Makefile $(OFILES)
	@echo "linking    $*.kernel"
	@mkdir -p build
	ld -T script.ld -N -m elf_i386 -e start -o $@ $(OFILES)

