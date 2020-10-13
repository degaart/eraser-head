AC_INIT([eraser-head], [0.1])
AC_PREREQ([2.69])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([1.14 foreign subdir-objects silent-rules -Wall -Werror])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O

AC_PROG_CC_STDC
AS_IF([test "$ac_cv_prog_cc_stdc" == "no"], [AC_MSG_ERROR([C compiler does not support c99.])])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_SRCDIR([src/main.c])

AS_IF([test "$(uname)" = Linux],
    [PKG_CHECK_MODULES([LIBBSD], [libbsd-overlay])],
)

AC_OUTPUT

