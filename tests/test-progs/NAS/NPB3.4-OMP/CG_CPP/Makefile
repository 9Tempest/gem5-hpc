SHELL=/bin/sh
BENCHMARK=cg_cpp
BENCHMARKU=CG_CPP

include ../config/make.def

include ../sys/make.common

OBJS = cg_cpp.o \
       ${COMMON}/c_print_results.o \
       ${COMMON}/c_timers.o \
       ${COMMON}/c_wtime.o \
	   m5op.o


${PROGRAM}: config ${OBJS}
	${CLINK} ${CLINKFLAGS} -o ${PROGRAM} ${OBJS} ${C_LIB}

.c.o:
	${CCOMPILE} $<

cg_cpp.o:             cg_cpp.c npbparams.h

m5op.o: /home/arkhadem/gem5-hpc/util/m5/build/x86/abi/x86/m5op.S
	$(CC) $(C_INC) $(CFLAGS) -c $^ -o $@

clean:
	- rm -f *.o *~ mputil*
	- rm -f npbparams.h core
	- if [ -d rii_files ]; then rm -r rii_files; fi
