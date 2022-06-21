/*
 * Copyright 2007-2022 Bastien Nocera <hadess@hadess.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <locale.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>

#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <stdlib.h>
#include <time.h>

#include "totem-pl-parser.h"
#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-private.h"
#include "utils.h"

gboolean option_debug = FALSE;
gboolean option_force = FALSE;
gboolean option_disable_unsafe = FALSE;
gboolean option_no_recurse = FALSE;
char *option_base_uri = NULL;

static GMainLoop *loop = NULL;
static char **uris = NULL;
static gboolean http_supported = FALSE;

typedef struct {
	int count;
	GMainLoop *mainloop;
	char *uri;
} AsyncParseData;

static char *
test_relative_real (const char *uri, const char *output)
{
	GFile *output_file;
	char *base;

	output_file = g_file_new_for_commandline_arg (output);
	base = totem_pl_parser_relative (output_file, uri);
	g_object_unref (output_file);

	return base;
}

static void
test_relative (void)
{
	g_assert_cmpstr (test_relative_real ("/home/hadess/test/test file.avi", "/home/hadess/foobar.m3u"), ==, "test/test file.avi");
	g_assert_cmpstr (test_relative_real ("file:///home/hadess/test/test%20file.avi", "/home/hadess/whatever.m3u"), ==, "test/test file.avi");
	g_assert_cmpstr (test_relative_real ("smb://server/share/file.mp3", "/home/hadess/whatever again.m3u"), ==, NULL);
	g_assert_cmpstr (test_relative_real ("smb://server/share/file.mp3", "smb://server/share/file.m3u"), ==, "file.mp3");
	g_assert_cmpstr (test_relative_real ("/home/hadess/test.avi", "/home/hadess/test/file.m3u"), ==, NULL);
	g_assert_cmpstr (test_relative_real ("http://foobar.com/test.avi", "/home/hadess/test/file.m3u"), ==, NULL);
	g_assert_cmpstr (test_relative_real ("file:///home/jan.old.old/myfile.avi", "file:///home/jan/myplaylist.m3u"), ==, NULL);
	g_assert_cmpstr (test_relative_real ("/1", "/test"), ==, "1");
}

static char *
test_resolution_real (const char *base_uri,
		      const char *relative_uri)
{
	GFile *base_gfile;
	char *ret;

	if (base_uri == NULL)
		base_gfile = NULL;
	else
		base_gfile = g_file_new_for_commandline_arg (base_uri);

	ret = totem_pl_parser_resolve_uri (base_gfile, relative_uri);
	if (base_gfile)
		g_object_unref (base_gfile);

	return ret;
}

static void
test_resolution (void)
{
	if (http_supported == FALSE) {
		g_test_message ("HTTP support required to test resolution");
		return;
	}

	if (g_strcmp0 (g_content_type_guess ("foo.html", NULL, 0, NULL), "text/html") != 0) {
		g_test_message ("“foo.html” isn't detected as text/html, is shared-mime-info installed?");
		g_test_fail ();
		return;
	}

	/* http://bugzilla.gnome.org/show_bug.cgi?id=555417 */
	g_assert_cmpstr (test_resolution_real ("http://www.yle.fi/player/player.jsp", "288629.asx?s=1000"), ==, "http://www.yle.fi/player/288629.asx?s=1000");
	g_assert_cmpstr (test_resolution_real ("http://www.yle.fi/player/player.jsp?actionpage=3&id=288629&locale", "288629.asx?s=1000"), ==, "http://www.yle.fi/player/288629.asx?s=1000");
	/* http://bugzilla.gnome.org/show_bug.cgi?id=577547 */
	g_assert_cmpstr (test_resolution_real ("http://localhost:12345/8.html", "anim.png"), ==, "http://localhost:12345/anim.png");
	g_assert_cmpstr (test_resolution_real (NULL, "http://foobar.com/anim.png"), ==, "http://foobar.com/anim.png");
	g_assert_cmpstr (test_resolution_real ("http://foobar.com/", "/anim.png"), ==, "http://foobar.com/anim.png");
	g_assert_cmpstr (test_resolution_real ("http://foobar.com/", "anim.png"), ==, "http://foobar.com/anim.png");
	g_assert_cmpstr (test_resolution_real ("http://foobar.com", "anim.png"), ==, "http://foobar.com/anim.png");
	g_assert_cmpstr (test_resolution_real ("/foobar/test/", "anim.png"), ==, "file:///foobar/test/anim.png");
}

static void
test_duration (void)
{
	gboolean verbose = g_test_verbose ();

	g_assert_cmpint (totem_pl_parser_parse_duration ("500", verbose), ==, 500);
	g_assert_cmpint (totem_pl_parser_parse_duration ("01:01", verbose), ==, 61);
	g_assert_cmpint (totem_pl_parser_parse_duration ("00:00:00.01", verbose), ==, 1);
	g_assert_cmpint (totem_pl_parser_parse_duration ("01:00:01.01", verbose), ==, 3601);
	g_assert_cmpint (totem_pl_parser_parse_duration ("01:00.01", verbose), ==, 60);
	g_assert_cmpint (totem_pl_parser_parse_duration ("24.59", verbose), ==, 1499);
	g_assert_cmpint (totem_pl_parser_parse_duration ("02m25s", verbose), ==, 145);
	g_assert_cmpint (totem_pl_parser_parse_duration ("2m25s", verbose), ==, 145);
}

