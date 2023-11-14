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

static void
test_image_link (void)
{
	char *uri;

	/* From http://www.101greatgoals.com/feed/ */
	uri = get_relative_uri (TEST_SRCDIR "empty-feed.xml");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, NULL);
	g_free (uri);
}

static void
test_no_url_podcast (void)
{
	char *uri;

	/* From http://feeds.guardian.co.uk/theguardian/football/rss */
	uri = get_relative_uri (TEST_SRCDIR "no-url-podcast.xml");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "http://www.guardian.co.uk/sport/video/2012/jul/26/london-2012-north-korea-flag-video");
	g_free (uri);
}

static void
test_itms_parsing (void)
{
	if (check_http() == FALSE) {
		g_test_message ("HTTP support required to test ITMS");
		return;
	}

	/* From https://itunes.apple.com/fr/podcast/chris-moyles-show-on-radio/id1042635536?mt=2&ign-mpt=uo=4 */
	g_assert_cmpstr (parser_test_get_playlist_uri ("https://itunes.apple.com/fr/podcast/chris-moyles-show-on-radio/id1042635536?mt=2&ign-mpt=uo%3D4#"), ==, "https://feeds.captivate.fm/the-chris-moyles-show/");
	g_assert_cmpstr (parser_test_get_playlist_uri ("http://itunes.apple.com/gb/podcast/radio-1-mini-mix/id268491175?uo=4"), ==, "https://podcasts.files.bbci.co.uk/p02nrtyg.rss");
}

static void
test_parsing_needle_carriage_return (void)
{
	char *uri;

	/* rss needle test */
	uri = get_relative_uri (TEST_SRCDIR "rss-needle-carriage-return");
	g_assert_cmpuint (parser_test_get_num_entries (uri), ==, 19);
	g_free (uri);

	/* atom needle test */
	uri = get_relative_uri (TEST_SRCDIR "atom.xml");
	g_assert_cmpuint (parser_test_get_num_entries (uri), ==, 1);
	g_free (uri);

	/* opml needle test */
	uri = get_relative_uri (TEST_SRCDIR "feeds.opml");
	g_assert_cmpuint (parser_test_get_num_entries (uri), ==, 7);
	g_free (uri);
}

static void
test_parsing_feed_content_type (void)
{
	char *uri;

	/* rss */
	uri = get_relative_uri (TEST_SRCDIR "podcast-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "application/rss+xml");
	g_free (uri);

	/* atom */
	uri = get_relative_uri (TEST_SRCDIR "atom.xml");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "application/atom+xml");
	g_free (uri);

	/* opml */
	uri = get_relative_uri (TEST_SRCDIR "feeds.opml");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "text/x-opml+xml");
	g_free (uri);
}

static void
test_parsing_item_content_type (void)
{
	char *uri;

	/* no audio content */
	uri = get_relative_uri (TEST_SRCDIR "no-url-podcast.xml");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, NULL);
	g_free (uri);

	/* <enclosure> without <media:content> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-description.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "audio/mpeg");
	g_free (uri);

	/* <media:content> followed by <enclosure> */
	uri = get_relative_uri (TEST_SRCDIR "HackerMedley");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "audio/mpeg");
	g_free (uri);

	/* <enclosure> followed by <media:content> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.2.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "audio/mpeg");
	g_free (uri);

	/* <enclosure> followed by <media:content> with image */
	uri = get_relative_uri (TEST_SRCDIR "791154-kqed.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_TYPE), ==, "audio/mpeg");
	g_free (uri);
}

static void
test_parsing_medium (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "791154-kqed.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "https://www.podtrac.com/pts/redirect.mp3/www.kqed.org/.stream/anon/radio/tcrmag/2017/12/TCRPodcastDec1.mp3");
	g_free (uri);
}

