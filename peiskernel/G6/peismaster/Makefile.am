bin_PROGRAMS = peismaster peisstatistics
INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"

peismaster_SOURCES = peismaster.c
peismaster_CFLAGS = -I../peiskernel  -g #-pg # -fprofile-arcs -ftest-coverage
#peismaster_LDFLAGS =  -g -lpeiskernel #-L../peiskernel -lpeiskernel -g
peismaster_LDFLAGS =  -g -lpeiskernel -L../peiskernel -lpeiskernel -g  #-pg #-fprofile-arcs -ftest-coverage

peisstatistics_SOURCES = peisstatistics.c
peisstatistics_CFLAGS = -I../peiskernel  -g
peisstatistics_LDFLAGS =  -g -lpeiskernel -L../peiskernel -g

