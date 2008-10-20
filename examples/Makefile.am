noinst_PROGRAMS = video-player video-player2 audio-player

INCLUDES = -I$(top_srcdir)/

video_player_SOURCES = video-player.c
video_player_CFLAGS = $(CLUTTER_CFLAGS) $(SURFACE_CFLAGS)
video_player_LDFLAGS =    \
    $(CLUTTER_LIBS) \
    $(SURFACE_LIBS)       \
    $(top_builddir)/clutter-helix/libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la 

video_player2_SOURCES = video-player2.c
video_player2_CFLAGS = $(CLUTTER_CFLAGS) $(SURFACE_CFLAGS)
video_player2_LDFLAGS =    \
    $(CLUTTER_LIBS) \
    $(SURFACE_LIBS)       \
    $(top_builddir)/clutter-helix/libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la 

audio_player_SOURCES = audio-player.c
audio_player_CFLAGS = $(CLUTTER_CFLAGS) $(SURFACE_CFLAGS)
audio_player_LDFLAGS =    \
    $(CLUTTER_LIBS) \
    $(SURFACE_LIBS)       \
    $(top_builddir)/clutter-helix/libclutter-helix-@CLUTTER_HELIX_MAJORMINOR@.la 
