#include <stdlib.h>

#include <clutter/clutter.h>
#include <clutter-helix/clutter-helix.h>

#define SEEK_H 20
#define SEEK_W 690

typedef struct PlayerApp
{
  ClutterActor *audio;
  ClutterActor *background;
  ClutterActor *control;
  ClutterActor *control_bg;
  ClutterActor *control_play;
  ClutterActor *control_pause;
  ClutterActor *control_seek1;
  ClutterActor *control_seek2;
  ClutterActor *control_seekbar;
  ClutterActor *control_label;
  gboolean      paused;
}
PlayerApp;

void
animate_exit (PlayerApp *app)
{
  ClutterTimeline       *timeline;
  ClutterEffectTemplate *tplt;

  timeline = clutter_timeline_new_for_duration (750);
  clutter_timeline_set_loop (timeline, FALSE);
  tplt = clutter_effect_template_new (timeline, CLUTTER_ALPHA_SINE_INC);

  clutter_effect_move (tplt, 
		       app->background, 
		       CLUTTER_STAGE_WIDTH()/2 - clutter_actor_get_width(app->background)/2, 
		       -(clutter_actor_get_height(app->background)),
		       NULL, NULL);
  clutter_effect_move (tplt, app->control_play,  -1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_pause, -1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_seek1,  1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_seek2,  1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_seekbar, 1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_bg, 1000, -1000, NULL, NULL);
  clutter_effect_move (tplt, app->control_label, 1000, 1000, 
		       clutter_main_quit, NULL);
}

void
toggle_pause_state (PlayerApp *app)
{
  if (app->paused)
    {
      clutter_media_set_playing (CLUTTER_MEDIA(app->audio), 
				 TRUE);
      app->paused = FALSE;
      clutter_actor_hide (app->control_play);
      clutter_actor_show (app->control_pause);
    }
  else
    {
      clutter_media_set_playing (CLUTTER_MEDIA(app->audio), 
				     FALSE);
      app->paused = TRUE;
      clutter_actor_hide (app->control_pause);
      clutter_actor_show (app->control_play);
    }
}

static gboolean
input_cb (ClutterStage *stage, 
	  ClutterEvent *event,
	  gpointer      user_data)
{
  PlayerApp *app = (PlayerApp*)user_data;

  switch (event->type)
    {
    case CLUTTER_BUTTON_PRESS:
	{
	  ClutterActor       *actor;
	  ClutterButtonEvent *bev = (ClutterButtonEvent *) event;

	  actor = clutter_stage_get_actor_at_pos (stage, bev->x, bev->y);

	  if (actor == app->control_pause || actor == app->control_play)
	    {
	      toggle_pause_state (app);
	      return;
	    }

	  if (actor == app->control_seek1 
	      || actor == app->control_seek2
	      || actor == app->control_seekbar)
	    {
	      gint x, y, dist, pos;

	      clutter_actor_get_transformed_position (app->control_seekbar,
                                                      &x, &y);

	      dist = bev->x - x;

	      CLAMP(dist, 0, SEEK_W);

	      pos = (dist * clutter_media_get_duration 
                               (CLUTTER_MEDIA(app->audio))) / SEEK_W;

	      clutter_media_set_position (CLUTTER_MEDIA(app->audio),
					      pos);
	    }
	}
      break;
    case CLUTTER_KEY_RELEASE:
      {
	ClutterKeyEvent* kev = (ClutterKeyEvent *) event;
	
	switch (clutter_key_event_symbol (kev))
	  {
	  case CLUTTER_q:
	  case CLUTTER_Escape:
	    animate_exit (app);
	    break;
	  default:
	    toggle_pause_state (app);
	    break;
	  }
      }
    default:
      break;
    }

  return FALSE;
}

void 
tick (GObject      *object,
      GParamSpec   *pspec,
      PlayerApp     *app)
{
  ClutterHelixAudio      *audio;
  gint                    w, h, position, duration, seek_w;
  gchar                   buf[256];

  audio = CLUTTER_HELIX_AUDIO(object);

  position = clutter_media_get_position (CLUTTER_MEDIA(audio));
  duration = clutter_media_get_duration (CLUTTER_MEDIA(audio));

  if (duration == 0 || position == 0)
    return;

  clutter_actor_set_size (app->control_seekbar, 
			  (position * SEEK_W) / duration, 
			  SEEK_H);  
}

void 
eos_cb (ClutterMedia *media, gpointer user_data)
{
  clutter_media_set_playing (media, TRUE);
}

