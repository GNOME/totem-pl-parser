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
 * support for several different types of playlist.
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
 * <example>
 *  <title>Writing a Playlist</title>
 *  <programlisting>
 * void
 * parser_func (GtkTreeModel *model, GtkTreeIter *iter,
 * 		gchar **uri, gchar **title, gboolean *custom_title,
 * 		gpointer user_data)
 * {
 * 	gtk_tree_model_get (model, iter,
 * 		0, uri,
 * 		1, title,
 * 		2, custom_title,
 * 		-1);
 * }
 *
 * {
 * 	TotemPlParser *pl;
 * 	GtkTreeModel *tree_model;
 *
 * 	pl = totem_pl_parser_new ();
 *
 * 	&slash;* Your tree model can be as simple or as complex as you like;
 * 	 * parser_func() will just have to return the entry title, URI and custom title flag from it. *&slash;
 * 	tree_model = gtk_list_store_new (3, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN);
 * 	populate_model (tree_model);
 *
 * 	if (totem_pl_parser_write (pl, tree_model, parser_func, "/tmp/playlist.pls",
 * 				   TOTEM_PL_PARSER_PLS, NULL, NULL) != TRUE) {
 * 		g_error ("Playlist writing failed.");
 * 	}
 *
 * 	g_object_unref (tree_model);
 * 	g_object_unref (pl);
 * }
 *  </programlisting>
 * </example>
 **/

#include "config.h"

#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gi18n-lib.h>
#include <gio/gio.h>

#ifndef TOTEM_PL_PARSER_MINI
#include <gobject/gvaluecollector.h>
#include <gtk/gtk.h>

#ifdef HAVE_CAMEL
#include <camel/camel-mime-utils.h>
#endif

#include "totem-pl-parser.h"
#include "totemplparser-marshal.h"
#include "totem-disc.h"
#endif /* !TOTEM_PL_PARSER_MINI */

#include "totem-pl-parser-mini.h"
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

#define READ_CHUNK_SIZE 8192
#define RECURSE_LEVEL_MAX 4

#define D(x) if (debug) x

typedef const char * (*PlaylistIdenCallback) (const char *data, gsize len);

#ifndef TOTEM_PL_PARSER_MINI
typedef TotemPlParserResult (*PlaylistCallback) (TotemPlParser *parser, GFile *uri, GFile *base_file, gpointer data);
#endif

