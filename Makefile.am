SUBDIRS = clutter-helix examples

DIST_SUBDIRS = clutter-helix

clutter-helix-@CLUTTER_HELIX_MAJORMINOR@.pc: clutter-helix.pc
	@cp -f clutter-helix.pc clutter-helix-@CLUTTER_HELIX_MAJORMINOR@.pc

pkgconfig_DATA = clutter-helix-@CLUTTER_HELIX_MAJORMINOR@.pc
pkgconfigdir   = $(libdir)/pkgconfig

EXTRA_DIST = clutter-helix.pc.in

CLEANFILES = clutter-helix-@CLUTTER_HELIX_MAJORMINOR@.pc

DISTCLEANFILES = clutter-helix.pc

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

# Extra clean files so that maintainer-clean removes *everything*
MAINTAINERCLEANFILES = aclocal.m4 compile config.guess config.sub \
                       configure depcomp install-sh ltmain.sh     \
                       Makefile.in missing config.h.in
