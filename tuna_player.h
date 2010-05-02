/* Andreas Tunek 2008 */

#ifndef _TUNA_PLAYER
#define _TUNA_PLAYER

#include <glib-object.h>
#include <gst/gst.h>

/* GObject stuff */
G_BEGIN_DECLS

#define TUNA_TYPE_PLAYER tuna_player_get_type()

#define TUNA_PLAYER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TUNA_TYPE_PLAYER, TunaPlayer))

#define TUNA_PLAYER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TUNA_TYPE_PLAYER, TunaPlayerClass))

#define TUNA_IS_PLAYER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TUNA_TYPE_PLAYER))

#define TUNA_IS_PLAYER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TUNA_TYPE_PLAYER))

#define TUNA_PLAYER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TUNA_TYPE_PLAYER, TunaPlayerClass))

typedef struct {
  GObject parent;

  /*don't access these directly*/
  GstFormat time_format;
  gint64 stream_length;
  GstElement *player;
} TunaPlayer;

typedef struct {
  GObjectClass parent_class;

  gint playing_signal_id;
} TunaPlayerClass;

//needed for all GObjects
GType tuna_player_get_type (void);

TunaPlayer* tuna_player_new (void);

/* Set the whole file path and name, eg "/home/tuna/movie.avi" */
gboolean tuna_player_set_filename(TunaPlayer*,
				  const gchar*);

gboolean tuna_player_play(TunaPlayer*);
gboolean tuna_player_pause(TunaPlayer*);

gboolean tuna_player_seek_relative(TunaPlayer*,
				   gint64);

/* Go to somewhere between 0 and 1 in the file
no error checking */
gboolean tuna_player_seek_absolute(TunaPlayer*,
				   gdouble);

/* Returns the fractional position of the playing file 
   make sure you have a file set*/
gdouble tuna_player_get_current_pos(TunaPlayer*);

G_END_DECLS

#endif /* _TUNA_PLAYER */
