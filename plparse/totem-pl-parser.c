/*
   Copyright (C) 2002, 2003, 2004, 2005, 2006 Bastien Nocera
   Copyright (C) 2003, 2004 Colin Walters <walters@rhythmbox.org>

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301  USA.

   Authors: Bastien Nocera <hadess@hadess.net>
 */

/**
 * SECTION:totem-pl-parser
 * @short_description: playlist parser
 * @stability: Stable
 * @include: totem-pl-parser.h
 *
 * #TotemPlParser is a general-purpose playlist parser and writer, with
 * support for several different types of playlist. Note that totem-pl-parser requires a main loop
 * to operate properly (e.g. for the #TotemPlParser::entry-parsed signal to be emitted).
 *
 * <example>
 *  <title>Reading a Playlist</title>
 *  <programlisting>
 * TotemPlParser *pl = totem_pl_parser_new ();
 * g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);
 * g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), NULL);
 * g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), NULL);
 *
 * if (totem_pl_parser_parse (pl, "http://example.com/playlist.pls", FALSE) != TOTEM_PL_PARSER_RESULT_SUCCESS)
 * 	g_error ("Playlist parsing failed.");
 *
 * g_object_unref (pl);
 *  </programlisting>
 * </example>
 *
 * <example>
 *  <title>Reading a Playlist Asynchronously</title>
 *  <programlisting>
 * TotemPlParser *pl = totem_pl_parser_new ();
 * g_object_set (pl, "recurse", FALSE, "disable-unsafe", TRUE, NULL);
 * g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), NULL);
 * g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), NULL);
 *
 * totem_pl_parser_parse_async (pl, "http://example.com/playlist.pls", FALSE, NULL, parse_cb, NULL);
 * g_object_unref (pl);
 *
 * static void
 * parse_cb (TotemPlParser *parser, GAsyncResult *result, gpointer user_data)
 * {
 *	GError *error = NULL;
 * 	if (totem_pl_parser_parse_finish (parser, result, &error) != TOTEM_PL_PARSER_RESULT_SUCCESS) {
 * 		g_error ("Playlist parsing failed: %s", error->message);
 * 		g_error_free (error);
 * 	}
 * }
 *  </programlisting>
 * </example>
 *
 * <example>
 *  <title>Getting Metadata from Entries</title>
 *  <programlisting>
 * static void
 * entry_parsed (TotemPlParser *parser, const gchar *uri, GHashTable *metadata, gpointer user_data)
 * {
 * 	gchar *title = g_hash_table_lookup (metadata, TOTEM_PL_PARSER_FIELD_TITLE);
 * 	if (title != NULL)
 * 		g_message ("Entry title: %s", title);
 * 	else
 * 		g_message ("Entry (URI: %s) has no title.", uri);
 * }
 *  </programlisting>
 * </example>
 *
 *
 * <example>
 *  <title>Writing a Playlist</title>
 *  <programlisting>
 * {
 * 	TotemPlParser *pl;
 * 	TotemPlPlaylist *playlist;
 * 	TotemPlPlaylistIter iter;
 * 	GFile *file;
 *
 * 	pl = totem_pl_parser_new ();
 * 	playlist = totem_pl_playlist_new ();
 * 	file = g_file_new_for_path ("/tmp/playlist.pls");
 *
 * 	totem_pl_playlist_append (playlist, &iter);
 * 	totem_pl_playlist_set (playlist, &iter,
 * 			       TOTEM_PL_PARSER_FIELD_URI, "file:///1.ogg",
 * 			       TOTEM_PL_PARSER_FIELD_TITLE, "1.ogg",
 * 			       NULL);
 *
 * 	totem_pl_playlist_append (playlist, &iter);
 * 	totem_pl_playlist_set (playlist, &iter,
 * 			       TOTEM_PL_PARSER_FIELD_URI, "file:///2.ogg",
 * 			       NULL);
 *
 * 	if (totem_pl_parser_save (pl, playlist, file, "Title",
 * 				  TOTEM_PL_PARSER_PLS, NULL) != TRUE) {
 * 		g_error ("Playlist writing failed.");
 * 	}
 *
 * 	g_object_unref (playlist);
 * 	g_object_unref (pl);
 * 	g_object_unref (file);
 * }
 *  </programlisting>
 * </example>
 **/

#include "config.h"

#include <string.h>
#include <fnmatch.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#ifndef TOTEM_PL_PARSER_MINI
#include <gobject/gvaluecollector.h>
#ifdef HAVE_UCHARDET
#include <uchardet.h>
#endif

#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-decode-date.h"
#include "totem-pl-parser-wm.h"
#include "totem-pl-parser-qt.h"
#include "totem-pl-parser-pls.h"
#include "totem-pl-parser-xspf.h"
#include "totem-pl-parser-media.h"
#include "totem-pl-parser-smil.h"
#include "totem-pl-parser-pla.h"
#include "totem-pl-parser-podcast.h"
#include "totem-pl-parser-lines.h"
#include "totem-pl-parser-misc.h"
#include "totem-pl-parser-private.h"
#include "totem-pl-parser-videosite.h"
#include "totem-pl-parser-amz.h"

#define READ_CHUNK_SIZE 8192
#define RECURSE_LEVEL_MAX 4
#define ILLEGAL_CONTEXT_LENGTH 20

#define D(x) if (debug) x

typedef const char * (*PlaylistIdenCallback) (const char *data, gsize len);

#ifndef TOTEM_PL_PARSER_MINI
typedef TotemPlParserResult (*PlaylistCallback) (TotemPlParser *parser, GFile *uri, GFile *base_file, TotemPlParseData *parse_data, gpointer data);
#endif

typedef struct {
	const char *mimetype;
#ifndef TOTEM_PL_PARSER_MINI
	PlaylistCallback func;
#endif
	PlaylistIdenCallback iden;
#ifndef TOTEM_PL_PARSER_MINI
	guint unsafe : 1;
#endif
} PlaylistTypes;

#ifndef TOTEM_PL_PARSER_MINI
#define PLAYLIST_TYPE(mime,cb,identcb,unsafe) { mime, cb, identcb, unsafe }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, cb, identcb }
#define PLAYLIST_TYPE3(mime) { mime, NULL, NULL, FALSE }
#else
#define PLAYLIST_TYPE(mime,cb,identcb,unsafe) { mime }
#define PLAYLIST_TYPE2(mime,cb,identcb) { mime, identcb }
#define PLAYLIST_TYPE3(mime) { mime }
#endif

