#include "config.h"

#include <locale.h>

#include <glib.h>
#include <gio/gio.h>

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>

#include "totem-pl-parser.h"
#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-private.h"

static GMainLoop *loop = NULL;
static gboolean option_no_recurse = FALSE;
static gboolean option_debug = FALSE;
static gboolean option_force = FALSE;
static gboolean option_disable_unsafe = FALSE;
static char *option_base_uri = NULL;
static char **uris = NULL;

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
	g_assert_cmpuint (totem_pl_parser_parse_date ("1990-12-31T15:59:60-08:00", verbose), ==, 662688000);
}

#define READ_CHUNK_SIZE 8192
#define MIME_READ_CHUNK_SIZE 1024

static char *
test_data_get_data (const char *uri, guint *len)
{
	gssize bytes_read;
	GFileInputStream *stream;
	GFile *file;
	GError *error = NULL;
	char *buffer;

	*len = 0;

	file = g_file_new_for_commandline_arg (uri);

	/* Open the file. */
	stream = g_file_read (file, NULL, &error);
	if (stream == NULL) {
		g_test_message ("URI '%s' couldn't be opened in test_data_get_data: '%s'", uri, error->message);
		g_error_free (error);
		return NULL;
	}

	buffer = g_malloc (MIME_READ_CHUNK_SIZE);
	bytes_read = g_input_stream_read (G_INPUT_STREAM (stream), buffer, MIME_READ_CHUNK_SIZE, NULL, &error);
	g_object_unref (G_INPUT_STREAM (stream));
	if (bytes_read == -1) {
		g_free (buffer);
		return NULL;
	}

	if (bytes_read == -1) {
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
test_parsability (void)
{
	guint i;
	struct {
		const char *uri;
		gboolean parsable;
		gboolean slow;
	} const files[] = {
		/* NOTE: For relative paths, don't add a protocol. */
		{ "560051.xml", TRUE, FALSE },
		{ "itms://ax.itunes.apple.com/WebObjects/MZStore.woa/wa/viewPodcast?id=271121520&ign-mscache=1", TRUE, TRUE },
		{ "http://phobos.apple.com/WebObjects/MZStore.woa/wa/viewPodcast?id=271121520", TRUE, TRUE },
		{ "file:///tmp/file_doesnt_exist.wmv", FALSE, FALSE },
		{ "http://live.hujjat.org:7860/main", FALSE, TRUE },
		{ "http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx", TRUE, TRUE },
		{ NULL,  FALSE, FALSE }
	};	

	/* Loop through the list, downloading the URIs and checking for parsability */
	for (i = 0; files[i].uri != NULL; ++i) {
		gboolean parsable;
		char *data;
		guint len;

		/* Slow test! */
		if (files[i].slow && !g_test_slow ())
			continue;

		g_test_message ("Testing \"%s\"...", files[i].uri);

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

		g_test_message ("Testing \"%s\"...", files[i].uri);
		g_assert (totem_pl_parser_can_parse_from_filename (files[i].uri, TRUE) == files[i].parsable);
	}
}

static TotemPlParserResult
simple_parser_test (const char *uri)
{
	TotemPlParserResult retval;
	TotemPlParser *pl = totem_pl_parser_new ();

	g_object_set (pl, "recurse", !option_no_recurse,
			  "debug", option_debug,
			  "force", option_force,
			  "disable-unsafe", option_disable_unsafe,
			  NULL);

	retval = totem_pl_parser_parse_with_base (pl, uri, option_base_uri, FALSE);
	g_object_unref (pl);

	return retval;
}

static void
test_parsing_hadess (void)
{
	g_assert (simple_parser_test ("file:///home/hadess/Movies") != TOTEM_PL_PARSER_RESULT_ERROR);
}

static void
test_parsing_nonexistent_files (void)
{
	g_test_bug ("330120");
	g_assert (simple_parser_test ("file:///tmp/file_doesnt_exist.wmv") == TOTEM_PL_PARSER_RESULT_ERROR);
}

static void
test_parsing_broken_asx (void)
{
	TotemPlParserResult result;

	/* Slow test! */
	if (!g_test_slow ())
		return;

	g_test_bug ("323683");
	result = simple_parser_test ("http://www.comedycentral.com/sitewide/media_player/videoswitcher.jhtml?showid=934&category=/shows/the_daily_show/videos/headlines&sec=videoId%3D36032%3BvideoFeatureId%3D%3BpoppedFrom%3D_shows_the_daily_show_index.jhtml%3BisIE%3Dfalse%3BisPC%3Dtrue%3Bpagename%3Dmedia_player%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bgateway%3Dshows%3Bsection_1%3Dthe_daily_show%3Bsection_2%3Dvideos%3Bsection_3%3Dheadlines%3Bzyg%3D%27%2Bif_nt_zyg%2B%27%3Bspan%3D%27%2Bif_nt_span%2B%27%3Bdemo%3D%27%2Bif_nt_demo%2B%27%3Bera%3D%27%2Bif_nt_era%2B%27%3Bbps%3D%27%2Bif_nt_bandwidth%2B%27%3Bfla%3D%27%2Bif_nt_Flash%2B%27&itemid=36032&clip=com/dailyshow/headlines/10156_headline.wmv&mswmext=.asx");
	g_assert (result != TOTEM_PL_PARSER_RESULT_ERROR);
}

static void
test_parsing_404_error (void)
{
	g_test_bug ("158052");
	g_assert (simple_parser_test ("http://live.hujjat.org:7860/main") == TOTEM_PL_PARSER_RESULT_ERROR);
}

static void
test_parsing_xml_head_comments (void)
{
	g_test_bug ("560051");
	g_assert (simple_parser_test ("file://" TEST_FILE_DIR "560051.xml") == TOTEM_PL_PARSER_RESULT_SUCCESS);
}

static void
test_parsing_xml_comment_whitespace (void)
{
	g_test_bug ("541405");
	g_assert (simple_parser_test ("file://" TEST_FILE_DIR "541405.xml") == TOTEM_PL_PARSER_RESULT_SUCCESS);
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

	g_type_init ();
	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	/* Parse our own command-line options */
	context = g_option_context_new ("- test parser functions");
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_print ("Option parsing failed: %s\n", error->message);
		return 1;
	}

	/* If we've been given no URIs, run the static tests */
	if (uris == NULL) {
		g_test_add_func ("/parser/duration", test_duration);
		g_test_add_func ("/parser/date", test_date);
		g_test_add_func ("/parser/relative", test_relative);
		g_test_add_func ("/parser/resolution", test_resolution);
		g_test_add_func ("/parser/parsability", test_parsability);
		g_test_add_func ("/parser/parsing/hadess", test_parsing_hadess);
		g_test_add_func ("/parser/parsing/nonexistent_files", test_parsing_nonexistent_files);
		g_test_add_func ("/parser/parsing/broken_asx", test_parsing_broken_asx);
		g_test_add_func ("/parser/parsing/404_error", test_parsing_404_error);
		g_test_add_func ("/parser/parsing/xml_head_comments", test_parsing_xml_head_comments);
		g_test_add_func ("/parser/parsing/xml_comment_whitespace", test_parsing_xml_comment_whitespace);

		return g_test_run ();
	}

	/* Test the parser on all the given input URIs */
	test_parsing ();

	return 0;
}
