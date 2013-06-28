# process this file with autoconf to produce a configure script.
AC_INIT([msg7231],[0.0.2])

# require at least this autoconf version
AC_PREREQ(2.63)

# set up the directory structure
AC_CONFIG_SRCDIR([src/msg7231.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# initialize automake
AM_INIT_AUTOMAKE([tar-ustar])

AC_CANONICAL_SYSTEM

# initialize libtool
LT_INIT([disable-static])
AC_LIBTOOL_WIN32_DLL


case $target_os in 
	*mingw*)
		mingw_found=yes
	;;
esac

AM_CONDITIONAL(BUILD_WIN32, test x$mingw_found = xyes)

AC_CONFIG_COMMANDS([libtool-hacking],[
if test "$mingw_found" = "yes" ; then
	AC_MSG_NOTICE([Hacking libtool to work with mingw...])
	sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
	cp -f ./libtool.tmp ./libtool
	rm -f ./libtool.tmp
fi
],[mingw_found=$mingw_found])


AC_ARG_ENABLE(strict,
AC_HELP_STRING([--enable-strict],[Enable error on compilation warning [default=yes]]),
[wall_werror=$enableval],
[
	wall_werror=yes
]
)

CFLAGS="$CFLAGS -Wall"

if test "$GCC" = "yes" && test "$wall_werror" = "yes" ; then
	CFLAGS="$CFLAGS -Werror"
fi

# check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(ORTP, ortp >= 0.16.0)
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.0.0)

# create the following files from their .in counterparts
AC_CONFIG_FILES([
	Makefile
	src/Makefile
	msg7231.iss
	msg7231.spec
])
AC_OUTPUT

