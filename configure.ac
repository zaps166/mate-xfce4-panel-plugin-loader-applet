AC_INIT([mate-xfce4-panel-plugin-loader-applet],
        [1.0.1])

AC_CONFIG_HEADERS(config.h)
AC_PROG_CC
AC_PROG_INSTALL
AM_INIT_AUTOMAKE([1.9 no-dist-gzip dist-xz tar-ustar -Wno-portability])
IT_PROG_INTLTOOL([0.35.0])
GLIB_GSETTINGS

GETTEXT_PACKAGE=mate-xfce4-panel-plugin-loader-applet
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", [Define the gettext package to be used])
AC_SUBST(GETTEXT_PACKAGE)
AM_GLIB_GNU_GETTEXT

# GTK+ API version checking
AC_MSG_CHECKING([Checking used GTK+ API version])
AC_ARG_WITH([gtk], [AS_HELP_STRING([--with-gtk=2.0],[Define GTK+ API version to use (default: 2.0)])],
  [case "$with_gtk" in
     2.0) ;;
     *) AC_MSG_ERROR([Invalid GTK+ API version specified]) ;;
   esac], [with_gtk=2.0])
AC_MSG_RESULT([$with_gtk])
case "$with_gtk" in
  2.0) ;;
esac

# matepanelapplet and xfce4panel
PKG_CHECK_MODULES(APPLET, libmatepanelapplet-4.0 >= 1.7.0 libxfce4panel-1.0 >= 4.10.0)
AC_SUBST(APPLET_CFLAGS)
AC_SUBST(APPLET_LIBS)

AC_CHECK_LIB([dl], [dlsym], [DL_LIBS=-ldl])
AC_SUBST([DL_LIBS])

AC_OUTPUT([
Makefile
src/Makefile
po/Makefile.in
])