int
main (int argc, char *argv[])
{
  PlayerApp             *app = NULL;
  ClutterActor          *stage;
  ClutterColor           stage_color = { 0x00, 0x00, 0x00, 0x00 };
  ClutterColor           control_color1 = { 73, 74, 77, 0xee };
  ClutterColor           control_color2 = { 0xcc, 0xcc, 0xcc, 0xff };
  gint                   x,y;
  ClutterTimeline       *timeline;
  ClutterEffectTemplate *tplt;

  if (argc != 2)
    {
      fprintf(stderr, "USAGE: %s MUSIC_FILE", argv[0]);
      exit(-1);
    }

  clutter_helix_init (&argc, &argv);

  stage = clutter_stage_get_default ();
  g_object_set (stage, "fullscreen", TRUE, NULL);
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color); 

  app = g_new0(PlayerApp, 1);

  /* Make a timeline to animate the controls onto the stage */
  timeline = clutter_timeline_new_for_duration (1500);
  clutter_timeline_set_loop (timeline, FALSE);
  tplt = clutter_effect_template_new (timeline, CLUTTER_ALPHA_SINE_INC);

  app->audio = clutter_helix_audio_new ();
  if (app->audio == NULL)
    {
      g_error("failed to create audio");
    }

  /* Load up out video texture */
  clutter_media_set_filename(CLUTTER_MEDIA(app->audio), argv[1]);

  /* Create the control UI */
  app->control = clutter_group_new ();

  app->control_bg =
    clutter_texture_new_from_file ("vid-panel.png", NULL);
  app->control_play =
    clutter_texture_new_from_file ("media-actions-start.png", NULL);
  app->control_pause =
    clutter_texture_new_from_file ("media-actions-pause.png", NULL);

  g_assert (app->control_bg && app->control_play && app->control_pause);

  app->control_seek1   = clutter_rectangle_new_with_color (&control_color1);
  app->control_seek2   = clutter_rectangle_new_with_color (&control_color2);
  app->control_seekbar = clutter_rectangle_new_with_color (&control_color1);
  clutter_actor_set_opacity (app->control_seekbar, 0x99);

  app->control_label 
    = clutter_label_new_with_text("Sans Bold 24", 
				  g_path_get_basename(argv[1]));
  clutter_label_set_color (CLUTTER_LABEL(app->control_label), &control_color1);
  
  clutter_group_add_many (CLUTTER_GROUP (app->control), 
			  app->control_bg, 
			  app->control_play, 
			  app->control_pause,
			  app->control_seek1,
			  app->control_seek2,
			  app->control_seekbar,
			  app->control_label,
			  NULL);

  clutter_actor_set_opacity (app->control, 0xee);

  clutter_actor_set_size (app->control_seek1, SEEK_W+10, SEEK_H+10);
  clutter_actor_set_size (app->control_seek2, SEEK_W, SEEK_H);
  clutter_actor_set_size (app->control_seekbar, 0, SEEK_H);

  clutter_actor_set_position (app->control_play, -1000, -1000);
  clutter_actor_set_position (app->control_pause, -1000, -1000);
  clutter_actor_set_position (app->control_seek1, -1000, -1000);
  clutter_actor_set_position (app->control_seek2, -1000, -1000);
  clutter_actor_set_position (app->control_seekbar, -1000, -1000);
  clutter_actor_set_position (app->control_label, -1000, -1000);

  clutter_effect_move (tplt, app->control_play, 30, 30, NULL, NULL);
  clutter_effect_move (tplt, app->control_pause, 30, 30, NULL, NULL);
  clutter_effect_move (tplt, app->control_seek1, 200, 100, NULL, NULL);
  clutter_effect_move (tplt, app->control_seek2, 205, 105, NULL, NULL);
  clutter_effect_move (tplt, app->control_seekbar, 205, 105, NULL, NULL);
  clutter_effect_move (tplt, app->control_label, 200, 40, NULL, NULL);

  app->background = clutter_texture_new_from_file ("clutter-logo-800x600.png", 
						   NULL);

  clutter_actor_set_position (app->background, 
			      CLUTTER_STAGE_WIDTH()/2 - clutter_actor_get_width(app->background)/2, 
			      -(clutter_actor_get_height(app->background)));
  clutter_effect_move (tplt, 
		       app->background,
		       CLUTTER_STAGE_WIDTH()/2 - clutter_actor_get_width(app->background)/2,
		       0, NULL, NULL);
  clutter_actor_set_opacity (app->background, 0xff/2);

  /* Add control UI to stage */
  clutter_group_add_many (CLUTTER_GROUP (stage),
			  app->background,
			  app->control, NULL);

  x = (CLUTTER_STAGE_WIDTH() - clutter_actor_get_width(app->control))/2;
  y = CLUTTER_STAGE_HEIGHT() - (CLUTTER_STAGE_HEIGHT()/3);

  clutter_actor_set_position (app->control, 0, 0);
  clutter_effect_move (tplt, app->control, x, y, NULL, NULL);
  clutter_effect_rotate (tplt, app->control,
			 CLUTTER_Z_AXIS, 
			 360,
			 CLUTTER_STAGE_WIDTH()/4, 
			 CLUTTER_STAGE_HEIGHT()/4, 
			 0,
			 CLUTTER_ROTATE_CW,
			 NULL, NULL);
  clutter_actor_set_opacity (app->control, 0);
  clutter_effect_fade (tplt, app->control, 0xff, NULL, NULL);

  /* Hook up other events */
  g_signal_connect (stage, "event",
		    G_CALLBACK (input_cb), 
		    app);
  g_signal_connect (app->audio,
		    "notify::position",
		    G_CALLBACK (tick),
		    app);
  g_signal_connect (app->audio, 
		    "eos", 
		    G_CALLBACK (eos_cb), 
		    app);

  clutter_media_set_playing (CLUTTER_MEDIA (app->audio), TRUE);

  clutter_actor_show_all (app->control);
  clutter_actor_hide (app->control_play);
  clutter_actor_show (app->control_pause);

  clutter_actor_show (stage);
  clutter_main();

  return 0;
}