static void
test_parsing_feed_description (void)
{
	char *uri;
	const char *description1;
	const char *description2;

	description1 =
		"At the end of the day, we're all black. Can we just get along? A podcast featuring "
		"conversations to improve understanding between Africans and African Americans.";
	description2 =
		"Bastian Bielendorfer und Reinhard Remfort haben sich vor einigen Jahren bei einem "
		"Dreh für das ZDF kennengelernt und schnell rausgefunden, dass sie eine gemeinsame "
		"Vergangenheit teilen. Beide kommen aus dem tiefsten Ruhrpott, beide waren die dicken "
		"Kinder in der Klasse und beide haben mindestens ein \"Sachbuch\" geschrieben. Heute "
		"stehen die beiden vor der Kamera, auf Bühnen und sprechen in Mikrofone um Wissen zu "
		"verbreiten und den Menschen in Ihrer Umgebung ein Lächeln ins Gesicht zu zaubern.\n\n"
		"\"Alliteration am Arsch\" ist dabei der Versuch die Menschen an dem Leben zweier "
		"ehemals dicker Kinder aus den 80ern teilhaben zu lassen.";

	/* test for longer feed description */
	uri = get_relative_uri (TEST_SRCDIR "podcast-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_DESCRIPTION), ==, description1);
	g_free (uri);

	/* test for empty feed description tags */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_DESCRIPTION), ==, description2);
	g_free (uri);
}

static void
test_parsing_item_description (void)
{
	char *uri;
	const char *description1;
	const char *description2;

	description1 =
		"H&M ad and hair standards in the black community";
	description2 =
		"Wie versprochen die zweite Hälfte unseres kleinen Auftritts in Frankfurt. Ist ab sofort auch in voller Länge auf YouTube zu finden: https://youtu.be/GAQakfNHGj8";

	/* test for longer item description */
	uri = get_relative_uri (TEST_SRCDIR "podcast-description.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_DESCRIPTION), ==, description1);
	g_free (uri);

	/* test for empty item description */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_DESCRIPTION), ==, description2);
	g_free (uri);
}

static void
test_parsing_feed_image (void)
{
	char *uri;

	/* <itunes:image> followed by <image> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, "http://i1.sndcdn.com/avatars-000325311522-dw14t0-original.jpg");
	g_free (uri);

	/* <image> followed by <itunes:image> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.2.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, "http://ichef.bbci.co.uk/images/ic/3000x3000/p076j2sr.jpg");
	g_free (uri);
}

static void
test_parsing_item_image (void)
{
	char *uri;

	/* no podcast item image */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.2.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, NULL);
	g_free (uri);

	/* only <itunes:image> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, "http://i1.sndcdn.com/avatars-000325311522-dw14t0-original.jpg");
	g_free (uri);

	/* <itunes:image> followed by <media:content> with image */
	uri = get_relative_uri (TEST_SRCDIR "podcast-different-item-images.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, "https://images.theabcdn.com/i/37623804.jpg");
	g_free (uri);

	/* <image><url> followed by <itunes:image> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_IMAGE_URI), ==, "https://images.podigee.com/0x,svlSD5_BDle5m2pLyGQT7_GWO0GW8iv2Kgr6AhbFe8vU=/https://cdn.podigee.com/uploads/u2254/dafcf335-4257-4401-bdc3-349ef792aba4.jpg");
	g_free (uri);
}

static void
test_parsing_feed_pubdate (void)
{
	char *uri;

	/* no <lastBuildDate> or <pubDate> */
	uri = get_relative_uri (TEST_SRCDIR "585407.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_PUB_DATE), ==, NULL);
	g_free (uri);

	/* only <lastBuildDate> */
	uri = get_relative_uri (TEST_SRCDIR "791154-kqed.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_PUB_DATE), ==, "Mon, 04 Dec 2017 08:01:09 +0000");
	g_free (uri);

	/* same <lastBuildDate> and <pubDate> */
	uri = get_relative_uri (TEST_SRCDIR "560051.xml");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_PUB_DATE), ==, "Mon, 8 Dec 2008 13:20:00 CST");
	g_free (uri);

	/* <pubDate> followed by <lastBuildDate> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_PUB_DATE), ==, "Sun, 26 Jul 2020 20:07:40 +0000");
	g_free (uri);

	/* <lastBuildDate> followed by <pubDate> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_PUB_DATE), ==, "Wed, 23 Aug 2017 01:55:17 +0000");
	g_free (uri);

}

static void
test_parsing_feed_author (void)
{
	char *uri;

	/* no <itunes:owner> or <itunes:author> */
	uri = get_relative_uri (TEST_SRCDIR "541405.xml");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_AUTHOR), ==, NULL);
	g_free (uri);

	/* <itunes:owner> without <itunes:author> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_AUTHOR), ==, "Bastian Bielendorfer und Reinhard Remfort");
	g_free (uri);

	/* same <itunes:owner> and <itunes:author> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_AUTHOR), ==, "Exit Poll New England");
	g_free (uri);

	/* different <itunes:owner> with <itunes:author> */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.2.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_AUTHOR), ==, "BBC Radio");
	g_free (uri);
}

