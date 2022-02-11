/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#include <config.h>

#include "totem-pl-parser-decode-date.h"

#define d(x)

/* date parser macros */
#define DATE_TOKEN_NON_NUMERIC          (1 << 0)
#define DATE_TOKEN_NON_WEEKDAY          (1 << 1)
#define DATE_TOKEN_NON_MONTH            (1 << 2)
#define DATE_TOKEN_NON_TIME             (1 << 3)
#define DATE_TOKEN_HAS_COLON            (1 << 4)
#define DATE_TOKEN_NON_TIMEZONE_ALPHA   (1 << 5)
#define DATE_TOKEN_NON_TIMEZONE_NUMERIC (1 << 6)
#define DATE_TOKEN_HAS_SIGN             (1 << 7)

static unsigned char gmime_datetok_table[256] = {
	128,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111, 79, 79,111,175,111,175,111,111,
	 38, 38, 38, 38, 38, 38, 38, 38, 38, 38,119,111,111,111,111,111,
	111, 75,111, 79, 75, 79,105, 79,111,111,107,111,111, 73, 75,107,
	 79,111,111, 73, 77, 79,111,109,111, 79, 79,111,111,111,111,111,
	111,105,107,107,109,105,111,107,105,105,111,111,107,107,105,105,
	107,111,105,105,105,105,107,111,111,105,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
	111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,111,
};

/* Timezone values defined in rfc5322 */
static struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "UT",       0 },
	{ "GMT",      0 },
	{ "EDT",   -400 },
	{ "EST",   -500 },
	{ "CDT",   -500 },
	{ "CST",   -600 },
	{ "MDT",   -600 },
	{ "MST",   -700 },
	{ "PDT",   -700 },
	{ "PST",   -800 },
	/* Note: rfc822 got the signs backwards for the military
	 * timezones so some sending clients may mistakenly use the
	 * wrong values. */
	{ "A",      100 },
	{ "B",      200 },
	{ "C",      300 },
	{ "D",      400 },
	{ "E",      500 },
	{ "F",      600 },
	{ "G",      700 },
	{ "H",      800 },
	{ "I",      900 },
	{ "K",     1000 },
	{ "L",     1100 },
	{ "M",     1200 },
	{ "N",     -100 },
	{ "O",     -200 },
	{ "P",     -300 },
	{ "Q",     -400 },
	{ "R",     -500 },
	{ "S",     -600 },
	{ "T",     -700 },
	{ "U",     -800 },
	{ "V",     -900 },
	{ "W",    -1000 },
	{ "X",    -1100 },
	{ "Y",    -1200 },
	{ "Z",        0 },
};