typedef struct {
	char *mimetype;
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
	PLAYLIST_TYPE ("audio/playlist", totem_pl_parser_add_m3u, NULL, FALSE),
	PLAYLIST_TYPE ("audio/x-scpls", totem_pl_parser_add_pls, NULL, FALSE),
	PLAYLIST_TYPE ("application/x-smil", totem_pl_parser_add_smil, NULL, FALSE),
	PLAYLIST_TYPE ("application/smil", totem_pl_parser_add_smil, NULL, FALSE),
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
	PLAYLIST_TYPE2 ("audio/x-ms-asx", totem_pl_parser_add_asx, totem_pl_parser_is_asx),
	PLAYLIST_TYPE2 ("video/x-ms-asf", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
	PLAYLIST_TYPE2 ("video/x-ms-wmv", totem_pl_parser_add_asf, totem_pl_parser_is_asf),
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
static gboolean i18n_done = FALSE;

static void totem_pl_parser_class_init (TotemPlParserClass *klass);
static void totem_pl_parser_base_class_finalize	(TotemPlParserClass *klass);
static void totem_pl_parser_init       (TotemPlParser *parser);
static void totem_pl_parser_finalize   (GObject *object);

static void totem_pl_parser_init       (TotemPlParser      *self);
static void totem_pl_parser_class_init (TotemPlParserClass *klass);
static gpointer totem_pl_parser_parent_class = NULL;

GType
totem_pl_parser_get_type (void)
{
	static volatile gsize g_define_type_id__volatile = 0;
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
	g_type_class_add_private (klass, sizeof (TotemPlParserPrivate));

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
	 * @metadata: a #GHashTable of metadata relating to the entry added
	 *
	 * The ::entry-parsed signal is emitted when a new entry is parsed.
	 */
	totem_pl_parser_table_signals[ENTRY_PARSED] =
		g_signal_new ("entry-parsed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (TotemPlParserClass, entry_parsed),
			      NULL, NULL,
			      totemplparser_marshal_VOID__STRING_BOXED,
			      G_TYPE_NONE, 2, G_TYPE_STRING, TOTEM_TYPE_PL_PARSER_METADATA);
	/**
	 * TotemPlParser::playlist-started:
	 * @parser: the object which received the signal
	 * @uri: the URI of the new playlist started
	 * @metadata: a #GHashTable of metadata relating to the playlist that
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
			      totemplparser_marshal_VOID__STRING_BOXED,
			      G_TYPE_NONE, 2, G_TYPE_STRING, TOTEM_TYPE_PL_PARSER_METADATA);
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
				     "Genre of the item to be added", NULL,
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

static void
totem_pl_parser_init_i18n (void)
{
	if (i18n_done == FALSE) {
		/* set up translation catalog */
		bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
		bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
		i18n_done = TRUE;
	}
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
	g_signal_emit (G_OBJECT (parser),
		       totem_pl_parser_table_signals[PLAYLIST_ENDED],
		       0, playlist_uri);
}

static char *
my_g_file_info_get_mime_type_with_data (GFile *file, gpointer *data, TotemPlParser *parser)
{
	char *buffer;
	gssize bytes_read;
	GFileInputStream *stream;
	GError *error = NULL;

	*data = NULL;

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
	bytes_read = g_input_stream_read (G_INPUT_STREAM (stream), buffer, MIME_READ_CHUNK_SIZE, NULL, &error);
	g_object_unref (G_INPUT_STREAM (stream));
	if (bytes_read == -1) {
		DEBUG(file, g_print ("Couldn't read data from '%s'\n", uri));
		g_free (buffer);
		return NULL;
	}

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
 * @error: return location for a #GError, or %NULL
 *
 * Writes the string @buf out to the file specified by @handle.
 * Possible error codes are as per totem_pl_parser_write_buffer().
 *
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write_string (GOutputStream *stream, const char *buf, GError **error)
{
	guint len;

	len = strlen (buf);
	return totem_pl_parser_write_buffer (stream, buf, len, error);
}

/**
 * totem_pl_parser_write_buffer:
 * @stream: a #GFileOutputStream to an open file
 * @buf: the string buffer to write out
 * @len: the length of the string to write out
 * @error: return location for a #GError, or %NULL
 *
 * Writes @len bytes of @buf to the file specified by @handle.
 *
 * A value of @len greater than #G_MAXSSIZE will cause a #G_IO_ERROR_INVALID_ARGUMENT argument.
 *
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write_buffer (GOutputStream *stream, const char *buf, guint len, GError **error)
{
	gsize bytes_written;

	if (g_output_stream_write_all (stream,
				       buf, len,
				       &bytes_written,
				       NULL, error) == FALSE) {
		g_object_unref (stream);
		return FALSE;
	}

	return TRUE;
}

/**
 * totem_pl_parser_num_entries:
 * @parser: a #TotemPlParser
 * @model: a #GtkTreeModel
 * @func: a pointer to a #TotemPlParserIterFunc callback function
 * @user_data: a pointer to be passed to each call of @func
 *
 * Returns the number of entries in @parser's playlist, and calls
 * @func for each valid entry in the playlist.
 *
 * Return value: the number of entries in the playlist
 **/
int
totem_pl_parser_num_entries (TotemPlParser *parser, GtkTreeModel *model,
			     TotemPlParserIterFunc func, gpointer user_data)
{
	int num_entries, i, ignored;

	num_entries = gtk_tree_model_iter_n_children (model, NULL);
	ignored = 0;

	for (i = 1; i <= num_entries; i++)
	{
		GtkTreeIter iter;
		char *uri, *title;
		GFile *file;
		gboolean custom_title;

		if (gtk_tree_model_iter_nth_child (model, &iter, NULL, i - 1) == FALSE)
			return i - ignored;

		func (model, &iter, &uri, &title, &custom_title, user_data);
		file = g_file_new_for_uri (uri);
		if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE)
			ignored++;

		g_object_unref (file);
		g_free (uri);
		g_free (title);
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
	content_type = g_content_type_guess (short_name, NULL, -1, NULL);
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
			char *base_uri;
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
			char *base_uri;
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
/**
 * totem_pl_parser_write_with_title:
 * @parser: a #TotemPlParser
 * @model: a #GtkTreeModel
 * @func: a pointer to a #TotemPlParserIterFunc callback function
 * @output: the output path and filename
 * @title: the playlist title
 * @type: a #TotemPlParserType for the ouputted playlist
 * @user_data: a pointer to be passed to each call of @func
 * @error: return location for a #GError, or %NULL
 *
 * Writes the playlist held by @parser and @model out to the file of
 * path @output. The playlist is written in the format @type and is
 * given the title @title.
 *
 * For each entry in the @model, the function @func is called (and passed
 * @user_data), which gets various metadata values about the entry for
 * the playlist writer.
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
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write_with_title (TotemPlParser *parser, GtkTreeModel *model,
				  TotemPlParserIterFunc func,
				  const char *output, const char *title,
				  TotemPlParserType type,
				  gpointer user_data, GError **error)
{
	GFile *file;

	file = g_file_new_for_commandline_arg (output);

	switch (type)
	{
	case TOTEM_PL_PARSER_PLS:
		return totem_pl_parser_write_pls (parser, model, func,
				file, title, user_data, error);
	case TOTEM_PL_PARSER_M3U:
	case TOTEM_PL_PARSER_M3U_DOS:
		return totem_pl_parser_write_m3u (parser, model, func,
				file, (type == TOTEM_PL_PARSER_M3U_DOS),
                                user_data, error);
	case TOTEM_PL_PARSER_XSPF:
		return totem_pl_parser_write_xspf (parser, model, func,
				file, title, user_data, error);
	case TOTEM_PL_PARSER_IRIVER_PLA:
		return totem_pl_parser_write_pla (parser, model, func,
				file, title, user_data, error);
	default:
		g_assert_not_reached ();
	}

	g_object_unref (file);

	return FALSE;
}

/**
 * totem_pl_parser_write:
 * @parser: a #TotemPlParser
 * @model: a #GtkTreeModel
 * @func: a pointer to a #TotemPlParserIterFunc callback function
 * @output: the output path and filename
 * @type: a #TotemPlParserType for the ouputted playlist
 * @user_data: a pointer to be passed to each call of @func
 * @error: return location for a #GError, or %NULL
 *
 * Writes the playlist held by @parser and @model out to the file of
 * path @output. The playlist is written in the format @type and is given
 * a %NULL title.
 *
 * For each entry in the @model, the function @func is called (and passed
 * @user_data), which gets various metadata values about the entry for
 * the playlist writer.
 *
 * Possible error codes are as per totem_pl_parser_write_with_title().
 *
 * Return value: %TRUE on success
 **/
gboolean
totem_pl_parser_write (TotemPlParser *parser, GtkTreeModel *model,
		       TotemPlParserIterFunc func,
		       const char *output, TotemPlParserType type,
		       gpointer user_data,
		       GError **error)
{
	return totem_pl_parser_write_with_title (parser, model, func, output,
			NULL, type, user_data, error);
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
 * @dos_mode: %TRUE if the returned string should end in \r\0, instead of \n\0
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
		gboolean dos_mode, const char *sep)
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
			glong len;

			bits = g_strsplit (line, sep, 2);
			if (bits[0] == NULL || bits [1] == NULL) {
				g_strfreev (bits);
				return NULL;
			}

			retval = g_strdup (bits[1]);
			len = strlen (retval);
			if (dos_mode && len >= 2 && retval[len-2] == '\r') {
				retval[len-2] = '\n';
				retval[len-1] = '\0';
			}

			g_strfreev (bits);
		}
	}

	return retval;
}