static void
test_date (void)
{
	gboolean verbose = g_test_verbose ();

	/* RSS */
	g_assert_cmpuint (totem_pl_parser_parse_date ("28 Mar 2007 10:28:18 GMT", verbose), ==, 1175077698);
	g_assert_cmpuint (totem_pl_parser_parse_date ("01 may 2007 12:34:19 GMT", verbose), ==, 1178022859);

	/* Atom */
	g_assert_cmpuint (totem_pl_parser_parse_date ("2003-12-13T18:30:02Z", verbose), ==, 1071340202);
	g_assert_cmpuint (totem_pl_parser_parse_date ("1990-12-31T15:59:59-08:00", verbose), ==, 662687999);
}

#define READ_CHUNK_SIZE 8192
#define MIME_READ_CHUNK_SIZE 1024

static char *
test_data_get_data (const char *uri, guint *len)
{
	gsize bytes_read;
	GFileInputStream *stream;
	GFile *file;
	GError *error = NULL;
	char *buffer;
	gboolean res;

	*len = 0;

	file = g_file_new_for_commandline_arg (uri);

	/* Open the file. */
	stream = g_file_read (file, NULL, &error);
	if (stream == NULL) {
		GFile *dir;

		/* Try to open the relative path in the source dir */
		g_object_unref (file);
		dir = g_file_new_for_path (TEST_SRCDIR);
		file = g_file_get_child (dir, uri);
		g_object_unref (dir);
		stream = g_file_read (file, NULL, NULL);
		if (stream == NULL) {
			g_object_unref (file);
			g_test_message ("URI '%s' couldn't be opened in test_data_get_data: '%s'", uri, error->message);
			g_error_free (error);
			return NULL;
		}
	}
	g_object_unref (file);

	buffer = g_malloc (MIME_READ_CHUNK_SIZE);
	res = g_input_stream_read_all (G_INPUT_STREAM (stream), buffer, MIME_READ_CHUNK_SIZE, &bytes_read, NULL, &error);
	g_object_unref (G_INPUT_STREAM (stream));
	if (res == FALSE) {
		g_test_message ("URI '%s' couldn't be read or closed in _get_mime_type_with_data: '%s'", uri, error->message);
		g_error_free (error);
		g_free (buffer);
		return NULL;
	}

	/* Return the file null-terminated. */
	buffer = g_realloc (buffer, bytes_read + 1);
	buffer[bytes_read] = '\0';
	*len = bytes_read;

	return buffer;
}

static void
test_videosite (void)
{
	const char *uri = "http://www.youtube.com/watch?v=oMLCrzy9TEs";

	g_test_message ("Testing data parsing \"%s\"...", uri);
	g_assert_true (totem_pl_parser_can_parse_from_uri (uri, TRUE));
}

static void
test_parsability (void)
{
	guint i;
	struct {
		const char *uri;
		gboolean parsable;
		gboolean parsable_by_data;
		gboolean slow;
	} const files[] = {
		/* NOTE: For relative paths, don't add a protocol. */
		{ TEST_SRCDIR "560051.xml", TRUE, TRUE, FALSE },
		{ "itms://ax.itunes.apple.com/WebObjects/MZStore.woa/wa/viewPodcast?id=271121520&ign-mscache=1", TRUE, FALSE, TRUE },
		{ "http://phobos.apple.com/WebObjects/MZStore.woa/wa/viewPodcast?id=271121520", TRUE, FALSE, TRUE },
		{ "file:///tmp/file_doesnt_exist.wmv", FALSE, TRUE, FALSE },
		{ "http://live.hujjat.org:7860/main", FALSE, TRUE, TRUE },
		{ "http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx", TRUE, FALSE, TRUE },
		{ TEST_SRCDIR "HackerMedley", TRUE, TRUE, FALSE }, /* From https://bugzilla.redhat.com/show_bug.cgi?id=582850 and http://feeds.feedburner.com/HackerMedley */
		{ "http://faif.us/feeds/cast-mp3", TRUE, TRUE, TRUE },
		{ NULL,  FALSE, FALSE }
	};

	if (http_supported == FALSE) {
		g_test_message ("HTTP support required to test parseability");
		return;
	}

	/* Loop through the list, downloading the URIs and checking for parsability */
	for (i = 0; files[i].uri != NULL; ++i) {
		gboolean parsable;
		char *data;
		guint len;

		/* Slow test! */
		if (files[i].slow && !g_test_slow ())
			continue;

		/* Skip URIs that aren't available anymore, we can't load data from there... */
		if (files[i].parsable_by_data == FALSE)
			continue;

		g_test_message ("Testing data parsing \"%s\"...", files[i].uri);

		data = test_data_get_data (files[i].uri, &len);
		if (files[i].parsable == TRUE)
			g_assert (data != NULL);
		else if (data == NULL)
			continue;

		parsable = totem_pl_parser_can_parse_from_data (data, len, TRUE);
		g_free (data);

		if (parsable != files[i].parsable) {
			g_test_message ("Failed to parse '%s' (idx %d)", files[i].uri, i);
			g_assert_not_reached ();
		}
	}

	/* Loop through again by filename */
	for (i = 0; files[i].uri != NULL; ++i) {
		/* Slow test! */
		if (files[i].slow && !g_test_slow ())
			continue;
		if (!g_str_has_prefix (files[i].uri, "file://") &&
		    *files[i].uri != '/') {
			continue;
		}

		g_test_message ("Testing filename parsing \"%s\"...", files[i].uri);
		g_assert_cmpint (totem_pl_parser_can_parse_from_filename (files[i].uri, TRUE), ==, files[i].parsable);
	}
}