/* These ones need a special treatment, mostly parser formats */
static PlaylistTypes special_types[] = {
	PLAYLIST_TYPE ("audio/x-mpegurl", totem_pl_parser_add_m3u, NULL, FALSE),
	PLAYLIST_TYPE ("video/vnd.mpegurl", totem_pl_parser_add_m4u, NULL, FALSE),
	PLAYLIST_TYPE ("audio/playlist", totem_pl_parser_add_m3u, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-scpls", totem_pl_parser_add_pls, NULL, FALSE),
	PLAYLIST_TYPE ("application/x-smil", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("application/smil", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("application/smil+xml", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("application/vnd.ms-wpl", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("video/x-ms-wvx", totem_pl_parser_add_asx, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-ms-wax", totem_pl_parser_add_asx, NULL, FALSE),
	PLAYLIST_TYPE ("application/xspf+xml", totem_pl_parser_add_xspf, NULL, FALSE),
	PLAYLIST_TYPE ("text/uri-list", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list, FALSE),
	PLAYLIST_TYPE ("text/x-google-video-pointer", totem_pl_parser_add_gvp, NULL, FALSE),
	PLAYLIST_TYPE ("text/google-video-pointer", totem_pl_parser_add_gvp, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-iriver-pla", totem_pl_parser_add_pla, NULL, FALSE),
	PLAYLIST_TYPE ("application/atom+xml", totem_pl_parser_add_atom, NULL, FALSE),
	PLAYLIST_TYPE ("application/rss+xml", totem_pl_parser_add_rss, totem_pl_parser_is_rss, FALSE),
	PLAYLIST_TYPE ("text/x-opml+xml", totem_pl_parser_add_opml, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-amzxml", totem_pl_parser_add_amz, NULL, FALSE),
#ifndef TOTEM_PL_PARSER_MINI
	PLAYLIST_TYPE ("application/x-desktop", totem_pl_parser_add_desktop, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-gnome-app-info", totem_pl_parser_add_desktop, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-cd-image", totem_pl_parser_add_iso, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-extension-img", totem_pl_parser_add_iso, NULL, TRUE),
	PLAYLIST_TYPE ("application/x-cue", totem_pl_parser_add_cue, NULL, TRUE),
	PLAYLIST_TYPE (DIR_MIME_TYPE, totem_pl_parser_add_directory, NULL, TRUE),
	PLAYLIST_TYPE (BLOCK_DEVICE_TYPE, totem_pl_parser_add_block, NULL, TRUE),
#endif
};

/* These ones are "dual" types, might be a video, might be a parser
 * Please keep the same _is_ functions together */
static PlaylistTypes dual_types[] = {
	PLAYLIST_TYPE2 ("audio/x-real-audio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("application/ram", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("application/vnd.rn-realmedia", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-pn-realaudio-plugin", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/vnd.rn-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("audio/x-realaudio", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("text/plain", totem_pl_parser_add_ra, totem_pl_parser_is_uri_list),
	PLAYLIST_TYPE2 ("application/x-php", NULL, NULL),
	PLAYLIST_TYPE2 ("audio/x-ms-asx", totem_pl_parser_add_asx, totem_pl_parser_is_asx),
	PLAYLIST_TYPE2 ("video/x-ms-asf", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("application/vnd.ms-asf", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/x-ms-wmv", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("audio/x-ms-wma", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/quicktime", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("video/mp4", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/x-quicktime-media-link", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/x-quicktimeplayer", totem_pl_parser_add_quicktime, totem_pl_parser_is_quicktime),
	PLAYLIST_TYPE2 ("application/xml", totem_pl_parser_add_xml_feed, totem_pl_parser_is_xml_feed),
};

static char *totem_pl_parser_mime_type_from_data (gconstpointer data, int len);

#ifndef TOTEM_PL_PARSER_MINI

static void totem_pl_parser_set_property (GObject *object,
					  guint prop_id,
					  const GValue *value,
					  GParamSpec *pspec);
static void totem_pl_parser_get_property (GObject *object,
					  guint prop_id,
					  GValue *value,
					  GParamSpec *pspec);

struct TotemPlParserPrivate {
	GHashTable *ignore_schemes; /* key = char *, value = boolean */
	GHashTable *ignore_mimetypes; /*key = char *, value = boolean */
	GHashTable *ignore_globs; /*key = char *, value = boolean */
	GMutex ignore_mutex;
	GThread *main_thread; /* see CALL_ASYNC() in *-private.h */

	guint recurse : 1;
	guint debug : 1;
	guint force : 1;
	guint disable_unsafe : 1;
};

enum {
	PROP_NONE,
	PROP_RECURSE,
	PROP_DEBUG,
	PROP_FORCE,
	PROP_DISABLE_UNSAFE
};

/* Signals */
enum {
	ENTRY_PARSED,
	PLAYLIST_STARTED,
	PLAYLIST_ENDED,
	LAST_SIGNAL
};

static int totem_pl_parser_table_signals[LAST_SIGNAL];
static GParamSpecPool *totem_pl_parser_pspec_pool = NULL;

static void totem_pl_parser_class_init (TotemPlParserClass *klass);
static void totem_pl_parser_base_class_finalize	(TotemPlParserClass *klass);
static void totem_pl_parser_init       (TotemPlParser *parser);
static void totem_pl_parser_finalize   (GObject *object);

static gpointer totem_pl_parser_parent_class = NULL;

#define TOTEM_PL_PARSER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), TOTEM_TYPE_PL_PARSER, TotemPlParserPrivate))

GType
totem_pl_parser_get_type (void)
{
	static gsize g_define_type_id__volatile = 0;
	if (g_once_init_enter (&g_define_type_id__volatile))
	{
		const GTypeInfo g_define_type_info = {
			sizeof (TotemPlParserClass),
			NULL,
			(GBaseFinalizeFunc) totem_pl_parser_base_class_finalize,
			(GClassInitFunc) totem_pl_parser_class_init,
			NULL,
			NULL,
			sizeof (TotemPlParser),
			0,
			(GInstanceInitFunc) totem_pl_parser_init,
		};
		GType g_define_type_id = g_type_register_static (G_TYPE_OBJECT, "TotemPlParser", &g_define_type_info, 0);
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
	}
	return g_define_type_id__volatile;
}

static void
totem_pl_parser_class_init (TotemPlParserClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	totem_pl_parser_parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = totem_pl_parser_finalize;
	object_class->set_property = totem_pl_parser_set_property;
	object_class->get_property = totem_pl_parser_get_property;

	/* Properties */

	/**
	 * TotemPlParser:recurse:
	 *
	 * If %TRUE, the parser will recursively fetch playlists linked to by
	 * the current one.
	 **/
	g_object_class_install_property (object_class,
					 PROP_RECURSE,
					 g_param_spec_boolean ("recurse",
							       "recurse",
							       "Whether or not to process URIs further",
							       TRUE,
							       G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	/**
	 * TotemPlParser:debug:
	 *
	 * If %TRUE, the parser will output debug information.
	 **/
	g_object_class_install_property (object_class,
					 PROP_DEBUG,
					 g_param_spec_boolean ("debug",
							       "debug",
							       "Whether or not to enable debugging output",
							       FALSE,
							       G_PARAM_READWRITE));

	/**
	 * TotemPlParser:force:
	 *
	 * If %TRUE, the parser will attempt to parse a playlist, even if it
	 * appears to be unsupported (usually because of its filename extension).
	 **/
	g_object_class_install_property (object_class,
					 PROP_FORCE,
					 g_param_spec_boolean ("force",
							       "force",
							       "Whether or not to force parsing the file if the playlist looks unsupported",
							       FALSE,
							       G_PARAM_READWRITE));

	/**
	 * TotemPlParser:disable-unsafe:
	 *
	 * If %TRUE, the parser will not parse unsafe locations, such as local devices
	 * and local files if the playlist isn't local. This is useful if the library
	 * is parsing a playlist from a remote location such as a website.
	 **/
	g_object_class_install_property (object_class,
					 PROP_DISABLE_UNSAFE,
					 g_param_spec_boolean ("disable-unsafe",
							       "disable-unsafe",
							       "Whether or not to disable parsing of unsafe locations",
							       FALSE,
							       G_PARAM_READWRITE));

	/**
	 * TotemPlParser::entry-parsed:
	 * @parser: the object which received the signal
	 * @uri: the URI of the entry parsed
	 * @metadata: (type GHashTable) (element-type utf8 utf8): a #GHashTable of metadata relating to the entry added
	 *
	 * The ::entry-parsed signal is emitted when a new entry is parsed.
	 */
	totem_pl_parser_table_signals[ENTRY_PARSED] =
		g_signal_new ("entry-parsed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, entry_parsed),
			      NULL, NULL,
			      _totemplparser_marshal_VOID__STRING_BOXED,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_HASH_TABLE);
	/**
	 * TotemPlParser::playlist-started:
	 * @parser: the object which received the signal
	 * @uri: the URI of the new playlist started
	 * @metadata: (type GHashTable) (element-type utf8 utf8): a #GHashTable of metadata relating to the playlist that
	 * started.
	 *
	 * The ::playlist-started signal is emitted when a playlist parsing has
	 * started. This signal isn't emitted for all types of playlists, but
	 * can be relied on to be called for playlists which support playlist
	 * metadata, such as title.
	 */
	totem_pl_parser_table_signals[PLAYLIST_STARTED] =
		g_signal_new ("playlist-started",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, playlist_started),
			      NULL, NULL,
			      _totemplparser_marshal_VOID__STRING_BOXED,
			      G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_HASH_TABLE);
	/**
	 * TotemPlParser::playlist-ended:
	 * @parser: the object which received the signal
	 * @uri: the URI of the playlist that finished parsing.
	 *
	 * The ::playlist-ended signal is emitted when a playlist is finished
	 * parsing. It is only called when #TotemPlParser::playlist-started
	 * has been called for that playlist.
	 */
	totem_pl_parser_table_signals[PLAYLIST_ENDED] =
		g_signal_new ("playlist-ended",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, playlist_ended),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__STRING,
			      G_TYPE_NONE, 1, G_TYPE_STRING);

	/* param specs */
	totem_pl_parser_pspec_pool = g_param_spec_pool_new (FALSE);
	pspec = g_param_spec_string ("url", "url",
				     "URI to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("title", "title",
				     "Title of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("author", "author",
				     "Author of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("genre", "genre",
				     "Primary genre of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("genres", "genres",
				     "Full genre of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("album", "album",
				     "Album of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("base", "base",
				     "Base URI of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("volume", "volume",
				     "Default playback volume (in percents)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("autoplay", "autoplay",
				     "Whether or not to autoplay the stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("duration", "duration",
				     "String representing the duration of the entry", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("duration-ms", "duration-ms",
				     "String representing the duration of the entry in milliseconds", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("starttime", "starttime",
				     "String representing the start time of the stream (initial seek)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("copyright", "copyright",
				     "Copyright of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("abstract", "abstract",
				     "Abstract of the item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("moreinfo", "moreinfo",
				     "URI to get more information for item to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("screensize", "screensize",
				     "String representing the default movie size (double, full or original)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("ui-mode", "ui-mode",
				     "String representing the default UI mode (only compact is supported)", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("endtime", "endtime",
				     "String representing the end time of the stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_boolean ("is-playlist", "is-playlist",
				      "Boolean saying whether the entry pushed is the top-level of a playlist", FALSE,
				      G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("description", "description",
				     "String representing the description of the stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("publication-date", "publication-date",
				     "String representing the publication date of the stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("filesize", "filesize",
				     "String representing the filesize of a file", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("language", "language",
				     "String representing the language of a stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("contact", "contact",
				     "String representing the contact for a playlist", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("image-url", "image-url",
				     "String representing the location of an image for a playlist", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_object ("gfile-object", "gfile-object",
				     "Object representing the GFile for an entry", G_TYPE_FILE,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_object ("gfile-object-base", "gfile-object-base",
				     "Object representing the GFile for base URI of an entry", G_TYPE_FILE,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("download-url", "download-url",
				     "String representing the location of a download URI", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("id", "id",
				     "String representing the identifier for an entry", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("subtitle-uri", "subtitle-uri",
				     "Subtitle URI to be added", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("content-type", "content-type",
				     "Content type for the video stream", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("playing", "playing",
				     "Whether the track is playing", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("audio-track", "audio-track",
				     "The default audio-track to play", NULL,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
	pspec = g_param_spec_string ("content-rating", "content-rating",
				     "String representing the content rating for an entry", TOTEM_PL_PARSER_CONTENT_RATING_UNRATED,
				     G_PARAM_READABLE & G_PARAM_WRITABLE);
	g_param_spec_pool_insert (totem_pl_parser_pspec_pool, pspec, TOTEM_TYPE_PL_PARSER);
}

static void
totem_pl_parser_base_class_finalize (TotemPlParserClass *klass)
{
	GList *list, *node;

	list = g_param_spec_pool_list_owned (totem_pl_parser_pspec_pool, G_OBJECT_CLASS_TYPE (klass));
	for (node = list; node; node = node->next) {
		GParamSpec *pspec = node->data;

		g_param_spec_pool_remove (totem_pl_parser_pspec_pool, pspec);
		g_param_spec_unref (pspec);
	}
	g_list_free (list);
}

static void
totem_pl_parser_set_property (GObject *object,
			      guint prop_id,
			      const GValue *value,
			      GParamSpec *pspec)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	switch (prop_id)
	{
	case PROP_RECURSE:
		parser->priv->recurse = g_value_get_boolean (value) != FALSE;
		break;
	case PROP_DEBUG:
		parser->priv->debug = g_value_get_boolean (value) != FALSE;
		break;
	case PROP_FORCE:
		parser->priv->force = g_value_get_boolean (value) != FALSE;
		break;
	case PROP_DISABLE_UNSAFE:
		parser->priv->disable_unsafe = g_value_get_boolean (value) != FALSE;
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
totem_pl_parser_get_property (GObject *object,
			      guint prop_id,
			      GValue *value,
			      GParamSpec *pspec)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	switch (prop_id)
	{
	case PROP_RECURSE:
		g_value_set_boolean (value, parser->priv->recurse);
		break;
	case PROP_DEBUG:
		g_value_set_boolean (value, parser->priv->debug);
		break;
	case PROP_FORCE:
		g_value_set_boolean (value, parser->priv->force);
		break;
	case PROP_DISABLE_UNSAFE:
		g_value_set_boolean (value, parser->priv->disable_unsafe);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

GQuark
totem_pl_parser_error_quark (void)
{
	static GQuark quark;
	if (!quark)
		quark = g_quark_from_static_string ("totem_pl_parser_error");

	return quark;
}

static gpointer
totem_pl_parser_real_init_i18n (gpointer data)
{
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	return NULL;
}

static void
totem_pl_parser_init_i18n (void)
{
	static GOnce my_once = G_ONCE_INIT;
	g_once (&my_once, totem_pl_parser_real_init_i18n, NULL);
}

/**
 * totem_pl_parser_new:
 *
 * Creates a #TotemPlParser object.
 *
 * Return value: a new #TotemPlParser
 */
TotemPlParser *
totem_pl_parser_new (void)
{
	totem_pl_parser_init_i18n ();
	return TOTEM_PL_PARSER (g_object_new (TOTEM_TYPE_PL_PARSER, NULL));
}

typedef struct {
	TotemPlParser *parser;
	char *playlist_uri;
} PlaylistEndedSignalData;

static gboolean
emit_playlist_ended_signal (PlaylistEndedSignalData *data)
{
	g_signal_emit (data->parser,
		       totem_pl_parser_table_signals[PLAYLIST_ENDED],
		       0, data->playlist_uri);

	/* Free the data */
	g_object_unref (data->parser);
	g_free (data->playlist_uri);
	g_free (data);

	return FALSE;
}

/**
 * totem_pl_parser_playlist_end:
 * @parser: a #TotemPlParser
 * @playlist_uri: the playlist URI
 *
 * Emits the #TotemPlParser::playlist-ended signal on @parser for
 * the playlist @playlist_uri.
 **/
void
totem_pl_parser_playlist_end (TotemPlParser *parser, const char *playlist_uri)
{
	PlaylistEndedSignalData *data;

	data = g_new (PlaylistEndedSignalData, 1);
	data->parser = g_object_ref (parser);
	data->playlist_uri = g_strdup (playlist_uri);

	CALL_ASYNC (parser, emit_playlist_ended_signal, data);
}

static char *
my_g_file_info_get_mime_type_with_data (GFile *file, gpointer *data, TotemPlParser *parser)
{
	char *buffer;
	gsize bytes_read;
	GFileInputStream *stream;
	GError *error = NULL;

	*data = NULL;

#ifndef _WIN32
	/* Stat for a block device, we're screwed as far as speed
	 * is concerned now */
	if (g_file_is_native (file) != FALSE) {
		struct stat buf;
		char *path;

		path = g_file_get_path (file);
		if (g_stat (path, &buf) == 0 && S_ISBLK (buf.st_mode)) {
			g_free (path);
			return g_strdup (BLOCK_DEVICE_TYPE);
		}
		g_free (path);
	}
#endif

	/* Open the file. */
	stream = g_file_read (file, NULL, &error);
	if (stream == NULL) {
		if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_IS_DIRECTORY) != FALSE) {
			g_error_free (error);
			return g_strdup (DIR_MIME_TYPE);
		}
		DEBUG(file, g_print ("URI '%s' couldn't be opened in _get_mime_type_with_data: '%s'\n", uri, error->message));
		g_error_free (error);
		return NULL;
	}
	DEBUG(file, g_print ("URI '%s' was opened successfully in _get_mime_type_with_data\n", uri));

	/* Read the whole thing, up to MIME_READ_CHUNK_SIZE */
	buffer = g_malloc (MIME_READ_CHUNK_SIZE);
	if (g_input_stream_read_all (G_INPUT_STREAM (stream), buffer, MIME_READ_CHUNK_SIZE, &bytes_read, NULL, &error) == FALSE) {
		g_object_unref (stream);
		DEBUG(file, g_print ("Couldn't read data from '%s'\n", uri));
		g_free (buffer);
		return NULL;
	}
	g_object_unref (G_INPUT_STREAM (stream));

	/* Empty file */
	if (bytes_read == 0) {
		g_free (buffer);
		DEBUG(file, g_print ("URI '%s' is empty in _get_mime_type_with_data\n", uri));
		return g_strdup (EMPTY_FILE_TYPE);
	}

	/* Return the file null-terminated. */
	buffer = g_realloc (buffer, bytes_read + 1);
	buffer[bytes_read] = '\0';
	*data = buffer;

	return totem_pl_parser_mime_type_from_data (*data, bytes_read);
}

/**
 * totem_pl_parser_is_debugging_enabled:
 * @parser: a #TotemPlParser
 *
 * Returns whether debugging is enabled. This is a private method, not exposed by the library.
 *
 * Return value: %TRUE if debugging is enabled, %FALSE otherwise
 **/
gboolean
totem_pl_parser_is_debugging_enabled (TotemPlParser *parser)
{
	return parser->priv->debug;
}

gboolean
totem_pl_parser_get_recurse (TotemPlParser *parser)
{
	return parser->priv->recurse;
}

/**
 * totem_pl_parser_base_uri:
 * @uri: a URI
 *
 * Returns the parent URI of @uri.
 *
 * Return value: a newly-allocated string containing @uri's parent URI, or %NULL
 **/
char *
totem_pl_parser_base_uri (GFile *uri)
{
	GFile *parent;
	char *ret;

	parent = g_file_get_parent (uri);
	ret = g_file_get_uri (parent);
	g_object_unref (uri);

	return ret;
}

/**
 * totem_pl_parser_line_is_empty:
 * @line: a playlist line to check
 *
 * Checks to see if the given string line is empty or %NULL,
 * counting tabs and spaces, but not newlines, as "empty".
 *
 * Return value: %TRUE if @line is empty
 **/
gboolean
totem_pl_parser_line_is_empty (const char *line)
{
	guint i;

	if (line == NULL)
		return TRUE;

	for (i = 0; line[i] != '\0'; i++) {
		if (line[i] != '\t' && line[i] != ' ')
			return FALSE;
	}
	return TRUE;
}

/**
 * totem_pl_parser_write_string:
 * @handle: a #GFileOutputStream to an open file
 * @buf: the string buffer to write out
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Writes the string @buf out to the file specified by @handle.
 * Possible error codes are as per totem_pl_parser_write_buffer().
 *
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write_string (GOutputStream  *stream,
			      const char     *buf,
			      GCancellable   *cancellable,
			      GError        **error)
{
	guint len;

	len = strlen (buf);
	return totem_pl_parser_write_buffer (stream, buf, len, cancellable, error);
}

/**
 * totem_pl_parser_write_buffer:
 * @stream: a #GFileOutputStream to an open file
 * @buf: the string buffer to write out
 * @len: the length of the string to write out
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @error: return location for a #GError, or %NULL
 *
 * Writes @len bytes of @buf to the file specified by @handle.
 *
 * A value of @len greater than #G_MAXSSIZE will cause a #G_IO_ERROR_INVALID_ARGUMENT argument.
 *
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write_buffer (GOutputStream  *stream,
			      const char     *buf,
			      guint           len,
			      GCancellable   *cancellable,
			      GError        **error)
{
	gsize bytes_written;

	if (g_output_stream_write_all (stream,
				       buf, len,
				       &bytes_written,
				       cancellable, error) == FALSE) {
		g_object_unref (stream);
		return FALSE;
	}

	return TRUE;
}

/**
 * totem_pl_parser_num_entries:
 * @parser: a #TotemPlParser
 * @playlist: a #TotemPlPlaylist
 *
 * Returns the number of valid entries in @playlist.
 *
 * Return value: the number of entries in the playlist
 **/
int
totem_pl_parser_num_entries (TotemPlParser   *parser,
                             TotemPlPlaylist *playlist)
{
	int num_entries, ignored;
        TotemPlPlaylistIter iter;
        gboolean valid;

	num_entries = totem_pl_playlist_size (playlist);
        valid = totem_pl_playlist_iter_first (playlist, &iter);
	ignored = 0;

        while (valid) {
                gchar *uri;
                GFile *file;

                totem_pl_playlist_get (playlist, &iter,
                                       TOTEM_PL_PARSER_FIELD_URI, &uri,
                                       NULL);

                valid = totem_pl_playlist_iter_next (playlist, &iter);

                if (!uri) {
                        ignored++;
                        continue;
                }

                file = g_file_new_for_uri (uri);

                if (totem_pl_parser_scheme_is_ignored (parser, file)) {
                        ignored++;
                }

                g_object_unref (file);
                g_free (uri);
        }

	return num_entries - ignored;
}

char *
totem_pl_parser_relative (GFile *output, const char *filepath)
{
	GFile *parent, *file;
	char *retval;

	parent = g_file_get_parent (output);
	file = g_file_new_for_commandline_arg (filepath);

	retval = g_file_get_relative_path (parent, file);

	g_object_unref (parent);
	g_object_unref (file);

	return retval;
}

static char *
relative_uri_remove_query (const char *uri, char **query)
{
	char *qmark;

	/* Look for '?' */
	qmark = strrchr (uri, '?');
	if (qmark == NULL)
		return NULL;

	if (query != NULL)
		*query = g_strdup (qmark);
	return g_strndup (uri, qmark - uri);
}

static const char *suffixes[] = {
	".jsp",
	".php",
	".asp"
};

static gboolean
is_probably_dir (const char *filename)
{
	gboolean ret;
	char *content_type, *short_name;

	short_name = relative_uri_remove_query (filename, NULL);
	if (short_name == NULL)
		short_name = g_strdup (filename);
	content_type = g_content_type_guess (short_name, NULL, 0, NULL);
	if (g_content_type_is_unknown (content_type) != FALSE) {
		guint i;
		for (i = 0; i < G_N_ELEMENTS (suffixes); i++) {
			if (g_str_has_suffix (short_name, suffixes[i]) != FALSE) {
				g_free (content_type);
				g_free (short_name);
				return FALSE;
			}
		}
		ret = TRUE;
	} else {
		ret = FALSE;
	}
	g_free (content_type);
	g_free (short_name);

	return ret;
}

char *
totem_pl_parser_resolve_uri (GFile *base_gfile,
			     const char *relative_uri)
{
	char *uri, *scheme, *query, *new_relative_uri, *base_uri;
	GFile *base_parent_gfile, *resolved_gfile;

	if (relative_uri == NULL) {
		if (base_gfile == NULL)
			return NULL;
		return g_file_get_uri (base_gfile);
	}

	if (base_gfile == NULL)
		return g_strdup (relative_uri);

	/* If |relative_uri| has a scheme, it's a full URI, just return it */
	scheme = g_uri_parse_scheme (relative_uri);
	if (scheme != NULL) {
		g_free (scheme);
		return g_strdup (relative_uri);
	}

	/* Check whether we need to get the parent for the base or not */
	base_uri = g_file_get_path (base_gfile);
	if (base_uri == NULL)
		base_uri = g_file_get_uri (base_gfile);
	if (is_probably_dir (base_uri) == FALSE)
		base_parent_gfile = g_file_get_parent (base_gfile);
	else
		base_parent_gfile = g_object_ref (base_gfile);
	g_free (base_uri);

	if (base_parent_gfile == NULL) {
		resolved_gfile = g_file_resolve_relative_path (base_gfile, relative_uri);
		uri = g_file_get_uri (resolved_gfile);
		g_object_unref (resolved_gfile);
		return uri;
	}

	/* Remove the query portion of the URI, to transplant it again
	 * if there is any */
	query = NULL;
	new_relative_uri = relative_uri_remove_query (relative_uri, &query);

	if (new_relative_uri) {
		char *tmpuri;

		resolved_gfile = g_file_resolve_relative_path (base_parent_gfile, new_relative_uri);
		g_object_unref (base_parent_gfile);
		if (!resolved_gfile) {
			base_uri = g_file_get_uri (base_gfile);
			g_warning ("Failed to resolve relative URI '%s' against base '%s'\n", relative_uri, base_uri);
			g_free (base_uri);
			g_free (new_relative_uri);
			g_free (query);
			return NULL;
		}

		tmpuri = g_file_get_uri (resolved_gfile);
		g_object_unref (resolved_gfile);
		uri = g_strdup_printf ("%s%s", tmpuri, query);

		g_free (tmpuri);
		g_free (new_relative_uri);
		g_free (query);

		return uri;
	} else {
		resolved_gfile = g_file_resolve_relative_path (base_parent_gfile, relative_uri);
		g_object_unref (base_parent_gfile);
		if (!resolved_gfile) {
			base_uri = g_file_get_uri (base_gfile);
			g_warning ("Failed to resolve relative URI '%s' against base '%s'\n", relative_uri, base_uri);
			g_free (base_uri);
			return NULL;
		}

		uri = g_file_get_uri (resolved_gfile);
		g_object_unref (resolved_gfile);

		return uri;
	}
}

#ifndef TOTEM_PL_PARSER_MINI
typedef struct {
	TotemPlPlaylist   *playlist;
	GFile             *dest;
	char              *title;
	TotemPlParserType  type;
} PlParserSaveData;

static void
pl_parser_save_data_free (PlParserSaveData *data)
{
	g_clear_object (&data->playlist);
	g_clear_object (&data->dest);
	g_clear_pointer (&data->title, g_free);
	g_free (data);
}

static void
pl_parser_save_thread (GTask        *task,
		       gpointer      source_object,
		       gpointer      task_data,
		       GCancellable *cancellable)
{
	PlParserSaveData *data = task_data;
	GError *error = NULL;
	gboolean ret = FALSE;

	switch (data->type) {
	case TOTEM_PL_PARSER_PLS:
		ret = totem_pl_parser_save_pls (source_object,
						data->playlist,
						data->dest,
						data->title,
						cancellable,
						&error);
		break;
	case TOTEM_PL_PARSER_M3U:
	case TOTEM_PL_PARSER_M3U_DOS:
		ret = totem_pl_parser_save_m3u (source_object,
						data->playlist,
						data->dest,
						(data->type == TOTEM_PL_PARSER_M3U_DOS),
						cancellable,
						&error);
		break;
	case TOTEM_PL_PARSER_XSPF:
		ret = totem_pl_parser_save_xspf (source_object,
						 data->playlist,
						 data->dest,
						 data->title,
						 cancellable,
						 &error);
		break;
	case TOTEM_PL_PARSER_IRIVER_PLA:
		ret = totem_pl_parser_save_pla (source_object,
						data->playlist,
						data->dest,
						data->title,
						cancellable,
						&error);
		break;
	default:
		g_assert_not_reached ();
	}

	if (ret == FALSE)
		g_task_return_error (task, error);
	else
		g_task_return_boolean (task, TRUE);
}

static gboolean
pl_parser_save_check_size (TotemPlPlaylist *playlist,
			   GTask           *task)
{
	if (totem_pl_playlist_size (playlist) > 0)
		return TRUE;

	/* FIXME add translation */
	g_task_return_new_error (task,
				 TOTEM_PL_PARSER_ERROR,
				 TOTEM_PL_PARSER_ERROR_EMPTY_PLAYLIST,
				 "Playlist selected for saving is empty");
	g_object_unref (task);
	return FALSE;
}

/**
 * totem_pl_parser_save:
 * @parser: a #TotemPlParser
 * @playlist: a #TotemPlPlaylist
 * @dest: output #GFile
 * @title: the playlist title
 * @type: a #TotemPlParserType for the outputted playlist
 * @error: return location for a #GError, or %NULL
 *
 * Writes the playlist held by @parser and @playlist out to the path
 * pointed by @dest. The playlist is written in the format @type and is
 * given the title @title.
 *
 * If the @output file is a directory the #G_IO_ERROR_IS_DIRECTORY error
 * will be returned, and if the file is some other form of non-regular file
 * then a #G_IO_ERROR_NOT_REGULAR_FILE error will be returned. Some file
 * systems don't allow all file names, and may return a
 * #G_IO_ERROR_INVALID_FILENAME error, and if the name is too long,
 * #G_IO_ERROR_FILENAME_TOO_LONG will be returned. Other errors are possible
 * too, and depend on what kind of filesystem the file is on.
 *
 * In extreme cases, a #G_IO_ERROR_INVALID_ARGUMENT error can be returned, if
 * parts of the playlist to be written are too long.
 *
 * If writing a PLA playlist and there is an error converting a URI's encoding,
 * a code from #GConvertError will be returned.
 *
 * Returns: %TRUE on success
 **/
gboolean
totem_pl_parser_save (TotemPlParser      *parser,
                      TotemPlPlaylist    *playlist,
                      GFile              *dest,
                      const gchar        *title,
                      TotemPlParserType   type,
                      GError            **error)
{
	GTask *task;
	PlParserSaveData *data;

	g_return_val_if_fail (TOTEM_PL_IS_PARSER (parser), FALSE);
	g_return_val_if_fail (TOTEM_PL_IS_PLAYLIST (playlist), FALSE);
	g_return_val_if_fail (G_IS_FILE (dest), FALSE);

	task = g_task_new (parser, NULL, NULL, NULL);
	if (!pl_parser_save_check_size (playlist, task))
		return g_task_propagate_boolean (task, error);

	data = g_new0 (PlParserSaveData, 1);
	data->playlist = g_object_ref (playlist);
	data->dest = g_object_ref (dest);
	data->title = g_strdup (title);
	data->type = type;

	g_task_set_task_data (task, data, (GDestroyNotify) pl_parser_save_data_free);
	g_task_run_in_thread_sync (task, pl_parser_save_thread);

	return g_task_propagate_boolean (task, error);
}

/**
 * totem_pl_parser_save_async:
 * @parser: a #TotemPlParser
 * @playlist: a #TotemPlPlaylist
 * @dest: output #GFile
 * @title: the playlist title
 * @type: a #TotemPlParserType for the outputted playlist
 * @cancellable: (allow-none): a #GCancellable, or %NULL
 * @callback: (allow-none): a #GAsyncReadyCallback to call when saving has finished
 * @user_data: data to pass to the @callback function
 *
 * Starts asynchronous version of totem_pl_parser_save(). For more details
 * see totem_pl_parser_save().
 *
 * When the operation is finished, @callback will be called. You can then call
 * totem_pl_parser_save_finish() to get the results of the operation.
 **/
void
totem_pl_parser_save_async (TotemPlParser        *parser,
			    TotemPlPlaylist      *playlist,
			    GFile                *dest,
			    const gchar          *title,
			    TotemPlParserType     type,
			    GCancellable         *cancellable,
			    GAsyncReadyCallback   callback,
			    gpointer              user_data)
{
	GTask *task;
	PlParserSaveData *data;

	g_return_if_fail (TOTEM_PL_IS_PARSER (parser));
	g_return_if_fail (TOTEM_PL_IS_PLAYLIST (playlist));
	g_return_if_fail (G_IS_FILE (dest));

	task = g_task_new (parser, cancellable, callback, user_data);
	if (!pl_parser_save_check_size (playlist, task))
		return;

	data = g_new0 (PlParserSaveData, 1);
	data->playlist = g_object_ref (playlist);
	data->dest = g_object_ref (dest);
	data->title = g_strdup (title);
	data->type = type;

	g_task_set_task_data (task, data, (GDestroyNotify) pl_parser_save_data_free);
	g_task_run_in_thread (task, pl_parser_save_thread);
}

/**
 * totem_pl_parser_save_finish:
 * @parser: a #TotemPlParser
 * @async_result: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous playlist saving operation started with totem_pl_parser_save_async().
 *
 * If saving of the playlist is cancelled part-way through, %G_IO_ERROR_CANCELLED will be
 * returned when this function is called.
 *
 * Return value: %TRUE on success, %FALSE on failure.
 **/
gboolean
totem_pl_parser_save_finish (TotemPlParser   *parser,
			     GAsyncResult    *async_result,
			     GError         **error)
{
	g_return_val_if_fail (g_task_is_valid (async_result, parser), FALSE);

	return g_task_propagate_boolean (G_TASK (async_result), error);
}
#endif /* TOTEM_PL_PARSER_MINI */

/**
 * totem_pl_parser_read_ini_line_int:
 * @lines: a NULL-terminated array of INI lines to read
 * @key: the key to match
 *
 * Returns the first integer value case-insensitively matching the specified
 * key as an integer. The parser ignores leading whitespace on lines.
 *
 * Return value: the integer value, or -1 on error
 **/
int
totem_pl_parser_read_ini_line_int (char **lines, const char *key)
{
	int retval = -1;
	int i;

	if (lines == NULL || key == NULL)
		return -1;

	for (i = 0; (lines[i] != NULL && retval == -1); i++) {
		char *line = lines[i];

		while (*line == '\t' || *line == ' ')
			line++;

		if (g_ascii_strncasecmp (line, key, strlen (key)) == 0) {
			char **bits;

			bits = g_strsplit (line, "=", 2);
			if (bits[0] == NULL || bits [1] == NULL) {
				g_strfreev (bits);
				return -1;
			}

			retval = (gint) g_strtod (bits[1], NULL);
			g_strfreev (bits);
		}
	}

	return retval;
}

/**
 * totem_pl_parser_read_ini_line_string_with_sep:
 * @lines: a NULL-terminated array of INI lines to read
 * @key: the key to match
 * @sep: the key-value separator
 *
 * Returns the first string value case-insensitively matching the specified
 * key, where the two are separated by @sep. The parser ignores leading whitespace
 * on lines.
 *
 * Return value: a newly-allocated string value, or %NULL
 **/
char*
totem_pl_parser_read_ini_line_string_with_sep (char **lines, const char *key,
		const char *sep)
{
	char *retval = NULL;
	int i;

	if (lines == NULL || key == NULL)
		return NULL;

	for (i = 0; (lines[i] != NULL && retval == NULL); i++) {
		char *line = lines[i];

		while (*line == '\t' || *line == ' ')
			line++;

		if (g_ascii_strncasecmp (line, key, strlen (key)) == 0) {
			char **bits;

			bits = g_strsplit (line, sep, 2);
			if (bits[0] == NULL || bits [1] == NULL) {
				g_strfreev (bits);
				return NULL;
			}

			retval = g_strdup (bits[1]);
			g_strfreev (bits);
		}
	}

	return retval;
}

/**
 * totem_pl_parser_read_ini_line_string:
 * @lines: a NULL-terminated array of INI lines to read
 * @key: the key to match
 *
 * Returns the first string value case-insensitively matching the
 * specified key. The parser ignores leading whitespace on lines.
 *
 * Return value: a newly-allocated string value, or %NULL
 **/
char*
totem_pl_parser_read_ini_line_string (char **lines, const char *key)
{
	return totem_pl_parser_read_ini_line_string_with_sep (lines, key, "=");
}

static void
totem_pl_parser_init (TotemPlParser *parser)
{
	parser->priv = g_new0 (TotemPlParserPrivate, 1);
	parser->priv->main_thread = g_thread_self ();
	g_mutex_init (&parser->priv->ignore_mutex);
	parser->priv->ignore_schemes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	parser->priv->ignore_mimetypes = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	parser->priv->ignore_globs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
totem_pl_parser_finalize (GObject *object)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);
	TotemPlParserPrivate *priv = parser->priv;

	g_clear_pointer (&priv->ignore_schemes, g_hash_table_destroy);
	g_clear_pointer (&priv->ignore_mimetypes, g_hash_table_destroy);
	g_clear_pointer (&priv->ignore_globs, g_hash_table_destroy);
	g_mutex_clear (&priv->ignore_mutex);
	g_clear_pointer (&parser->priv, g_free);

	G_OBJECT_CLASS (totem_pl_parser_parent_class)->finalize (object);
}

typedef struct {
	TotemPlParser *parser;
	guint signal_id;
	char *uri;
	GHashTable *metadata;
} EntryParsedSignalData;

static gboolean
emit_entry_parsed_signal (EntryParsedSignalData *data)
{
	g_signal_emit (data->parser, data->signal_id, 0, data->uri, data->metadata);

	/* Free the data */
	g_object_unref (data->parser);
	g_free (data->uri);
	g_hash_table_unref (data->metadata);
	g_free (data);

	return FALSE;
}

gboolean
totem_pl_parser_fix_string (const char  *name,
			    const char  *value,
			    char       **ret)
{
	char *fixed = NULL;

	/* Check for UTF-8 or ISO8859-1 string */
	if (g_utf8_validate (value, -1, NULL) == FALSE) {
		fixed = g_convert (value, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
		if (fixed == NULL) {
			g_warning ("Ignored non-UTF-8 and non-ISO8859-1 string for field '%s'", name);
			return FALSE;
		}
	}

	/* Remove trailing spaces from titles */
	if (g_str_equal (name, TOTEM_PL_PARSER_FIELD_TITLE)) {
		if (fixed == NULL)
			fixed = g_strchomp (g_strdup (value));
		else
			g_strchomp (fixed);
	}

	*ret = fixed;

	return TRUE;
}

void
totem_pl_parser_add_hash_table (TotemPlParser *parser,
				GHashTable    *metadata,
				const char    *uri,
				gboolean       is_playlist)
{
	if (g_hash_table_size (metadata) > 0 || uri != NULL) {
		EntryParsedSignalData *data;

		/* Make sure to emit the signals asynchronously, as we could be in the main loop
		 * *or* a worker thread at this point. */
		data = g_new (EntryParsedSignalData, 1);
		data->parser = g_object_ref (parser);
		data->uri = g_strdup (uri);
		data->metadata = g_hash_table_ref (metadata);

		if (is_playlist == FALSE)
			data->signal_id = totem_pl_parser_table_signals[ENTRY_PARSED];
		else
			data->signal_id = totem_pl_parser_table_signals[PLAYLIST_STARTED];

		CALL_ASYNC (parser, emit_entry_parsed_signal, data);
	}
}

static void
totem_pl_parser_add_uri_valist (TotemPlParser *parser,
				const gchar *first_property_name,
				va_list      var_args)
{
	const char *name;
	char *uri;
	GHashTable *metadata;
	gboolean is_playlist;

	uri = NULL;
	is_playlist = FALSE;

	g_object_ref (G_OBJECT (parser));
	metadata = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	name = first_property_name;

	while (name) {
		GValue value = { 0, };
		GParamSpec *pspec;
		char *error = NULL;
		const char *string;

		pspec = g_param_spec_pool_lookup (totem_pl_parser_pspec_pool,
						  name,
						  G_OBJECT_TYPE (parser),
						  FALSE);

		if (!pspec) {
			g_warning ("Unknown property '%s'", name);
			name = va_arg (var_args, char*);
			continue;
		}

		g_value_init (&value, G_PARAM_SPEC_VALUE_TYPE (pspec));
		G_VALUE_COLLECT (&value, var_args, 0, &error);
		if (error != NULL) {
			g_warning ("Error getting the value for property '%s'", name);
			break;
		}

		if (strcmp (name, TOTEM_PL_PARSER_FIELD_URI) == 0) {
			if (uri == NULL)
				uri = g_value_dup_string (&value);
		} else if (strcmp (name, TOTEM_PL_PARSER_FIELD_FILE) == 0) {
			GFile *file;

			file = g_value_get_object (&value);
			uri = g_file_get_uri (file);

			g_value_unset (&value);
			name = va_arg (var_args, char*);
			continue;
		} else if (strcmp (name, TOTEM_PL_PARSER_FIELD_BASE_FILE) == 0) {
			GFile *file;
			char *base_uri;

			file = g_value_get_object (&value);
			base_uri = g_file_get_uri (file);

			g_hash_table_insert (metadata,
					     g_strdup (TOTEM_PL_PARSER_FIELD_BASE),
					     base_uri);

			g_value_unset (&value);
			name = va_arg (var_args, char*);
			continue;
		} else if (strcmp (name, TOTEM_PL_PARSER_FIELD_IS_PLAYLIST) == 0) {
			is_playlist = g_value_get_boolean (&value);
			g_value_unset (&value);
			name = va_arg (var_args, char*);
			continue;
		}

		/* Ignore empty values */
		string = g_value_get_string (&value);
		if (string != NULL && string[0] != '\0') {
			char *fixed = NULL;

			if (!totem_pl_parser_fix_string (name, string, &fixed)) {
				g_value_unset (&value);
				name = va_arg (var_args, char*);
				continue;
			}

			/* Add other values to the metadata hashtable */
			g_hash_table_insert (metadata,
					     g_strdup (name),
					     fixed ? fixed : g_strdup (string));
		}

		g_value_unset (&value);
		name = va_arg (var_args, char*);
	}

	if (parser->priv->disable_unsafe != FALSE) {
		//FIXME fix this! 396710
	}

	totem_pl_parser_add_hash_table (parser,
					metadata,
					uri,
					is_playlist);

	g_hash_table_unref (metadata);

	g_free (uri);
	g_object_unref (G_OBJECT (parser));
}

/**
 * totem_pl_parser_add_uri:
 * @parser: a #TotemPlParser
 * @first_property_name: the first property name
 * @...: value for the first property, followed optionally by more
 * name/value pairs, followed by %NULL
 *
 * Adds a URI to the playlist with the properties given in @first_property_name
 * and @....
 **/
void
totem_pl_parser_add_uri (TotemPlParser *parser,
			 const char *first_property_name,
			 ...)
{
	va_list var_args;
	va_start (var_args, first_property_name);
	totem_pl_parser_add_uri_valist (parser, first_property_name, var_args);
	va_end (var_args);
}

/**
 * totem_pl_parser_add_one_uri:
 * @parser: a #TotemPlParser
 * @uri: the entry's URI
 * @title: the entry's title
 *
 * Adds a single URI entry with only URI and title strings to the playlist.
 **/
void
totem_pl_parser_add_one_uri (TotemPlParser *parser, const char *uri, const char *title)
{
	totem_pl_parser_add_uri (parser,
				 TOTEM_PL_PARSER_FIELD_URI, uri,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 NULL);
}

void
totem_pl_parser_add_one_file (TotemPlParser *parser, GFile *file, const char *title)
{
	totem_pl_parser_add_uri (parser,
				 TOTEM_PL_PARSER_FIELD_FILE, file,
				 TOTEM_PL_PARSER_FIELD_TITLE, title,
				 NULL);
}

static PlaylistTypes ignore_types[] = {
	PLAYLIST_TYPE3 ("image/*"),
	PLAYLIST_TYPE3 ("text/plain"),
	PLAYLIST_TYPE3 ("application/x-rar"),
	PLAYLIST_TYPE3 ("application/zip"),
	PLAYLIST_TYPE3 ("application/x-trash"),
};

/**
 * totem_pl_parser_scheme_is_ignored:
 * @parser: a #TotemPlParser
 * @uri: a URI
 *
 * Checks to see if @uri's scheme is in the @parser's list of
 * schemes to ignore.
 *
 * Return value: %TRUE if @uri's scheme is ignored
 **/
gboolean
totem_pl_parser_scheme_is_ignored (TotemPlParser *parser, GFile *uri)
{
	char *scheme;
	gboolean ret;

	g_mutex_lock (&parser->priv->ignore_mutex);

	scheme = g_file_get_uri_scheme (uri);
	if (!scheme) {
		g_mutex_unlock (&parser->priv->ignore_mutex);
		return TRUE;
	}
	ret = GPOINTER_TO_INT (g_hash_table_lookup (parser->priv->ignore_schemes, scheme));
	g_free (scheme);

	g_mutex_unlock (&parser->priv->ignore_mutex);

	return ret;
}

static gboolean
totem_pl_parser_mimetype_is_ignored (TotemPlParser *parser,
				     const char *mimetype)
{
	gboolean ret;

	g_mutex_lock (&parser->priv->ignore_mutex);
	ret = GPOINTER_TO_INT (g_hash_table_lookup (parser->priv->ignore_mimetypes, mimetype));
	g_mutex_unlock (&parser->priv->ignore_mutex);

	return ret;
}

static gboolean
totem_pl_parser_glob_is_ignored (TotemPlParser *parser,
				 const char *filename)
{
	GHashTableIter iter;
	gpointer key;
	int ret = FNM_NOMATCH;

	g_mutex_lock (&parser->priv->ignore_mutex);
	g_hash_table_iter_init (&iter, parser->priv->ignore_globs);
	while (g_hash_table_iter_next (&iter, &key, NULL)) {
		const char *glob = key;

		ret = fnmatch (glob, filename, 0);
		if (ret == 0)
			break;
	}
	g_mutex_unlock (&parser->priv->ignore_mutex);

	return (ret == 0);
}

/**
 * totem_pl_parser_ignore:
 * @parser: a #TotemPlParser
 * @uri: a URI
 *
 * Checks if the URI should be ignored. URIs are <emphasis>not</emphasis> ignored if:
 * <itemizedlist>
 *  <listitem><para>they have an unknown mimetype,</para></listitem>
 *  <listitem><para>they have a special mimetype,</para></listitem>
 *  <listitem><para>they have a mimetype which could be a video or a playlist.</para></listitem>
 * </itemizedlist>
 *
 * URIs are automatically ignored if their scheme is ignored as per totem_pl_parser_scheme_is_ignored(),
 * and are ignored if all the other tests are inconclusive.
 *
 * Return value: %TRUE if @uri is to be ignored
 **/
gboolean
totem_pl_parser_ignore (TotemPlParser *parser, const char *uri)
{
	g_autofree char *mimetype = NULL;
	g_autoptr(GFile) file = NULL;
	guint i;

	if (totem_pl_parser_glob_is_ignored (parser, uri) != FALSE)
		return TRUE;

	file = g_file_new_for_path (uri);
	if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE)
		return TRUE;

	//FIXME wrong for win32
	mimetype = g_content_type_guess (uri, NULL, 0, NULL);
	if (mimetype == NULL || strcmp (mimetype, UNKNOWN_TYPE) == 0)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0)
			return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS (dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0)
			return FALSE;
	}

	return TRUE;
}

/**
 * totem_pl_parser_cleanup_xml:
 * @contents: the contents of the file
 *
 * Removes HTML comments from a string representing the contents of an XML file.
 * The function modifies the string in place.
 */
static void
totem_pl_parser_cleanup_xml (char *contents)
{
	char *needle;

	needle = contents;
	while ((needle = strstr (needle, "<!--")) != NULL) {
		char *end;

		/* Find end of comments */
		end = strstr (needle, "-->");
		/* Broken file? */
		if (end == NULL)
			return;
		if (g_strstr_len (needle, end - needle, "]]>") != NULL) {
			/* Advance 3 and skip */
			needle += 3;
			continue;
		}
		/* Empty the comment */
		memset (needle, ' ', end + 3 - needle);
	}
}

#ifdef HAVE_UCHARDET
static char *
guess_text_encoding (const char *text,
		     gsize       len)
{
	uchardet_t handle;
	char *encoding = NULL;
	int ret;

	handle = uchardet_new ();
	ret = uchardet_handle_data (handle, text, len);
	if (ret == 0) {
		uchardet_data_end (handle);
		encoding = g_strdup (uchardet_get_charset (handle));
	}

	uchardet_delete (handle);
	return encoding;
}
#else
static char *
guess_text_encoding (const char *text,
		     gsize       len)
{
	return NULL;
}
#endif /* HAVE_UCHARDET */

xml_node_t *
totem_pl_parser_parse_xml_relaxed (char *contents,
				   gsize size)
{
	xml_node_t* doc, *node;
	g_autoptr(GError) error = NULL;
	g_autofree char *encoding = NULL;
	g_autofree char *new_contents = NULL;
	gsize new_size, bytes_read;
	xml_parser_t *xml_parser;

	totem_pl_parser_cleanup_xml (contents);
	xml_parser = xml_parser_init_r (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options_r (xml_parser, &doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0) {
		xml_parser_finalize_r (xml_parser);
		return NULL;
	}

	xml_parser_finalize_r (xml_parser);

	encoding = NULL;
	for (node = doc; node != NULL; node = node->next) {
		if (node->name == NULL || g_str_equal (node->name, "?XML") == FALSE)
			continue;
		encoding = g_strdup (xml_parser_get_property (node, "ENCODING"));
		break;
	}

	if (encoding == NULL || g_ascii_strcasecmp (encoding, "UTF-8") == 0) {
		if (g_utf8_validate (contents, -1, NULL))
			return doc;
		g_debug ("Document %s pretended to be in UTF-8 but didn't validate",
			 encoding ? "explicitly" : "implicitly");
		g_free (encoding);
		encoding = guess_text_encoding (contents, size);
		if (!encoding)
			return NULL;

		g_debug ("Guessed text encoding of XML data as '%s'", encoding);
		/* fall-through with the detected encoding */
	}

	xml_parser_free_tree (doc);

	new_contents = g_convert (contents, size, "UTF-8", encoding, &bytes_read, &new_size, &error);
	if (new_contents == NULL) {
		g_autofree char *message = NULL;
		message = g_strdup_printf ("Failed to convert XML data from '%s' to '%s': %s",
					   encoding, "UTF-8", error->message);

		if (error->code == G_CONVERT_ERROR_ILLEGAL_SEQUENCE) {
			int context_length = MIN (bytes_read, ILLEGAL_CONTEXT_LENGTH);

			g_warning ("%s: byte offset %" G_GSIZE_FORMAT ", byte: '%.1s', byte context: '%.*s'",
				   message, bytes_read, contents + bytes_read,
				   context_length + 1,
				   contents + bytes_read - context_length);
		} else {
			g_warning ("%s", message);
		}

		return NULL;
	}

	xml_parser = xml_parser_init_r (new_contents, new_size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options_r (xml_parser, &doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0) {
		xml_parser_finalize_r (xml_parser);
		return NULL;
	}

	xml_parser_finalize_r (xml_parser);

	return doc;
}

static gboolean
totem_pl_parser_ignore_from_mimetype (TotemPlParser *parser, const char *mimetype)
{
	guint i;

	if (mimetype == NULL)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (ignore_types); i++) {
		/* Up until we have a way to detect private inheritance
		 * in shared-mime-info */
		if (strcmp (mimetype, "application/vnd.apple.mpegurl") != 0 &&
		    strcmp (mimetype, "audio/x-mpegurl") != 0 &&
		    strcmp (mimetype, "video/x-mjpeg") != 0 &&
		    g_content_type_is_a (mimetype, ignore_types[i].mimetype) != FALSE) {
			if (parser->priv->debug)
				g_print ("Ignoring %s because it's a %s\n", mimetype, ignore_types[i].mimetype);
			return TRUE;
		}
		if (g_content_type_equals (mimetype, ignore_types[i].mimetype) != FALSE) {
			if (parser->priv->debug)
				g_print ("Ignoring %s because it's equal to %s\n", mimetype, ignore_types[i].mimetype);
			return TRUE;
		}
	}

	return FALSE;
}

static PlaylistCallback
totem_pl_parser_get_function_for_mimetype (const char *mimetype)
{
	guint i;

	if (mimetype == NULL)
		return NULL;

	for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0)
			return special_types[i].func;
	}
	for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0)
			return dual_types[i].func;
	}
	return NULL;
}

TotemPlParserResult
totem_pl_parser_parse_internal (TotemPlParser *parser,
				GFile *file,
				GFile *base_file,
				TotemPlParseData *parse_data)
{
	g_autofree char *mimetype = NULL;
	g_autofree gpointer data = NULL;
	g_autofree char *uri = NULL;
	guint i;
	TotemPlParserResult ret = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	gboolean found = FALSE;

	if (parse_data->recurse_level > RECURSE_LEVEL_MAX)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (g_file_has_uri_scheme (file, "mms") != FALSE
			|| g_file_has_uri_scheme (file, "rtsp") != FALSE
			|| g_file_has_uri_scheme (file, "rtmp") != FALSE
			|| g_file_has_uri_scheme (file, "icy") != FALSE
			|| g_file_has_uri_scheme (file, "pnm") != FALSE) {
		DEBUG(file, g_print ("URI '%s' is MMS, RTSP, RTMP, PNM or ICY, not a playlist\n", uri));
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	/* Fix up itpc, see http://www.apple.com/itunes/store/podcaststechspecs.html,
	 * feed:// as used by Firefox 3,
	 * as well as zcast:// as used by ZENCast */
	if (g_file_has_uri_scheme (file, "itpc") != FALSE
	    || g_file_has_uri_scheme (file, "feed") != FALSE
	    || g_file_has_uri_scheme (file, "zcast") != FALSE) {
		DEBUG(file, g_print ("URI '%s' is getting special cased for ITPC/FEED/ZCAST parsing\n", uri));
		return totem_pl_parser_add_itpc (parser, file, base_file, parse_data, NULL);
	}
	if (g_file_has_uri_scheme (file, "zune") != FALSE) {
		DEBUG(file, g_print ("URI '%s' is getting special cased for ZUNE parsing\n", uri));
		return totem_pl_parser_add_zune (parser, file, base_file, parse_data, NULL);
	}
	/* Try itms Podcast references, see itunes.py in PenguinTV */
	if (totem_pl_parser_is_itms_feed (file) != FALSE) {
		DEBUG(file, g_print ("URI '%s' is getting special cased for ITMS parsing\n", uri));
		return totem_pl_parser_add_itms (parser, file, NULL, parse_data, NULL);
	}

	if (!parse_data->recurse && parse_data->recurse_level > 0)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	uri = g_file_get_uri (file);

	/* Should we try to parse it with quvi? */
	if (g_file_has_uri_scheme (file, "http") ||
	    g_file_has_uri_scheme (file, "https")) {
		if (uri != NULL && parse_data->recurse && totem_pl_parser_is_videosite (uri, parser->priv->debug) != FALSE) {
			ret = totem_pl_parser_add_videosite (parser, file, base_file, parse_data, NULL);
			if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS)
				return ret;
		}
	}

	if (uri != NULL) {
		if (totem_pl_parser_glob_is_ignored (parser, uri))
			return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	/* In force mode we want to get the data */
	if (parse_data->force != FALSE) {
		mimetype = my_g_file_info_get_mime_type_with_data (file, &data, parser);
	} else {
		char *uri;

		uri = g_file_get_uri (file);
#ifdef G_OS_WIN32
		{
			char *content_type;
			content_type = g_content_type_guess (uri, NULL, 0, NULL);
			mimetype = g_content_type_get_mime_type (content_type);
			g_free (content_type);
		}
#else
		mimetype = g_content_type_guess (uri, NULL, 0, NULL);
#endif

		g_free (uri);
	}

	/* We're much more likely to have an MP2T file instead */
	if (g_strcmp0 (mimetype, "application/x-linguist") == 0 ||
	    g_strcmp0 (mimetype, "text/vnd.trolltech.linguist") == 0) {
		g_free (mimetype);
		mimetype = g_strdup ("video/mp2t");
	}

	/* Not a directory on http though */
	if (g_strcmp0 (mimetype, "inode/directory") == 0 &&
	    g_file_has_uri_scheme (file, "http")) {
		g_clear_pointer (&mimetype, g_free);
	}

	DEBUG(file, g_print ("_get_mime_type_for_name for '%s' returned '%s'\n", uri, mimetype));
	if (mimetype == NULL ||
	    strcmp (UNKNOWN_TYPE, mimetype) == 0 ||
	    g_content_type_is_a (mimetype, "text/plain") != FALSE) {
		char *new_mimetype;
		new_mimetype = my_g_file_info_get_mime_type_with_data (file, &data, parser);
		if (new_mimetype) {
			g_free (mimetype);
			mimetype = new_mimetype;
			DEBUG(file, g_print ("_get_mime_type_with_data for '%s' returned '%s'\n", uri, mimetype ? mimetype : "NULL"));
		} else {
			DEBUG(file, g_print ("_get_mime_type_with_data for '%s' returned NULL, ignoring\n", uri));
		}
	}

	if (mimetype == NULL)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	if (strcmp (mimetype, EMPTY_FILE_TYPE) == 0)
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	else if (strcmp (mimetype, HLS_MIME_TYPE) == 0)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	/* If we're at the top-level of the parsing, try to get more
	 * data from the playlist parser */
	if (strcmp (mimetype, AUDIO_MPEG_TYPE) == 0 && parse_data->recurse_level == 0 && data == NULL) {
		char *tmp;
		tmp = my_g_file_info_get_mime_type_with_data (file, &data, parser);
		if (tmp != NULL) {
			g_free (mimetype);
			mimetype = tmp;
		}
		DEBUG(file, g_print ("_get_mime_type_with_data for '%s' returned '%s' (was %s)\n", uri, mimetype, AUDIO_MPEG_TYPE));

		if (strcmp (mimetype, AUDIO_MPEG_TYPE) == 0)
			return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	if (totem_pl_parser_mimetype_is_ignored (parser, mimetype) != FALSE)
		return TOTEM_PL_PARSER_RESULT_IGNORED;

	if (parse_data->recurse || parse_data->recurse_level == 0) {
		parse_data->recurse_level++;

		for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
			if (strcmp (special_types[i].mimetype, mimetype) == 0) {
				DEBUG(file, g_print ("URI '%s' is special type '%s'\n", uri, mimetype));
				if (parse_data->disable_unsafe != FALSE && special_types[i].unsafe != FALSE) {
					DEBUG(file, g_print ("URI '%s' is unsafe so was ignored\n", uri));
					return TOTEM_PL_PARSER_RESULT_IGNORED;
				}
				if (base_file == NULL)
					base_file = g_file_get_parent (file);
				else
					base_file = g_object_ref (base_file);

				DEBUG (file, g_print ("Using %s function for '%s'\n", special_types[i].mimetype, uri));
				ret = (* special_types[i].func) (parser, file, base_file, parse_data, data);

				if (base_file != NULL)
					g_object_unref (base_file);

				found = TRUE;
				break;
			}
		}

		for (i = 0; i < G_N_ELEMENTS(dual_types) && found == FALSE; i++) {
			if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
				PlaylistCallback func;

				DEBUG(file, g_print ("URI '%s' is dual type '%s'\n", uri, mimetype));
				if (data == NULL) {
					g_free (mimetype);
					mimetype = my_g_file_info_get_mime_type_with_data (file, &data, parser);
					DEBUG(file, g_print ("URI '%s' dual type has type '%s' from data\n", uri, mimetype));
				}
				/* If it's _still_ a text/plain, we don't want it */
				if (mimetype != NULL &&
				    g_content_type_is_a (mimetype, "text/plain") &&
				    g_content_type_is_a (mimetype, "application/xml") == FALSE) {
					DEBUG(file, g_print ("Ignoring URI '%s' dual type because '%s' is a text/plain\n", uri, mimetype));
					ret = TOTEM_PL_PARSER_RESULT_IGNORED;
					g_free (mimetype);
					mimetype = NULL;
					break;
				}
				/* Now look for the proper function to use */
				func = totem_pl_parser_get_function_for_mimetype (mimetype);
				if ((func == NULL && mimetype != NULL) || (mimetype == NULL && dual_types[i].func == NULL)) {
					DEBUG(file, g_print ("Ignoring URI '%s' because we couldn't find a playlist parser for '%s'\n", uri, mimetype));
					ret = TOTEM_PL_PARSER_RESULT_UNHANDLED;
					g_clear_pointer (&mimetype, g_free);
					break;
				} else if (func == NULL) {
					func = dual_types[i].func;
				}

				if (base_file == NULL)
					base_file = g_file_get_parent (file);
				else
					base_file = g_object_ref (base_file);

				ret = (* func) (parser, file, base_file ? base_file : file, parse_data, data);

				if (base_file != NULL)
					g_object_unref (base_file);

				found = TRUE;
				break;
			}
		}

		parse_data->recurse_level--;
	}

	if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS)
		return ret;

	if (totem_pl_parser_ignore_from_mimetype (parser, mimetype) != FALSE)
		return TOTEM_PL_PARSER_RESULT_IGNORED;

	if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && parse_data->fallback) {
		totem_pl_parser_add_one_file (parser, file, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return ret;
}

typedef struct {
	char *uri;
	char *base;
	gboolean fallback;
} ParseAsyncData;

static void
parse_async_data_free (ParseAsyncData *data)
{
	g_free (data->uri);
	g_free (data->base);
	g_slice_free (ParseAsyncData, data);
}

static void
parse_thread (GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (source_object);
	TotemPlParserResult parse_result;
	GError *error = NULL;
	ParseAsyncData *data = task_data;

	/* Check to see if it's been cancelled already */
	if (g_cancellable_set_error_if_cancelled (cancellable, &error) == TRUE) {
		g_task_return_error (task, error);
		return;
	}

	/* Parse and return */
	parse_result = totem_pl_parser_parse_with_base (parser, data->uri, data->base, data->fallback);
	g_task_return_int (task, parse_result);
}

/**
 * totem_pl_parser_parse_with_base_async:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @base: (allow-none): the base path for relative filenames, or %NULL
 * @fallback: %TRUE if the parser should add the playlist URI to the
 * end of the playlist on parse failure
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (allow-none): a #GAsyncReadyCallback to call when parsing is finished
 * @user_data: data to pass to the @callback function
 *
 * Starts asynchronous parsing of a playlist given by the absolute URI @uri, using @base to resolve relative paths where appropriate.
 * @parser and @uri are both reffed/copied when this function is called, so can safely be freed after this function returns.
 *
 * For more details, see totem_pl_parser_parse_with_base(), which is the synchronous version of this function.
 *
 * When the operation is finished, @callback will be called. You can then call totem_pl_parser_parse_finish()
 * to get the results of the operation.
 **/
void
totem_pl_parser_parse_with_base_async (TotemPlParser *parser, const char *uri, const char *base, gboolean fallback,
				       GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	GTask *task;
	ParseAsyncData *data;

	g_return_if_fail (TOTEM_PL_IS_PARSER (parser));
	g_return_if_fail (uri != NULL);
	g_return_if_fail (strstr (uri, "://") != NULL);

	data = g_slice_new (ParseAsyncData);
	data->uri = g_strdup (uri);
	data->base = g_strdup (base);
	data->fallback = fallback;

	task = g_task_new (parser, cancellable, callback, user_data);
	g_task_set_task_data (task, data, (GDestroyNotify) parse_async_data_free);
	g_task_run_in_thread (task, parse_thread);
	g_object_unref (task);
}

/**
 * totem_pl_parser_parse_with_base:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @base: (allow-none): the base path for relative filenames, or %NULL
 * @fallback: %TRUE if the parser should add the playlist URI to the
 * end of the playlist on parse failure
 *
 * Parses a playlist given by the absolute URI @uri, using
 * @base to resolve relative paths where appropriate.
 *
 * Return value: a #TotemPlParserResult
 **/
TotemPlParserResult
totem_pl_parser_parse_with_base (TotemPlParser *parser, const char *uri,
				 const char *base, gboolean fallback)
{
	GFile *file, *base_file;
	TotemPlParserResult retval;
	TotemPlParseData data;

	g_return_val_if_fail (TOTEM_PL_IS_PARSER (parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (uri != NULL, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (strstr (uri, "://") != NULL,
			TOTEM_PL_PARSER_RESULT_ERROR);

	file = g_file_new_for_uri (uri);
	base_file = NULL;

	if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE) {
		g_object_unref (file);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	/* Use a struct to store copies of the options as set for this parse operation */
	data.recurse_level = 0;
	data.fallback = fallback;
	data.recurse = parser->priv->recurse;
	data.force = parser->priv->force;
	data.disable_unsafe = parser->priv->disable_unsafe;

	if (base != NULL)
		base_file = g_file_new_for_uri (base);
	retval = totem_pl_parser_parse_internal (parser, file, base_file, &data);

	g_object_unref (file);
	if (base_file != NULL)
		g_object_unref (base_file);

	return retval;
}

/**
 * totem_pl_parser_parse_async:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @fallback: %TRUE if the parser should add the playlist URI to the
 * end of the playlist on parse failure
 * @cancellable: (allow-none): optional #GCancellable object, or %NULL
 * @callback: (allow-none): a #GAsyncReadyCallback to call when parsing is finished
 * @user_data: data to pass to the @callback function
 *
 * Starts asynchronous parsing of a playlist given by the absolute URI @uri. @parser and @uri are both reffed/copied
 * when this function is called, so can safely be freed after this function returns.
 *
 * For more details, see totem_pl_parser_parse(), which is the synchronous version of this function.
 *
 * When the operation is finished, @callback will be called. You can then call totem_pl_parser_parse_finish()
 * to get the results of the operation.
 **/
void
totem_pl_parser_parse_async (TotemPlParser *parser, const char *uri, gboolean fallback,
			     GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
	totem_pl_parser_parse_with_base_async (parser, uri, NULL, fallback, cancellable, callback, user_data);
}

/**
 * totem_pl_parser_parse_finish:
 * @parser: a #TotemPlParser
 * @async_result: a #GAsyncResult
 * @error: a #GError, or %NULL
 *
 * Finishes an asynchronous playlist parsing operation started with totem_pl_parser_parse_async()
 * or totem_pl_parser_parse_with_base_async().
 *
 * If parsing of the playlist is cancelled part-way through, %TOTEM_PL_PARSER_RESULT_CANCELLED is returned when
 * this function is called.
 *
 * Return value: a #TotemPlParserResult
 **/
TotemPlParserResult
totem_pl_parser_parse_finish (TotemPlParser *parser, GAsyncResult *async_result, GError **error)
{
	GTask *task = G_TASK (async_result);
	GError *local_error = NULL;
	int ret;

	g_return_val_if_fail (TOTEM_PL_IS_PARSER (parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (g_task_is_valid (async_result, parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);

	/* Propagate any errors which were caught and return the result; otherwise just return the result */
	ret = g_task_propagate_int (task, &local_error);
	if (ret == -1) {
		if (g_error_matches (local_error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
			ret = TOTEM_PL_PARSER_RESULT_CANCELLED;
			g_error_free (local_error);
		} else {
			g_warning ("Unexpected error from asynchronous parsing: %s", local_error->message);
			g_propagate_error (error, local_error);
		}
	}
	return ret;
}

/**
 * totem_pl_parser_parse:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @fallback: %TRUE if the parser should add the playlist URI to the
 * end of the playlist on parse failure
 *
 * Parses a playlist given by the absolute URI @uri. This method is
 * synchronous, and will block on (e.g.) network requests to slow
 * servers. totem_pl_parser_parse_async() is recommended instead.
 *
 * Return values are as totem_pl_parser_parse_with_base().
 *
 * Return value: a #TotemPlParserResult
 **/
TotemPlParserResult
totem_pl_parser_parse (TotemPlParser *parser, const char *uri,
		       gboolean fallback)
{
	return totem_pl_parser_parse_with_base (parser, uri, NULL, fallback);
}

/**
 * totem_pl_parser_add_ignored_scheme:
 * @parser: a #TotemPlParser
 * @scheme: the scheme to ignore
 *
 * Adds a scheme to the list of schemes to ignore, so that
 * any URI using that scheme is ignored during playlist parsing.
 **/
void
totem_pl_parser_add_ignored_scheme (TotemPlParser *parser,
		const char *scheme)
{
	char *s;

	g_return_if_fail (TOTEM_PL_IS_PARSER (parser));

	g_mutex_lock (&parser->priv->ignore_mutex);

	s = g_strdup (scheme);
	if (s[strlen (s) - 1] == ':')
		s[strlen (s) - 1] = '\0';
	g_hash_table_insert (parser->priv->ignore_schemes, s, GINT_TO_POINTER (1));

	g_mutex_unlock (&parser->priv->ignore_mutex);
}

/**
 * totem_pl_parser_add_ignored_mimetype:
 * @parser: a #TotemPlParser
 * @mimetype: the mimetype to ignore
 *
 * Adds a mimetype to the list of mimetypes to ignore, so that
 * any URI of that mimetype is ignored during playlist parsing.
 **/
void
totem_pl_parser_add_ignored_mimetype (TotemPlParser *parser,
		const char *mimetype)
{
	g_return_if_fail (TOTEM_PL_IS_PARSER (parser));

	g_mutex_lock (&parser->priv->ignore_mutex);
	g_hash_table_insert (parser->priv->ignore_mimetypes, g_strdup (mimetype), GINT_TO_POINTER (1));
	g_mutex_unlock (&parser->priv->ignore_mutex);
}

/**
 * totem_pl_parser_add_ignored_glob:
 * @parser: a #TotemPlParser
 * @glob: a glob to ignore
 *
 * Adds a glob to the list of mimetypes to ignore, so that
 * any URI of that glob is ignored during playlist parsing.
 *
 * Since: 3.26.4
 **/
void
totem_pl_parser_add_ignored_glob (TotemPlParser *parser,
				  const char    *glob)
{
	g_return_if_fail (TOTEM_PL_IS_PARSER (parser));

	g_mutex_lock (&parser->priv->ignore_mutex);
	g_hash_table_insert (parser->priv->ignore_globs, g_strdup (glob), GINT_TO_POINTER (1));
	g_mutex_unlock (&parser->priv->ignore_mutex);
}

/**
 * totem_pl_parser_parse_duration:
 * @duration: the duration string to parse
 * @debug: %TRUE if debug statements should be printed
 *
 * Parses the given duration string and returns it as a <type>gint64</type>
 * denoting the duration in seconds.
 *
 * Return value: the duration in seconds, or -1 on error
 **/
gint64
totem_pl_parser_parse_duration (const char *duration, gboolean debug)
{
	int hours, minutes, seconds, fractions;

	if (duration == NULL) {
		D(g_print ("No duration passed\n"));
		return -1;
	}

	/* Formats used by both ASX and RAM files */
	if (sscanf (duration, "%d:%d:%d.%d", &hours, &minutes, &seconds, &fractions) == 4) {
		gint64 ret = (gint64) hours * 3600 + (gint64) minutes * 60 + seconds;
		if (ret == 0 && fractions > 0) {
			D(g_print ("Used 00:00:00.00 format, with fractions rounding\n"));
			ret = 1;
		} else {
			D(g_print ("Used 00:00:00.00 format\n"));
		}
		return ret;
	}
	if (sscanf (duration, "%d:%d:%d", &hours, &minutes, &seconds) == 3) {
		D(g_print ("Used 00:00:00 format\n"));
		return (gint64) hours * 3600 + (gint64) minutes * 60 + seconds;
	}
	if (sscanf (duration, "%d:%d.%d", &minutes, &seconds, &fractions) == 3) {
		gint64 ret = minutes * 60 + seconds;
		if (ret == 0 && fractions > 0) {
			D(g_print ("Used 00:00.00 format, with fractions rounding\n"));
			ret = 1;
		} else {
			D(g_print ("Used 00:00.00 format\n"));
		}
		return ret;
	}
	if (sscanf (duration, "%d:%d", &minutes, &seconds) == 2) {
		D(g_print ("Used 00:00 format\n"));
		return (gint64) minutes * 60 + seconds;
	}
	if (sscanf (duration, "%d.%d", &minutes, &seconds) == 2) {
		D(g_print ("Used broken float format (00.00)\n"));
		return (gint64) minutes * 60 + seconds;
	}
	/* YouTube format */
	if (sscanf (duration, "%dm%ds", &minutes, &seconds) == 2) {
		D(g_print ("Used YouTube format\n"));
		return (gint64) minutes * 60 + seconds;
	}
	/* PLS files format */
	if (sscanf (duration, "%d", &seconds) == 1) {
		D(g_print ("Used PLS format\n"));
		return seconds;
	}

	D(g_message ("Couldn't parse duration '%s'\n", duration));

	return -1;
}


/**
 * totem_pl_parser_parse_date:
 * @date_str: the date string to parse
 * @debug: %TRUE if debug statements should be printed
 *
 * Parses the given date string and returns it as a <type>gint64</type>
 * denoting the date in seconds since the UNIX Epoch.
 *
 * Return value: the date in seconds, or -1 on error
 **/
guint64
totem_pl_parser_parse_date (const char *date_str, gboolean debug)
{
	g_autoptr(GDateTime) date = NULL;

	g_return_val_if_fail (date_str != NULL, -1);

	/* Try to parse as an ISO8601/RFC3339 date */
	date = g_date_time_new_from_iso8601 (date_str, NULL);
	if (date != NULL) {
		D(g_message ("Parsed duration '%s' using the ISO8601 parser", date_str));
		return g_date_time_to_unix (date);
	}
	D(g_message ("Failed to parse duration '%s' using the ISO8601 parser", date_str));
	/* Fall back to RFC 2822 date parsing */
	date = g_mime_utils_header_decode_date (date_str);
	if (!date) {
		D(g_message ("Failed to parse duration '%s' using the RFC 2822 parser", date_str));
		return -1;
	}
	return g_date_time_to_unix (date);
}
#endif /* !TOTEM_PL_PARSER_MINI */

static char *
totem_pl_parser_mime_type_from_data (gconstpointer data, int len)
{
	char *mime_type;
	gboolean uncertain;

#ifdef G_OS_WIN32
	char *content_type;

	content_type = g_content_type_guess (NULL, data, len, &uncertain);
	if (uncertain == FALSE) {
		mime_type = g_content_type_get_mime_type (content_type);
		g_free (content_type);
	} else {
		mime_type = NULL;
	}
#else
	mime_type = g_content_type_guess (NULL, data, len, &uncertain);
	if (uncertain != FALSE) {
		g_free (mime_type);
		mime_type = NULL;
	}
#endif

	if (mime_type != NULL &&
	    (strcmp (mime_type, "text/plain") == 0 ||
	     strcmp (mime_type, "application/octet-stream") == 0 ||
	     strcmp (mime_type, "application/xml") == 0 ||
	     strcmp (mime_type, "text/html") == 0)) {
		PlaylistIdenCallback func;
		guint i;

		func = NULL;

		for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
			const char *res;

			if (func == dual_types[i].iden)
				continue;
			func = dual_types[i].iden;
			if (func == NULL)
				continue;
			res = func (data, len);
			if (res != NULL) {
				g_free (mime_type);
				return g_strdup (res);
			}
		}

		return NULL;
	}

	return mime_type;
}

/**
 * totem_pl_parser_can_parse_from_data:
 * @data: the data to check for parsability
 * @len: the length of data to check
 * @debug: %TRUE if debug statements should be printed
 *
 * Checks if the first @len bytes of @data can be parsed.
 *
 * Return value: %TRUE if @data can be parsed
 **/
gboolean
totem_pl_parser_can_parse_from_data (const char *data,
				     gsize len,
				     gboolean debug)
{
	char *mimetype;
	guint i;

	g_return_val_if_fail (data != NULL, FALSE);

	/* Bad cast! */
	mimetype = totem_pl_parser_mime_type_from_data ((gpointer) data, (int) len);

	if (mimetype == NULL) {
		D(g_message ("totem_pl_parser_can_parse_from_data couldn't get mimetype"));
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Is special type '%s'", mimetype));
			g_free (mimetype);
			return TRUE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
			D(g_message ("Should be dual type '%s', making sure now", mimetype));
			if (dual_types[i].iden != NULL) {
				gboolean retval = ((* dual_types[i].iden) (data, len) != NULL);
				D(g_message ("%s dual type '%s'",
					     retval ? "Is" : "Is not", mimetype));
				g_free (mimetype);
				return retval;
			}
			g_free (mimetype);
			return FALSE;
		}
	}

	D(g_message ("Is unsupported mime-type '%s'", mimetype));

	g_free (mimetype);

	return FALSE;
}

/**
 * totem_pl_parser_can_parse_from_filename:
 * @filename: the file to check for parsability
 * @debug: %TRUE if debug statements should be printed
 *
 * Checks if the file can be parsed. Files can be parsed if:
 * <itemizedlist>
 *  <listitem><para>they have a special mimetype, or</para></listitem>
 *  <listitem><para>they have a mimetype which could be a video or a playlist.</para></listitem>
 * </itemizedlist>
 *
 * Return value: %TRUE if @filename can be parsed
 **/
gboolean
totem_pl_parser_can_parse_from_filename (const char *filename, gboolean debug)
{
	GMappedFile *map;
	GError *err = NULL;
	gboolean retval;

	g_return_val_if_fail (filename != NULL, FALSE);

	map = g_mapped_file_new (filename, FALSE, &err);
	if (map == NULL) {
		D(g_message ("couldn't mmap %s: %s", filename, err->message));
		g_error_free (err);
		return FALSE;
	}

	retval = totem_pl_parser_can_parse_from_data
		(g_mapped_file_get_contents (map),
		 g_mapped_file_get_length (map), debug);

	g_mapped_file_unref (map);

	return retval;
}

/**
 * totem_pl_parser_can_parse_from_uri:
 * @uri: the remote URI to check for parsability
 * @debug: %TRUE if debug statements should be printed
 *
 * Checks if the remote URI can be parsed. Note that this does
 * not actually try to open the remote URI, or deduce its mime-type
 * from filename, as this would bring too many false positives.
 *
 * Return value: %TRUE if @uri could be parsed
 **/
gboolean
totem_pl_parser_can_parse_from_uri (const char *uri, gboolean debug)
{
	return totem_pl_parser_is_videosite (uri, debug);
}

#ifndef TOTEM_PL_PARSER_MINI
GType
totem_pl_parser_metadata_get_type (void)
{
	static gsize g_define_type_id__volatile = 0;
	if (g_once_init_enter (&g_define_type_id__volatile))
	{
		/* NOTE: This is equivalent to the definition for GHashTable in gboxed.c, in that it uses the same copy/free functions.
		 * This means that if we box a TotemPlParserMetadata inside a GValue, we can safely unbox it as a GHashTable (and vice-versa).
		 * This means we can hide TotemPlParserMetadata from introspection, and just pretend it's actually been a GHashTable all along. */
		GType g_define_type_id = g_boxed_type_register_static (
		    g_intern_static_string ("TotemPlParserMetadata"),
		    (GBoxedCopyFunc) g_hash_table_ref,
		    (GBoxedFreeFunc) g_hash_table_unref);
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
	}
	return g_define_type_id__volatile;
}
#endif /* !TOTEM_PL_PARSER_MINI */

