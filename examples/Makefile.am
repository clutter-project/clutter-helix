NULL = #

noinst_PROGRAMS = video-player audio-player

INCLUDES = -I$(top_srcdir) \
	   $(MAINTAINER_CFLAGS) \
	   $(NULL)

video_player_SOURCES = video-player.c
video_player_CFLAGS = $(CLUTTER_CFLAGS) $(SURFACE_CFLAGS)
video_player_LDFLAGS =    \
    $(CLUTTER_LIBS) \
    $(SURFACE_LIBS)       \
    $(top_builddir)/clutter-helix/libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la 

audio_player_SOURCES = audio-player.c
audio_player_CFLAGS = $(CLUTTER_CFLAGS) $(SURFACE_CFLAGS)
audio_player_LDFLAGS =    \
    $(CLUTTER_LIBS) \
    $(SURFACE_LIBS)       \
    $(top_builddir)/clutter-helix/libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la 

EXTRA_DIST = media-actions-pause.png  \
             media-actions-start.png  \
             vid-panel.png            \
             README 