static void
cancelled_parse_async_ready (GObject *pl, GAsyncResult *result, gpointer userdata)
{
	AsyncParseData *data = userdata;
	TotemPlParserResult retval;
	g_autoptr(GError) error = NULL;

	retval = totem_pl_parser_parse_finish (TOTEM_PL_PARSER (pl), result, &error);
	g_assert_cmpint (retval, ==, TOTEM_PL_PARSER_RESULT_CANCELLED);
	g_assert_no_error (error);

	data->count = 0xFEED;

	g_main_loop_quit (data->mainloop);
	g_object_unref (pl);
}

static void
test_cancelled_parsing (void)
{
	TotemPlParser *pl = totem_pl_parser_new ();
	GCancellable *cancellable;
	AsyncParseData data;

	cancellable = g_cancellable_new ();

	g_object_set (pl, "recurse", FALSE,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	data.uri = get_relative_uri (TEST_SRCDIR "separator.m3u");
	data.mainloop = g_main_loop_new (NULL, FALSE);
	data.count = 0;

	totem_pl_parser_parse_async (pl, data.uri, FALSE, cancellable, cancelled_parse_async_ready, &data);
	g_cancellable_cancel (cancellable);
	g_main_loop_run (data.mainloop);

	g_free (data.uri);
	g_assert_cmpint (data.count, ==, 0xFEED);
}

static void
test_youtube_starttime (void)
{
	const char *uri;

	/* old type of direct link */
	uri = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc#t=2m30s";
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_STARTTIME), ==, "150");

	/* new type of embed */
	uri = "http://www.youtube.com/embed/Nc9xq-TVyHI?start=110";
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_STARTTIME), ==, "110");

	/* new type of direct link */
	uri = "http://www.youtube.com/watch?v=Fk2bUvrv-Uc&t=2m30s";
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_STARTTIME), ==, "150");
}

static void
test_m3u_audio_track (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "radios-freebox.m3u");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_AUDIO_TRACK), ==, "1");
	g_free (uri);
}

static void
test_m3u_relative (void)
{
	char *uri, *file_uri;

	uri = get_relative_uri (TEST_SRCDIR "relative.m3u");
	file_uri = get_relative_uri (TEST_SRCDIR "3gpp-file.mp4");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, file_uri);
	g_free (uri);
	g_free (file_uri);
}

static void
test_pl_content_type (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "old-lastfm-output.xspf");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "application/xspf+xml");
	g_free (uri);
}

static void
test_lastfm_parsing (void)
{
	char *uri;

	g_test_bug ("625823");

	uri = get_relative_uri (TEST_SRCDIR "old-lastfm-output.xspf");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_DOWNLOAD_URI), ==, "http://freedownloads.last.fm/download/188024406/Kondratiev%2BWinter.mp3");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_ID), ==, "d092a");
	g_free (uri);

	uri = get_relative_uri (TEST_SRCDIR "new-lastfm-output.xspf");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_DOWNLOAD_URI), ==, "http://freedownloads.last.fm/download/402599273/Yellow.mp3");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_ID), ==, "20a82");
	g_free (uri);
}

static void
test_m3u_separator (void)
{
	char *uri;

	g_test_bug ("609091");

	uri = get_relative_uri (TEST_SRCDIR "separator.m3u");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_TITLE), ==, "Music Tech Sessions (Friday 22 January 2010 20:00 - 00:00)");
	g_free (uri);
}

static void
test_parsing_xspf_genre (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "playlist.xspf");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Test Genre");
	g_free (uri);

	uri = get_relative_uri (TEST_SRCDIR "decrypted-amazon-track.xspf");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Dance & DJ/House");
	g_free (uri);
}

static void
test_parsing_xspf_escaping (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "playlist.xspf");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "http://207.200.96.226:8000 extraparam=1");
	g_free (uri);
}

