# clutter package version number, (as distinct from shared library version)
# An odd micro number indicates in-progress developmen
# An even micro number indicates a released version.
m4_define(clutter_helix_major_version, 0)
m4_define(clutter_helix_minor_version, 8)
m4_define(clutter_helix_micro_version, 0)

m4_define([clutter_helix_version],
          [clutter_helix_major_version.clutter_helix_minor_version.clutter_helix_micro_version])

m4_define([clutter_helix_api_version],
          [clutter_helix_major_version.clutter_helix_minor_version])

# increase the interface age for each release; if the API changes, set to 0
m4_define([clutter_helix_interface_age], [0])
m4_define([clutter_helix_binary_age],
          [m4_eval(100 * clutter_helix_minor_version + clutter_helix_micro_version)])

AC_PREREQ(2.53)

AC_INIT([clutter-helix], [clutter_helix_version],
        [http://bugzilla.o-hand.com/enter_bug.cgi?product=Clutter-helix])
AC_CONFIG_SRCDIR([clutter-helix/clutter-helix.h])
AM_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE([1.9])

CLUTTER_HELIX_MAJOR_VERSION=clutter_helix_major_version
CLUTTER_HELIX_MINOR_VERSION=clutter_helix_minor_version
CLUTTER_HELIX_MICRO_VERSION=clutter_helix_micro_version
CLUTTER_HELIX_VERSION=clutter_helix_version
CLUTTER_HELIX_API_VERSION=clutter_helix_api_version
CLUTTER_HELIX_MAJORMINOR=clutter_helix_api_version
AC_SUBST(CLUTTER_HELIX_MAJOR_VERSION)
AC_SUBST(CLUTTER_HELIX_MINOR_VERSION)
AC_SUBST(CLUTTER_HELIX_MICRO_VERSION)
AC_SUBST(CLUTTER_HELIX_VERSION)
AC_SUBST(CLUTTER_HELIX_API_VERSION)
AC_SUBST(CLUTTER_HELIX_MAJORMINOR)

m4_define([lt_current],
          [m4_eval(100 * clutter_helix_minor_version + clutter_helix_micro_version - clutter_helix_interface_age)])
m4_define([lt_revision], [clutter_helix_interface_age])
m4_define([lt_age], [m4_eval(clutter_helix_binary_age - clutter_helix_interface_age)])

CLUTTER_HELIX_LT_CURRENT=lt_current
CLUTTER_HELIX_LT_REV=lt_revision
CLUTTER_HELIX_LT_AGE=lt_age
CLUTTER_HELIX_LT_VERSION="$CLUTTER_HELIX_LT_CURRENT:$CLUTTER_HELIX_LT_REV:$CLUTTER_HELIX_LT_AGE"
CLUTTER_HELIX_LT_LDFLAGS="-version-info $CLUTTER_HELIX_LT_VERSION"

AC_SUBST(CLUTTER_HELIX_LT_VERSION)
AC_SUBST(CLUTTER_HELIX_LT_LDFLAGS)

dnl ========================================================================

# Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([memset munmap strcasecmp strdup])


dnl ========================================================================

pkg_modules="clutter-0.8 >= 0.8.0"
PKG_CHECK_MODULES(CLUTTER, [$pkg_modules])


dnl ========================================================================

pkg_modules="helix-player"
PKG_CHECK_MODULES(SURFACE, [$pkg_modules])

dnl ========================================================================

pkg_modules="gtk+-2.0"
PKG_CHECK_MODULES(GTK, [$pkg_modules])

dnl ========================================================================

pkg_modules="gdk-2.0"
PKG_CHECK_MODULES(GDK, [$pkg_modules])

dnl ========================================================================

pkg_modules="glib-2.0"
PKG_CHECK_MODULES(GLIB, [$pkg_modules])

dnl ========================================================================

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall"
fi

dnl ========================================================================

GTK_DOC_CHECK([1.8])

AM_CONDITIONAL([BUILD_GTK_DOC],
               [test "x$enable_gtk_doc" = "xyes" || test ! -f "autogen.sh"])

dnl ========================================================================

AC_SUBST(GCC_FLAGS)

AC_SUBST(SURFACE_CFLAGS)
AC_SUBST(SURFACE_LIBS)

AC_SUBST(CLUTTER_CFLAGS)
AC_SUBST(CLUTTER_LIBS)

AC_OUTPUT([
        Makefile
        examples/Makefile
        doc/Makefile
        doc/reference/Makefile
        doc/reference/version.xml
        clutter-helix/Makefile
        clutter-helix/clutter-helix-version.h
        clutter-helix.pc
])

dnl ========================================================================

echo ""
echo "                  Clutter-Helix $VERSION"
echo "                 ========================="
echo ""
echo "                  prefix:         ${prefix}"
echo "                  SURFACE_CFLAGS: ${SURFACE_CFLAGS}"
echo "                  SURFACE_LIBS:   ${SURFACE_LIBS}"
echo "                  CLUTTER_CFLAGS: ${CLUTTER_CFLAGS}"
echo "                  CLUTTER_LIBS:   ${CLUTTER_LIBS}"
echo ""
echo "                  Documentation:  ${enable_gtk_doc}"
echo ""
