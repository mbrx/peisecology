## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@

bin_PROGRAMS = tupleview

# Use these flags for extra debugging only 
# (includes profiling slowing it down!)
#tupleview_CFLAGS =  -g -pg -fprofile-arcs -ftest-coverage
tupleview_CFLAGS =  -I/usr/local/include
#tupleview_LDFLAGS =  -g -pg -fprofile-arcs -ftest-coverage

tupleview_SOURCES = \
	main.c tupleview.h \
	tokenizer.c tokenizer.h \
	support.c support.h \
	callbacks.c callbacks.h \
	connectDialogue.c aboutDialogue.c preferencesDialogue.c ecologyView.c editValueFileNameDialogue.c


tupleview_LDADD = @PACKAGE_LIBS@ $(INTLLIBS)