static void
test_parsing_xspf_metadata (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "playlist.xspf");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE), ==, "Test Playlist");
	g_free (uri);
}

static void
test_parsing_xspf_xml_base (void)
{
	char *uri;

	return;

	uri = get_relative_uri (TEST_SRCDIR "xml-base.xspf");
	/* FIXME: The URL is incorrect here as we're not on HTTP, but
	 * the parsing is incorrect as well, as we ignore xml:base:
	 * http://wiki.xiph.org/index.php/XSPF_v1_Notes_and_Errata#xml:base */
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "http://example.org/three/four");
	g_free (uri);
}

static void
test_smi_starttime (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "big5.smi");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_STARTTIME), ==, "00:04:00");
	g_free (uri);
}

static void
test_m3u_leading_tabs (void)
{
	char *uri;
	/* From http://media.artistserver.com/tracks/23985/21898/1/1/1/O_G_Money_-_Girl_Gotta_girlfriend_Feat._O_G_Money_Snoop_Dogg.m3u */
	uri = get_relative_uri (TEST_SRCDIR "O_G_Money_Snoop_Dogg.m3u");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_TITLE), ==, "O G Money - Girl Gotta girlfriend Feat. O G Money, Snoop Dogg");
	g_free (uri);
}

static void
test_directory_recurse (void)
{
	char *uri, *path;

	uri = get_relative_uri (TEST_SRCDIR "foo");
	path = g_filename_from_uri (uri, NULL, NULL);
	if (g_file_test (path, G_FILE_TEST_IS_DIR)) {
		/* The file inside the directory will be ignored */
		g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_TITLE), ==, NULL);
		/* But the parsing will succeed */
		g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	}
	g_free (path);
	g_free (uri);
}

static void
test_empty_asx (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "empty-asx.asx");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_empty_pls (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "emptyplaylist.pls");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_free (uri);
}

static void
test_parsing_rtsp_text_multi (void)
{
	char *uri;
	g_test_bug ("602127");
	uri = get_relative_uri (TEST_SRCDIR "602127.qtl");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "rtsp://host.org/video.mp4");
	g_free (uri);
}

static void
test_parsing_rtsp_text (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "single-line.qtl");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "rtsp://host.org/video.mp4");
	g_free (uri);
}

static void
test_parsing_hadess (void)
{
	if (g_strcmp0 (g_get_user_name (), "hadess") == 0)
		g_assert_cmpint (simple_parser_test ("file:///home/hadess/Videos"), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
}

static void
test_parsing_nonexistent_files (void)
{
	g_test_bug ("330120");
	g_assert_cmpint (simple_parser_test ("file:///tmp/file_doesnt_exist.wmv"), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
}

static void
test_parsing_broken_asx (void)
{
	TotemPlParserResult result;

	/* FIXME
	 * URL is gone now */
	return;

	/* Slow test! */
	if (!g_test_slow ())
		return;

	if (http_supported == FALSE) {
		g_test_message ("HTTP support required to test broken ASX");
		return;
	}

	g_test_bug ("323683");
	result = simple_parser_test ("http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx");
	g_assert_cmpint (result, !=, TOTEM_PL_PARSER_RESULT_ERROR);
}

static void
test_parsing_out_of_order_asx (void)
{
	char *uri;
	gboolean result;

	/* From http://82.135.234.195/pukas.wax */
	uri = get_relative_uri (TEST_SRCDIR "pukas.wax");
	result = parser_test_get_order_result (uri);
	g_free (uri);
	g_assert_true (result);
}

static void
test_parsing_out_of_order_xspf (void)
{
	char *uri;
	gboolean result;

	uri = get_relative_uri (TEST_SRCDIR "new-lastfm-output.xspf");
	result = parser_test_get_order_result (uri);
	g_free (uri);
	g_assert_true (result);
}

static void
test_parsing_num_entries (void)
{
	char *uri;
	guint num;

	uri = get_relative_uri (TEST_SRCDIR "missing-items.pls");
	num = parser_test_get_num_entries (uri);
	g_free (uri);
	g_assert_cmpint (num, ==, 19);
}

/*
static void
test_parsing_404_error (void)
{
	if (http_supported == FALSE) {
		g_test_message ("HTTP support required to test 404");
		return;
	}

	g_test_bug ("158052");
	g_assert_cmpint (simple_parser_test ("http://live.hujjat.org:7860/main"), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
}
*/

static void
test_parsing_3gpp_not_ignored (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "3gpp-file.mp4");
	g_test_bug ("594359@bugs.debian.org");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_free (uri);
}

static void
test_parsing_ts_not_ignored (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "dont-ignore-mp2t.ts");
	g_test_bug ("678163");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_free (uri);
}

static void
test_parsing_mp4_is_flv (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "really-flv.mp4");
	g_test_bug ("620039");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_free (uri);
}

