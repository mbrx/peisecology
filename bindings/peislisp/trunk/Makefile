CC= gcc
LD_FLAGS= ${LDFLAGS} -lm -lpeiskernel -lpeiskernel_mt -lpthread
CFLAGS = -c -O3 -fno-common

# Change to your prefix path of LISP
#LISP_PREFIX=/usr/local
LISP_PREFIX=/usr
LISP_PREFIX2=\"${LISP_PREFIX}\"

## MacOS 
#LISP_SUFFIX=.ppcf
#INSTALL=install

## Linux
LISP_SUFFIX=.x86f
INSTALL=install -D

OBJS= main.o peislisp.o
SRCS= main.o peislisp.o

.SUFFIXES: .c
.c.o:
	$(CC) $(CFLAGS) -c $< 	-DLISP_PREFIX=${LISP_PREFIX2}

all:  main runlisp startup-library peislisp${LISP_SUFFIX}
	@echo "Remember to also do make startup-library"

# This is just a program for testing things, please ignore
main: ${OBJS}
	${CC}  ${OBJS} -o main ${LD_FLAGS}	

# A program to can be used as the interpreter for .lisp scripts
runlisp: runlisp.o
	${CC}  runlisp.o -o runlisp ${LD_FLAGS}

# The lisp library which is used by the interpreter above
startup-library:
	lisp -eval '(compile-file "start") (quit)'
	chmod 644 start${LISP_SUFFIX}
	${INSTALL} start.* ${LISP_PREFIX}/lib/cmucl/

# Lisp interface
peislisp${LISP_SUFFIX}:
	${INSTALL} peislisp.o /usr/local/peislisp/peislisp.o
	lisp -eval '(load "peislisp") (compile-file "peislisp") (quit)'
	chmod 644 peislisp${LISP_SUFFIX}

clean:
	rm -rf *.o core *~

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

install: peislisp${LISP_SUFFIX}
	${INSTALL} peislisp.* /usr/local/peislisp/
	${INSTALL} template.lisp /usr/local/peislisp/
	${INSTALL} runlisp /usr/local/bin/

