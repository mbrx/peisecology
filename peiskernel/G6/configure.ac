# Process this file with autoconf to produce a configure script.
AC_INIT
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([peis], [0.6.0.6], [mbl@aass.oru.se])
AC_CONFIG_SRCDIR([peiskernel/peiskernel.c])
AM_CONFIG_HEADER(config.h)
AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL

# For symlinks used by libtool
AC_CONFIG_MACRO_DIR([m4])


# Checks for some standard libraries.
CFLAGS="${CFLAGS} -I/usr/local/include -I/usr/local/include/peiskernel"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

AC_CHECK_LIB([e], [main])
AC_CHECK_LIB([inks], [main])
AC_CHECK_LIB(z,main,,[AC_MSG_ERROR([you are probably missing the libzlib-dev package])])
AC_CHECK_LIB(m,main,,[AC_MSG_ERROR([download libHighSchool])])
#AC_CHECK_LIB(pthread,main,,[AC_MSG_ERROR([missing posix threads])])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

# Checks for library functions.
AC_CHECK_FUNCS([atexit sqrt strdup])

	
AM_PROG_CC_C_O

AC_ARG_WITH([bluetooth], AC_HELP_STRING([--with-bluetooth],[Enables bluetooth support (default no)]),
		    [with_bluetooth=$withval],[with_bluetooth=no])
AS_IF([test "x$with_bluetooth" != xno],
      [
      # Test that we realy have bluetooth support
      AC_CHECK_HEADERS([bluetooth/bluetooth.h], [],
                       [AC_MSG_ERROR([Install libbluetooth-dev, OR use --without-bluetooth])])
      AC_CHECK_LIB([bluetooth], [main],,[AC_MSG_ERROR([Install libbluetooth-dev, OR use --without-bluetooth])])      
      CFLAGS="$CXXFLAGS -DWITH_BLUETOOTH"
      ]
      [])
AC_ARG_WITH([readline], AC_HELP_STRING([--with-readline],[Enables readline support (default no)]),
	    [with_readline=$withval],[with_readline=no])
AS_IF([test "x$with_readline" != xno],
      [
      # Test that we realy have readline support
      AC_CHECK_HEADERS([readline/readline.h], [],
                       [AC_MSG_ERROR([Install libreadline-dev, OR use --without-readline])])
      AC_CHECK_LIB([readline], [readline],,[AC_MSG_ERROR([Install libreadline-dev, OR use --without-readline])])      
      CFLAGS="$CXXFLAGS -DWITH_READLINE"
      ][])


# OS specific tests
echo -n "Testing for DARWIN... "
if test `uname` = "Darwin" ; then
  echo "yes"
  CFLAGS="${CFLAGS} -DDARWIN"
else
  echo "no"
fi;

echo "Checking if cross compiling... "
if test "x$host_alias" = "xarm-linux" ; then
  echo "Yes, using alternative LDFLAGS"
  CFLAGS="${CFLAGS} -DGUMSTIX"
  LDFLAGS="-L$prefix/lib -L/$prefix/../../gumstix-buildroot/build_arm_nofpu/staging_dir/lib/ $LDFLAGS"
fi;

#		 peisproxy/Makefile
AC_CONFIG_FILES([Makefile
	         docs/Makefile
		 peiskernel/Makefile
		 peismaster/Makefile
		 samples/Makefile
		 tuplescript/Makefile
		 ])

AC_OUTPUT