static void
test_parsing_live_streaming (void)
{
	char *uri;
	g_test_bug ("594036");
	/* File from http://tools.ietf.org/html/draft-pantos-http-live-streaming-02#section-7.1 */
	uri = get_relative_uri (TEST_SRCDIR "live-streaming.m3u");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
	g_free (uri);
}

static void
test_parsing_m3u_streaming (void)
{
	char *uri;
	g_test_bug ("695652");

	/* File from http://radioclasica.rtve.stream.flumotion.com/rtve/radioclasica.mp3.m3u */
	uri = get_relative_uri (TEST_SRCDIR "radioclasica.mp3.m3u");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_parsing_not_asx_playlist (void)
{
	char *uri;
	g_test_bug ("610471");
	/* File from https://bugzilla.gnome.org/show_bug.cgi?id=610471#c0 */
	uri = get_relative_uri (TEST_SRCDIR "asf-with-asx-suffix.asx");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_parsing_wma_asf (void)
{
	char *uri;
	g_test_bug ("639958");
	/* File from https://bugzilla.gnome.org/show_bug.cgi?id=639958#c5 */
	uri = get_relative_uri (TEST_SRCDIR "WMA9.1_98_quality_48khz_vbr_s.wma");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "mmsh://195.134.224.231/wma/WMA9.1_98_quality_48khz_vbr_s.wma?MSWMExt=.asf");
	g_free (uri);
}

static void
test_parsing_not_really_php (void)
{
	char *uri;
	g_test_bug ("590722");
	/* File from http://startwars.org/dump/remote_xspf.php */
	uri = get_relative_uri (TEST_SRCDIR "remote_xspf.php");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_parsing_remote_mp3 (void)
{
	g_test_bug ("19");
	/* URL from https://gitlab.gnome.org/GNOME/totem-pl-parser/issues/19 */
	g_assert_cmpint (simple_parser_test ("http://feeds.soundcloud.com/stream/303432626-opensourcesecuritypodcast-episode-28-rsa-conference-2017.mp3"), ==, TOTEM_PL_PARSER_RESULT_UNHANDLED);
}

static void
test_parsing_not_really_php_but_html_instead (void)
{
	char *uri;
	g_test_bug ("624341");
	/* File from http://www.novabrasilfm.com.br/ao-vivo/audio.php */
	uri = get_relative_uri (TEST_SRCDIR "audio.php");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_IGNORED);
	g_free (uri);
}

static void
parse_async_ready (GObject *pl, GAsyncResult *result, gpointer userdata)
{
	AsyncParseData *data = userdata;
	TotemPlParserResult retval;

	retval = totem_pl_parser_parse_finish (TOTEM_PL_PARSER (pl), result, NULL);
	g_test_message ("Got retval %d for uri '%s'", retval, data->uri);
	g_test_message ("Parsed entry count is %d for uri '%s'", data->count, data->uri);

	g_assert_cmpint (data->count, >, 0);

	g_main_loop_quit (data->mainloop);
	g_object_unref (pl);
}

static gboolean
block_main_loop_idle (gpointer data)
{
	/* one second should be long enough to parse a local file */
	g_usleep (G_USEC_PER_SEC);
	return FALSE;
}

static void
entry_parsed_num_cb (TotemPlParser *parser,
		     const char *uri,
		     GHashTable *metadata,
		     guint *ret)
{
	*ret = *ret + 1;
}

static void
test_async_parsing_signal_order (void)
{
	AsyncParseData data;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_test_bug ("631727");
	/* doesn't really matter what test file we use here, we just want something
	 * with at least one valid entry in it.
	 */
	data.uri = get_relative_uri (TEST_SRCDIR "separator.m3u");
	data.mainloop = g_main_loop_new (NULL, FALSE);
	data.count = 0;

	g_object_set (pl, "recurse", FALSE,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "entry-parsed",
			  G_CALLBACK (entry_parsed_num_cb), &data.count);

	/* we need the mainloop to be busy while the signal emission idle sources are
	 * added, so we just block it for a while.
	 */
	g_idle_add_full (G_PRIORITY_HIGH, block_main_loop_idle, NULL, NULL);
	totem_pl_parser_parse_async (pl, data.uri, FALSE, NULL, parse_async_ready, &data);
	g_main_loop_run (data.mainloop);

	/* The number of entries in separator.m3u */
	g_assert(data.count == 1);

	g_free (data.uri);
	g_main_loop_unref (data.mainloop);
}

static void
add_pl_iter_metadata (TotemPlPlaylist     *playlist,
		      TotemPlPlaylistIter *pl_iter)
{
	totem_pl_playlist_set (playlist, pl_iter,
			       TOTEM_PL_PARSER_FIELD_URI, "file:///fake/uri.mp4",
			       TOTEM_PL_PARSER_FIELD_TITLE, "custom title",
			       TOTEM_PL_PARSER_FIELD_SUBTITLE_URI, "file:///fake/uri.srt",
			       TOTEM_PL_PARSER_FIELD_PLAYING, "true",
			       TOTEM_PL_PARSER_FIELD_CONTENT_TYPE, "video/mp4",
			       TOTEM_PL_PARSER_FIELD_STARTTIME, "1",
			       NULL);
}

static void
test_saving_sync (void)
{
	g_autoptr(TotemPlParser) parser = NULL;
	g_autoptr(TotemPlPlaylist) playlist = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) output = NULL;
	TotemPlPlaylistIter pl_iter;
	g_autofree char *path = NULL;
	int fd;

	parser = totem_pl_parser_new ();
	playlist = totem_pl_playlist_new ();
	fd = g_file_open_tmp (NULL, &path, &error);
	g_assert_no_error (error);
	close (fd);
	output = g_file_new_for_path (path);

	totem_pl_parser_save (parser,
			      playlist,
			      output,
			      NULL, TOTEM_PL_PARSER_XSPF, &error);
	g_assert_error (error, TOTEM_PL_PARSER_ERROR, TOTEM_PL_PARSER_ERROR_EMPTY_PLAYLIST);
	g_clear_error (&error);

	totem_pl_playlist_append (playlist, &pl_iter);
	add_pl_iter_metadata (playlist, &pl_iter);
	totem_pl_parser_save (parser,
			      playlist,
			      output,
			      NULL, TOTEM_PL_PARSER_XSPF, &error);
	g_assert_no_error (error);
}