/**
 * totem_pl_parser_read_ini_line_string:
 * @lines: a NULL-terminated array of INI lines to read
 * @key: the key to match
 * @dos_mode: %TRUE if the returned string should end in \r\0, instead of \n\0
 *
 * Returns the first string value case-insensitively matching the
 * specified key. The parser ignores leading whitespace on lines.
 *
 * Return value: a newly-allocated string value, or %NULL
 **/
char*
totem_pl_parser_read_ini_line_string (char **lines, const char *key, gboolean dos_mode)
{
	return totem_pl_parser_read_ini_line_string_with_sep (lines, key, dos_mode, "=");
}

static void
totem_pl_parser_init (TotemPlParser *parser)
{
	parser->priv = G_TYPE_INSTANCE_GET_PRIVATE (parser, TOTEM_TYPE_PL_PARSER, TotemPlParserPrivate);
}

static void
totem_pl_parser_finalize (GObject *object)
{
	TotemPlParser *parser = TOTEM_PL_PARSER (object);

	g_return_if_fail (object != NULL);
	g_return_if_fail (parser->priv != NULL);

	g_list_foreach (parser->priv->ignore_schemes, (GFunc) g_free, NULL);
	g_list_free (parser->priv->ignore_schemes);

	g_list_foreach (parser->priv->ignore_mimetypes, (GFunc) g_free, NULL);
	g_list_free (parser->priv->ignore_mimetypes);

	G_OBJECT_CLASS (totem_pl_parser_parent_class)->finalize (object);
}

