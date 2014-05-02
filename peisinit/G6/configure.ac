# Process this file with autoconf to produce a configure script.
AC_INIT
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([peisinit], [0.4.4.0], [mbl@aass.oru.se])
AC_CONFIG_SRCDIR([src/peisinit.c])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

# Checks for some standard libraries.
AC_CHECK_LIB([e], [main])
AC_CHECK_LIB([inks], [main])
AC_CHECK_LIB([kenny], [life_signs],,[echo "oh my god! They killed kenny"; echo "You bastards"])

AC_CHECK_LIB(z,main,,[AC_MSG_ERROR([zlib should be included in most unix distributions, this is a strange error])])
AC_CHECK_LIB(m,main,,[AC_MSG_ERROR([download libHighSchool])])

AC_CHECK_LIB(pthread,main,,[AC_MSG_ERROR([missing posix threads])])
AC_CHECK_LIB(peiskernel,main,,[AC_MSG_ERROR([You need the peiskernel installed on this computer])])
AC_CHECK_LIB(peiskernel_mt,main,,[AC_MSG_ERROR([You need the peiskernel-mt installed on this computer])])

#AC_CHECK_LIB(jpeg,main,,[AC_MSG_ERROR([download libjpeg])])

#dnl Check for GUILE
#GUILE_FLAGS
#LIBS="${GUILE_LDFLAGS} ${LIBS}"
#CFLAGS="${GUILE_CFLAGS} ${CFLAGS}"
#CXXFLAGS="${CFLAGS} ${GUILE_CFLAGS}"


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

# Checks for library functions.
AC_CHECK_FUNCS([atexit sqrt strdup])

AC_ARG_WITH([javaspace], AC_HELP_STRING([--with-javaspace],[Enable if you want to
also compile the javaspace version of peiskernel]),
                         [WITH_JAVASPACE=yes],[WITH_JAVASPACE=no])
AM_CONDITIONAL(JAVASPACE, test $WITH_JAVASPACE = yes)

AC_CONFIG_FILES([Makefile
	         docs/Makefile
		 src/Makefile
		 share/Makefile
		 ])
AC_OUTPUT


