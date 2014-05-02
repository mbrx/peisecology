# Process this file with autoconf to produce a configure script.
AC_INIT
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([peiscam], [0.6.0.0], [mbl@aass.oru.se])
AC_CONFIG_SRCDIR([src/peiscam.c])
AM_CONFIG_HEADER(config.h)

# Checks for programs.
AC_PROG_AWK
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_CC_C_O

# OS specific tests
echo -n "Testing for DARWIN... "
if test `uname` = "Darwin" ; then
  echo "yes"
  CFLAGS="${CFLAGS} -DDARWIN"
  LDFLAGS="${LD_FLAGS} -L/sw/lib"
else
  echo "no"
fi;



# Checks for some standard libraries.
AC_CHECK_LIB(z,main,,[AC_MSG_ERROR([libz not found, but required])])
AC_CHECK_LIB(pthread,main,,[AC_MSG_ERROR([missing posix threads])])
AC_CHECK_LIB(jpeg,main,,[AC_MSG_ERROR([libjpeg not found, but required])])
AC_CHECK_LIB(peiskernel,main,,[AC_MSG_ERROR([Install the peiskernel])])
#AC_CHECK_LIB(peiskernel_mt,main,,[AC_MSG_ERROR([Install the peiskernel])])
AC_CHECK_LIB(png,main,,[AC_MSG_ERROR([libpng not found, but required])])

dnl LIBS="-lGLU $LIBS"

AC_ARG_WITH([javaspace], AC_HELP_STRING([--with-javaspace],[Enable if you want to
also compile the javaspace version of peiskernel]),
                         [WITH_JAVASPACE=yes],[WITH_JAVASPACE=no])
AM_CONDITIONAL(JAVASPACE, test $WITH_JAVASPACE = yes)

AC_ARG_WITH([v4l2], AC_HELP_STRING([--with-v4l2],[Enables V4L2 support (default yes)]),
		    [],[with_v4l2=yes])
AS_IF([test "x$with_v4l2" != xno],
      [CXXFLAGS="$CXXFLAGS -DUSE_V4L2"
       CFLAGS="$CFLAGS -DUSE_V4L2"]
      [])
AC_ARG_WITH([v4l], AC_HELP_STRING([--with-v4l],[Enables V4L support (default yes)]),
		    [],[with_v4l=yes])
AS_IF([test "x$with_v4l" != xno],
      [CXXFLAGS="$CXXFLAGS -DUSE_V4L"
       CFLAGS="$CFLAGS -DUSE_V4L"
       AC_CHECK_HEADER(linux/videodev.h,, [AC_MSG_ERROR([Please use --without-v4l OR install libv4l-dev])])
       ]
      [])

AC_ARG_WITH([player], AC_HELP_STRING([--with-player],[Enables Player support (default yes)]),
		    [],[with_player=yes])
AS_IF([test "x$with_player" != xno],
      [
        # To allow the player stuff to be compiled. 
        PKG_CHECK_MODULES(PLAYERC, playerc >= 2.0,,[AC_MSG_ERROR([Please use --without-player OR install Player 2.0/libplayerc])])
        AC_SUBST(PLAYERC_CFLAGS)
        AC_SUBST(PLAYERC_LIBS)

        CXXFLAGS="$CXXFLAGS -DUSE_PLAYER"
        CFLAGS="$CFLAGS -DUSE_PLAYER"
      ]
      [])



# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h sys/time.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

# Checks for library functions.
AC_CHECK_FUNCS([atexit sqrt strdup])


		
AC_CONFIG_FILES([Makefile
                 src/Makefile])
AC_OUTPUT