static void
test_parsing_feed_explicit (void)
{
	char *uri;

	/* clean feed */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_CLEAN);
	g_free (uri);

	/* explicit feed */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_EXPLICIT);
	g_free (uri);

	/* unrated feed */
	uri = get_relative_uri (TEST_SRCDIR "content-no-rating.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_UNRATED);
	g_free (uri);
}

static void
test_parsing_item_explicit (void)
{
	char *uri;

	/* clean item */
	uri = get_relative_uri (TEST_SRCDIR "podcast-image-url.1.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_CLEAN);
	g_free (uri);

	/* explicit item */
	uri = get_relative_uri (TEST_SRCDIR "podcast-empty-description.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_EXPLICIT);
	g_free (uri);

	/* unrated item */
	uri = get_relative_uri (TEST_SRCDIR "content-no-rating.rss");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_CONTENT_RATING), ==, TOTEM_PL_PARSER_CONTENT_RATING_UNRATED);
	g_free (uri);
}

static void
test_invalid_characters (void)
{
	char *uri;

	uri = get_relative_uri (TEST_SRCDIR "invalid-characters.rss");
#ifdef HAVE_UCHARDET
	if (g_test_subprocess ()) {
		/* This call should abort with 'Invalid byte sequence in conversion input' */
		simple_parser_test (uri);
		g_assert_not_reached ();
		return;
	}

	g_test_trap_subprocess (NULL, 0, 0);
	g_test_trap_assert_failed ();
	g_test_trap_assert_stderr ("*byte offset 22493,*");
#else
	g_assert_cmpint (simple_parser_test (uri), !=, TOTEM_PL_PARSER_RESULT_SUCCESS);
#endif
	g_free (uri);
}

static void
test_invalid_utf8_characters (void)
{
	char *uri;

	/* Test all entries have been parsed by checking entry count */
	uri = get_relative_uri (TEST_SRCDIR "invalid-utf8-characters.rss");
#ifdef HAVE_UCHARDET
	g_assert_cmpuint (parser_test_get_num_entries (uri), ==, 4);
#else
	g_assert_cmpint (simple_parser_test (uri), !=, TOTEM_PL_PARSER_RESULT_SUCCESS);
#endif
	g_free (uri);
}

static void
test_parsing_xml_cdata (void)
{
	char *uri;
	const char *description =
		"POLL (Podcast LPM LONTAR) merupakan sebuah media yang kami gunakan "
		"untuk menyalurkan potensi anggota LPM LONTAR dengan menyajikan beragam topik diskusi.";

	uri = get_relative_uri (TEST_SRCDIR "cdata.rss");

	/* empty cdata */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_COPYRIGHT), ==, NULL);

	/* single char */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_CONTACT), ==, "X");

	/* two chars */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_LANGUAGE), ==, "in");

	/* one word */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE), ==, "POLL");

	/* two words */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_AUTHOR), ==, "POLL author");

	/* long string */
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_DESCRIPTION), ==, description);

	g_free (uri);
}