static char *tm_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *tm_days[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


/* This is where it gets ugly... */

typedef struct _date_token {
	struct _date_token *next;
	unsigned char mask;
	const char *start;
	size_t len;
} date_token;

#define date_token_free(tok) g_slice_free (date_token, tok)
#define date_token_new() g_slice_new (date_token)

static date_token *
datetok (const char *date)
{
	date_token tokens, *token, *tail;
	const char *start, *end;
        unsigned char mask;

	tail = (date_token *) &tokens;
	tokens.next = NULL;

	start = date;
	while (*start) {
		/* kill leading whitespace */
		while (*start == ' ' || *start == '\t')
			start++;

		if (*start == '\0')
			break;

		mask = gmime_datetok_table[(unsigned char) *start];

		/* find the end of this token */
		end = start + 1;
		while (*end && !strchr ("-/,\t\r\n ", *end))
			mask |= gmime_datetok_table[(unsigned char) *end++];

		if (end != start) {
			token = date_token_new ();
			token->next = NULL;
			token->start = start;
			token->len = end - start;
			token->mask = mask;

			tail->next = token;
			tail = token;
		}

		if (*end)
			start = end + 1;
		else
			break;
	}

	return tokens.next;
}

static int
decode_int (const char *in, size_t inlen)
{
	register const char *inptr = in;
	const char *inend = in + inlen;
	int val = 0;

	while (inptr < inend) {
		if (!(*inptr >= '0' && *inptr <= '9'))
			return -1;
		if (val > (INT_MAX - (*inptr - '0')) / 10)
			return -1;
		val = (val * 10) + (*inptr - '0');
		inptr++;
	}

	return val;
}

static int
get_wday (const char *in, size_t inlen)
{
	int wday;

	g_return_val_if_fail (in != NULL, -1);

	if (inlen < 3)
		return -1;

	for (wday = 0; wday < 7; wday++) {
		if (!g_ascii_strncasecmp (in, tm_days[wday], 3))
			return wday;
	}

	return -1;  /* unknown week day */
}

static int
get_mday (const char *in, size_t inlen)
{
	int mday;

	g_return_val_if_fail (in != NULL, -1);

	mday = decode_int (in, inlen);

	if (mday < 0 || mday > 31)
		mday = -1;

	return mday;
}

static int
get_month (const char *in, size_t inlen)
{
	int i;

	g_return_val_if_fail (in != NULL, -1);

	if (inlen < 3)
		return -1;

	for (i = 0; i < 12; i++) {
		if (!g_ascii_strncasecmp (in, tm_months[i], 3))
			return i + 1;
	}

	return -1;  /* unknown month */
}

static int
get_year (const char *in, size_t inlen)
{
	int year;

	g_return_val_if_fail (in != NULL, -1);

	if ((year = decode_int (in, inlen)) == -1)
		return -1;

	if (year < 100)
		year += (year < 70) ? 2000 : 1900;

	if (year < 1969)
		return -1;

	return year;
}

static gboolean
get_time (const char *in, size_t inlen, int *hour, int *min, int *sec)
{
	register const char *inptr;
	int *val, max, colons = 0;
	const char *inend;

	*hour = *min = *sec = 0;

	inend = in + inlen;
	val = hour;
	max = 23;
	for (inptr = in; inptr < inend; inptr++) {
		if (*inptr == ':') {
			colons++;
			switch (colons) {
			case 1:
				val = min;
				max = 59;
				break;
			case 2:
				val = sec;
				max = 60;
				break;
			default:
				return FALSE;
			}
		} else if (!(*inptr >= '0' && *inptr <= '9'))
			return FALSE;
		else {
			*val = (*val * 10) + (*inptr - '0');
			if (*val > max)
				return FALSE;
		}
	}

	return TRUE;
}

static GTimeZone *
get_tzone (date_token **token)
{
	const char *inptr, *inend;
	char tzone[8];
	size_t len, n;
	int value, i;
	guint t;

	for (i = 0; *token && i < 2; *token = (*token)->next, i++) {
		inptr = (*token)->start;
		len = (*token)->len;
		inend = inptr + len;

		if (len >= 6)
			continue;

		if (len == 5 && (*inptr == '+' || *inptr == '-')) {
			if ((value = decode_int (inptr + 1, len - 1)) == -1)
				return NULL;

			memcpy (tzone, inptr, len);
			tzone[len] = '\0';

			return g_time_zone_new_identifier (tzone);
		}

		if (*inptr == '(') {
			inptr++;
			if (*(inend - 1) == ')')
				len -= 2;
			else
				len--;
		}

		for (t = 0; t < G_N_ELEMENTS (tz_offsets); t++) {
			n = strlen (tz_offsets[t].name);

			if (n != len || strncmp (inptr, tz_offsets[t].name, n) != 0)
				continue;

			snprintf (tzone, 6, "%+05d", tz_offsets[t].offset);

			return g_time_zone_new_identifier (tzone);
		}
	}

	return NULL;
}

static GDateTime *
parse_rfc822_date (date_token *tokens)
{
	int year, month, day, hour, min, sec, n;
	GTimeZone *tz = NULL;
	date_token *token;
	GDateTime *date;

	token = tokens;

	year = month = day = hour = min = sec = 0;

	if ((n = get_wday (token->start, token->len)) != -1) {
		/* not all dates may have this... */
		token = token->next;
	}

	/* get the mday */
	if (!token || (n = get_mday (token->start, token->len)) == -1)
		return NULL;

	token = token->next;
	day = n;

	/* get the month */
	if (!token || (n = get_month (token->start, token->len)) == -1)
		return NULL;

	token = token->next;
	month = n;

	/* get the year */
	if (!token || (n = get_year (token->start, token->len)) == -1)
		return NULL;

	token = token->next;
	year = n;

	/* get the hour/min/sec */
	if (!token || !get_time (token->start, token->len, &hour, &min, &sec))
		return NULL;

	token = token->next;

	/* get the timezone */
	if (!token || !(tz = get_tzone (&token))) {
		/* I guess we assume tz is GMT? */
		tz = g_time_zone_new_utc ();
	}

	date = g_date_time_new (tz, year, month, day, hour, min, (gdouble) sec);
	g_time_zone_unref (tz);

	return date;
}


#define date_token_mask(t)  (((date_token *) t)->mask)
#define is_numeric(t)       ((date_token_mask (t) & DATE_TOKEN_NON_NUMERIC) == 0)
#define is_weekday(t)       ((date_token_mask (t) & DATE_TOKEN_NON_WEEKDAY) == 0)
#define is_month(t)         ((date_token_mask (t) & DATE_TOKEN_NON_MONTH) == 0)
#define is_time(t)          (((date_token_mask (t) & DATE_TOKEN_NON_TIME) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_COLON))
#define is_tzone_alpha(t)   ((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_ALPHA) == 0)
#define is_tzone_numeric(t) (((date_token_mask (t) & DATE_TOKEN_NON_TIMEZONE_NUMERIC) == 0) && (date_token_mask (t) & DATE_TOKEN_HAS_SIGN))
#define is_tzone(t)         (is_tzone_alpha (t) || is_tzone_numeric (t))

#define YEAR    (1 << 0)
#define MONTH   (1 << 1)
#define DAY     (1 << 2)
#define WEEKDAY (1 << 3)
#define TIME    (1 << 4)
#define TZONE   (1 << 5)

static GDateTime *
parse_broken_date (date_token *tokens)
{
	int year, month, day, hour, min, sec, n;
	GTimeZone *tz = NULL;
	date_token *token;
	GDateTime *date;
	int mask;

	year = month = day = hour = min = sec = 0;
	mask = 0;

	token = tokens;
	while (token) {
		if (is_weekday (token) && !(mask & WEEKDAY)) {
			if ((n = get_wday (token->start, token->len)) != -1) {
				d(printf ("weekday; "));
				mask |= WEEKDAY;
				goto next;
			}
		}

		if (is_month (token) && !(mask & MONTH)) {
			if ((n = get_month (token->start, token->len)) != -1) {
				d(printf ("month; "));
				mask |= MONTH;
				month = n;
				goto next;
			}
		}

		if (is_time (token) && !(mask & TIME)) {
			if (get_time (token->start, token->len, &hour, &min, &sec)) {
				d(printf ("time; "));
				mask |= TIME;
				goto next;
			}
		}

		if (is_tzone (token) && !(mask & TZONE)) {
			date_token *t = token;

			if ((tz = get_tzone (&t))) {
				d(printf ("tzone; "));
				mask |= TZONE;
				goto next;
			}
		}

		if (is_numeric (token)) {
			if (token->len == 4 && !(mask & YEAR)) {
				if ((n = get_year (token->start, token->len)) != -1) {
					d(printf ("year; "));
					mask |= YEAR;
					year = n;
					goto next;
				}
			} else {
				/* Note: assumes MM-DD-YY ordering if '0 < MM < 12' holds true */
				if (!(mask & MONTH) && token->next && is_numeric (token->next)) {
					if ((n = decode_int (token->start, token->len)) > 12) {
						goto mday;
					} else if (n > 0) {
						d(printf ("mon; "));
						mask |= MONTH;
						month = n;
					}
					goto next;
				} else if (!(mask & DAY) && (n = get_mday (token->start, token->len)) != -1) {
				mday:
					d(printf ("mday; "));
					mask |= DAY;
					day = n;
					goto next;
				} else if (!(mask & YEAR)) {
					if ((n = get_year (token->start, token->len)) != -1) {
						d(printf ("2-digit year; "));
						mask |= YEAR;
						year = n;
					}
					goto next;
				}
			}
		}

		d(printf ("???; "));

	next:

		token = token->next;
	}

	d(printf ("\n"));

	if (!(mask & (YEAR | MONTH | DAY | TIME))) {
		if (tz != NULL)
			g_time_zone_unref (tz);

		return NULL;
	}

	if (tz == NULL)
		tz = g_time_zone_new_utc ();

	date = g_date_time_new (tz, year, month, day, hour, min, (gdouble) sec);
	g_time_zone_unref (tz);

	return date;
}

/**
 * g_mime_utils_header_decode_date:
 * @str: input date string
 *
 * Parses the rfc822 date string.
 *
 * Returns: (nullable) (transfer full): the #GDateTime representation of the date
 * string specified by @str or %NULL on error.
 **/
GDateTime *
g_mime_utils_header_decode_date (const char *str)
{
	date_token *token, *tokens;
	GDateTime *date;

	if (!(tokens = datetok (str)))
		return NULL;

	if (!(date = parse_rfc822_date (tokens)))
		date = parse_broken_date (tokens);

	/* cleanup */
	while (tokens) {
		token = tokens;
		tokens = tokens->next;
		date_token_free (token);
	}

	return date;
}
