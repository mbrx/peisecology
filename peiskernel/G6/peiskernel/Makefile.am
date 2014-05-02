
# This install all include files from both libraries under the common include/peiskerel/*
dev_includedir = $(includedir)/peiskernel
dev_include_HEADERS = peiskernel.h tuples.h peiskernel_mt.h hashtable.h \
	peiskernel_private.h tuples_private.h p2p.h bluetooth.h services.h linklayer.h udp.h



# And the choice for which ones *will* actucally be compiled
lib_LTLIBRARIES = libpeiskernel.la libpeiskernel_mt.la

# Compiles and installs singlethreaded peiskernel
libpeiskernel_la_CFLAGS = -g  -Wall -DVERSION=\"${VERSION}\"
libpeiskernel_la_LDFLAGS = -version-info 1:0:0 -g -no-undefined
libpeiskernel_la_SOURCES = \
	peiskernel.c linklayer.c p2p.c services.c tuples.c tuplesAPI.c peiskernel_tcpip.c hashtable.c bluetooth.c \
	\
	peiskernel.h linklayer.h p2p.h tuples.h peiskernel_tcpip.h hashtable.h peiskernel_private.h \
	tuples_private.h bluetooth.h

# Compiles and installs the threaded wrapper around the peiskernel
libpeiskernel_mt_la_CFLAGS = -g -Wall
libpeiskernel_mt_la_LDFLAGS = -version-info 1:0:0 -g -no-undefined -lpeiskernel 
libpeiskernel_mt_la_SOURCES = \
	peiskernel_mt.c \
	\
	peiskernel_mt.h


# Provides profiling information by linking to the sources directly
bin_PROGRAMS = peisprofiler
peisprofiler_SOURCES = peisprofiler.c peiskernel.c linklayer.c bluetooth.c p2p.c services.c \
	tuples.c tuplesAPI.c  peiskernel_tcpip.c hashtable.c

peisprofiler_CFLAGS = -g -pg -fprofile-arcs -ftest-coverage -DVERSION=\"${VERSION}\"
peisprofiler_LDFLAGS =  -g -pg -fprofile-arcs -ftest-coverage

# Support for emacs fly-make mode
check-syntax:
	gcc -o nul -S  -DFLYMAKE ${CFLAGS} ${CHK_SOURCES}