static void
test_parsing_feed_genres (void)
{
	char *uri;

	/* missing genre */
	uri = get_relative_uri (TEST_SRCDIR "541405.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, NULL);
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, NULL);
	g_free (uri);

	/* single genre 1 */
	uri = get_relative_uri (TEST_SRCDIR "podcast-description.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Society & Culture");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "Society & Culture");
	g_free (uri);

	/* single genre 2 */
	uri = get_relative_uri (TEST_SRCDIR "791154-kqed.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "News & Politics");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "News & Politics");
	g_free (uri);

	/* single genre (with single subgenre) */
	uri = get_relative_uri (TEST_SRCDIR "HackerMedley");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Technology/Tech News");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "Technology/Tech News");
	g_free (uri);

	/* multiple genre 1 */
	uri = get_relative_uri (TEST_SRCDIR "560051.xml");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Society & Culture");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "Society & Culture,News & Politics,Religion & Spirituality");
	g_free (uri);

	/* multiple genre (with single subgenre)  */
	uri = get_relative_uri (TEST_SRCDIR "585407.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Business");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "Business,Health/Self-Help");
	g_free (uri);

	/* multiple genre (with subgenres) */
	uri = get_relative_uri (TEST_SRCDIR "content-no-rating.rss");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRE), ==, "Health & Fitness/Alternative Health");
	g_assert_cmpstr (parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_GENRES), ==, "Health & Fitness/Alternative Health,Education/Self-Improvement,Health & Fitness/Nutrition");
	g_free (uri);
}

