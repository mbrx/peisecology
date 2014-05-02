
bin_PROGRAMS = tuple_generator tuple_reader tuple_reader2 tuple_generator_mt peisecho stress marcopolo simpledb

INCLUDES=-DSHARE_DIR=\"${pkgdatadir}\" -DPACKAGE=\"${PACKAGE}\" -DVERSION=\"${VERSION}\"

LDFLAG = -L../peiskernel

tuple_generator_SOURCES = tuple_generator.c 
tuple_generator_CFLAGS = -I../peiskernel  -g -ggdb
tuple_generator_LDFLAGS =  -g ${LDFLAG} -lpeiskernel 

tuple_generator_mt_SOURCES = tuple_generator_mt.c 
tuple_generator_mt_CFLAGS = -I../peiskernel  -g -ggdb
tuple_generator_mt_LDFLAGS =  -g -lpeiskernel ${LDFLAG} -lpeiskernel_mt -lpthread

tuple_reader_SOURCES = tuple_reader.c 
tuple_reader_CFLAGS = -I../peiskernel  -g
tuple_reader_LDFLAGS =  -g -lpeiskernel ${LDFLAG}

tuple_reader2_SOURCES = tuple_reader2.c 
tuple_reader2_CFLAGS = -I../peiskernel  -g
tuple_reader2_LDFLAGS =  -g -lpeiskernel ${LDFLAG}

stress_SOURCES = stress.c
stress_CFLAGS =  -I../peiskernel  
stress_LDFLAGS = -lpeiskernel ${LDFLAG}

peisecho_SOURCES = peisecho.c 
peisecho_CFLAGS = -I../peiskernel  -g
peisecho_LDFLAGS =  -g -lpeiskernel -lpeiskernel_mt -lpthread ${LDFLAG} 

marcopolo_SOURCES = marcopolo.c
marcopolo_CFLAGS =  -I../peiskernel  
marcopolo_LDFLAGS = -lpeiskernel ${LDFLAG} -lpeiskernel_mt -lpthread

simpledb_SOURCES = simpledb.c 
simpledb_CFLAGS = -I../peiskernel  -g 
simpledb_LDFLAGS =  -g -lpeiskernel -lpeiskernel_mt -lpthread ${LDFLAG}

#largeData_SOURCES = largeData.c 
#largeData_CFLAGS =   -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -I../peiskernel -g 
#largeData_LDFLAGS =   ${LDFLAG} -L${JAVA_HOME}/jre/lib/i386/client -ljvm -L${JAVA_HOME}/jre/lib/i386/native_threads -lhpi -L${JAVA_HOME}/jre/lib/i386  -g -lpeiskernel -lpeiskernel_js

#largeDataRev_SOURCES = largeDataRev.c 
#largeDataRev_CFLAGS =   -I${JAVA_HOME}/include -I${JAVA_HOME}/include/linux -I../peiskernel -g 
#largeDataRev_LDFLAGS =   ${LDFLAG} -L${JAVA_HOME}/jre/lib/i386/client -ljvm -L${JAVA_HOME}/jre/lib/i386/native_threads -lhpi -L${JAVA_HOME}/jre/lib/i386  -g -lpeiskernel -lpeiskernel_js

