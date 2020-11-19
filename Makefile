SHELL := $(shell which bash)
TEST_EXTS = .ok .dir
UTCS_ID ?= $(shell pwd | sed -e 's/.*_//')

MY_TESTS = ${addprefix ${UTCS_ID},${TEST_EXTS}}
SIG = ${firstword ${shell (ls ${MY_TESTS} > /dev/null 2>&1  && (md5sum `find ${MY_TESTS} -type f -print` | sort | md5sum)) || echo "?" }}

TESTS_DIR ?= .

POSSIBLE_TESTS = ${notdir ${basename ${wildcard ${TESTS_DIR}/*${firstword ${TEST_EXTS}}}}}
TESTS = ${sort ${POSSIBLE_TESTS}}
TEST_OKS = ${addsuffix .ok,${TESTS}}
TEST_RESULTS = ${addsuffix .result,${TESTS}}
TEST_TARGETS = ${addsuffix .test,${TESTS}}
TEST_OUTS = ${addsuffix .out,${TESTS}}
TEST_RAWS = ${addsuffix .raw,${TESTS}}
TEST_DIFFS = ${addsuffix .diff,${TESTS}}
TEST_LOOPS = ${addsuffix .loop,${TESTS}}
TEST_FAILS = ${addsuffix .fail,${TESTS}}
TEST_IMAGES = ${addprefix disks/,${addsuffix .image,${TESTS}}}

ORIGIN_URL=${shell git config --get remote.origin.url}
ORIGIN_REPO=${shell echo ${ORIGIN_URL} | sed -e 's/.*://'}
STUDENT_NAME=${shell echo ${ORIGIN_REPO} | sed -e 's/.*_//'}
PROJECT_NAME=${shell echo ${ORIGIN_REPO} | sed -e 's/_${STUDENT_NAME}$$//'}
GIT_SERVER=${shell echo ${ORIGIN_URL} | sed -e 's/:.*//'}


# customize by setting environment variables
QEMU_ACCEL ?= tcg,thread=single
QEMU_CPU ?= max
QEMU_SMP ?= 4
QEMU_MEM ?= 128m
QEMU_TIMEOUT ?= 10
QEMU_TIMEOUT_CMD ?= timeout

QEMU_PREFER = ~gheith/public/qemu_5.1.0/bin/qemu-system-i386
QEMU_CMD ?= ${shell (test -x ${QEMU_PREFER} && echo ${QEMU_PREFER}) || echo qemu-system-i386}

QEMU_CONFIG_FLAGS = -accel ${QEMU_ACCEL} \
                    -cpu ${QEMU_CPU} \
                    -smp ${QEMU_SMP} \
                    -m ${QEMU_MEM}

QEMU_FLAGS = -no-reboot \
	     ${QEMU_CONFIG_FLAGS} \
	     -nographic\
	     --monitor none \
	     --serial file:$*.raw \
             -drive file=kernel/build/kernel.img,index=0,media=disk,format=raw \
             -drive file=disks/$*.image,index=1,media=disk,format=raw \
	     -device isa-debug-exit,iobase=0xf4,iosize=0x04

TIME = $(shell which time)

.PHONY: ${TESTS} kernel sig test tests all clean ${TEST_TARGETS} help qemu_config_flags qemu_cmd before_test history ${TEST_IMAGES}

all : kernel;

${TESTS} : % : kernel disks/%.image;

help:
	@echo ""
	@echo "Makefile for ${ORIGIN_URL}"
	@echo
	@echo "Useful targets:"
	@echo "    make -s kernel          # build the kernel"
	@echo "    make -s t0.image        # build disk image for t0"
	@echo "    make -s t0.test         # run t0 and report results"
	@echo "    make -s t0.loop         # run t0 10 times and report results"
	@echo "    make -s t0.fail         # run t0 until it fails (max LOOP_LIMIT times)"
	@echo "    make -s test            # run all tests once"
	@echo "    make -s test.loop       # use only if you absolutely have to"
	@echo "                            # and after checking that no one else"
	@echo "                            # is using this machine"
	@echo "    make -s get_tests       # get (or update) peer tests from the server"
	@echo "                            # saves the tests in ./all_tests"
	@echo "                            # please don't push back to git"
	@echo "    make -s get_results     # get (or update) your reuslts from the server"
	@echo "                            # saves the reulsts in ./my_results"
	@echo "                            # please don't push back to git"
	@echo "    make -s get_summary     # get (or update) all reuslts from the server"
	@echo "                            # saves the reulsts in ./all_results"
	@echo "                            # please don't push back to git"
	@echo "    make -s get_submission  # get (or update) a view of your submission"
	@echo "                            # please don't push back to git"
	@echo "    make -s diff_submission # compare your working directory with your submission"
	@echo "    make history            # summarizes your test history"
	@echo ""
	@echo "Environment Variables:"
	@echo "    number of loop iterations: LOOP_LIMIT       (${LOOP_LIMIT})"
	@echo "    qemu acceleration flag   : QEMU_ACCEL       (${QEMU_ACCEL})"
	@echo "    qemu command             : QEMU_CMD         (${QEMU_CMD})"
	@echo "    qemu cpu                 : QEMU_CPU         (${QEMU_CPU})"
	@echo "    simulated memory         : QEMU_MEM         (${QEMU_MEM})"
	@echo "    number of cores          : QEMU_SMP         (${QEMU_SMP})"
	@echo "    timeout                  : QEMU_TIMEOUT     (${QEMU_TIMEOUT})"
	@echo "    timeout command          : QEMU_TIMEOUT_CMD (${QEMU_TIMEOUT_CMD})"
	@echo "    tests directory          : TESTS_DIR        (${TESTS_DIR})"
	@echo ""

origin:
	@echo "repo     : ${ORIGIN_REPO}"
	@echo "project  : ${PROJECT_NAME}"
	@echo "students : ${STUDENT_NAME}"
	@echo "server   : ${GIT_SERVER}"

get_tests:
	test -d all_tests || git clone ${GIT_SERVER}:${PROJECT_NAME}__tests all_tests
	(cd all_tests ; git pull)
	@echo ""
	@echo "Tests copied to all_tests (cd all_tests)"
	@echo "   Please don't add the all_tests directory to git"
	@echo ""

get_summary:
	test -d all_results || git clone ${GIT_SERVER}:${PROJECT_NAME}__results all_results
	(cd all_results ; git pull)
	python tools/summarize.py all_results

get_results:
	test -d my_results || git clone ${GIT_SERVER}:${PROJECT_NAME}_${STUDENT_NAME}_results my_results
	(cd my_results ; git pull)
	@(cd my_results;                                                      \
		for i in *.result; do                                         \
			name=$$(echo $$i | sed -e 's/\..*//');                \
			echo "$$name `cat $$name.result` `cat $$name.time`";  \
		done;                                                         \
		echo "";                                                      \
		echo "`grep pass *.result | wc -l` / `ls *.result | wc -l`";  \
	)
	@echo ""
	@echo "More details in my_results (cd my_results)"
	@echo "    Please don't add my_results directory to git"
	@echo ""

get_submission:
	test -d my_submission || git clone ${GIT_SERVER}:${PROJECT_NAME}_${STUDENT_NAME} my_submission
	(cd my_submission && git pull)
	@echo ""
	@echo "A fresh copy of your submission is in my_submissions"
	@echo "    Please don't add my_submission to git"
	@echo "    Please don't do any development in my_submission"
	@echo "    It is here to help you view what you've submitted"
	@echo ""

diff_submission: clean get_submission
	@echo "======================================================================"
	@echo "Here are the differences between what you have and what the server has"
	@echo "   More details in my_submission                                      "
	@echo "   Please remember that the server will replace some of your files    "
	@echo "   before running your code. Those changes are not shown here.        "
	@echo "======================================================================"
	@diff -rqyl . my_submission --exclude=.git --exclude=my_submission || true
	@echo ""

qemu_cmd:
	@echo "${QEMU_CMD}"

qemu_config_flags:
	@echo "${QEMU_CONFIG_FLAGS}"

kernel :
	@$(MAKE) -C kernel --no-print-directory build/kernel.img

clean:
	rm -rf *.diff *.raw *.out *.result *.kernel *.failure *.time disks
	(make -C kernel clean)

BLOCK_SIZE = 4096
${TEST_IMAGES} : disks/%.image :
	mkdir -p disks
	rm -f disks/$*.image
	touch disks/$*.image
	(test -d $*.dir && mkfs.ext2 -q -b ${BLOCK_SIZE} -i ${BLOCK_SIZE} -d ${TESTS_DIR}/$*.dir  -I 128 -r 0 -t ext2 disks/$*.image 10m 2> /dev/null) || touch disks/$*.image
	
${TEST_RAWS} : %.raw : Makefile % disks/%.image
	@echo -n "$* ... "
	@rm -f $*.raw $*.failure
	@touch $*.failure
	@echo "*** failed to run, look in $*.failure for more details" > $*.raw
	-(${TIME} --quiet -o $*.time -f "%E" ${QEMU_TIMEOUT_CMD} ${QEMU_TIMEOUT} ${QEMU_CMD} ${QEMU_FLAGS} > $*.failure 2>&1); if [ $$? -eq 124 ]; then echo "timeout" > $*.failure; echo "timeout" > $*.time; fi

${TEST_OUTS} : %.out : Makefile %.raw
	-egrep '^\*\*\*' $*.raw > $*.out 2> /dev/null || true

${TEST_DIFFS} : %.diff : Makefile %.out ${TESTS_DIR}/%.ok
	-(diff -a -wBb $*.out ${TESTS_DIR}/$*.ok > $*.diff 2> /dev/null || true)

${TEST_RESULTS} : %.result : Makefile %.diff
	(test -z "`cat $*.diff`" && echo "pass" > $*.result) || echo "fail" > $*.result
	echo "$* `cat $*.result` `cat $*.time` [`qemu-system-i386 --version | head -n 1`] [`g++ --version | head -n 1`] [`/bin/date`]" >> history

${TEST_TARGETS} : %.test : Makefile %.result
	@echo "`cat $*.result` `cat $*.time`"

OTHER_USERS = ${shell who | sed -e 's/ .*//' | sort | uniq}
HOW_MANY = ${shell who | sed -e 's/ .*//' | sort | uniq | wc -l}
LOOP_LIMIT ?= 10

loop_warning.%:
	@echo "*******************************************************************************"
	@echo "*** This is NOT the sort of thing you run ALL THE TIME on a SHARED MACHINE  ***"
	@echo "*** In particular long running tests and tests that timeout                 ***"
	@echo "*******************************************************************************"
	@echo ""
	@echo "You can use LOOP_LIMIT to control the number if iterations. For example:"
	@echo "   LOOP_LIMIT=7 make -s $*.loop"
	@echo ""
	@echo "::::::: You are 1 of ${HOW_MANY} users on this machine"
	@echo ":::::::         ${OTHER_USERS}"
	@echo ":::::::   all of them value their work and their time as much as you value yours"
	@echo ":::::::"
	@echo ""


${TEST_LOOPS} : %.loop : loop_warning.% %
	@let pass=0; \
	for ((i=1; i<=${LOOP_LIMIT}; i++)); do \
		echo -n  "[$$i/${LOOP_LIMIT}] "; \
		$(MAKE) -s $*.test; \
		if [ "`cat $*.result`" = "pass" ]; then let pass=pass+1; fi; \
	done; \
	echo ""; \
	echo "$$(basename $$(pwd)) $* $$pass/${LOOP_LIMIT}"; \
	echo ""

${TEST_FAILS} : %.fail : loop_warning.% %
	@let pass=0; \
	for ((i=1; i<=${LOOP_LIMIT}; i++)); do \
		echo -n  "[$$i/${LOOP_LIMIT}] "; \
		$(MAKE) -s $*.test; \
		if [ "`cat $*.result`" = "pass" ]; then let pass=pass+1; else break; fi; \
	done; \
	echo ""; \
	echo "$$(basename $$(pwd)) $* $$pass/${LOOP_LIMIT}"; \
	echo ""	

before_test:
	rm -f *.result *.time *.out *.raw *.failure

test: before_test Makefile ${TESTS} ${TEST_TARGETS} ;
	-@echo ""
	-@echo -n "$$(basename $$(pwd)) "
	-@echo "pass:`(grep pass *.result | wc -l) || echo 0`/`(ls *.result | wc -l) || echo 0`"
	-@echo ""

test.loop: loop_warning.test ${TEST_LOOPS}

failed:
	-@for i in "`grep -l fail *.result`"; do \
		t=`echo $$i | sed -e "s/\.result//"`; \
		echo ""; \
		echo "**************** $$t ****************"; \
		cat $$t.diff; \
	done

HISTORY_TESTS=${shell touch history; (cat history | cut -d ' ' -f 1 | tr ':' 'X' | sort | uniq)}

%.history:
	@echo -n "$*"
	@echo -n " `egrep '^$* pass' history | wc -l`"
	@echo -n "/`egrep '^$* ' history | wc -l`"
	@echo -n " [`egrep '^$* ' history | cut -d ' ' -f 3 | sort | head -1`"
	@echo -n "..`egrep '^$* ' history | cut -d ' ' -f 3 | sort -r | head -1`"
	@echo "]"

history: ${addsuffix .history,${HISTORY_TESTS}};


###################### Test #################

sig:
	@echo ${SIG}

tests:
	@echo "$(TESTS)"

copy_student_tests:
ifeq ($(SIG),?)
	@echo "no tests"
else
ifeq ($(TEST_DIR),)
	@echo "no TEST_DIR"
else
	@for i in $(TEST_EXTS); do \
	    cp -a $(UTCS_ID)$$i $(TEST_DIR)/$(SIG)$$i; \
	done
endif
endif

copy_given_tests:
ifeq ($(TEST_DIR),)
	@echo "no TEST_DIR"
else
	@for i in $(TEST_EXTS); do \
	    cp -a t?$$i $(TEST_DIR); \
	done
endif

remove_tests:
	@for i in $(TEST_EXTS); do \
	    rm -rf *$$i; \
	done

copy_results:
ifeq ($(RESULTS_DIR),)
	@echo "no RESULTS_DIR"
else
	-cp *.result $(RESULTS_DIR)
	-cp ${TESTS_DIR}/*.ok $(RESULTS_DIR)
	-cp *.out $(RESULTS_DIR)
	-cp *.raw $(RESULTS_DIR)
	-cp *.time $(RESULTS_DIR)
	-cp *.failure $(RESULTS_DIR)
endif

