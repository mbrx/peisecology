
# This install all include files from both libraries under the common include/peiskerel/*
foo_includedir = $(includedir)/peisproxy

foo_include_HEADERS = peisproxy.h peisproxy_private.h auxiliary.h peisproxy_sublist.h

lib_LTLIBRARIES = libpeisproxy.la 


# Compiles and installs singlethreaded pesikernel
libpeisproxy_la_CFLAGS = -g  -Wall 
libpeisproxy_la_LDFLAGS = -g -lpeiskernel -lpeiskernel_mt -lpthread -L../peiskernel/
libpeisproxy_la_SOURCES = \
	auxiliary.c interface.c proxy.c sublist.c
	\
	peisproxy.h peisproxy_sublist.h peisproxy_private.h  auxiliary.h


bin_PROGRAMS = proxytest

INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"

#LDFLAG = -L../peiskernel

proxytest_SOURCES = proxytest.c auxiliary.c interface.c proxy.c sublist.c
proxytest_CFLAGS =  -g -pg
proxytest_LDFLAGS =  -g -pg -lpeiskernel -lpeiskernel_mt -lpthread -L../peiskernel


# proxytest_SOURCES = proxytest.c
# proxytest_CFLAGS =  -g  -ggdb
# proxytest_LDFLAGS = -g ${LDFLAG} -lpeisproxy -lpeiskernel -lpeiskernel_mt -lpthread

# twoIfaceProxy_SOURCES = proxytest.c auxiliary.c interface.c proxy.c sublist.c
# twoIfaceProxy_CFLAGS =  -g -pg
# twoIfaceProxy_LDFLAGS =  -g -pg -lpeiskernel -lpeiskernel_mt -lpthread



check-syntax:
	gcc -o nul -S ${CHK_SOURCES}