static void
test_saving_sync_cb (GObject       *source_object,
		     GAsyncResult  *res,
		     gpointer       user_data)
{
	GMainLoop *loop = user_data;
	g_autoptr(GError) error = NULL;

	totem_pl_parser_save_finish (TOTEM_PL_PARSER (source_object), res, &error);
	g_assert_no_error (error);
	g_main_loop_quit (loop);
}

static void
test_saving_async (void)
{
	g_autoptr(TotemPlParser) parser = NULL;
	g_autoptr(TotemPlPlaylist) playlist = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) output = NULL;
	TotemPlPlaylistIter pl_iter;
	g_autofree char *path = NULL;
	GMainLoop *loop;
	int fd;

	parser = totem_pl_parser_new ();
	playlist = totem_pl_playlist_new ();
	fd = g_file_open_tmp (NULL, &path, &error);
	g_assert_no_error (error);
	close (fd);
	output = g_file_new_for_path (path);

	totem_pl_playlist_append (playlist, &pl_iter);
	add_pl_iter_metadata (playlist, &pl_iter);
	loop = g_main_loop_new (NULL, FALSE);
	totem_pl_parser_save_async (parser,
				    playlist,
				    output,
				    NULL, TOTEM_PL_PARSER_XSPF,
				    NULL, test_saving_sync_cb, loop);
	g_main_loop_run (loop);
}

static void
test_saving_parsing_xspf_title (void)
{
	g_autoptr(TotemPlParser) parser = NULL;
	g_autoptr(TotemPlPlaylist) playlist = NULL;
	g_autoptr(GError) error = NULL;
	g_autoptr(GFile) output = NULL;
	TotemPlPlaylistIter pl_iter;
	g_autofree char *path = NULL;
	int fd;
	char *uri;
	char *title;

	parser = totem_pl_parser_new ();
	playlist = totem_pl_playlist_new ();
	fd = g_file_open_tmp (NULL, &path, &error);
	g_assert_no_error (error);
	close (fd);
	output = g_file_new_for_path (path);
	uri = g_file_get_uri (output);

	/* save empty playlist file and check empty title */
	{
		totem_pl_parser_save (parser,
				      playlist,
				      output,
				      NULL, TOTEM_PL_PARSER_XSPF, &error);
		g_assert_error (error, TOTEM_PL_PARSER_ERROR, TOTEM_PL_PARSER_ERROR_EMPTY_PLAYLIST);
		g_clear_error (&error);
		title = parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE);
		g_assert_cmpstr (title, ==, NULL);
	}

	/* save non-empty playlist file with empty title and check empty title */
	{
		totem_pl_playlist_append (playlist, &pl_iter);
		add_pl_iter_metadata (playlist, &pl_iter);
		totem_pl_parser_save (parser,
				      playlist,
				      output,
				      NULL, TOTEM_PL_PARSER_XSPF, &error);
		g_assert_no_error (error);
		title = parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE);
		g_assert_cmpstr (title, ==, NULL);
	}

	/* save non-empty playlist file with non-empty title and check non-empty title */
	{
		totem_pl_parser_save (parser,
				      playlist,
				      output,
				      "샘플 재생 목록 제목", TOTEM_PL_PARSER_XSPF, &error);
		g_assert_no_error (error);
		title = parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE);
		g_assert_cmpstr (title, ==, "샘플 재생 목록 제목");
	}

	g_free (uri);
}

#define MAX_DESCRIPTION_LEN 128
#define DATE_BUFSIZE 512
#define PRINT_DATE_FORMAT "%Y-%m-%dT%H:%M:%SZ"

