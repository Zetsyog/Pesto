AC_PREREQ([2.72])
AC_INIT([cloog],[0.1.0])
AC_CONFIG_AUX_DIR(autoconf)
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([-Wall foreign subdir-objects])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

m4_pattern_forbid([AX_CHECK_COMPILE_FLAG])


dnl /**************************************************************************
dnl  *                              Checking                                  *
dnl  **************************************************************************/

dnl Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AX_CXX_COMPILE_STDCXX_17

AM_PROG_AR
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_CHECK_PROG(CD, cd)

AC_PATH_PROGS(BASH, bash)

dnl Configure needs an empty install.sh file with this, i HATE that...
AC_PROG_INSTALL
LT_INIT
AC_CHECK_PROG(TEXI2DVI, texi2dvi, texi2dvi, [])
AM_CONDITIONAL(HAVE_TEXI2DVI, test -n "$TEXI2DVI")

AC_SUBST(CFLAGS_WARN)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

AC_SUBST(PLUTO_INCLUDE)
AC_SUBST(PLUTO_LDFLAGS)
AC_SUBST(PLUTO_LIBADD)

AC_SUBST(ISL_INCLUDE)
AC_SUBST(ISL_LIBADD)
AC_SUBST(OSL_INCLUDE)
AC_SUBST(OSL_LIBADD)
AC_SUBST(CLAN_INCLUDE)
AC_SUBST(CLAN_LIBADD)
AC_SUBST(CLOOG_INCLUDE)
AC_SUBST(CLOOG_LIBADD)


echo ""
echo "======================"
echo "Configuring pluto"
echo "======================"
AC_CONFIG_SUBDIRS([pluto])



AC_CONFIG_FILES([
  Makefile 
  lib/Makefile 
  tool/Makefile 
  pesto.sh
])

AC_OUTPUT

echo " /*-----------------------------------------------*"
echo "  *            PESTO configuration is OK          *"
echo "  *-----------------------------------------------*/"
echo "Your system is ready to compile PESTO"
echo "Run 'make' next to compile"
# echo "Then, run 'make test' to check if everything is working correctly"
echo "Use pesto (see README)"