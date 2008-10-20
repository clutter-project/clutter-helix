source_h = 					\
	$(srcdir)/clutter-helix.h 		\
	$(srcdir)/clutter-helix-util.h 		\
	$(srcdir)/clutter-helix-video-texture.h \
	$(srcdir)/clutter-helix-audio.h

source_c = clutter-helix-util.c          \
           clutter-helix-video-texture.c \
           clutter-helix-audio.c

libclutter_helix_@CLUTTER_HELIX_MAJORMINOR@_la_SOURCES = $(MARSHALFILES)  \
                                                         $(source_c)      \
                                                         $(source_h)

INCLUDES =                               \
	-I$(top_srcdir)                  \
	-DG_LOG_DOMAIN=\"Clutter-Helix\" \
	@GCC_FLAGS@                      \
	@CLUTTER_CFLAGS@                 \
	$(SURFACE_CFLAGS)

lib_LTLIBRARIES = libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la

libclutter_helix_@CLUTTER_HELIX_MAJORMINOR@_la_LIBADD  = @CLUTTER_LIBS@ $(SURFACE_LIBS)
libclutter_helix_@CLUTTER_HELIX_MAJORMINOR@_la_LDFLAGS = @CLUTTER_HELIX_LT_LDFLAGS@

clutterhelixheadersdir = $(includedir)/clutter-@CLUTTER_HELIX_MAJORMINOR@/clutter-helix
clutterhelixheaders_HEADERS = $(source_h) clutter-helix.h