static void
test_xml_is_text_plain (void)
{
	TotemPlParserResult result;

	if (check_http() == FALSE) {
		g_test_message ("HTTP support required to test text/plain XML");
		return;
	}

	g_test_bug ("655378");
	result = simple_parser_test ("http://leoville.tv/podcasts/floss.xml");
	g_assert_cmpint (result, ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
}

static void
test_compressed_content_encoding (void)
{
	TotemPlParserResult result;

	if (check_http() == FALSE) {
		g_test_message ("HTTP support required to test compressed content-encoding");
		return;
	}

	/* Requires:
	 * http://git.gnome.org/browse/gvfs/commit/?id=6929e9f9661b4d1e68f8912d8e60107366255a47
	 * https://mail.gnome.org/archives/rhythmbox-devel/2011-November/thread.html#00010 */
	result = simple_parser_test ("https://escapepod.org/feed/");
	g_assert_cmpint (result, ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
}

static void
test_parsing_xml_head_comments (void)
{
	char *uri;
	g_test_bug ("560051");
	uri = get_relative_uri (TEST_SRCDIR "560051.xml");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_parsing_xml_comment_whitespace (void)
{
	char *uri;
	g_test_bug ("541405");
	uri = get_relative_uri (TEST_SRCDIR "541405.xml");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
test_parsing_xml_mixed_cdata (void)
{
	char *uri;
	g_test_bug ("585407");
	/* File from http://www.davidco.com/podcast.php */
	uri = get_relative_uri (TEST_SRCDIR "585407.rss");
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
	g_free (uri);
}

static void
video_links_slow_parsing (const char *uri, gfloat timeout)
{
	time_t start, end;
	double run_time;

	g_setenv ("SLOW_PARSING", "1", TRUE);

	start = time (NULL);
	option_no_recurse = TRUE;
	parser_test_get_playlist_field (uri, TOTEM_PL_PARSER_FIELD_TITLE);
	end = time (NULL);

	run_time = difftime (end, start);

	g_assert_cmpfloat (run_time, <, timeout);
}

static void
test_video_links_slow_parsing ()
{
	char *uri;
	gfloat timeout = 2.0; /* seconds */

	/* rss feed with 400 entries. should take 400 * 1 = 400
	 * seconds with videosite check, and approx. less than 1
	 * second if we bypass videosite check.
	 */
	uri = get_relative_uri (TEST_SRCDIR "podcast-different-item-images.rss");
	video_links_slow_parsing (uri, timeout);
	g_free (uri);

	/* atom feed with 20 entries. should take 20 * 1 = 20
	 * seconds with videosite check, and approx. less than 1
	 * second if we bypass videosite check.
	 */
	uri = get_relative_uri (TEST_SRCDIR "gitlab-issues.atom");
	video_links_slow_parsing (uri, timeout);
	g_free (uri);

	/* atom feed with 5 entries. should take 5 * 1 = 5
	 * seconds with videosite check, and approx. less than 1
	 * second if we bypass videosite check.
	 */
	uri = get_relative_uri (TEST_SRCDIR "status-gnome-org.atom");
	video_links_slow_parsing (uri, timeout);
	g_free (uri);
}

static void
test_parsing_rss_id (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "rss.xml");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_ID), ==, "http://example.com/video1/from-rss");
	g_free (uri);
}

static void
test_parsing_rss_link (void)
{
	char *uri;
	uri = get_relative_uri (TEST_SRCDIR "rss.xml");
	g_assert_cmpstr (parser_test_get_entry_field (uri, TOTEM_PL_PARSER_FIELD_URI), ==, "http://www.guardian.co.uk/technology/audio/2011/may/03/tech-weekly-art-love-bin-laden");
	g_free (uri);
}

static void
test_xml_trailing_space (void)
{
	g_autofree char *uri = NULL;
	g_test_bug ("28");
	uri = get_relative_uri (TEST_SRCDIR "xml-trailing-space.xml");
	/* URL from https://gitlab.gnome.org/GNOME/totem-pl-parser/-/issues/28 */
	g_assert_cmpint (simple_parser_test (uri), ==, TOTEM_PL_PARSER_RESULT_SUCCESS);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);
	g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=");

	check_http ();
	g_setenv ("TOTEM_PL_PARSER_VIDEOSITE_SCRIPT", TEST_SRCDIR "/videosite-tester.sh", TRUE);

	option_debug = TRUE;

	g_test_add_func ("/parser/image_link", test_image_link);
	g_test_add_func ("/parser/no_url_podcast", test_no_url_podcast);
	g_test_add_func ("/parser/xml_is_text_plain", test_xml_is_text_plain);
	g_test_add_func ("/parser/compressed_content_encoding", test_compressed_content_encoding);
	g_test_add_func ("/parser/parsing/xml_head_comments", test_parsing_xml_head_comments);
	g_test_add_func ("/parser/parsing/xml_comment_whitespace", test_parsing_xml_comment_whitespace);
	g_test_add_func ("/parser/parsing/podcast_needle_carriage_return", test_parsing_needle_carriage_return);
	g_test_add_func ("/parser/parsing/podcast_feed_content_type", test_parsing_feed_content_type);
	g_test_add_func ("/parser/parsing/podcast_item_content_type", test_parsing_item_content_type);
	g_test_add_func ("/parser/parsing/podcast_medium", test_parsing_medium);
	g_test_add_func ("/parser/parsing/podcast_feed_description", test_parsing_feed_description);
	g_test_add_func ("/parser/parsing/podcast_item_description", test_parsing_item_description);
	g_test_add_func ("/parser/parsing/podcast_feed_image", test_parsing_feed_image);
	g_test_add_func ("/parser/parsing/podcast_item_image", test_parsing_item_image);
	g_test_add_func ("/parser/parsing/podcast_feed_pubdate", test_parsing_feed_pubdate);
	g_test_add_func ("/parser/parsing/podcast_feed_author", test_parsing_feed_author);
	g_test_add_func ("/parser/parsing/podcast_feed_explicit", test_parsing_feed_explicit);
	g_test_add_func ("/parser/parsing/podcast_item_explicit", test_parsing_item_explicit);
	g_test_add_func ("/parser/parsing/invalid_characters", test_invalid_characters);
	g_test_add_func ("/parser/parsing/invalid_utf8_characters", test_invalid_utf8_characters);
	g_test_add_func ("/parser/parsing/podcast_feed_genres", test_parsing_feed_genres);
	g_test_add_func ("/parser/parsing/xml_mixed_cdata", test_parsing_xml_mixed_cdata);
	g_test_add_func ("/parser/parsing/xml_cdata", test_parsing_xml_cdata);
	g_test_add_func ("/parser/parsing/rss_id", test_parsing_rss_id);
	g_test_add_func ("/parser/parsing/rss_link", test_parsing_rss_link);
	g_test_add_func ("/parser/parsing/itms_link", test_itms_parsing);
	g_test_add_func ("/parser/parsing/xml_trailing_space", test_xml_trailing_space);

	/* set an envvar, keep at the end */
	g_test_add_func ("/parser/parsing/video_links_slow_parsing", test_video_links_slow_parsing);


	return g_test_run ();
}
