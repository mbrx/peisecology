bin_PROGRAMS = peistest largepackages
INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"
peistest_SOURCES = \
	general.c peistest.c \
	\
	general.h 
peistest_CFLAGS = -I../peiskernel  -g
#peistest_LDFLAGS =  -g -lpeiskernel ##-L../peiskernel -lpeiskernel -g
peistest_LDFLAGS =  -g -lpeiskernel -L../peiskernel -lpeiskernel -g

largepackages_SOURCES =	largepackages.c 
largepackages_CFLAGS = -I../peiskernel  -g
#largepackages_LDFLAGS =  -g -lpeiskernel ##-L../peiskernel -lpeiskernel -g
largepackages_LDFLAGS =  -g -lpeiskernel -L../peiskernel -lpeiskernel -g