static void
totem_pl_parser_add_uri_valist (TotemPlParser *parser,
				const gchar *first_property_name,
				va_list      var_args)
{
	const char *name;
	char *title, *uri;
	GHashTable *metadata;
	gboolean is_playlist;

	title = uri = NULL;
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

			if (g_utf8_validate (string, -1, NULL) == FALSE) {
				fixed = g_convert (string, -1, "UTF-8", "ISO8859-1", NULL, NULL, NULL);
				if (fixed == NULL) {
					g_value_unset (&value);
					name = va_arg (var_args, char*);
					continue;
				}
			}

			/* Remove trailing spaces from titles */
			if (strcmp (name, "title") == 0) {
				if (fixed == NULL)
					fixed = g_strchomp (g_strdup (string));
				else
					g_strchomp (fixed);
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

	if (g_hash_table_size (metadata) > 0 || uri != NULL) {
		if (is_playlist == FALSE) {
			g_signal_emit (G_OBJECT (parser),
				       totem_pl_parser_table_signals[ENTRY_PARSED],
				       0, uri, metadata);
		} else {
			g_signal_emit (G_OBJECT (parser),
				       totem_pl_parser_table_signals[PLAYLIST_STARTED],
				       0, uri, metadata);
		}
	}

	g_hash_table_unref (metadata);

	g_free (uri);
	g_object_unref (G_OBJECT (parser));
}

/**
 * totem_pl_parser_add_uri:
 * @parser: a #TotemPlParser
 * @first_property_name: the first property name
 * @Varargs: value for the first property, followed optionally by more
 * name/value pairs, followed by %NULL
 *
 * Adds a URI to the playlist with the properties given in @first_property_name
 * and @Varargs.
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
	GList *l;

	if (parser->priv->ignore_schemes == NULL)
		return FALSE;

	for (l = parser->priv->ignore_schemes; l != NULL; l = l->next) {
		const char *scheme = l->data;
		if (g_file_has_uri_scheme (uri, scheme) != FALSE)
			return TRUE;
	}

	return FALSE;
}

static gboolean
totem_pl_parser_mimetype_is_ignored (TotemPlParser *parser,
				     const char *mimetype)
{
	GList *l;

	if (parser->priv->ignore_mimetypes == NULL)
		return FALSE;

	for (l = parser->priv->ignore_mimetypes; l != NULL; l = l->next)
	{
		const char *item = l->data;
		if (strcmp (mimetype, item) == 0)
			return TRUE;
	}

	return FALSE;
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
	char *mimetype;
	GFile *file;
	guint i;

	file = g_file_new_for_path (uri);
	if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE) {
		g_object_unref (file);
		return TRUE;
	}
	g_object_unref (file);

	//FIXME wrong for win32
	mimetype = g_content_type_guess (uri, NULL, 0, NULL);
	if (mimetype == NULL || strcmp (mimetype, UNKNOWN_TYPE) == 0) {
		g_free (mimetype);
		return FALSE;
	}

	for (i = 0; i < G_N_ELEMENTS (special_types); i++) {
		if (strcmp (special_types[i].mimetype, mimetype) == 0) {
			g_free (mimetype);
			return FALSE;
		}
	}

	for (i = 0; i < G_N_ELEMENTS (dual_types); i++) {
		if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
			g_free (mimetype);
			return FALSE;
		}
	}

	g_free (mimetype);

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

	while ((needle = strstr (contents, "<!--")) != NULL) {
		while (strncmp (needle, "-->", 3) != 0) {
			*needle = ' ';
			needle++;
			if (*needle == '\0')
				break;
		}
		if (strncmp (needle, "-->", 3) == 0) {
			guint i;
			for (i = 0; i < 3; i++)
				*(needle + i) = ' ';
		}
	}
}

xml_node_t *
totem_pl_parser_parse_xml_relaxed (char *contents,
				   gsize size)
{
	xml_node_t* doc, *node;
	char *encoding, *new_contents;
	gsize new_size;

	totem_pl_parser_cleanup_xml (contents);
	xml_parser_init (contents, size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options (&doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0)
		return NULL;

	encoding = NULL;
	for (node = doc; node != NULL; node = node->next) {
		if (node->name == NULL || g_str_equal (node->name, "?XML") == FALSE)
			continue;
		encoding = g_strdup (xml_parser_get_property (node, "ENCODING"));
		break;
	}

	if (encoding == NULL || g_str_equal (encoding, "UTF-8") != FALSE) {
		g_free (encoding);
		return doc;
	}

	xml_parser_free_tree (doc);

	new_contents = g_convert (contents, size, "UTF-8", encoding, NULL, &new_size, NULL);
	if (new_contents == NULL) {
		g_warning ("Failed to convert XML data to UTF-8");
		g_free (encoding);
		return NULL;
	}
	g_free (encoding);

	xml_parser_init (new_contents, new_size, XML_PARSER_CASE_INSENSITIVE);
	if (xml_parser_build_tree_with_options (&doc, XML_PARSER_RELAXED | XML_PARSER_MULTI_TEXT) < 0) {
		g_free (new_contents);
		return NULL;
	}

	g_free (new_contents);

	return doc;
}

static gboolean
totem_pl_parser_ignore_from_mimetype (TotemPlParser *parser, const char *mimetype)
{
	guint i;

	if (mimetype == NULL)
		return FALSE;

	for (i = 0; i < G_N_ELEMENTS (ignore_types); i++) {
		if (g_content_type_is_a (mimetype, ignore_types[i].mimetype) != FALSE)
			return TRUE;
		if (g_content_type_equals (mimetype, ignore_types[i].mimetype) != FALSE)
			return TRUE;
	}

	return FALSE;
}

TotemPlParserResult
totem_pl_parser_parse_internal (TotemPlParser *parser,
				GFile *file,
				GFile *base_file)
{
	char *mimetype;
	guint i;
	gpointer data = NULL;
	TotemPlParserResult ret = TOTEM_PL_PARSER_RESULT_UNHANDLED;
	gboolean found = FALSE;

	if (parser->priv->recurse_level > RECURSE_LEVEL_MAX)
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
		return totem_pl_parser_add_itpc (parser, file, base_file, NULL);
	}
	if (g_file_has_uri_scheme (file, "zune") != FALSE) {
		DEBUG(file, g_print ("URI '%s' is getting special cased for ZUNE parsing\n", uri));
		return totem_pl_parser_add_zune (parser, file, base_file, NULL);
	}
	/* Try itms Podcast references, see itunes.py in PenguinTV */
	if (totem_pl_parser_is_itms_feed (file) != FALSE) {
	    	DEBUG(file, g_print ("URI '%s' is getting special cased for ITMS parsing\n", uri));
	    	return totem_pl_parser_add_itms (parser, file, NULL, NULL);
	}

	if (!parser->priv->recurse && parser->priv->recurse_level > 0)
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;

	/* In force mode we want to get the data */
	if (parser->priv->force != FALSE) {
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

	DEBUG(file, g_print ("_get_mime_type_for_name for '%s' returned '%s'\n", uri, mimetype));
	if (mimetype == NULL || strcmp (UNKNOWN_TYPE, mimetype) == 0
	    || (g_file_is_native (file) && g_content_type_is_a (mimetype, "text/plain") != FALSE)) {
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

	if (mimetype == NULL) {
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	if (strcmp (mimetype, EMPTY_FILE_TYPE) == 0) {
		g_free (data);
		g_free (mimetype);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	/* If we're at the top-level of the parsing, try to get more
	 * data from the playlist parser */
	if (strcmp (mimetype, AUDIO_MPEG_TYPE) == 0 && parser->priv->recurse_level == 0 && data == NULL) {
		char *tmp;
		tmp = my_g_file_info_get_mime_type_with_data (file, &data, parser);
		if (tmp != NULL) {
			g_free (mimetype);
			mimetype = tmp;
		}
		DEBUG(file, g_print ("_get_mime_type_with_data for '%s' returned '%s' (was %s)\n", uri, mimetype, AUDIO_MPEG_TYPE));
	}

	if (totem_pl_parser_mimetype_is_ignored (parser, mimetype) != FALSE) {
		g_free (mimetype);
		g_free (data);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}

	if (parser->priv->recurse || parser->priv->recurse_level == 0) {
		parser->priv->recurse_level++;

		for (i = 0; i < G_N_ELEMENTS(special_types); i++) {
			if (strcmp (special_types[i].mimetype, mimetype) == 0) {
				DEBUG(file, g_print ("URI '%s' is special type '%s'\n", uri, mimetype));
				if (parser->priv->disable_unsafe != FALSE && special_types[i].unsafe != FALSE) {
					DEBUG(file, g_print ("URI '%s' is unsafe so was ignored\n", uri));
					g_free (mimetype);
					g_free (data);
					return TOTEM_PL_PARSER_RESULT_IGNORED;
				}
				if (base_file == NULL)
					base_file = g_file_get_parent (file);
				else
					base_file = g_object_ref (base_file);

				DEBUG (file, g_print ("Using %s function for '%s'\n", special_types[i].mimetype, uri));
				ret = (* special_types[i].func) (parser, file, base_file, data);

				if (base_file != NULL)
					g_object_unref (base_file);

				found = TRUE;
				break;
			}
		}

		for (i = 0; i < G_N_ELEMENTS(dual_types) && found == FALSE; i++) {
			if (strcmp (dual_types[i].mimetype, mimetype) == 0) {
				DEBUG(file, g_print ("URI '%s' is dual type '%s'\n", uri, mimetype));
				if (data == NULL) {
					g_free (mimetype);
					mimetype = my_g_file_info_get_mime_type_with_data (file, &data, parser);
					/* If it's _still_ a text/plain, we don't want it */
					if (mimetype == NULL || strcmp (mimetype, "text/plain") == 0) {
						g_free (mimetype);
						mimetype = NULL;
						break;
					}
				}
				if (base_file == NULL)
					base_file = g_file_get_parent (file);
				else
					base_file = g_object_ref (base_file);

				ret = (* dual_types[i].func) (parser, file, base_file ? base_file : file, data);

				if (base_file != NULL)
					g_object_unref (base_file);

				found = TRUE;
				break;
			}
		}

		g_free (data);

		parser->priv->recurse_level--;
	}

	if (ret == TOTEM_PL_PARSER_RESULT_SUCCESS) {
		g_free (mimetype);
		return ret;
	}

	if (totem_pl_parser_ignore_from_mimetype (parser, mimetype) != FALSE) {
		g_free (mimetype);
		return TOTEM_PL_PARSER_RESULT_IGNORED;
	}
	g_free (mimetype);

	if (ret != TOTEM_PL_PARSER_RESULT_SUCCESS && parser->priv->fallback) {
		totem_pl_parser_add_one_file (parser, file, NULL);
		return TOTEM_PL_PARSER_RESULT_SUCCESS;
	}

	return ret;
}

/**
 * totem_pl_parser_parse_with_base:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @base: the base path for relative filenames
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

	g_return_val_if_fail (TOTEM_IS_PL_PARSER (parser), TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (uri != NULL, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_return_val_if_fail (strstr (uri, "://") != NULL,
			TOTEM_PL_PARSER_RESULT_ERROR);

	file = g_file_new_for_uri (uri);
	base_file = NULL;

	if (totem_pl_parser_scheme_is_ignored (parser, file) != FALSE) {
		g_object_unref (file);
		return TOTEM_PL_PARSER_RESULT_UNHANDLED;
	}

	parser->priv->recurse_level = 0;
	parser->priv->fallback = fallback != FALSE;
	if (base != NULL)
		base_file = g_file_new_for_uri (base);
	retval = totem_pl_parser_parse_internal (parser, file, base_file);

	g_object_unref (file);
	if (base_file != NULL)
		g_object_unref (base_file);

	return retval;
}

/**
 * totem_pl_parser_parse:
 * @parser: a #TotemPlParser
 * @uri: the URI of the playlist to parse
 * @fallback: %TRUE if the parser should add the playlist URI to the
 * end of the playlist on parse failure
 *
 * Parses a playlist given by the absolute URI @uri.
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

	g_return_if_fail (TOTEM_IS_PL_PARSER (parser));

	s = g_strdup (scheme);
	if (s[strlen (s) - 1] == ':')
		s[strlen (s) - 1] = '\0';
	parser->priv->ignore_schemes = g_list_prepend
		(parser->priv->ignore_schemes, s);
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
	g_return_if_fail (TOTEM_IS_PL_PARSER (parser));

	parser->priv->ignore_mimetypes = g_list_prepend
		(parser->priv->ignore_mimetypes, g_strdup (mimetype));
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
		gint64 ret = hours * 3600 + minutes * 60 + seconds;
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
		return hours * 3600 + minutes * 60 + seconds;
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
		return minutes * 60 + seconds;
	}
	if (sscanf (duration, "%d.%d", &minutes, &seconds) == 2) {
		D(g_print ("Used broken float format (00.00)\n"));
		return minutes * 60 + seconds;
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
#ifdef HAVE_CAMEL
	GTimeVal val;

	g_return_val_if_fail (date_str != NULL, -1);

	memset (&val, 0, sizeof(val));
	/* Try to parse as an ISO8601/RFC3339 date */
	if (g_time_val_from_iso8601 (date_str, &val) != FALSE) {
		D(g_message ("Parsed duration '%s' using the ISO8601 parser", date_str));
		return val.tv_sec;
	}
	D(g_message ("Failed to parse duration '%s' using the ISO8601 parser", date_str));
	/* Fall back to RFC 2822 date parsing */
	return camel_header_decode_date (date_str, NULL);
#else
	WARN_NO_CAMEL;
#endif /* HAVE_CAMEL */
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
	     strcmp (mime_type, "application/xml") == 0)) {
		PlaylistIdenCallback func;
		guint i;

		func = NULL;

		for (i = 0; i < G_N_ELEMENTS(dual_types); i++) {
			const char *res;

			if (func == dual_types[i].iden)
				continue;
			func = dual_types[i].iden;
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
 * Checks if the first @len bytes of @data can be parsed, using the same checks
 * and precedences as totem_pl_parser_ignore().
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
				return retval;
			}
			return FALSE;
		}
	}

	D(g_message ("Is unsupported mime-type '%s'", mimetype));

	return FALSE;
}

/**
 * totem_pl_parser_can_parse_from_filename:
 * @filename: the file to check for parsability
 * @debug: %TRUE if debug statements should be printed
 *
 * Checks if the file can be parsed, using the same checks and precedences
 * as totem_pl_parser_ignore().
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

	g_mapped_file_free (map);

	return retval;
}

#ifndef TOTEM_PL_PARSER_MINI
GType
totem_pl_parser_metadata_get_type (void)
{
	static volatile gsize g_define_type_id__volatile = 0;
	if (g_once_init_enter (&g_define_type_id__volatile))
	{ 
		GType g_define_type_id = g_boxed_type_register_static (
		    g_intern_static_string ("TotemPlParserMetadata"),
		    (GBoxedCopyFunc) g_hash_table_ref,
		    (GBoxedFreeFunc) g_hash_table_unref);
		g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
	}
	return g_define_type_id__volatile;
}
#endif /* !TOTEM_PL_PARSER_MINI */