static void
entry_metadata_foreach (const char *key, const char *value, gpointer data)
{
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_URI) == 0)
		return;
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_DESCRIPTION) == 0
	    && strlen (value) > MAX_DESCRIPTION_LEN) {
	    	char *tmp = g_strndup (value, MAX_DESCRIPTION_LEN), *s;
	    	for (s = tmp; s - tmp < MAX_DESCRIPTION_LEN; s++)
	    		if (*s == '\n' || *s == '\r') {
	    			*s = '\0';
	    			break;
			}
	    	g_message ("\t%s = '%s' (truncated)", key, tmp);
		g_free (tmp);
	    	return;
	}
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_PUB_DATE) == 0) {
		struct tm *tm;
		guint64 res;
		char res_str[DATE_BUFSIZE];

		res = totem_pl_parser_parse_date (value, option_debug);
		if (res != (guint64) -1) {
			tm = gmtime ((time_t *) &res);
			strftime ((char *) &res_str, DATE_BUFSIZE, PRINT_DATE_FORMAT, tm);

			g_message ("\t%s = '%s' (%"G_GUINT64_FORMAT"/'%s')", key, res_str, res, value);
		} else {
			g_message ("\t%s = '%s' (date parsing failed)", key, value);
		}
		return;
	}
	if (g_ascii_strcasecmp (key, TOTEM_PL_PARSER_FIELD_STARTTIME) == 0) {
		gint64 res;

		res = totem_pl_parser_parse_duration (value, option_debug);
		if (res == -1)
			g_message ("\t%s = '%s' (duration parsing failed)", key, value);
		else
			g_message ("\t%s = '%s' (%"G_GINT64_FORMAT" sec)", key, value, res);

		return;
	}

	g_message ("\t%s = '%s'", key, value);
}

static void
entry_parsed (TotemPlParser *parser, const char *uri, GHashTable *metadata, gpointer data)
{
	g_message ("Added URI \"%s\"...", uri);
	g_hash_table_foreach (metadata, (GHFunc) entry_metadata_foreach, NULL);
}

static void
test_parsing_real (TotemPlParser *pl, const char *uri)
{
	TotemPlParserResult res;

	res = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	if (res != TOTEM_PL_PARSER_RESULT_SUCCESS) {
		switch (res) {
		case TOTEM_PL_PARSER_RESULT_UNHANDLED:
			g_message ("URI \"%s\" unhandled.", uri);
			break;
		case TOTEM_PL_PARSER_RESULT_ERROR:
			g_message ("Error handling URI \"%s\".", uri);
			break;
		case TOTEM_PL_PARSER_RESULT_IGNORED:
			g_message ("Ignored URI \"%s\".", uri);
			break;
		case TOTEM_PL_PARSER_RESULT_CANCELLED:
			g_message ("Cancelled URI \"%s\".", uri);
			break;
		case TOTEM_PL_PARSER_RESULT_SUCCESS:
		default:
			g_assert_not_reached ();
			;;
		}
	}
}

static gboolean
push_parser (gpointer data)
{
	guint i;
	TotemPlParser *pl = TOTEM_PL_PARSER (data);

	for (i = 0; uris[i] != NULL; ++i)
		test_parsing_real (pl, uris[i]);

	g_main_loop_quit (loop);

	return FALSE;
}

static void
playlist_started (TotemPlParser *parser, const char *uri, GHashTable *metadata)
{
	g_message ("Started playlist \"%s\"...", uri);
	g_hash_table_foreach (metadata, (GHFunc) entry_metadata_foreach, NULL);
}

static void
playlist_ended (TotemPlParser *parser, const char *uri)
{
	g_message ("Playlist \"%s\" ended.", uri);
}

static void
test_parsing (void)
{
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);
	g_signal_connect (G_OBJECT (pl), "entry-parsed", G_CALLBACK (entry_parsed), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-started", G_CALLBACK (playlist_started), NULL);
	g_signal_connect (G_OBJECT (pl), "playlist-ended", G_CALLBACK (playlist_ended), NULL);

	g_idle_add (push_parser, pl);
	loop = g_main_loop_new (NULL, FALSE);
	g_main_loop_run (loop);
}

