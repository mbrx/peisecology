

bin_PROGRAMS = peiscam

INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\" $(PLAYERC_CFLAGS)
LIBS+=$(PLAYERC_LIBS)

peiscam_SOURCES = \
	peiscam.c general.c jpg_utils.c ccvt_c.c png_utils.c utils.c \
	\
	general.h jpg_utils.h ccvt.h png_utils.h utils.h huffman.h

peiscam_CFLAGS = -g 
peiscam_CCFLAGS = -g 
peiscam_LDFLAGS = -g -lpeiskernel_mt
