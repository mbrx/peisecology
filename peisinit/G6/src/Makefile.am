bin_PROGRAMS = peisinit
INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"

peisinit_SOURCES = peisinit.c 
# add gprof-helper.c and -pg -ldl if you want to use profilíng on this application
peisinit_CFLAGS =  -g -I/usr/local/include
peisinit_LDFLAGS =  -g