int
main (int argc, char *argv[])
{
	GError *error = NULL;
	GOptionContext *context;
	const GOptionEntry entries[] = {
		{ "no-recurse", 'n', 0, G_OPTION_ARG_NONE, &option_no_recurse, "Disable recursion", NULL },
		{ "debug", 'd', 0, G_OPTION_ARG_NONE, &option_debug, "Enable debug", NULL },
		{ "force", 'f', 0, G_OPTION_ARG_NONE, &option_force, "Force parsing", NULL },
		{ "disable-unsafe", 'u', 0, G_OPTION_ARG_NONE, &option_disable_unsafe, "Disabling unsafe playlist-types", NULL },
		{ "base-uri", 'b', 0, G_OPTION_ARG_STRING, &option_base_uri, "Base URI from which to resolve relative items", NULL },
		{ G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &uris, NULL, "[URI...]" },
		{ NULL }
	};

	setlocale (LC_ALL, "");

	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* Parse our own command-line options */
	context = g_option_context_new ("- test parser functions");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("Option parsing failed: %s\n", error->message);
		return 1;
	}

	if (option_debug) {
		g_setenv ("G_MESSAGES_DEBUG", "all", TRUE);
	}

	/* If we've been given no URIs, run the static tests */
	if (uris == NULL) {
		http_supported = check_http ();
		g_setenv ("TOTEM_PL_PARSER_VIDEOSITE_SCRIPT", TEST_SRCDIR "/videosite-tester.sh", TRUE);

		option_debug = TRUE;

		g_test_add_func ("/parser/duration", test_duration);
		g_test_add_func ("/parser/date", test_date);
		g_test_add_func ("/parser/relative", test_relative);
		g_test_add_func ("/parser/resolution", test_resolution);
		g_test_add_func ("/parser/parsability", test_parsability);
		g_test_add_func ("/parser/cancelled", test_cancelled_parsing);
		g_test_add_func ("/parser/m3u_relative", test_m3u_relative);
		g_test_add_func ("/parser/m3u_audio_track", test_m3u_audio_track);
		g_test_add_func ("/parser/parsing/hadess", test_parsing_hadess);
		g_test_add_func ("/parser/parsing/nonexistent_files", test_parsing_nonexistent_files);
		g_test_add_func ("/parser/parsing/broken_asx", test_parsing_broken_asx);
		/* Disabled, as the host is unavailable most of the time
		 * g_test_add_func ("/parser/parsing/404_error", test_parsing_404_error); */
		g_test_add_func ("/parser/parsing/3gpp_not_ignored", test_parsing_3gpp_not_ignored);
		g_test_add_func ("/parser/parsing/parsing_ts_not_ignored", test_parsing_ts_not_ignored);
		g_test_add_func ("/parser/parsing/mp4_is_flv", test_parsing_mp4_is_flv);
		g_test_add_func ("/parser/parsing/out_of_order_asx", test_parsing_out_of_order_asx);
		g_test_add_func ("/parser/parsing/out_of_order_xspf", test_parsing_out_of_order_xspf);
		g_test_add_func ("/parser/parsing/multi_line_rtsptext", test_parsing_rtsp_text_multi);
		g_test_add_func ("/parser/parsing/single_line_rtsptext", test_parsing_rtsp_text);
		g_test_add_func ("/parser/parsing/live_streaming", test_parsing_live_streaming);
		g_test_add_func ("/parser/parsing/m3u_streaming", test_parsing_m3u_streaming);
		g_test_add_func ("/parser/videosite", test_videosite);
		g_test_add_func ("/parser/parsing/youtube_starttime", test_youtube_starttime);
		g_test_add_func ("/parser/parsing/not_asx_playlist", test_parsing_not_asx_playlist);
		g_test_add_func ("/parser/parsing/not_really_php", test_parsing_not_really_php);
		g_test_add_func ("/parser/parsing/not_really_php_but_html_instead", test_parsing_not_really_php_but_html_instead);
		g_test_add_func ("/parser/parsing/num_items_in_pls", test_parsing_num_entries);
		g_test_add_func ("/parser/parsing/xspf_genre", test_parsing_xspf_genre);
		g_test_add_func ("/parser/parsing/xspf_escaping", test_parsing_xspf_escaping);
		g_test_add_func ("/parser/parsing/xspf_metadata", test_parsing_xspf_metadata);
		g_test_add_func ("/parser/parsing/xspf_xml_base", test_parsing_xspf_xml_base);
		g_test_add_func ("/parser/parsing/test_pl_content_type", test_pl_content_type);
		g_test_add_func ("/parser/parsing/lastfm-attributes", test_lastfm_parsing);
		g_test_add_func ("/parser/parsing/m3u_separator", test_m3u_separator);
		g_test_add_func ("/parser/parsing/smi_starttime", test_smi_starttime);
		g_test_add_func ("/parser/parsing/m3u_leading_tabs", test_m3u_leading_tabs);
		g_test_add_func ("/parser/parsing/empty-asx.asx", test_empty_asx);
		g_test_add_func ("/parser/parsing/emptyplaylist.pls", test_empty_pls);
		g_test_add_func ("/parser/parsing/dir_recurse", test_directory_recurse);
		g_test_add_func ("/parser/parsing/async_signal_order", test_async_parsing_signal_order);
		g_test_add_func ("/parser/parsing/wma_asf", test_parsing_wma_asf);
		g_test_add_func ("/parser/parsing/remote_mp3", test_parsing_remote_mp3);
		g_test_add_func ("/parser/saving/parsing/xspf_title", test_saving_parsing_xspf_title);
		g_test_add_func ("/parser/saving/sync", test_saving_sync);
		g_test_add_func ("/parser/saving/async", test_saving_async);

		return g_test_run ();
	}

	/* Test the parser on all the given input URIs */
	test_parsing ();

	return 0;
}
