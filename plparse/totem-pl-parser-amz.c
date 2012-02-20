/*
   Copyright (C) 2010 Bastien Nocera <hadess@hadess.net>

   Copyright (c) 2010 William Pitcock <nenolod@dereferenced.org>
                 for amzfile_decrypt_blob().

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

   Author: Bastien Nocera <hadess@hadess.net>
 */

#include "config.h"

#ifndef TOTEM_PL_PARSER_MINI
#include <string.h>
#include <glib.h>

#include "totem-pl-parser-mini.h"
#include "totem-pl-parser-amz.h"
#include "totem-pl-parser-xspf.h"
#include "totem-pl-parser-private.h"

#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>

/*
 * LOL.
 *
 * These guys use DES encryption which has been broken since the 1970s.  Keys
 * cracked in 37 seconds on a Core i7 processor running at 2.7GHz.
 */
static const guchar amazon_key[8] = { 0x29, 0xAB, 0x9D, 0x18, 0xB2, 0x44, 0x9E, 0x31 };
static const guchar amazon_iv[8]  = { 0x5E, 0x72, 0xD7, 0x9A, 0x11, 0xB3, 0x4F, 0xEE };

/*
 * decrypts the underlying XSPF playlist.
 * does not *parse* the XSPF playlist.
 */
static gboolean
amzfile_decrypt_blob(gchar *indata, gsize inlen, gchar **outdata)
{
	gcry_cipher_hd_t hd;
	gcry_error_t err;
	guchar *unpackdata, *decryptdata;
	gsize unpacklen;
	gint i;

	unpackdata = g_base64_decode(indata, &unpacklen);
	if (unpackdata == NULL)
	{
		g_print("g_base64_decode failed\n");
		return FALSE;
	}

	if (unpacklen % 8)
		unpacklen -= (unpacklen % 8);

	decryptdata = g_malloc0(unpacklen + 1);

	if ((err = gcry_cipher_open(&hd, GCRY_CIPHER_DES, GCRY_CIPHER_MODE_CBC, 0)))
	{
		g_print("unable to initialise gcrypt: %s", gcry_strerror(err));
		g_free(unpackdata);
		g_free(decryptdata);
		return FALSE;
	}

	if ((err = gcry_cipher_setkey(hd, amazon_key, 8)))
	{
		g_print("unable to set key for DES block cipher: %s", gcry_strerror(err));
		gcry_cipher_close(hd);
		g_free(unpackdata);
		g_free(decryptdata);
		return FALSE;
	}

	if ((err = gcry_cipher_setiv(hd, amazon_iv, 8)))
	{
		g_print("unable to set initialisation vector for DES block cipher: %s", gcry_strerror(err));
		gcry_cipher_close(hd);
		g_free(unpackdata);
		g_free(decryptdata);
		return FALSE;
	}

	if ((err = gcry_cipher_decrypt(hd, decryptdata, unpacklen, unpackdata, unpacklen)))
	{
		g_print("unable to decrypt embedded DES-encrypted XSPF document: %s", gcry_strerror(err));
		gcry_cipher_close(hd);
		g_free(unpackdata);
		g_free(decryptdata);
		return FALSE;
	}

	g_free(unpackdata);
	gcry_cipher_close(hd);

	/* remove padding from XSPF document */
	for (i = unpacklen; i > 0; i--)
	{
		if (decryptdata[i - 1] == '\n' || decryptdata[i] == '\r' || decryptdata[i - 1] >= ' ')
			break;
	}
	decryptdata[i] = 0;

	*outdata = (char *) decryptdata;
	return TRUE;
}
#endif /* HAVE_LIBGCRYPT */

TotemPlParserResult
totem_pl_parser_add_amz (TotemPlParser *parser,
			 GFile *file,
			 GFile *base_file,
			 TotemPlParseData *parse_data,
			 gpointer data)
{
#ifdef HAVE_LIBGCRYPT
	char *b64data, *contents;
	TotemPlParserResult ret;
	gsize b64len;

	if (g_file_load_contents (file, NULL, &b64data, &b64len, NULL, NULL) == FALSE)
		return TOTEM_PL_PARSER_RESULT_ERROR;

	if (amzfile_decrypt_blob (b64data, b64len, &contents) == FALSE) {
		g_free (b64data);
		return TOTEM_PL_PARSER_RESULT_ERROR;
	}

	ret = totem_pl_parser_add_xspf_with_contents (parser, file, base_file,
						      contents, parse_data);

	g_free (contents);

	return ret;
#else
	return TOTEM_PL_PARSER_RESULT_UNHANDLED;
#endif /* HAVE_LIBGCRYPT */
}

#endif /* !TOTEM_PL_PARSER_MINI */
