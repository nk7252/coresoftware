AC_INIT(EventDisplay,[1.00])
AC_CONFIG_SRCDIR([configure.ac])

AM_INIT_AUTOMAKE
AC_PROG_CXX(CC g++)
LT_INIT([disable-static])

dnl leaving this here in case we want to play with different compiler 
dnl specific flags
dnl case $CXX in
dnl  clang++)
dnl   CXXFLAGS="$CXXFLAGS -Wall -Werror"
dnl  ;;
dnl  *g++)
dnl   CXXFLAGS="$CXXFLAGS -Wall -Werror"
dnl  ;;
dnl esac

if test $ac_cv_prog_gxx = yes; then
     CXXFLAGS="$CXXFLAGS -Wall -Wextra -Wshadow -Werror"
fi

case $CXX in
 clang++)
   CXXFLAGS="$CXXFLAGS -Wno-dangling -fopenmp"
 ;;
esac


AC_CONFIG_FILES([Makefile])
AC_OUTPUT
