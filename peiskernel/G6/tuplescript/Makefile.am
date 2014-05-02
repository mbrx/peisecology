
bin_PROGRAMS = tuplescript 

INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"

LDFLAG = -L../peiskernel

tuplescript_SOURCES = tuplescript.c tokenizer.c values.c
tuplescript_CFLAGS = -I../peiskernel  -g -O0
tuplescript_LDFLAGS =  -g -lpeiskernel -lpeiskernel_mt -lpthread ${LDFLAG}
