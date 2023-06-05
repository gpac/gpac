/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2007-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <gpac/utf.h>

//for now only for SVG
#if !defined(GPAC_DISABLE_SVG) && !defined(GPAC_DISABLE_COMPOSITOR)

/*------------------------------------------------------------------------
	Bidirectional Character Types

	as defined by the Unicode Bidirectional Algorithm Table 3-7.

------------------------------------------------------------------------*/
enum
{
	// input types
	// ON MUST be zero, code relies on ON = N = 0
	ON = 0,  // Other Neutral
	L,       // Left Letter
	R,       // Right Letter
	AN,      // Arabic Number
	EN,      // European Number
	AL,      // Arabic Letter (Right-to-left)
	NSM,     // Non-spacing Mark
	CS,      // Common Separator
	ES,      // European Separator
	ET,      // European Terminator (post/prefix e.g. $ and %)

	// resolved types
	BN,      // Boundary neutral (type of RLE etc after explicit levels)

	// input types,
	S,       // Segment Separator (TAB)		// used only in L1
	WS,      // White space					// used only in L1
	B,       // Paragraph Separator (aka as PS)

	// types for explicit controls
	RLO,     // these are used only in X1-X9
	RLE,
	LRO,
	LRE,
	PDF,

	// resolved types, also resolved directions
	N = ON,  // alias, where ON, WS and S are treated the same
};

/*----------------------------------------------------------------------
   The following array maps character codes to types for the purpose of
   this sample implementation. The legend string gives a human readable
   explanation of the pseudo alphabet.

   For simplicity, characters entered by buttons are given a 1:1 mapping
   between their type and pseudo character value. Pseudo characters that
   can be typed from the keyboard are explained in the legend string.

   Use the Unicode Character Database for the real values in real use.
 ---------------------------------------------------------------------*/

#define LRM 4
#define RLM 5
#define LS 0x13

static int bidi_get_class(u32 val);



GF_EXPORT
Bool gf_utf8_is_right_to_left(u16 *utf_string)
{
	u32 i = 0;
	while (1) {
		u32 c = utf_string[i];
		if (!c) return GF_FALSE;
		switch (bidi_get_class(c)) {
		case L:
			return GF_FALSE;
		case R:
			return GF_TRUE;
		case AN:
			return GF_TRUE;
		case EN:
			return GF_FALSE;
		case AL:
			return GF_TRUE;
		default:
			break;
		}
		i++;
	}
	return GF_FALSE;
}

/*a very VERY basic bidi reorderer */
GF_EXPORT
Bool gf_utf8_reorder_bidi(u16 *utf_string, u32 len)
{
	u32 i, j, start, stop, cur_dir, slen, main_dir;
	Bool is_start;

	/*get main direction of the layour*/
	Bool rev = gf_utf8_is_right_to_left(utf_string);

	if (rev) {
		for (i=0; i<len/2; i++) {
			u32 v = utf_string[i];
			utf_string[i] = utf_string[len-1-i];
			utf_string[len-1-i] = v;
		}
	}
	cur_dir = rev ? 1 : 0;
	main_dir = cur_dir;
	is_start = GF_TRUE;

	start = stop = 0;

	for (i=0; i<len; i++) {
		Bool rtl;
		u32 c = bidi_get_class(utf_string[i]);
		switch (c) {
		case R:
		case AN:
		case AL:
			rtl = GF_TRUE;
			break;
		case L:
		case EN:
			rtl = GF_FALSE;
			break;
		default:
			if (is_start) {
				start = i;
			} else if (!stop) {
				stop = i;
			}
			continue;
		}
		if (cur_dir != rtl) {
			if (!stop)
				stop = i;

			if (is_start) {
				is_start = GF_FALSE;
			} else {
				is_start = GF_TRUE;

				if (main_dir != cur_dir) {
					slen = stop-start+1;
					for (j=0; j<slen/2; j++) {
						u32 v = utf_string[start + j];
						utf_string[start + j] = utf_string[stop-j];
						utf_string[stop-j] = v;
					}
				}
			}
			cur_dir = rtl;
		}
		stop = 0;
	}

	/*not flushed yet*/
	if (!is_start) {
		if (!stop) stop = len-1;
		slen = stop-start+1;
		for (j=0; j<slen/2; j++) {
			u32 v = utf_string[start + j];
			utf_string[start + j] = utf_string[stop-j];
			utf_string[stop-j] = v;
		}
	}
	return rev;
}


static int bidi_get_class(u32 val)
{
	if ((val>=0x0041) && (val<=0x005A)) return L;	//# L&  [26] LATIN CAPITAL LETTER A..LATIN CAPITAL LETTER Z
	if ((val>=0x0061) && (val<=0x007A)) return L;	//# L&  [26] LATIN SMALL LETTER A..LATIN SMALL LETTER Z
	if (val==0x000000AA) return L;	//# L&       FEMININE ORDINAL INDICATOR
	if (val==0x000000B5) return L;	//# L&       MICRO SIGN
	if (val==0x000000BA) return L;	//# L&       MASCULINE ORDINAL INDICATOR
	if ((val>=0x00C0) && (val<=0x00D6)) return L;	//# L&  [23] LATIN CAPITAL LETTER A WITH GRAVE..LATIN CAPITAL LETTER O WITH DIAERESIS
	if ((val>=0x00D8) && (val<=0x00F6)) return L;	//# L&  [31] LATIN CAPITAL LETTER O WITH STROKE..LATIN SMALL LETTER O WITH DIAERESIS
	if ((val>=0x00F8) && (val<=0x01BA)) return L;	//# L& [195] LATIN SMALL LETTER O WITH STROKE..LATIN SMALL LETTER EZH WITH TAIL
	if (val==0x000001BB) return L;	//# Lo       LATIN LETTER TWO WITH STROKE
	if ((val>=0x01BC) && (val<=0x01BF)) return L;	//# L&   [4] LATIN CAPITAL LETTER TONE FIVE..LATIN LETTER WYNN
	if ((val>=0x01C0) && (val<=0x01C3)) return L;	//# Lo   [4] LATIN LETTER DENTAL CLICK..LATIN LETTER RETROFLEX CLICK
	if ((val>=0x01C4) && (val<=0x0293)) return L;	//# L& [208] LATIN CAPITAL LETTER DZ WITH CARON..LATIN SMALL LETTER EZH WITH CURL
	if (val==0x00000294) return L;	//# Lo       LATIN LETTER GLOTTAL STOP
	if ((val>=0x0295) && (val<=0x02AF)) return L;	//# L&  [27] LATIN LETTER PHARYNGEAL VOICED FRICATIVE..LATIN SMALL LETTER TURNED H WITH FISHHOOK AND TAIL
	if ((val>=0x02B0) && (val<=0x02B8)) return L;	//# Lm   [9] MODIFIER LETTER SMALL H..MODIFIER LETTER SMALL Y
	if ((val>=0x02BB) && (val<=0x02C1)) return L;	//# Lm   [7] MODIFIER LETTER TURNED COMMA..MODIFIER LETTER REVERSED GLOTTAL STOP
	if ((val>=0x02D0) && (val<=0x02D1)) return L;	//# Lm   [2] MODIFIER LETTER TRIANGULAR COLON..MODIFIER LETTER HALF TRIANGULAR COLON
	if ((val>=0x02E0) && (val<=0x02E4)) return L;	//# Lm   [5] MODIFIER LETTER SMALL GAMMA..MODIFIER LETTER SMALL REVERSED GLOTTAL STOP
	if (val==0x000002EE) return L;	//# Lm       MODIFIER LETTER DOUBLE APOSTROPHE
	if (val==0x0000037A) return L;	//# Lm       GREEK YPOGEGRAMMENI
	if ((val>=0x037B) && (val<=0x037D)) return L;	//# L&   [3] GREEK SMALL REVERSED LUNATE SIGMA SYMBOL..GREEK SMALL REVERSED DOTTED LUNATE SIGMA SYMBOL
	if (val==0x00000386) return L;	//# L&       GREEK CAPITAL LETTER ALPHA WITH TONOS
	if ((val>=0x0388) && (val<=0x038A)) return L;	//# L&   [3] GREEK CAPITAL LETTER EPSILON WITH TONOS..GREEK CAPITAL LETTER IOTA WITH TONOS
	if (val==0x0000038C) return L;	//# L&       GREEK CAPITAL LETTER OMICRON WITH TONOS
	if ((val>=0x038E) && (val<=0x03A1)) return L;	//# L&  [20] GREEK CAPITAL LETTER UPSILON WITH TONOS..GREEK CAPITAL LETTER RHO
	if ((val>=0x03A3) && (val<=0x03CE)) return L;	//# L&  [44] GREEK CAPITAL LETTER SIGMA..GREEK SMALL LETTER OMEGA WITH TONOS
	if ((val>=0x03D0) && (val<=0x03F5)) return L;	//# L&  [38] GREEK BETA SYMBOL..GREEK LUNATE EPSILON SYMBOL
	if ((val>=0x03F7) && (val<=0x0481)) return L;	//# L& [139] GREEK CAPITAL LETTER SHO..CYRILLIC SMALL LETTER KOPPA
	if (val==0x00000482) return L;	//# So       CYRILLIC THOUSANDS SIGN
	if ((val>=0x048A) && (val<=0x0513)) return L;	//# L& [138] CYRILLIC CAPITAL LETTER SHORT I WITH TAIL..CYRILLIC SMALL LETTER EL WITH HOOK
	if ((val>=0x0531) && (val<=0x0556)) return L;	//# L&  [38] ARMENIAN CAPITAL LETTER AYB..ARMENIAN CAPITAL LETTER FEH
	if (val==0x00000559) return L;	//# Lm       ARMENIAN MODIFIER LETTER LEFT HALF RING
	if ((val>=0x055A) && (val<=0x055F)) return L;	//# Po   [6] ARMENIAN APOSTROPHE..ARMENIAN ABBREVIATION MARK
	if ((val>=0x0561) && (val<=0x0587)) return L;	//# L&  [39] ARMENIAN SMALL LETTER AYB..ARMENIAN SMALL LIGATURE ECH YIWN
	if (val==0x00000589) return L;	//# Po       ARMENIAN FULL STOP
	if (val==0x00000903) return L;	//# Mc       DEVANAGARI SIGN VISARGA
	if ((val>=0x0904) && (val<=0x0939)) return L;	//# Lo  [54] DEVANAGARI LETTER SHORT A..DEVANAGARI LETTER HA
	if (val==0x0000093D) return L;	//# Lo       DEVANAGARI SIGN AVAGRAHA
	if ((val>=0x093E) && (val<=0x0940)) return L;	//# Mc   [3] DEVANAGARI VOWEL SIGN AA..DEVANAGARI VOWEL SIGN II
	if ((val>=0x0949) && (val<=0x094C)) return L;	//# Mc   [4] DEVANAGARI VOWEL SIGN CANDRA O..DEVANAGARI VOWEL SIGN AU
	if (val==0x00000950) return L;	//# Lo       DEVANAGARI OM
	if ((val>=0x0958) && (val<=0x0961)) return L;	//# Lo  [10] DEVANAGARI LETTER QA..DEVANAGARI LETTER VOCALIC LL
	if ((val>=0x0964) && (val<=0x0965)) return L;	//# Po   [2] DEVANAGARI DANDA..DEVANAGARI DOUBLE DANDA
	if ((val>=0x0966) && (val<=0x096F)) return L;	//# Nd  [10] DEVANAGARI DIGIT ZERO..DEVANAGARI DIGIT NINE
	if (val==0x00000970) return L;	//# Po       DEVANAGARI ABBREVIATION SIGN
	if ((val>=0x097B) && (val<=0x097F)) return L;	//# Lo   [5] DEVANAGARI LETTER GGA..DEVANAGARI LETTER BBA
	if ((val>=0x0982) && (val<=0x0983)) return L;	//# Mc   [2] BENGALI SIGN ANUSVARA..BENGALI SIGN VISARGA
	if ((val>=0x0985) && (val<=0x098C)) return L;	//# Lo   [8] BENGALI LETTER A..BENGALI LETTER VOCALIC L
	if ((val>=0x098F) && (val<=0x0990)) return L;	//# Lo   [2] BENGALI LETTER E..BENGALI LETTER AI
	if ((val>=0x0993) && (val<=0x09A8)) return L;	//# Lo  [22] BENGALI LETTER O..BENGALI LETTER NA
	if ((val>=0x09AA) && (val<=0x09B0)) return L;	//# Lo   [7] BENGALI LETTER PA..BENGALI LETTER RA
	if (val==0x000009B2) return L;	//# Lo       BENGALI LETTER LA
	if ((val>=0x09B6) && (val<=0x09B9)) return L;	//# Lo   [4] BENGALI LETTER SHA..BENGALI LETTER HA
	if (val==0x000009BD) return L;	//# Lo       BENGALI SIGN AVAGRAHA
	if ((val>=0x09BE) && (val<=0x09C0)) return L;	//# Mc   [3] BENGALI VOWEL SIGN AA..BENGALI VOWEL SIGN II
	if ((val>=0x09C7) && (val<=0x09C8)) return L;	//# Mc   [2] BENGALI VOWEL SIGN E..BENGALI VOWEL SIGN AI
	if ((val>=0x09CB) && (val<=0x09CC)) return L;	//# Mc   [2] BENGALI VOWEL SIGN O..BENGALI VOWEL SIGN AU
	if (val==0x000009CE) return L;	//# Lo       BENGALI LETTER KHANDA TA
	if (val==0x000009D7) return L;	//# Mc       BENGALI AU LENGTH MARK
	if ((val>=0x09DC) && (val<=0x09DD)) return L;	//# Lo   [2] BENGALI LETTER RRA..BENGALI LETTER RHA
	if ((val>=0x09DF) && (val<=0x09E1)) return L;	//# Lo   [3] BENGALI LETTER YYA..BENGALI LETTER VOCALIC LL
	if ((val>=0x09E6) && (val<=0x09EF)) return L;	//# Nd  [10] BENGALI DIGIT ZERO..BENGALI DIGIT NINE
	if ((val>=0x09F0) && (val<=0x09F1)) return L;	//# Lo   [2] BENGALI LETTER RA WITH MIDDLE DIAGONAL..BENGALI LETTER RA WITH LOWER DIAGONAL
	if ((val>=0x09F4) && (val<=0x09F9)) return L;	//# No   [6] BENGALI CURRENCY NUMERATOR ONE..BENGALI CURRENCY DENOMINATOR SIXTEEN
	if (val==0x000009FA) return L;	//# So       BENGALI ISSHAR
	if (val==0x00000A03) return L;	//# Mc       GURMUKHI SIGN VISARGA
	if ((val>=0x0A05) && (val<=0x0A0A)) return L;	//# Lo   [6] GURMUKHI LETTER A..GURMUKHI LETTER UU
	if ((val>=0x0A0F) && (val<=0x0A10)) return L;	//# Lo   [2] GURMUKHI LETTER EE..GURMUKHI LETTER AI
	if ((val>=0x0A13) && (val<=0x0A28)) return L;	//# Lo  [22] GURMUKHI LETTER OO..GURMUKHI LETTER NA
	if ((val>=0x0A2A) && (val<=0x0A30)) return L;	//# Lo   [7] GURMUKHI LETTER PA..GURMUKHI LETTER RA
	if ((val>=0x0A32) && (val<=0x0A33)) return L;	//# Lo   [2] GURMUKHI LETTER LA..GURMUKHI LETTER LLA
	if ((val>=0x0A35) && (val<=0x0A36)) return L;	//# Lo   [2] GURMUKHI LETTER VA..GURMUKHI LETTER SHA
	if ((val>=0x0A38) && (val<=0x0A39)) return L;	//# Lo   [2] GURMUKHI LETTER SA..GURMUKHI LETTER HA
	if ((val>=0x0A3E) && (val<=0x0A40)) return L;	//# Mc   [3] GURMUKHI VOWEL SIGN AA..GURMUKHI VOWEL SIGN II
	if ((val>=0x0A59) && (val<=0x0A5C)) return L;	//# Lo   [4] GURMUKHI LETTER KHHA..GURMUKHI LETTER RRA
	if (val==0x00000A5E) return L;	//# Lo       GURMUKHI LETTER FA
	if ((val>=0x0A66) && (val<=0x0A6F)) return L;	//# Nd  [10] GURMUKHI DIGIT ZERO..GURMUKHI DIGIT NINE
	if ((val>=0x0A72) && (val<=0x0A74)) return L;	//# Lo   [3] GURMUKHI IRI..GURMUKHI EK ONKAR
	if (val==0x00000A83) return L;	//# Mc       GUJARATI SIGN VISARGA
	if ((val>=0x0A85) && (val<=0x0A8D)) return L;	//# Lo   [9] GUJARATI LETTER A..GUJARATI VOWEL CANDRA E
	if ((val>=0x0A8F) && (val<=0x0A91)) return L;	//# Lo   [3] GUJARATI LETTER E..GUJARATI VOWEL CANDRA O
	if ((val>=0x0A93) && (val<=0x0AA8)) return L;	//# Lo  [22] GUJARATI LETTER O..GUJARATI LETTER NA
	if ((val>=0x0AAA) && (val<=0x0AB0)) return L;	//# Lo   [7] GUJARATI LETTER PA..GUJARATI LETTER RA
	if ((val>=0x0AB2) && (val<=0x0AB3)) return L;	//# Lo   [2] GUJARATI LETTER LA..GUJARATI LETTER LLA
	if ((val>=0x0AB5) && (val<=0x0AB9)) return L;	//# Lo   [5] GUJARATI LETTER VA..GUJARATI LETTER HA
	if (val==0x00000ABD) return L;	//# Lo       GUJARATI SIGN AVAGRAHA
	if ((val>=0x0ABE) && (val<=0x0AC0)) return L;	//# Mc   [3] GUJARATI VOWEL SIGN AA..GUJARATI VOWEL SIGN II
	if (val==0x00000AC9) return L;	//# Mc       GUJARATI VOWEL SIGN CANDRA O
	if ((val>=0x0ACB) && (val<=0x0ACC)) return L;	//# Mc   [2] GUJARATI VOWEL SIGN O..GUJARATI VOWEL SIGN AU
	if (val==0x00000AD0) return L;	//# Lo       GUJARATI OM
	if ((val>=0x0AE0) && (val<=0x0AE1)) return L;	//# Lo   [2] GUJARATI LETTER VOCALIC RR..GUJARATI LETTER VOCALIC LL
	if ((val>=0x0AE6) && (val<=0x0AEF)) return L;	//# Nd  [10] GUJARATI DIGIT ZERO..GUJARATI DIGIT NINE
	if ((val>=0x0B02) && (val<=0x0B03)) return L;	//# Mc   [2] ORIYA SIGN ANUSVARA..ORIYA SIGN VISARGA
	if ((val>=0x0B05) && (val<=0x0B0C)) return L;	//# Lo   [8] ORIYA LETTER A..ORIYA LETTER VOCALIC L
	if ((val>=0x0B0F) && (val<=0x0B10)) return L;	//# Lo   [2] ORIYA LETTER E..ORIYA LETTER AI
	if ((val>=0x0B13) && (val<=0x0B28)) return L;	//# Lo  [22] ORIYA LETTER O..ORIYA LETTER NA
	if ((val>=0x0B2A) && (val<=0x0B30)) return L;	//# Lo   [7] ORIYA LETTER PA..ORIYA LETTER RA
	if ((val>=0x0B32) && (val<=0x0B33)) return L;	//# Lo   [2] ORIYA LETTER LA..ORIYA LETTER LLA
	if ((val>=0x0B35) && (val<=0x0B39)) return L;	//# Lo   [5] ORIYA LETTER VA..ORIYA LETTER HA
	if (val==0x00000B3D) return L;	//# Lo       ORIYA SIGN AVAGRAHA
	if (val==0x00000B3E) return L;	//# Mc       ORIYA VOWEL SIGN AA
	if (val==0x00000B40) return L;	//# Mc       ORIYA VOWEL SIGN II
	if ((val>=0x0B47) && (val<=0x0B48)) return L;	//# Mc   [2] ORIYA VOWEL SIGN E..ORIYA VOWEL SIGN AI
	if ((val>=0x0B4B) && (val<=0x0B4C)) return L;	//# Mc   [2] ORIYA VOWEL SIGN O..ORIYA VOWEL SIGN AU
	if (val==0x00000B57) return L;	//# Mc       ORIYA AU LENGTH MARK
	if ((val>=0x0B5C) && (val<=0x0B5D)) return L;	//# Lo   [2] ORIYA LETTER RRA..ORIYA LETTER RHA
	if ((val>=0x0B5F) && (val<=0x0B61)) return L;	//# Lo   [3] ORIYA LETTER YYA..ORIYA LETTER VOCALIC LL
	if ((val>=0x0B66) && (val<=0x0B6F)) return L;	//# Nd  [10] ORIYA DIGIT ZERO..ORIYA DIGIT NINE
	if (val==0x00000B70) return L;	//# So       ORIYA ISSHAR
	if (val==0x00000B71) return L;	//# Lo       ORIYA LETTER WA
	if (val==0x00000B83) return L;	//# Lo       TAMIL SIGN VISARGA
	if ((val>=0x0B85) && (val<=0x0B8A)) return L;	//# Lo   [6] TAMIL LETTER A..TAMIL LETTER UU
	if ((val>=0x0B8E) && (val<=0x0B90)) return L;	//# Lo   [3] TAMIL LETTER E..TAMIL LETTER AI
	if ((val>=0x0B92) && (val<=0x0B95)) return L;	//# Lo   [4] TAMIL LETTER O..TAMIL LETTER KA
	if ((val>=0x0B99) && (val<=0x0B9A)) return L;	//# Lo   [2] TAMIL LETTER NGA..TAMIL LETTER CA
	if (val==0x00000B9C) return L;	//# Lo       TAMIL LETTER JA
	if ((val>=0x0B9E) && (val<=0x0B9F)) return L;	//# Lo   [2] TAMIL LETTER NYA..TAMIL LETTER TTA
	if ((val>=0x0BA3) && (val<=0x0BA4)) return L;	//# Lo   [2] TAMIL LETTER NNA..TAMIL LETTER TA
	if ((val>=0x0BA8) && (val<=0x0BAA)) return L;	//# Lo   [3] TAMIL LETTER NA..TAMIL LETTER PA
	if ((val>=0x0BAE) && (val<=0x0BB9)) return L;	//# Lo  [12] TAMIL LETTER MA..TAMIL LETTER HA
	if ((val>=0x0BBE) && (val<=0x0BBF)) return L;	//# Mc   [2] TAMIL VOWEL SIGN AA..TAMIL VOWEL SIGN I
	if ((val>=0x0BC1) && (val<=0x0BC2)) return L;	//# Mc   [2] TAMIL VOWEL SIGN U..TAMIL VOWEL SIGN UU
	if ((val>=0x0BC6) && (val<=0x0BC8)) return L;	//# Mc   [3] TAMIL VOWEL SIGN E..TAMIL VOWEL SIGN AI
	if ((val>=0x0BCA) && (val<=0x0BCC)) return L;	//# Mc   [3] TAMIL VOWEL SIGN O..TAMIL VOWEL SIGN AU
	if (val==0x00000BD7) return L;	//# Mc       TAMIL AU LENGTH MARK
	if ((val>=0x0BE6) && (val<=0x0BEF)) return L;	//# Nd  [10] TAMIL DIGIT ZERO..TAMIL DIGIT NINE
	if ((val>=0x0BF0) && (val<=0x0BF2)) return L;	//# No   [3] TAMIL NUMBER TEN..TAMIL NUMBER ONE THOUSAND
	if ((val>=0x0C01) && (val<=0x0C03)) return L;	//# Mc   [3] TELUGU SIGN CANDRABINDU..TELUGU SIGN VISARGA
	if ((val>=0x0C05) && (val<=0x0C0C)) return L;	//# Lo   [8] TELUGU LETTER A..TELUGU LETTER VOCALIC L
	if ((val>=0x0C0E) && (val<=0x0C10)) return L;	//# Lo   [3] TELUGU LETTER E..TELUGU LETTER AI
	if ((val>=0x0C12) && (val<=0x0C28)) return L;	//# Lo  [23] TELUGU LETTER O..TELUGU LETTER NA
	if ((val>=0x0C2A) && (val<=0x0C33)) return L;	//# Lo  [10] TELUGU LETTER PA..TELUGU LETTER LLA
	if ((val>=0x0C35) && (val<=0x0C39)) return L;	//# Lo   [5] TELUGU LETTER VA..TELUGU LETTER HA
	if ((val>=0x0C41) && (val<=0x0C44)) return L;	//# Mc   [4] TELUGU VOWEL SIGN U..TELUGU VOWEL SIGN VOCALIC RR
	if ((val>=0x0C60) && (val<=0x0C61)) return L;	//# Lo   [2] TELUGU LETTER VOCALIC RR..TELUGU LETTER VOCALIC LL
	if ((val>=0x0C66) && (val<=0x0C6F)) return L;	//# Nd  [10] TELUGU DIGIT ZERO..TELUGU DIGIT NINE
	if ((val>=0x0C82) && (val<=0x0C83)) return L;	//# Mc   [2] KANNADA SIGN ANUSVARA..KANNADA SIGN VISARGA
	if ((val>=0x0C85) && (val<=0x0C8C)) return L;	//# Lo   [8] KANNADA LETTER A..KANNADA LETTER VOCALIC L
	if ((val>=0x0C8E) && (val<=0x0C90)) return L;	//# Lo   [3] KANNADA LETTER E..KANNADA LETTER AI
	if ((val>=0x0C92) && (val<=0x0CA8)) return L;	//# Lo  [23] KANNADA LETTER O..KANNADA LETTER NA
	if ((val>=0x0CAA) && (val<=0x0CB3)) return L;	//# Lo  [10] KANNADA LETTER PA..KANNADA LETTER LLA
	if ((val>=0x0CB5) && (val<=0x0CB9)) return L;	//# Lo   [5] KANNADA LETTER VA..KANNADA LETTER HA
	if (val==0x00000CBD) return L;	//# Lo       KANNADA SIGN AVAGRAHA
	if (val==0x00000CBE) return L;	//# Mc       KANNADA VOWEL SIGN AA
	if (val==0x00000CBF) return L;	//# Mn       KANNADA VOWEL SIGN I
	if ((val>=0x0CC0) && (val<=0x0CC4)) return L;	//# Mc   [5] KANNADA VOWEL SIGN II..KANNADA VOWEL SIGN VOCALIC RR
	if (val==0x00000CC6) return L;	//# Mn       KANNADA VOWEL SIGN E
	if ((val>=0x0CC7) && (val<=0x0CC8)) return L;	//# Mc   [2] KANNADA VOWEL SIGN EE..KANNADA VOWEL SIGN AI
	if ((val>=0x0CCA) && (val<=0x0CCB)) return L;	//# Mc   [2] KANNADA VOWEL SIGN O..KANNADA VOWEL SIGN OO
	if ((val>=0x0CD5) && (val<=0x0CD6)) return L;	//# Mc   [2] KANNADA LENGTH MARK..KANNADA AI LENGTH MARK
	if (val==0x00000CDE) return L;	//# Lo       KANNADA LETTER FA
	if ((val>=0x0CE0) && (val<=0x0CE1)) return L;	//# Lo   [2] KANNADA LETTER VOCALIC RR..KANNADA LETTER VOCALIC LL
	if ((val>=0x0CE6) && (val<=0x0CEF)) return L;	//# Nd  [10] KANNADA DIGIT ZERO..KANNADA DIGIT NINE
	if ((val>=0x0D02) && (val<=0x0D03)) return L;	//# Mc   [2] MALAYALAM SIGN ANUSVARA..MALAYALAM SIGN VISARGA
	if ((val>=0x0D05) && (val<=0x0D0C)) return L;	//# Lo   [8] MALAYALAM LETTER A..MALAYALAM LETTER VOCALIC L
	if ((val>=0x0D0E) && (val<=0x0D10)) return L;	//# Lo   [3] MALAYALAM LETTER E..MALAYALAM LETTER AI
	if ((val>=0x0D12) && (val<=0x0D28)) return L;	//# Lo  [23] MALAYALAM LETTER O..MALAYALAM LETTER NA
	if ((val>=0x0D2A) && (val<=0x0D39)) return L;	//# Lo  [16] MALAYALAM LETTER PA..MALAYALAM LETTER HA
	if ((val>=0x0D3E) && (val<=0x0D40)) return L;	//# Mc   [3] MALAYALAM VOWEL SIGN AA..MALAYALAM VOWEL SIGN II
	if ((val>=0x0D46) && (val<=0x0D48)) return L;	//# Mc   [3] MALAYALAM VOWEL SIGN E..MALAYALAM VOWEL SIGN AI
	if ((val>=0x0D4A) && (val<=0x0D4C)) return L;	//# Mc   [3] MALAYALAM VOWEL SIGN O..MALAYALAM VOWEL SIGN AU
	if (val==0x00000D57) return L;	//# Mc       MALAYALAM AU LENGTH MARK
	if ((val>=0x0D60) && (val<=0x0D61)) return L;	//# Lo   [2] MALAYALAM LETTER VOCALIC RR..MALAYALAM LETTER VOCALIC LL
	if ((val>=0x0D66) && (val<=0x0D6F)) return L;	//# Nd  [10] MALAYALAM DIGIT ZERO..MALAYALAM DIGIT NINE
	if ((val>=0x0D82) && (val<=0x0D83)) return L;	//# Mc   [2] SINHALA SIGN ANUSVARAYA..SINHALA SIGN VISARGAYA
	if ((val>=0x0D85) && (val<=0x0D96)) return L;	//# Lo  [18] SINHALA LETTER AYANNA..SINHALA LETTER AUYANNA
	if ((val>=0x0D9A) && (val<=0x0DB1)) return L;	//# Lo  [24] SINHALA LETTER ALPAPRAANA KAYANNA..SINHALA LETTER DANTAJA NAYANNA
	if ((val>=0x0DB3) && (val<=0x0DBB)) return L;	//# Lo   [9] SINHALA LETTER SANYAKA DAYANNA..SINHALA LETTER RAYANNA
	if (val==0x00000DBD) return L;	//# Lo       SINHALA LETTER DANTAJA LAYANNA
	if ((val>=0x0DC0) && (val<=0x0DC6)) return L;	//# Lo   [7] SINHALA LETTER VAYANNA..SINHALA LETTER FAYANNA
	if ((val>=0x0DCF) && (val<=0x0DD1)) return L;	//# Mc   [3] SINHALA VOWEL SIGN AELA-PILLA..SINHALA VOWEL SIGN DIGA AEDA-PILLA
	if ((val>=0x0DD8) && (val<=0x0DDF)) return L;	//# Mc   [8] SINHALA VOWEL SIGN GAETTA-PILLA..SINHALA VOWEL SIGN GAYANUKITTA
	if ((val>=0x0DF2) && (val<=0x0DF3)) return L;	//# Mc   [2] SINHALA VOWEL SIGN DIGA GAETTA-PILLA..SINHALA VOWEL SIGN DIGA GAYANUKITTA
	if (val==0x00000DF4) return L;	//# Po       SINHALA PUNCTUATION KUNDDALIYA
	if ((val>=0x0E01) && (val<=0x0E30)) return L;	//# Lo  [48] THAI CHARACTER KO KAI..THAI CHARACTER SARA A
	if ((val>=0x0E32) && (val<=0x0E33)) return L;	//# Lo   [2] THAI CHARACTER SARA AA..THAI CHARACTER SARA AM
	if ((val>=0x0E40) && (val<=0x0E45)) return L;	//# Lo   [6] THAI CHARACTER SARA E..THAI CHARACTER LAKKHANGYAO
	if (val==0x00000E46) return L;	//# Lm       THAI CHARACTER MAIYAMOK
	if (val==0x00000E4F) return L;	//# Po       THAI CHARACTER FONGMAN
	if ((val>=0x0E50) && (val<=0x0E59)) return L;	//# Nd  [10] THAI DIGIT ZERO..THAI DIGIT NINE
	if ((val>=0x0E5A) && (val<=0x0E5B)) return L;	//# Po   [2] THAI CHARACTER ANGKHANKHU..THAI CHARACTER KHOMUT
	if ((val>=0x0E81) && (val<=0x0E82)) return L;	//# Lo   [2] LAO LETTER KO..LAO LETTER KHO SUNG
	if (val==0x00000E84) return L;	//# Lo       LAO LETTER KHO TAM
	if ((val>=0x0E87) && (val<=0x0E88)) return L;	//# Lo   [2] LAO LETTER NGO..LAO LETTER CO
	if (val==0x00000E8A) return L;	//# Lo       LAO LETTER SO TAM
	if (val==0x00000E8D) return L;	//# Lo       LAO LETTER NYO
	if ((val>=0x0E94) && (val<=0x0E97)) return L;	//# Lo   [4] LAO LETTER DO..LAO LETTER THO TAM
	if ((val>=0x0E99) && (val<=0x0E9F)) return L;	//# Lo   [7] LAO LETTER NO..LAO LETTER FO SUNG
	if ((val>=0x0EA1) && (val<=0x0EA3)) return L;	//# Lo   [3] LAO LETTER MO..LAO LETTER LO LING
	if (val==0x00000EA5) return L;	//# Lo       LAO LETTER LO LOOT
	if (val==0x00000EA7) return L;	//# Lo       LAO LETTER WO
	if ((val>=0x0EAA) && (val<=0x0EAB)) return L;	//# Lo   [2] LAO LETTER SO SUNG..LAO LETTER HO SUNG
	if ((val>=0x0EAD) && (val<=0x0EB0)) return L;	//# Lo   [4] LAO LETTER O..LAO VOWEL SIGN A
	if ((val>=0x0EB2) && (val<=0x0EB3)) return L;	//# Lo   [2] LAO VOWEL SIGN AA..LAO VOWEL SIGN AM
	if (val==0x00000EBD) return L;	//# Lo       LAO SEMIVOWEL SIGN NYO
	if ((val>=0x0EC0) && (val<=0x0EC4)) return L;	//# Lo   [5] LAO VOWEL SIGN E..LAO VOWEL SIGN AI
	if (val==0x00000EC6) return L;	//# Lm       LAO KO LA
	if ((val>=0x0ED0) && (val<=0x0ED9)) return L;	//# Nd  [10] LAO DIGIT ZERO..LAO DIGIT NINE
	if ((val>=0x0EDC) && (val<=0x0EDD)) return L;	//# Lo   [2] LAO HO NO..LAO HO MO
	if (val==0x00000F00) return L;	//# Lo       TIBETAN SYLLABLE OM
	if ((val>=0x0F01) && (val<=0x0F03)) return L;	//# So   [3] TIBETAN MARK GTER YIG MGO TRUNCATED A..TIBETAN MARK GTER YIG MGO -UM GTER TSHEG MA
	if ((val>=0x0F04) && (val<=0x0F12)) return L;	//# Po  [15] TIBETAN MARK INITIAL YIG MGO MDUN MA..TIBETAN MARK RGYA GRAM SHAD
	if ((val>=0x0F13) && (val<=0x0F17)) return L;	//# So   [5] TIBETAN MARK CARET -DZUD RTAGS ME LONG CAN..TIBETAN ASTROLOGICAL SIGN SGRA GCAN -CHAR RTAGS
	if ((val>=0x0F1A) && (val<=0x0F1F)) return L;	//# So   [6] TIBETAN SIGN RDEL DKAR GCIG..TIBETAN SIGN RDEL DKAR RDEL NAG
	if ((val>=0x0F20) && (val<=0x0F29)) return L;	//# Nd  [10] TIBETAN DIGIT ZERO..TIBETAN DIGIT NINE
	if ((val>=0x0F2A) && (val<=0x0F33)) return L;	//# No  [10] TIBETAN DIGIT HALF ONE..TIBETAN DIGIT HALF ZERO
	if (val==0x00000F34) return L;	//# So       TIBETAN MARK BSDUS RTAGS
	if (val==0x00000F36) return L;	//# So       TIBETAN MARK CARET -DZUD RTAGS BZHI MIG CAN
	if (val==0x00000F38) return L;	//# So       TIBETAN MARK CHE MGO
	if ((val>=0x0F3E) && (val<=0x0F3F)) return L;	//# Mc   [2] TIBETAN SIGN YAR TSHES..TIBETAN SIGN MAR TSHES
	if ((val>=0x0F40) && (val<=0x0F47)) return L;	//# Lo   [8] TIBETAN LETTER KA..TIBETAN LETTER JA
	if ((val>=0x0F49) && (val<=0x0F6A)) return L;	//# Lo  [34] TIBETAN LETTER NYA..TIBETAN LETTER FIXED-FORM RA
	if (val==0x00000F7F) return L;	//# Mc       TIBETAN SIGN RNAM BCAD
	if (val==0x00000F85) return L;	//# Po       TIBETAN MARK PALUTA
	if ((val>=0x0F88) && (val<=0x0F8B)) return L;	//# Lo   [4] TIBETAN SIGN LCE TSA CAN..TIBETAN SIGN GRU MED RGYINGS
	if ((val>=0x0FBE) && (val<=0x0FC5)) return L;	//# So   [8] TIBETAN KU RU KHA..TIBETAN SYMBOL RDO RJE
	if ((val>=0x0FC7) && (val<=0x0FCC)) return L;	//# So   [6] TIBETAN SYMBOL RDO RJE RGYA GRAM..TIBETAN SYMBOL NOR BU BZHI -KHYIL
	if (val==0x00000FCF) return L;	//# So       TIBETAN SIGN RDEL NAG GSUM
	if ((val>=0x0FD0) && (val<=0x0FD1)) return L;	//# Po   [2] TIBETAN MARK BSKA- SHOG GI MGO RGYAN..TIBETAN MARK MNYAM YIG GI MGO RGYAN
	if ((val>=0x1000) && (val<=0x1021)) return L;	//# Lo  [34] MYANMAR LETTER KA..MYANMAR LETTER A
	if ((val>=0x1023) && (val<=0x1027)) return L;	//# Lo   [5] MYANMAR LETTER I..MYANMAR LETTER E
	if ((val>=0x1029) && (val<=0x102A)) return L;	//# Lo   [2] MYANMAR LETTER O..MYANMAR LETTER AU
	if (val==0x0000102C) return L;	//# Mc       MYANMAR VOWEL SIGN AA
	if (val==0x00001031) return L;	//# Mc       MYANMAR VOWEL SIGN E
	if (val==0x00001038) return L;	//# Mc       MYANMAR SIGN VISARGA
	if ((val>=0x1040) && (val<=0x1049)) return L;	//# Nd  [10] MYANMAR DIGIT ZERO..MYANMAR DIGIT NINE
	if ((val>=0x104A) && (val<=0x104F)) return L;	//# Po   [6] MYANMAR SIGN LITTLE SECTION..MYANMAR SYMBOL GENITIVE
	if ((val>=0x1050) && (val<=0x1055)) return L;	//# Lo   [6] MYANMAR LETTER SHA..MYANMAR LETTER VOCALIC LL
	if ((val>=0x1056) && (val<=0x1057)) return L;	//# Mc   [2] MYANMAR VOWEL SIGN VOCALIC R..MYANMAR VOWEL SIGN VOCALIC RR
	if ((val>=0x10A0) && (val<=0x10C5)) return L;	//# L&  [38] GEORGIAN CAPITAL LETTER AN..GEORGIAN CAPITAL LETTER HOE
	if ((val>=0x10D0) && (val<=0x10FA)) return L;	//# Lo  [43] GEORGIAN LETTER AN..GEORGIAN LETTER AIN
	if (val==0x000010FB) return L;	//# Po       GEORGIAN PARAGRAPH SEPARATOR
	if (val==0x000010FC) return L;	//# Lm       MODIFIER LETTER GEORGIAN NAR
	if ((val>=0x1100) && (val<=0x1159)) return L;	//# Lo  [90] HANGUL CHOSEONG KIYEOK..HANGUL CHOSEONG YEORINHIEUH
	if ((val>=0x115F) && (val<=0x11A2)) return L;	//# Lo  [68] HANGUL CHOSEONG FILLER..HANGUL JUNGSEONG SSANGARAEA
	if ((val>=0x11A8) && (val<=0x11F9)) return L;	//# Lo  [82] HANGUL JONGSEONG KIYEOK..HANGUL JONGSEONG YEORINHIEUH
	if ((val>=0x1200) && (val<=0x1248)) return L;	//# Lo  [73] ETHIOPIC SYLLABLE HA..ETHIOPIC SYLLABLE QWA
	if ((val>=0x124A) && (val<=0x124D)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE QWI..ETHIOPIC SYLLABLE QWE
	if ((val>=0x1250) && (val<=0x1256)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE QHA..ETHIOPIC SYLLABLE QHO
	if (val==0x00001258) return L;	//# Lo       ETHIOPIC SYLLABLE QHWA
	if ((val>=0x125A) && (val<=0x125D)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE QHWI..ETHIOPIC SYLLABLE QHWE
	if ((val>=0x1260) && (val<=0x1288)) return L;	//# Lo  [41] ETHIOPIC SYLLABLE BA..ETHIOPIC SYLLABLE XWA
	if ((val>=0x128A) && (val<=0x128D)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE XWI..ETHIOPIC SYLLABLE XWE
	if ((val>=0x1290) && (val<=0x12B0)) return L;	//# Lo  [33] ETHIOPIC SYLLABLE NA..ETHIOPIC SYLLABLE KWA
	if ((val>=0x12B2) && (val<=0x12B5)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE KWI..ETHIOPIC SYLLABLE KWE
	if ((val>=0x12B8) && (val<=0x12BE)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE KXA..ETHIOPIC SYLLABLE KXO
	if (val==0x000012C0) return L;	//# Lo       ETHIOPIC SYLLABLE KXWA
	if ((val>=0x12C2) && (val<=0x12C5)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE KXWI..ETHIOPIC SYLLABLE KXWE
	if ((val>=0x12C8) && (val<=0x12D6)) return L;	//# Lo  [15] ETHIOPIC SYLLABLE WA..ETHIOPIC SYLLABLE PHARYNGEAL O
	if ((val>=0x12D8) && (val<=0x1310)) return L;	//# Lo  [57] ETHIOPIC SYLLABLE ZA..ETHIOPIC SYLLABLE GWA
	if ((val>=0x1312) && (val<=0x1315)) return L;	//# Lo   [4] ETHIOPIC SYLLABLE GWI..ETHIOPIC SYLLABLE GWE
	if ((val>=0x1318) && (val<=0x135A)) return L;	//# Lo  [67] ETHIOPIC SYLLABLE GGA..ETHIOPIC SYLLABLE FYA
	if (val==0x00001360) return L;	//# So       ETHIOPIC SECTION MARK
	if ((val>=0x1361) && (val<=0x1368)) return L;	//# Po   [8] ETHIOPIC WORDSPACE..ETHIOPIC PARAGRAPH SEPARATOR
	if ((val>=0x1369) && (val<=0x137C)) return L;	//# No  [20] ETHIOPIC DIGIT ONE..ETHIOPIC NUMBER TEN THOUSAND
	if ((val>=0x1380) && (val<=0x138F)) return L;	//# Lo  [16] ETHIOPIC SYLLABLE SEBATBEIT MWA..ETHIOPIC SYLLABLE PWE
	if ((val>=0x13A0) && (val<=0x13F4)) return L;	//# Lo  [85] CHEROKEE LETTER A..CHEROKEE LETTER YV
	if ((val>=0x1401) && (val<=0x166C)) return L;	//# Lo [620] CANADIAN SYLLABICS E..CANADIAN SYLLABICS CARRIER TTSA
	if ((val>=0x166D) && (val<=0x166E)) return L;	//# Po   [2] CANADIAN SYLLABICS CHI SIGN..CANADIAN SYLLABICS FULL STOP
	if ((val>=0x166F) && (val<=0x1676)) return L;	//# Lo   [8] CANADIAN SYLLABICS QAI..CANADIAN SYLLABICS NNGAA
	if ((val>=0x1681) && (val<=0x169A)) return L;	//# Lo  [26] OGHAM LETTER BEITH..OGHAM LETTER PEITH
	if ((val>=0x16A0) && (val<=0x16EA)) return L;	//# Lo  [75] RUNIC LETTER FEHU FEOH FE F..RUNIC LETTER X
	if ((val>=0x16EB) && (val<=0x16ED)) return L;	//# Po   [3] RUNIC SINGLE PUNCTUATION..RUNIC CROSS PUNCTUATION
	if ((val>=0x16EE) && (val<=0x16F0)) return L;	//# Nl   [3] RUNIC ARLAUG SYMBOL..RUNIC BELGTHOR SYMBOL
	if ((val>=0x1700) && (val<=0x170C)) return L;	//# Lo  [13] TAGALOG LETTER A..TAGALOG LETTER YA
	if ((val>=0x170E) && (val<=0x1711)) return L;	//# Lo   [4] TAGALOG LETTER LA..TAGALOG LETTER HA
	if ((val>=0x1720) && (val<=0x1731)) return L;	//# Lo  [18] HANUNOO LETTER A..HANUNOO LETTER HA
	if ((val>=0x1735) && (val<=0x1736)) return L;	//# Po   [2] PHILIPPINE SINGLE PUNCTUATION..PHILIPPINE DOUBLE PUNCTUATION
	if ((val>=0x1740) && (val<=0x1751)) return L;	//# Lo  [18] BUHID LETTER A..BUHID LETTER HA
	if ((val>=0x1760) && (val<=0x176C)) return L;	//# Lo  [13] TAGBANWA LETTER A..TAGBANWA LETTER YA
	if ((val>=0x176E) && (val<=0x1770)) return L;	//# Lo   [3] TAGBANWA LETTER LA..TAGBANWA LETTER SA
	if ((val>=0x1780) && (val<=0x17B3)) return L;	//# Lo  [52] KHMER LETTER KA..KHMER INDEPENDENT VOWEL QAU
	if ((val>=0x17B4) && (val<=0x17B5)) return L;	//# Cf   [2] KHMER VOWEL INHERENT AQ..KHMER VOWEL INHERENT AA
	if (val==0x000017B6) return L;	//# Mc       KHMER VOWEL SIGN AA
	if ((val>=0x17BE) && (val<=0x17C5)) return L;	//# Mc   [8] KHMER VOWEL SIGN OE..KHMER VOWEL SIGN AU
	if ((val>=0x17C7) && (val<=0x17C8)) return L;	//# Mc   [2] KHMER SIGN REAHMUK..KHMER SIGN YUUKALEAPINTU
	if ((val>=0x17D4) && (val<=0x17D6)) return L;	//# Po   [3] KHMER SIGN KHAN..KHMER SIGN CAMNUC PII KUUH
	if (val==0x000017D7) return L;	//# Lm       KHMER SIGN LEK TOO
	if ((val>=0x17D8) && (val<=0x17DA)) return L;	//# Po   [3] KHMER SIGN BEYYAL..KHMER SIGN KOOMUUT
	if (val==0x000017DC) return L;	//# Lo       KHMER SIGN AVAKRAHASANYA
	if ((val>=0x17E0) && (val<=0x17E9)) return L;	//# Nd  [10] KHMER DIGIT ZERO..KHMER DIGIT NINE
	if ((val>=0x1810) && (val<=0x1819)) return L;	//# Nd  [10] MONGOLIAN DIGIT ZERO..MONGOLIAN DIGIT NINE
	if ((val>=0x1820) && (val<=0x1842)) return L;	//# Lo  [35] MONGOLIAN LETTER A..MONGOLIAN LETTER CHI
	if (val==0x00001843) return L;	//# Lm       MONGOLIAN LETTER TODO LONG VOWEL SIGN
	if ((val>=0x1844) && (val<=0x1877)) return L;	//# Lo  [52] MONGOLIAN LETTER TODO E..MONGOLIAN LETTER MANCHU ZHA
	if ((val>=0x1880) && (val<=0x18A8)) return L;	//# Lo  [41] MONGOLIAN LETTER ALI GALI ANUSVARA ONE..MONGOLIAN LETTER MANCHU ALI GALI BHA
	if ((val>=0x1900) && (val<=0x191C)) return L;	//# Lo  [29] LIMBU VOWEL-CARRIER LETTER..LIMBU LETTER HA
	if ((val>=0x1923) && (val<=0x1926)) return L;	//# Mc   [4] LIMBU VOWEL SIGN EE..LIMBU VOWEL SIGN AU
	if ((val>=0x1930) && (val<=0x1931)) return L;	//# Mc   [2] LIMBU SMALL LETTER KA..LIMBU SMALL LETTER NGA
	if ((val>=0x1933) && (val<=0x1938)) return L;	//# Mc   [6] LIMBU SMALL LETTER TA..LIMBU SMALL LETTER LA
	if ((val>=0x1946) && (val<=0x194F)) return L;	//# Nd  [10] LIMBU DIGIT ZERO..LIMBU DIGIT NINE
	if ((val>=0x1950) && (val<=0x196D)) return L;	//# Lo  [30] TAI LE LETTER KA..TAI LE LETTER AI
	if ((val>=0x1970) && (val<=0x1974)) return L;	//# Lo   [5] TAI LE LETTER TONE-2..TAI LE LETTER TONE-6
	if ((val>=0x1980) && (val<=0x19A9)) return L;	//# Lo  [42] NEW TAI LUE LETTER HIGH QA..NEW TAI LUE LETTER LOW XVA
	if ((val>=0x19B0) && (val<=0x19C0)) return L;	//# Mc  [17] NEW TAI LUE VOWEL SIGN VOWEL SHORTENER..NEW TAI LUE VOWEL SIGN IY
	if ((val>=0x19C1) && (val<=0x19C7)) return L;	//# Lo   [7] NEW TAI LUE LETTER FINAL V..NEW TAI LUE LETTER FINAL B
	if ((val>=0x19C8) && (val<=0x19C9)) return L;	//# Mc   [2] NEW TAI LUE TONE MARK-1..NEW TAI LUE TONE MARK-2
	if ((val>=0x19D0) && (val<=0x19D9)) return L;	//# Nd  [10] NEW TAI LUE DIGIT ZERO..NEW TAI LUE DIGIT NINE
	if ((val>=0x1A00) && (val<=0x1A16)) return L;	//# Lo  [23] BUGINESE LETTER KA..BUGINESE LETTER HA
	if ((val>=0x1A19) && (val<=0x1A1B)) return L;	//# Mc   [3] BUGINESE VOWEL SIGN E..BUGINESE VOWEL SIGN AE
	if ((val>=0x1A1E) && (val<=0x1A1F)) return L;	//# Po   [2] BUGINESE PALLAWA..BUGINESE END OF SECTION
	if (val==0x00001B04) return L;	//# Mc       BALINESE SIGN BISAH
	if ((val>=0x1B05) && (val<=0x1B33)) return L;	//# Lo  [47] BALINESE LETTER AKARA..BALINESE LETTER HA
	if (val==0x00001B35) return L;	//# Mc       BALINESE VOWEL SIGN TEDUNG
	if (val==0x00001B3B) return L;	//# Mc       BALINESE VOWEL SIGN RA REPA TEDUNG
	if ((val>=0x1B3D) && (val<=0x1B41)) return L;	//# Mc   [5] BALINESE VOWEL SIGN LA LENGA TEDUNG..BALINESE VOWEL SIGN TALING REPA TEDUNG
	if ((val>=0x1B43) && (val<=0x1B44)) return L;	//# Mc   [2] BALINESE VOWEL SIGN PEPET TEDUNG..BALINESE ADEG ADEG
	if ((val>=0x1B45) && (val<=0x1B4B)) return L;	//# Lo   [7] BALINESE LETTER KAF SASAK..BALINESE LETTER ASYURA SASAK
	if ((val>=0x1B50) && (val<=0x1B59)) return L;	//# Nd  [10] BALINESE DIGIT ZERO..BALINESE DIGIT NINE
	if ((val>=0x1B5A) && (val<=0x1B60)) return L;	//# Po   [7] BALINESE PANTI..BALINESE PAMENENG
	if ((val>=0x1B61) && (val<=0x1B6A)) return L;	//# So  [10] BALINESE MUSICAL SYMBOL DONG..BALINESE MUSICAL SYMBOL DANG GEDE
	if ((val>=0x1B74) && (val<=0x1B7C)) return L;	//# So   [9] BALINESE MUSICAL SYMBOL RIGHT-HAND OPEN DUG..BALINESE MUSICAL SYMBOL LEFT-HAND OPEN PING
	if ((val>=0x1D00) && (val<=0x1D2B)) return L;	//# L&  [44] LATIN LETTER SMALL CAPITAL A..CYRILLIC LETTER SMALL CAPITAL EL
	if ((val>=0x1D2C) && (val<=0x1D61)) return L;	//# Lm  [54] MODIFIER LETTER CAPITAL A..MODIFIER LETTER SMALL CHI
	if ((val>=0x1D62) && (val<=0x1D77)) return L;	//# L&  [22] LATIN SUBSCRIPT SMALL LETTER I..LATIN SMALL LETTER TURNED G
	if (val==0x00001D78) return L;	//# Lm       MODIFIER LETTER CYRILLIC EN
	if ((val>=0x1D79) && (val<=0x1D9A)) return L;	//# L&  [34] LATIN SMALL LETTER INSULAR G..LATIN SMALL LETTER EZH WITH RETROFLEX HOOK
	if ((val>=0x1D9B) && (val<=0x1DBF)) return L;	//# Lm  [37] MODIFIER LETTER SMALL TURNED ALPHA..MODIFIER LETTER SMALL THETA
	if ((val>=0x1E00) && (val<=0x1E9B)) return L;	//# L& [156] LATIN CAPITAL LETTER A WITH RING BELOW..LATIN SMALL LETTER LONG S WITH DOT ABOVE
	if ((val>=0x1EA0) && (val<=0x1EF9)) return L;	//# L&  [90] LATIN CAPITAL LETTER A WITH DOT BELOW..LATIN SMALL LETTER Y WITH TILDE
	if ((val>=0x1F00) && (val<=0x1F15)) return L;	//# L&  [22] GREEK SMALL LETTER ALPHA WITH PSILI..GREEK SMALL LETTER EPSILON WITH DASIA AND OXIA
	if ((val>=0x1F18) && (val<=0x1F1D)) return L;	//# L&   [6] GREEK CAPITAL LETTER EPSILON WITH PSILI..GREEK CAPITAL LETTER EPSILON WITH DASIA AND OXIA
	if ((val>=0x1F20) && (val<=0x1F45)) return L;	//# L&  [38] GREEK SMALL LETTER ETA WITH PSILI..GREEK SMALL LETTER OMICRON WITH DASIA AND OXIA
	if ((val>=0x1F48) && (val<=0x1F4D)) return L;	//# L&   [6] GREEK CAPITAL LETTER OMICRON WITH PSILI..GREEK CAPITAL LETTER OMICRON WITH DASIA AND OXIA
	if ((val>=0x1F50) && (val<=0x1F57)) return L;	//# L&   [8] GREEK SMALL LETTER UPSILON WITH PSILI..GREEK SMALL LETTER UPSILON WITH DASIA AND PERISPOMENI
	if (val==0x00001F59) return L;	//# L&       GREEK CAPITAL LETTER UPSILON WITH DASIA
	if (val==0x00001F5B) return L;	//# L&       GREEK CAPITAL LETTER UPSILON WITH DASIA AND VARIA
	if (val==0x00001F5D) return L;	//# L&       GREEK CAPITAL LETTER UPSILON WITH DASIA AND OXIA
	if ((val>=0x1F5F) && (val<=0x1F7D)) return L;	//# L&  [31] GREEK CAPITAL LETTER UPSILON WITH DASIA AND PERISPOMENI..GREEK SMALL LETTER OMEGA WITH OXIA
	if ((val>=0x1F80) && (val<=0x1FB4)) return L;	//# L&  [53] GREEK SMALL LETTER ALPHA WITH PSILI AND YPOGEGRAMMENI..GREEK SMALL LETTER ALPHA WITH OXIA AND YPOGEGRAMMENI
	if ((val>=0x1FB6) && (val<=0x1FBC)) return L;	//# L&   [7] GREEK SMALL LETTER ALPHA WITH PERISPOMENI..GREEK CAPITAL LETTER ALPHA WITH PROSGEGRAMMENI
	if (val==0x00001FBE) return L;	//# L&       GREEK PROSGEGRAMMENI
	if ((val>=0x1FC2) && (val<=0x1FC4)) return L;	//# L&   [3] GREEK SMALL LETTER ETA WITH VARIA AND YPOGEGRAMMENI..GREEK SMALL LETTER ETA WITH OXIA AND YPOGEGRAMMENI
	if ((val>=0x1FC6) && (val<=0x1FCC)) return L;	//# L&   [7] GREEK SMALL LETTER ETA WITH PERISPOMENI..GREEK CAPITAL LETTER ETA WITH PROSGEGRAMMENI
	if ((val>=0x1FD0) && (val<=0x1FD3)) return L;	//# L&   [4] GREEK SMALL LETTER IOTA WITH VRACHY..GREEK SMALL LETTER IOTA WITH DIALYTIKA AND OXIA
	if ((val>=0x1FD6) && (val<=0x1FDB)) return L;	//# L&   [6] GREEK SMALL LETTER IOTA WITH PERISPOMENI..GREEK CAPITAL LETTER IOTA WITH OXIA
	if ((val>=0x1FE0) && (val<=0x1FEC)) return L;	//# L&  [13] GREEK SMALL LETTER UPSILON WITH VRACHY..GREEK CAPITAL LETTER RHO WITH DASIA
	if ((val>=0x1FF2) && (val<=0x1FF4)) return L;	//# L&   [3] GREEK SMALL LETTER OMEGA WITH VARIA AND YPOGEGRAMMENI..GREEK SMALL LETTER OMEGA WITH OXIA AND YPOGEGRAMMENI
	if ((val>=0x1FF6) && (val<=0x1FFC)) return L;	//# L&   [7] GREEK SMALL LETTER OMEGA WITH PERISPOMENI..GREEK CAPITAL LETTER OMEGA WITH PROSGEGRAMMENI
	if (val==0x0000200E) return L;	//# Cf       LEFT-TO-RIGHT MARK
	if (val==0x00002071) return L;	//# L&       SUPERSCRIPT LATIN SMALL LETTER I
	if (val==0x0000207F) return L;	//# L&       SUPERSCRIPT LATIN SMALL LETTER N
	if ((val>=0x2090) && (val<=0x2094)) return L;	//# Lm   [5] LATIN SUBSCRIPT SMALL LETTER A..LATIN SUBSCRIPT SMALL LETTER SCHWA
	if (val==0x00002102) return L;	//# L&       DOUBLE-STRUCK CAPITAL C
	if (val==0x00002107) return L;	//# L&       EULER CONSTANT
	if ((val>=0x210A) && (val<=0x2113)) return L;	//# L&  [10] SCRIPT SMALL G..SCRIPT SMALL L
	if (val==0x00002115) return L;	//# L&       DOUBLE-STRUCK CAPITAL N
	if ((val>=0x2119) && (val<=0x211D)) return L;	//# L&   [5] DOUBLE-STRUCK CAPITAL P..DOUBLE-STRUCK CAPITAL R
	if (val==0x00002124) return L;	//# L&       DOUBLE-STRUCK CAPITAL Z
	if (val==0x00002126) return L;	//# L&       OHM SIGN
	if (val==0x00002128) return L;	//# L&       BLACK-LETTER CAPITAL Z
	if ((val>=0x212A) && (val<=0x212D)) return L;	//# L&   [4] KELVIN SIGN..BLACK-LETTER CAPITAL C
	if ((val>=0x212F) && (val<=0x2134)) return L;	//# L&   [6] SCRIPT SMALL E..SCRIPT SMALL O
	if ((val>=0x2135) && (val<=0x2138)) return L;	//# Lo   [4] ALEF SYMBOL..DALET SYMBOL
	if (val==0x00002139) return L;	//# L&       INFORMATION SOURCE
	if ((val>=0x213C) && (val<=0x213F)) return L;	//# L&   [4] DOUBLE-STRUCK SMALL PI..DOUBLE-STRUCK CAPITAL PI
	if ((val>=0x2145) && (val<=0x2149)) return L;	//# L&   [5] DOUBLE-STRUCK ITALIC CAPITAL D..DOUBLE-STRUCK ITALIC SMALL J
	if (val==0x0000214E) return L;	//# L&       TURNED SMALL F
	if ((val>=0x2160) && (val<=0x2182)) return L;	//# Nl  [35] ROMAN NUMERAL ONE..ROMAN NUMERAL TEN THOUSAND
	if ((val>=0x2183) && (val<=0x2184)) return L;	//# L&   [2] ROMAN NUMERAL REVERSED ONE HUNDRED..LATIN SMALL LETTER REVERSED C
	if ((val>=0x2336) && (val<=0x237A)) return L;	//# So  [69] APL FUNCTIONAL SYMBOL I-BEAM..APL FUNCTIONAL SYMBOL ALPHA
	if (val==0x00002395) return L;	//# So       APL FUNCTIONAL SYMBOL QUAD
	if ((val>=0x249C) && (val<=0x24E9)) return L;	//# So  [78] PARENTHESIZED LATIN SMALL LETTER A..CIRCLED LATIN SMALL LETTER Z
	if (val==0x000026AC) return L;	//# So       MEDIUM SMALL WHITE CIRCLE
	if ((val>=0x2800) && (val<=0x28FF)) return L;	//# So [256] BRAILLE PATTERN BLANK..BRAILLE PATTERN DOTS-12345678
	if ((val>=0x2C00) && (val<=0x2C2E)) return L;	//# L&  [47] GLAGOLITIC CAPITAL LETTER AZU..GLAGOLITIC CAPITAL LETTER LATINATE MYSLITE
	if ((val>=0x2C30) && (val<=0x2C5E)) return L;	//# L&  [47] GLAGOLITIC SMALL LETTER AZU..GLAGOLITIC SMALL LETTER LATINATE MYSLITE
	if ((val>=0x2C60) && (val<=0x2C6C)) return L;	//# L&  [13] LATIN CAPITAL LETTER L WITH DOUBLE BAR..LATIN SMALL LETTER Z WITH DESCENDER
	if ((val>=0x2C74) && (val<=0x2C77)) return L;	//# L&   [4] LATIN SMALL LETTER V WITH CURL..LATIN SMALL LETTER TAILLESS PHI
	if ((val>=0x2C80) && (val<=0x2CE4)) return L;	//# L& [101] COPTIC CAPITAL LETTER ALFA..COPTIC SYMBOL KAI
	if ((val>=0x2D00) && (val<=0x2D25)) return L;	//# L&  [38] GEORGIAN SMALL LETTER AN..GEORGIAN SMALL LETTER HOE
	if ((val>=0x2D30) && (val<=0x2D65)) return L;	//# Lo  [54] TIFINAGH LETTER YA..TIFINAGH LETTER YAZZ
	if (val==0x00002D6F) return L;	//# Lm       TIFINAGH MODIFIER LETTER LABIALIZATION MARK
	if ((val>=0x2D80) && (val<=0x2D96)) return L;	//# Lo  [23] ETHIOPIC SYLLABLE LOA..ETHIOPIC SYLLABLE GGWE
	if ((val>=0x2DA0) && (val<=0x2DA6)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE SSA..ETHIOPIC SYLLABLE SSO
	if ((val>=0x2DA8) && (val<=0x2DAE)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE CCA..ETHIOPIC SYLLABLE CCO
	if ((val>=0x2DB0) && (val<=0x2DB6)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE ZZA..ETHIOPIC SYLLABLE ZZO
	if ((val>=0x2DB8) && (val<=0x2DBE)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE CCHA..ETHIOPIC SYLLABLE CCHO
	if ((val>=0x2DC0) && (val<=0x2DC6)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE QYA..ETHIOPIC SYLLABLE QYO
	if ((val>=0x2DC8) && (val<=0x2DCE)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE KYA..ETHIOPIC SYLLABLE KYO
	if ((val>=0x2DD0) && (val<=0x2DD6)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE XYA..ETHIOPIC SYLLABLE XYO
	if ((val>=0x2DD8) && (val<=0x2DDE)) return L;	//# Lo   [7] ETHIOPIC SYLLABLE GYA..ETHIOPIC SYLLABLE GYO
	if (val==0x00003005) return L;	//# Lm       IDEOGRAPHIC ITERATION MARK
	if (val==0x00003006) return L;	//# Lo       IDEOGRAPHIC CLOSING MARK
	if (val==0x00003007) return L;	//# Nl       IDEOGRAPHIC NUMBER ZERO
	if ((val>=0x3021) && (val<=0x3029)) return L;	//# Nl   [9] HANGZHOU NUMERAL ONE..HANGZHOU NUMERAL NINE
	if ((val>=0x3031) && (val<=0x3035)) return L;	//# Lm   [5] VERTICAL KANA REPEAT MARK..VERTICAL KANA REPEAT MARK LOWER HALF
	if ((val>=0x3038) && (val<=0x303A)) return L;	//# Nl   [3] HANGZHOU NUMERAL TEN..HANGZHOU NUMERAL THIRTY
	if (val==0x0000303B) return L;	//# Lm       VERTICAL IDEOGRAPHIC ITERATION MARK
	if (val==0x0000303C) return L;	//# Lo       MASU MARK
	if ((val>=0x3041) && (val<=0x3096)) return L;	//# Lo  [86] HIRAGANA LETTER SMALL A..HIRAGANA LETTER SMALL KE
	if ((val>=0x309D) && (val<=0x309E)) return L;	//# Lm   [2] HIRAGANA ITERATION MARK..HIRAGANA VOICED ITERATION MARK
	if (val==0x0000309F) return L;	//# Lo       HIRAGANA DIGRAPH YORI
	if ((val>=0x30A1) && (val<=0x30FA)) return L;	//# Lo  [90] KATAKANA LETTER SMALL A..KATAKANA LETTER VO
	if ((val>=0x30FC) && (val<=0x30FE)) return L;	//# Lm   [3] KATAKANA-HIRAGANA PROLONGED SOUND MARK..KATAKANA VOICED ITERATION MARK
	if (val==0x000030FF) return L;	//# Lo       KATAKANA DIGRAPH KOTO
	if ((val>=0x3105) && (val<=0x312C)) return L;	//# Lo  [40] BOPOMOFO LETTER B..BOPOMOFO LETTER GN
	if ((val>=0x3131) && (val<=0x318E)) return L;	//# Lo  [94] HANGUL LETTER KIYEOK..HANGUL LETTER ARAEAE
	if ((val>=0x3190) && (val<=0x3191)) return L;	//# So   [2] IDEOGRAPHIC ANNOTATION LINKING MARK..IDEOGRAPHIC ANNOTATION REVERSE MARK
	if ((val>=0x3192) && (val<=0x3195)) return L;	//# No   [4] IDEOGRAPHIC ANNOTATION ONE MARK..IDEOGRAPHIC ANNOTATION FOUR MARK
	if ((val>=0x3196) && (val<=0x319F)) return L;	//# So  [10] IDEOGRAPHIC ANNOTATION TOP MARK..IDEOGRAPHIC ANNOTATION MAN MARK
	if ((val>=0x31A0) && (val<=0x31B7)) return L;	//# Lo  [24] BOPOMOFO LETTER BU..BOPOMOFO FINAL LETTER H
	if ((val>=0x31F0) && (val<=0x31FF)) return L;	//# Lo  [16] KATAKANA LETTER SMALL KU..KATAKANA LETTER SMALL RO
	if ((val>=0x3200) && (val<=0x321C)) return L;	//# So  [29] PARENTHESIZED HANGUL KIYEOK..PARENTHESIZED HANGUL CIEUC U
	if ((val>=0x3220) && (val<=0x3229)) return L;	//# No  [10] PARENTHESIZED IDEOGRAPH ONE..PARENTHESIZED IDEOGRAPH TEN
	if ((val>=0x322A) && (val<=0x3243)) return L;	//# So  [26] PARENTHESIZED IDEOGRAPH MOON..PARENTHESIZED IDEOGRAPH REACH
	if ((val>=0x3260) && (val<=0x327B)) return L;	//# So  [28] CIRCLED HANGUL KIYEOK..CIRCLED HANGUL HIEUH A
	if (val==0x0000327F) return L;	//# So       KOREAN STANDARD SYMBOL
	if ((val>=0x3280) && (val<=0x3289)) return L;	//# No  [10] CIRCLED IDEOGRAPH ONE..CIRCLED IDEOGRAPH TEN
	if ((val>=0x328A) && (val<=0x32B0)) return L;	//# So  [39] CIRCLED IDEOGRAPH MOON..CIRCLED IDEOGRAPH NIGHT
	if ((val>=0x32C0) && (val<=0x32CB)) return L;	//# So  [12] IDEOGRAPHIC TELEGRAPH SYMBOL FOR JANUARY..IDEOGRAPHIC TELEGRAPH SYMBOL FOR DECEMBER
	if ((val>=0x32D0) && (val<=0x32FE)) return L;	//# So  [47] CIRCLED KATAKANA A..CIRCLED KATAKANA WO
	if ((val>=0x3300) && (val<=0x3376)) return L;	//# So [119] SQUARE APAATO..SQUARE PC
	if ((val>=0x337B) && (val<=0x33DD)) return L;	//# So  [99] SQUARE ERA NAME HEISEI..SQUARE WB
	if ((val>=0x33E0) && (val<=0x33FE)) return L;	//# So  [31] IDEOGRAPHIC TELEGRAPH SYMBOL FOR DAY ONE..IDEOGRAPHIC TELEGRAPH SYMBOL FOR DAY THIRTY-ONE
	if ((val>=0x3400) && (val<=0x4DB5)) return L;	//# Lo [6582] CJK UNIFIED IDEOGRAPH-3400..CJK UNIFIED IDEOGRAPH-4DB5
	if ((val>=0x4E00) && (val<=0x9FBB)) return L;	//# Lo [20924] CJK UNIFIED IDEOGRAPH-4E00..CJK UNIFIED IDEOGRAPH-9FBB
	if ((val>=0xA000) && (val<=0xA014)) return L;	//# Lo  [21] YI SYLLABLE IT..YI SYLLABLE E
	if (val==0x0000A015) return L;	//# Lm       YI SYLLABLE WU
	if ((val>=0xA016) && (val<=0xA48C)) return L;	//# Lo [1143] YI SYLLABLE BIT..YI SYLLABLE YYR
	if ((val>=0xA800) && (val<=0xA801)) return L;	//# Lo   [2] SYLOTI NAGRI LETTER A..SYLOTI NAGRI LETTER I
	if ((val>=0xA803) && (val<=0xA805)) return L;	//# Lo   [3] SYLOTI NAGRI LETTER U..SYLOTI NAGRI LETTER O
	if ((val>=0xA807) && (val<=0xA80A)) return L;	//# Lo   [4] SYLOTI NAGRI LETTER KO..SYLOTI NAGRI LETTER GHO
	if ((val>=0xA80C) && (val<=0xA822)) return L;	//# Lo  [23] SYLOTI NAGRI LETTER CO..SYLOTI NAGRI LETTER HO
	if ((val>=0xA823) && (val<=0xA824)) return L;	//# Mc   [2] SYLOTI NAGRI VOWEL SIGN A..SYLOTI NAGRI VOWEL SIGN I
	if (val==0x0000A827) return L;	//# Mc       SYLOTI NAGRI VOWEL SIGN OO
	if ((val>=0xA840) && (val<=0xA873)) return L;	//# Lo  [52] PHAGS-PA LETTER KA..PHAGS-PA LETTER CANDRABINDU
	if ((val>=0xAC00) && (val<=0xD7A3)) return L;	//# Lo [11172] HANGUL SYLLABLE GA..HANGUL SYLLABLE HIH
	if ((val>=0xE000) && (val<=0xF8FF)) return L;	//# Co [6400] <private-use-E000>..<private-use-F8FF>
	if ((val>=0xF900) && (val<=0xFA2D)) return L;	//# Lo [302] CJK COMPATIBILITY IDEOGRAPH-F900..CJK COMPATIBILITY IDEOGRAPH-FA2D
	if ((val>=0xFA30) && (val<=0xFA6A)) return L;	//# Lo  [59] CJK COMPATIBILITY IDEOGRAPH-FA30..CJK COMPATIBILITY IDEOGRAPH-FA6A
	if ((val>=0xFA70) && (val<=0xFAD9)) return L;	//# Lo [106] CJK COMPATIBILITY IDEOGRAPH-FA70..CJK COMPATIBILITY IDEOGRAPH-FAD9
	if ((val>=0xFB00) && (val<=0xFB06)) return L;	//# L&   [7] LATIN SMALL LIGATURE FF..LATIN SMALL LIGATURE ST
	if ((val>=0xFB13) && (val<=0xFB17)) return L;	//# L&   [5] ARMENIAN SMALL LIGATURE MEN NOW..ARMENIAN SMALL LIGATURE MEN XEH
	if ((val>=0xFF21) && (val<=0xFF3A)) return L;	//# L&  [26] FULLWIDTH LATIN CAPITAL LETTER A..FULLWIDTH LATIN CAPITAL LETTER Z
	if ((val>=0xFF41) && (val<=0xFF5A)) return L;	//# L&  [26] FULLWIDTH LATIN SMALL LETTER A..FULLWIDTH LATIN SMALL LETTER Z
	if ((val>=0xFF66) && (val<=0xFF6F)) return L;	//# Lo  [10] HALFWIDTH KATAKANA LETTER WO..HALFWIDTH KATAKANA LETTER SMALL TU
	if (val==0x0000FF70) return L;	//# Lm       HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK
	if ((val>=0xFF71) && (val<=0xFF9D)) return L;	//# Lo  [45] HALFWIDTH KATAKANA LETTER A..HALFWIDTH KATAKANA LETTER N
	if ((val>=0xFF9E) && (val<=0xFF9F)) return L;	//# Lm   [2] HALFWIDTH KATAKANA VOICED SOUND MARK..HALFWIDTH KATAKANA SEMI-VOICED SOUND MARK
	if ((val>=0xFFA0) && (val<=0xFFBE)) return L;	//# Lo  [31] HALFWIDTH HANGUL FILLER..HALFWIDTH HANGUL LETTER HIEUH
	if ((val>=0xFFC2) && (val<=0xFFC7)) return L;	//# Lo   [6] HALFWIDTH HANGUL LETTER A..HALFWIDTH HANGUL LETTER E
	if ((val>=0xFFCA) && (val<=0xFFCF)) return L;	//# Lo   [6] HALFWIDTH HANGUL LETTER YEO..HALFWIDTH HANGUL LETTER OE
	if ((val>=0xFFD2) && (val<=0xFFD7)) return L;	//# Lo   [6] HALFWIDTH HANGUL LETTER YO..HALFWIDTH HANGUL LETTER YU
	if ((val>=0xFFDA) && (val<=0xFFDC)) return L;	//# Lo   [3] HALFWIDTH HANGUL LETTER EU..HALFWIDTH HANGUL LETTER I
	if ((val>=0x10000) && (val<=0x1000B)) return L;	//# Lo  [12] LINEAR B SYLLABLE B008 A..LINEAR B SYLLABLE B046 JE
	if ((val>=0x1000D) && (val<=0x10026)) return L;	//# Lo  [26] LINEAR B SYLLABLE B036 JO..LINEAR B SYLLABLE B032 QO
	if ((val>=0x10028) && (val<=0x1003A)) return L;	//# Lo  [19] LINEAR B SYLLABLE B060 RA..LINEAR B SYLLABLE B042 WO
	if ((val>=0x1003C) && (val<=0x1003D)) return L;	//# Lo   [2] LINEAR B SYLLABLE B017 ZA..LINEAR B SYLLABLE B074 ZE
	if ((val>=0x1003F) && (val<=0x1004D)) return L;	//# Lo  [15] LINEAR B SYLLABLE B020 ZO..LINEAR B SYLLABLE B091 TWO
	if ((val>=0x10050) && (val<=0x1005D)) return L;	//# Lo  [14] LINEAR B SYMBOL B018..LINEAR B SYMBOL B089
	if ((val>=0x10080) && (val<=0x100FA)) return L;	//# Lo [123] LINEAR B IDEOGRAM B100 MAN..LINEAR B IDEOGRAM VESSEL B305
	if (val==0x00010100) return L;	//# Po       AEGEAN WORD SEPARATOR LINE
	if (val==0x00010102) return L;	//# So       AEGEAN CHECK MARK
	if ((val>=0x10107) && (val<=0x10133)) return L;	//# No  [45] AEGEAN NUMBER ONE..AEGEAN NUMBER NINETY THOUSAND
	if ((val>=0x10137) && (val<=0x1013F)) return L;	//# So   [9] AEGEAN WEIGHT BASE UNIT..AEGEAN MEASURE THIRD SUBUNIT
	if ((val>=0x10300) && (val<=0x1031E)) return L;	//# Lo  [31] OLD ITALIC LETTER A..OLD ITALIC LETTER UU
	if ((val>=0x10320) && (val<=0x10323)) return L;	//# No   [4] OLD ITALIC NUMERAL ONE..OLD ITALIC NUMERAL FIFTY
	if ((val>=0x10330) && (val<=0x10340)) return L;	//# Lo  [17] GOTHIC LETTER AHSA..GOTHIC LETTER PAIRTHRA
	if (val==0x00010341) return L;	//# Nl       GOTHIC LETTER NINETY
	if ((val>=0x10342) && (val<=0x10349)) return L;	//# Lo   [8] GOTHIC LETTER RAIDA..GOTHIC LETTER OTHAL
	if (val==0x0001034A) return L;	//# Nl       GOTHIC LETTER NINE HUNDRED
	if ((val>=0x10380) && (val<=0x1039D)) return L;	//# Lo  [30] UGARITIC LETTER ALPA..UGARITIC LETTER SSU
	if (val==0x0001039F) return L;	//# Po       UGARITIC WORD DIVIDER
	if ((val>=0x103A0) && (val<=0x103C3)) return L;	//# Lo  [36] OLD PERSIAN SIGN A..OLD PERSIAN SIGN HA
	if ((val>=0x103C8) && (val<=0x103CF)) return L;	//# Lo   [8] OLD PERSIAN SIGN AURAMAZDAA..OLD PERSIAN SIGN BUUMISH
	if (val==0x000103D0) return L;	//# Po       OLD PERSIAN WORD DIVIDER
	if ((val>=0x103D1) && (val<=0x103D5)) return L;	//# Nl   [5] OLD PERSIAN NUMBER ONE..OLD PERSIAN NUMBER HUNDRED
	if ((val>=0x10400) && (val<=0x1044F)) return L;	//# L&  [80] DESERET CAPITAL LETTER LONG I..DESERET SMALL LETTER EW
	if ((val>=0x10450) && (val<=0x1049D)) return L;	//# Lo  [78] SHAVIAN LETTER PEEP..OSMANYA LETTER OO
	if ((val>=0x104A0) && (val<=0x104A9)) return L;	//# Nd  [10] OSMANYA DIGIT ZERO..OSMANYA DIGIT NINE
	if ((val>=0x12000) && (val<=0x1236E)) return L;	//# Lo [879] CUNEIFORM SIGN A..CUNEIFORM SIGN ZUM
	if ((val>=0x12400) && (val<=0x12462)) return L;	//# Nl  [99] CUNEIFORM NUMERIC SIGN TWO ASH..CUNEIFORM NUMERIC SIGN OLD ASSYRIAN ONE QUARTER
	if ((val>=0x12470) && (val<=0x12473)) return L;	//# Po   [4] CUNEIFORM PUNCTUATION SIGN OLD ASSYRIAN WORD DIVIDER..CUNEIFORM PUNCTUATION SIGN DIAGONAL TRICOLON
	if ((val>=0x1D000) && (val<=0x1D0F5)) return L;	//# So [246] BYZANTINE MUSICAL SYMBOL PSILI..BYZANTINE MUSICAL SYMBOL GORGON NEO KATO
	if ((val>=0x1D100) && (val<=0x1D126)) return L;	//# So  [39] MUSICAL SYMBOL SINGLE BARLINE..MUSICAL SYMBOL DRUM CLEF-2
	if ((val>=0x1D12A) && (val<=0x1D164)) return L;	//# So  [59] MUSICAL SYMBOL DOUBLE SHARP..MUSICAL SYMBOL ONE HUNDRED TWENTY-EIGHTH NOTE
	if ((val>=0x1D165) && (val<=0x1D166)) return L;	//# Mc   [2] MUSICAL SYMBOL COMBINING STEM..MUSICAL SYMBOL COMBINING SPRECHGESANG STEM
	if ((val>=0x1D16A) && (val<=0x1D16C)) return L;	//# So   [3] MUSICAL SYMBOL FINGERED TREMOLO-1..MUSICAL SYMBOL FINGERED TREMOLO-3
	if ((val>=0x1D16D) && (val<=0x1D172)) return L;	//# Mc   [6] MUSICAL SYMBOL COMBINING AUGMENTATION DOT..MUSICAL SYMBOL COMBINING FLAG-5
	if ((val>=0x1D183) && (val<=0x1D184)) return L;	//# So   [2] MUSICAL SYMBOL ARPEGGIATO UP..MUSICAL SYMBOL ARPEGGIATO DOWN
	if ((val>=0x1D18C) && (val<=0x1D1A9)) return L;	//# So  [30] MUSICAL SYMBOL RINFORZANDO..MUSICAL SYMBOL DEGREE SLASH
	if ((val>=0x1D1AE) && (val<=0x1D1DD)) return L;	//# So  [48] MUSICAL SYMBOL PEDAL MARK..MUSICAL SYMBOL PES SUBPUNCTIS
	if ((val>=0x1D360) && (val<=0x1D371)) return L;	//# No  [18] COUNTING ROD UNIT DIGIT ONE..COUNTING ROD TENS DIGIT NINE
	if ((val>=0x1D400) && (val<=0x1D454)) return L;	//# L&  [85] MATHEMATICAL BOLD CAPITAL A..MATHEMATICAL ITALIC SMALL G
	if ((val>=0x1D456) && (val<=0x1D49C)) return L;	//# L&  [71] MATHEMATICAL ITALIC SMALL I..MATHEMATICAL SCRIPT CAPITAL A
	if ((val>=0x1D49E) && (val<=0x1D49F)) return L;	//# L&   [2] MATHEMATICAL SCRIPT CAPITAL C..MATHEMATICAL SCRIPT CAPITAL D
	if (val==0x0001D4A2) return L;	//# L&       MATHEMATICAL SCRIPT CAPITAL G
	if ((val>=0x1D4A5) && (val<=0x1D4A6)) return L;	//# L&   [2] MATHEMATICAL SCRIPT CAPITAL J..MATHEMATICAL SCRIPT CAPITAL K
	if ((val>=0x1D4A9) && (val<=0x1D4AC)) return L;	//# L&   [4] MATHEMATICAL SCRIPT CAPITAL N..MATHEMATICAL SCRIPT CAPITAL Q
	if ((val>=0x1D4AE) && (val<=0x1D4B9)) return L;	//# L&  [12] MATHEMATICAL SCRIPT CAPITAL S..MATHEMATICAL SCRIPT SMALL D
	if (val==0x0001D4BB) return L;	//# L&       MATHEMATICAL SCRIPT SMALL F
	if ((val>=0x1D4BD) && (val<=0x1D4C3)) return L;	//# L&   [7] MATHEMATICAL SCRIPT SMALL H..MATHEMATICAL SCRIPT SMALL N
	if ((val>=0x1D4C5) && (val<=0x1D505)) return L;	//# L&  [65] MATHEMATICAL SCRIPT SMALL P..MATHEMATICAL FRAKTUR CAPITAL B
	if ((val>=0x1D507) && (val<=0x1D50A)) return L;	//# L&   [4] MATHEMATICAL FRAKTUR CAPITAL D..MATHEMATICAL FRAKTUR CAPITAL G
	if ((val>=0x1D50D) && (val<=0x1D514)) return L;	//# L&   [8] MATHEMATICAL FRAKTUR CAPITAL J..MATHEMATICAL FRAKTUR CAPITAL Q
	if ((val>=0x1D516) && (val<=0x1D51C)) return L;	//# L&   [7] MATHEMATICAL FRAKTUR CAPITAL S..MATHEMATICAL FRAKTUR CAPITAL Y
	if ((val>=0x1D51E) && (val<=0x1D539)) return L;	//# L&  [28] MATHEMATICAL FRAKTUR SMALL A..MATHEMATICAL DOUBLE-STRUCK CAPITAL B
	if ((val>=0x1D53B) && (val<=0x1D53E)) return L;	//# L&   [4] MATHEMATICAL DOUBLE-STRUCK CAPITAL D..MATHEMATICAL DOUBLE-STRUCK CAPITAL G
	if ((val>=0x1D540) && (val<=0x1D544)) return L;	//# L&   [5] MATHEMATICAL DOUBLE-STRUCK CAPITAL I..MATHEMATICAL DOUBLE-STRUCK CAPITAL M
	if (val==0x0001D546) return L;	//# L&       MATHEMATICAL DOUBLE-STRUCK CAPITAL O
	if ((val>=0x1D54A) && (val<=0x1D550)) return L;	//# L&   [7] MATHEMATICAL DOUBLE-STRUCK CAPITAL S..MATHEMATICAL DOUBLE-STRUCK CAPITAL Y
	if ((val>=0x1D552) && (val<=0x1D6A5)) return L;	//# L& [340] MATHEMATICAL DOUBLE-STRUCK SMALL A..MATHEMATICAL ITALIC SMALL DOTLESS J
	if ((val>=0x1D6A8) && (val<=0x1D6C0)) return L;	//# L&  [25] MATHEMATICAL BOLD CAPITAL ALPHA..MATHEMATICAL BOLD CAPITAL OMEGA
	if (val==0x0001D6C1) return L;	//# Sm       MATHEMATICAL BOLD NABLA
	if ((val>=0x1D6C2) && (val<=0x1D6DA)) return L;	//# L&  [25] MATHEMATICAL BOLD SMALL ALPHA..MATHEMATICAL BOLD SMALL OMEGA
	if (val==0x0001D6DB) return L;	//# Sm       MATHEMATICAL BOLD PARTIAL DIFFERENTIAL
	if ((val>=0x1D6DC) && (val<=0x1D6FA)) return L;	//# L&  [31] MATHEMATICAL BOLD EPSILON SYMBOL..MATHEMATICAL ITALIC CAPITAL OMEGA
	if (val==0x0001D6FB) return L;	//# Sm       MATHEMATICAL ITALIC NABLA
	if ((val>=0x1D6FC) && (val<=0x1D714)) return L;	//# L&  [25] MATHEMATICAL ITALIC SMALL ALPHA..MATHEMATICAL ITALIC SMALL OMEGA
	if (val==0x0001D715) return L;	//# Sm       MATHEMATICAL ITALIC PARTIAL DIFFERENTIAL
	if ((val>=0x1D716) && (val<=0x1D734)) return L;	//# L&  [31] MATHEMATICAL ITALIC EPSILON SYMBOL..MATHEMATICAL BOLD ITALIC CAPITAL OMEGA
	if (val==0x0001D735) return L;	//# Sm       MATHEMATICAL BOLD ITALIC NABLA
	if ((val>=0x1D736) && (val<=0x1D74E)) return L;	//# L&  [25] MATHEMATICAL BOLD ITALIC SMALL ALPHA..MATHEMATICAL BOLD ITALIC SMALL OMEGA
	if (val==0x0001D74F) return L;	//# Sm       MATHEMATICAL BOLD ITALIC PARTIAL DIFFERENTIAL
	if ((val>=0x1D750) && (val<=0x1D76E)) return L;	//# L&  [31] MATHEMATICAL BOLD ITALIC EPSILON SYMBOL..MATHEMATICAL SANS-SERIF BOLD CAPITAL OMEGA
	if (val==0x0001D76F) return L;	//# Sm       MATHEMATICAL SANS-SERIF BOLD NABLA
	if ((val>=0x1D770) && (val<=0x1D788)) return L;	//# L&  [25] MATHEMATICAL SANS-SERIF BOLD SMALL ALPHA..MATHEMATICAL SANS-SERIF BOLD SMALL OMEGA
	if (val==0x0001D789) return L;	//# Sm       MATHEMATICAL SANS-SERIF BOLD PARTIAL DIFFERENTIAL
	if ((val>=0x1D78A) && (val<=0x1D7A8)) return L;	//# L&  [31] MATHEMATICAL SANS-SERIF BOLD EPSILON SYMBOL..MATHEMATICAL SANS-SERIF BOLD ITALIC CAPITAL OMEGA
	if (val==0x0001D7A9) return L;	//# Sm       MATHEMATICAL SANS-SERIF BOLD ITALIC NABLA
	if ((val>=0x1D7AA) && (val<=0x1D7C2)) return L;	//# L&  [25] MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL ALPHA..MATHEMATICAL SANS-SERIF BOLD ITALIC SMALL OMEGA
	if (val==0x0001D7C3) return L;	//# Sm       MATHEMATICAL SANS-SERIF BOLD ITALIC PARTIAL DIFFERENTIAL
	if ((val>=0x1D7C4) && (val<=0x1D7CB)) return L;	//# L&   [8] MATHEMATICAL SANS-SERIF BOLD ITALIC EPSILON SYMBOL..MATHEMATICAL BOLD SMALL DIGAMMA
	if ((val>=0x20000) && (val<=0x2A6D6)) return L;	//# Lo [42711] CJK UNIFIED IDEOGRAPH-20000..CJK UNIFIED IDEOGRAPH-2A6D6
	if ((val>=0x2F800) && (val<=0x2FA1D)) return L;	//# Lo [542] CJK COMPATIBILITY IDEOGRAPH-2F800..CJK COMPATIBILITY IDEOGRAPH-2FA1D
	if ((val>=0xF0000) && (val<=0xFFFFD)) return L;	//# Co [65534] <private-use-F0000>..<private-use-FFFFD>
	if ((val>=0x100000) && (val<=0x10FFFD)) return L;	//# Co [65534] <private-use-100000>..<private-use-10FFFD>
	if (val==0x00000590) return R;	//# Cn       <reserved-0590>
	if (val==0x000005BE) return R;	//# Po       HEBREW PUNCTUATION MAQAF
	if (val==0x000005C0) return R;	//# Po       HEBREW PUNCTUATION PASEQ
	if (val==0x000005C3) return R;	//# Po       HEBREW PUNCTUATION SOF PASUQ
	if (val==0x000005C6) return R;	//# Po       HEBREW PUNCTUATION NUN HAFUKHA
	if ((val>=0x05C8) && (val<=0x05CF)) return R;	//# Cn   [8] <reserved-05C8>..<reserved-05CF>
	if ((val>=0x05D0) && (val<=0x05EA)) return R;	//# Lo  [27] HEBREW LETTER ALEF..HEBREW LETTER TAV
	if ((val>=0x05EB) && (val<=0x05EF)) return R;	//# Cn   [5] <reserved-05EB>..<reserved-05EF>
	if ((val>=0x05F0) && (val<=0x05F2)) return R;	//# Lo   [3] HEBREW LIGATURE YIDDISH DOUBLE VAV..HEBREW LIGATURE YIDDISH DOUBLE YOD
	if ((val>=0x05F3) && (val<=0x05F4)) return R;	//# Po   [2] HEBREW PUNCTUATION GERESH..HEBREW PUNCTUATION GERSHAYIM
	if ((val>=0x05F5) && (val<=0x05FF)) return R;	//# Cn  [11] <reserved-05F5>..<reserved-05FF>
	if ((val>=0x07C0) && (val<=0x07C9)) return R;	//# Nd  [10] NKO DIGIT ZERO..NKO DIGIT NINE
	if ((val>=0x07CA) && (val<=0x07EA)) return R;	//# Lo  [33] NKO LETTER A..NKO LETTER JONA RA
	if ((val>=0x07F4) && (val<=0x07F5)) return R;	//# Lm   [2] NKO HIGH TONE APOSTROPHE..NKO LOW TONE APOSTROPHE
	if (val==0x000007FA) return R;	//# Lm       NKO LAJANYALAN
	if ((val>=0x07FB) && (val<=0x08FF)) return R;	//# Cn [261] <reserved-07FB>..<reserved-08FF>
	if (val==0x0000200F) return R;	//# Cf       RIGHT-TO-LEFT MARK
	if (val==0x0000FB1D) return R;	//# Lo       HEBREW LETTER YOD WITH HIRIQ
	if ((val>=0xFB1F) && (val<=0xFB28)) return R;	//# Lo  [10] HEBREW LIGATURE YIDDISH YOD YOD PATAH..HEBREW LETTER WIDE TAV
	if ((val>=0xFB2A) && (val<=0xFB36)) return R;	//# Lo  [13] HEBREW LETTER SHIN WITH SHIN DOT..HEBREW LETTER ZAYIN WITH DAGESH
	if (val==0x0000FB37) return R;	//# Cn       <reserved-FB37>
	if ((val>=0xFB38) && (val<=0xFB3C)) return R;	//# Lo   [5] HEBREW LETTER TET WITH DAGESH..HEBREW LETTER LAMED WITH DAGESH
	if (val==0x0000FB3D) return R;	//# Cn       <reserved-FB3D>
	if (val==0x0000FB3E) return R;	//# Lo       HEBREW LETTER MEM WITH DAGESH
	if (val==0x0000FB3F) return R;	//# Cn       <reserved-FB3F>
	if ((val>=0xFB40) && (val<=0xFB41)) return R;	//# Lo   [2] HEBREW LETTER NUN WITH DAGESH..HEBREW LETTER SAMEKH WITH DAGESH
	if (val==0x0000FB42) return R;	//# Cn       <reserved-FB42>
	if ((val>=0xFB43) && (val<=0xFB44)) return R;	//# Lo   [2] HEBREW LETTER FINAL PE WITH DAGESH..HEBREW LETTER PE WITH DAGESH
	if (val==0x0000FB45) return R;	//# Cn       <reserved-FB45>
	if ((val>=0xFB46) && (val<=0xFB4F)) return R;	//# Lo  [10] HEBREW LETTER TSADI WITH DAGESH..HEBREW LIGATURE ALEF LAMED
	if ((val>=0x10800) && (val<=0x10805)) return R;	//# Lo   [6] CYPRIOT SYLLABLE A..CYPRIOT SYLLABLE JA
	if ((val>=0x10806) && (val<=0x10807)) return R;	//# Cn   [2] <reserved-10806>..<reserved-10807>
	if (val==0x00010808) return R;	//# Lo       CYPRIOT SYLLABLE JO
	if (val==0x00010809) return R;	//# Cn       <reserved-10809>
	if ((val>=0x1080A) && (val<=0x10835)) return R;	//# Lo  [44] CYPRIOT SYLLABLE KA..CYPRIOT SYLLABLE WO
	if (val==0x00010836) return R;	//# Cn       <reserved-10836>
	if ((val>=0x10837) && (val<=0x10838)) return R;	//# Lo   [2] CYPRIOT SYLLABLE XA..CYPRIOT SYLLABLE XE
	if ((val>=0x10839) && (val<=0x1083B)) return R;	//# Cn   [3] <reserved-10839>..<reserved-1083B>
	if (val==0x0001083C) return R;	//# Lo       CYPRIOT SYLLABLE ZA
	if ((val>=0x1083D) && (val<=0x1083E)) return R;	//# Cn   [2] <reserved-1083D>..<reserved-1083E>
	if (val==0x0001083F) return R;	//# Lo       CYPRIOT SYLLABLE ZO
	if ((val>=0x10840) && (val<=0x108FF)) return R;	//# Cn [192] <reserved-10840>..<reserved-108FF>
	if ((val>=0x10900) && (val<=0x10915)) return R;	//# Lo  [22] PHOENICIAN LETTER ALF..PHOENICIAN LETTER TAU
	if ((val>=0x10916) && (val<=0x10919)) return R;	//# No   [4] PHOENICIAN NUMBER ONE..PHOENICIAN NUMBER ONE HUNDRED
	if ((val>=0x1091A) && (val<=0x1091E)) return R;	//# Cn   [5] <reserved-1091A>..<reserved-1091E>
	if ((val>=0x10920) && (val<=0x109FF)) return R;	//# Cn [224] <reserved-10920>..<reserved-109FF>
	if (val==0x00010A00) return R;	//# Lo       KHAROSHTHI LETTER A
	if (val==0x00010A04) return R;	//# Cn       <reserved-10A04>
	if ((val>=0x10A07) && (val<=0x10A0B)) return R;	//# Cn   [5] <reserved-10A07>..<reserved-10A0B>
	if ((val>=0x10A10) && (val<=0x10A13)) return R;	//# Lo   [4] KHAROSHTHI LETTER KA..KHAROSHTHI LETTER GHA
	if (val==0x00010A14) return R;	//# Cn       <reserved-10A14>
	if ((val>=0x10A15) && (val<=0x10A17)) return R;	//# Lo   [3] KHAROSHTHI LETTER CA..KHAROSHTHI LETTER JA
	if (val==0x00010A18) return R;	//# Cn       <reserved-10A18>
	if ((val>=0x10A19) && (val<=0x10A33)) return R;	//# Lo  [27] KHAROSHTHI LETTER NYA..KHAROSHTHI LETTER TTTHA
	if ((val>=0x10A34) && (val<=0x10A37)) return R;	//# Cn   [4] <reserved-10A34>..<reserved-10A37>
	if ((val>=0x10A3B) && (val<=0x10A3E)) return R;	//# Cn   [4] <reserved-10A3B>..<reserved-10A3E>
	if ((val>=0x10A40) && (val<=0x10A47)) return R;	//# No   [8] KHAROSHTHI DIGIT ONE..KHAROSHTHI NUMBER ONE THOUSAND
	if ((val>=0x10A48) && (val<=0x10A4F)) return R;	//# Cn   [8] <reserved-10A48>..<reserved-10A4F>
	if ((val>=0x10A50) && (val<=0x10A58)) return R;	//# Po   [9] KHAROSHTHI PUNCTUATION DOT..KHAROSHTHI PUNCTUATION LINES
	if ((val>=0x10A59) && (val<=0x10FFF)) return R;	//# Cn [1447] <reserved-10A59>..<reserved-10FFF>
	if ((val>=0x0030) && (val<=0x0039)) return EN;	//# Nd  [10] DIGIT ZERO..DIGIT NINE
	if ((val>=0x00B2) && (val<=0x00B3)) return EN;	//# No   [2] SUPERSCRIPT TWO..SUPERSCRIPT THREE
	if (val==0x000000B9) return EN;	//# No       SUPERSCRIPT ONE
	if ((val>=0x06F0) && (val<=0x06F9)) return EN;	//# Nd  [10] EXTENDED ARABIC-INDIC DIGIT ZERO..EXTENDED ARABIC-INDIC DIGIT NINE
	if (val==0x00002070) return EN;	//# No       SUPERSCRIPT ZERO
	if ((val>=0x2074) && (val<=0x2079)) return EN;	//# No   [6] SUPERSCRIPT FOUR..SUPERSCRIPT NINE
	if ((val>=0x2080) && (val<=0x2089)) return EN;	//# No  [10] SUBSCRIPT ZERO..SUBSCRIPT NINE
	if ((val>=0x2488) && (val<=0x249B)) return EN;	//# No  [20] DIGIT ONE FULL STOP..NUMBER TWENTY FULL STOP
	if ((val>=0xFF10) && (val<=0xFF19)) return EN;	//# Nd  [10] FULLWIDTH DIGIT ZERO..FULLWIDTH DIGIT NINE
	if ((val>=0x1D7CE) && (val<=0x1D7FF)) return EN;	//# Nd  [50] MATHEMATICAL BOLD DIGIT ZERO..MATHEMATICAL MONOSPACE DIGIT NINE
	if (val==0x0000002B) return ES;	//# Sm       PLUS SIGN
	if (val==0x0000002D) return ES;	//# Pd       HYPHEN-MINUS
	if ((val>=0x207A) && (val<=0x207B)) return ES;	//# Sm   [2] SUPERSCRIPT PLUS SIGN..SUPERSCRIPT MINUS
	if ((val>=0x208A) && (val<=0x208B)) return ES;	//# Sm   [2] SUBSCRIPT PLUS SIGN..SUBSCRIPT MINUS
	if (val==0x00002212) return ES;	//# Sm       MINUS SIGN
	if (val==0x0000FB29) return ES;	//# Sm       HEBREW LETTER ALTERNATIVE PLUS SIGN
	if (val==0x0000FE62) return ES;	//# Sm       SMALL PLUS SIGN
	if (val==0x0000FE63) return ES;	//# Pd       SMALL HYPHEN-MINUS
	if (val==0x0000FF0B) return ES;	//# Sm       FULLWIDTH PLUS SIGN
	if (val==0x0000FF0D) return ES;	//# Pd       FULLWIDTH HYPHEN-MINUS
	if (val==0x00000023) return ET;	//# Po       NUMBER SIGN
	if (val==0x00000024) return ET;	//# Sc       DOLLAR SIGN
	if (val==0x00000025) return ET;	//# Po       PERCENT SIGN
	if ((val>=0x00A2) && (val<=0x00A5)) return ET;	//# Sc   [4] CENT SIGN..YEN SIGN
	if (val==0x000000B0) return ET;	//# So       DEGREE SIGN
	if (val==0x000000B1) return ET;	//# Sm       PLUS-MINUS SIGN
	if (val==0x0000066A) return ET;	//# Po       ARABIC PERCENT SIGN
	if ((val>=0x09F2) && (val<=0x09F3)) return ET;	//# Sc   [2] BENGALI RUPEE MARK..BENGALI RUPEE SIGN
	if (val==0x00000AF1) return ET;	//# Sc       GUJARATI RUPEE SIGN
	if (val==0x00000BF9) return ET;	//# Sc       TAMIL RUPEE SIGN
	if (val==0x00000E3F) return ET;	//# Sc       THAI CURRENCY SYMBOL BAHT
	if (val==0x000017DB) return ET;	//# Sc       KHMER CURRENCY SYMBOL RIEL
	if ((val>=0x2030) && (val<=0x2034)) return ET;	//# Po   [5] PER MILLE SIGN..TRIPLE PRIME
	if ((val>=0x20A0) && (val<=0x20B5)) return ET;	//# Sc  [22] EURO-CURRENCY SIGN..CEDI SIGN
	if (val==0x0000212E) return ET;	//# So       ESTIMATED SYMBOL
	if (val==0x00002213) return ET;	//# Sm       MINUS-OR-PLUS SIGN
	if (val==0x0000FE5F) return ET;	//# Po       SMALL NUMBER SIGN
	if (val==0x0000FE69) return ET;	//# Sc       SMALL DOLLAR SIGN
	if (val==0x0000FE6A) return ET;	//# Po       SMALL PERCENT SIGN
	if (val==0x0000FF03) return ET;	//# Po       FULLWIDTH NUMBER SIGN
	if (val==0x0000FF04) return ET;	//# Sc       FULLWIDTH DOLLAR SIGN
	if (val==0x0000FF05) return ET;	//# Po       FULLWIDTH PERCENT SIGN
	if ((val>=0xFFE0) && (val<=0xFFE1)) return ET;	//# Sc   [2] FULLWIDTH CENT SIGN..FULLWIDTH POUND SIGN
	if ((val>=0xFFE5) && (val<=0xFFE6)) return ET;	//# Sc   [2] FULLWIDTH YEN SIGN..FULLWIDTH WON SIGN
	if ((val>=0x0660) && (val<=0x0669)) return AN;	//# Nd  [10] ARABIC-INDIC DIGIT ZERO..ARABIC-INDIC DIGIT NINE
	if ((val>=0x066B) && (val<=0x066C)) return AN;	//# Po   [2] ARABIC DECIMAL SEPARATOR..ARABIC THOUSANDS SEPARATOR
	if (val==0x0000002C) return CS;	//# Po       COMMA
	if ((val>=0x002E) && (val<=0x002F)) return CS;	//# Po   [2] FULL STOP..SOLIDUS
	if (val==0x0000003A) return CS;	//# Po       COLON
	if (val==0x000000A0) return CS;	//# Zs       NO-BREAK SPACE
	if (val==0x0000060C) return CS;	//# Po       ARABIC COMMA
	if (val==0x0000202F) return CS;	//# Zs       NARROW NO-BREAK SPACE
	if (val==0x00002044) return CS;	//# Sm       FRACTION SLASH
	if (val==0x0000FE50) return CS;	//# Po       SMALL COMMA
	if (val==0x0000FE52) return CS;	//# Po       SMALL FULL STOP
	if (val==0x0000FE55) return CS;	//# Po       SMALL COLON
	if (val==0x0000FF0C) return CS;	//# Po       FULLWIDTH COMMA
	if ((val>=0xFF0E) && (val<=0xFF0F)) return CS;	//# Po   [2] FULLWIDTH FULL STOP..FULLWIDTH SOLIDUS
	if (val==0x0000FF1A) return CS;	//# Po       FULLWIDTH COLON
	if (val==0x0000000A) return B;	//# Cc       <control-000A>
	if (val==0x0000000D) return B;	//# Cc       <control-000D>
	if ((val>=0x001C) && (val<=0x001E)) return B;	//# Cc   [3] <control-001C>..<control-001E>
	if (val==0x00000085) return B;	//# Cc       <control-0085>
	if (val==0x00002029) return B;	//# Zp       PARAGRAPH SEPARATOR
	if (val==0x00000009) return S;	//# Cc       <control-0009>
	if (val==0x0000000B) return S;	//# Cc       <control-000B>
	if (val==0x0000001F) return S;	//# Cc       <control-001F>
	if (val==0x0000000C) return WS;	//# Cc       <control-000C>
	if (val==0x00000020) return WS;	//# Zs       SPACE
	if (val==0x00001680) return WS;	//# Zs       OGHAM SPACE MARK
	if (val==0x0000180E) return WS;	//# Zs       MONGOLIAN VOWEL SEPARATOR
	if ((val>=0x2000) && (val<=0x200A)) return WS;	//# Zs  [11] EN QUAD..HAIR SPACE
	if (val==0x00002028) return WS;	//# Zl       LINE SEPARATOR
	if (val==0x0000205F) return WS;	//# Zs       MEDIUM MATHEMATICAL SPACE
	if (val==0x00003000) return WS;	//# Zs       IDEOGRAPHIC SPACE
	if ((val>=0x0021) && (val<=0x0022)) return ON;	//# Po   [2] EXCLAMATION MARK..QUOTATION MARK
	if ((val>=0x0026) && (val<=0x0027)) return ON;	//# Po   [2] AMPERSAND..APOSTROPHE
	if (val==0x00000028) return ON;	//# Ps       LEFT PARENTHESIS
	if (val==0x00000029) return ON;	//# Pe       RIGHT PARENTHESIS
	if (val==0x0000002A) return ON;	//# Po       ASTERISK
	if (val==0x0000003B) return ON;	//# Po       SEMICOLON
	if ((val>=0x003C) && (val<=0x003E)) return ON;	//# Sm   [3] LESS-THAN SIGN..GREATER-THAN SIGN
	if ((val>=0x003F) && (val<=0x0040)) return ON;	//# Po   [2] QUESTION MARK..COMMERCIAL AT
	if (val==0x0000005B) return ON;	//# Ps       LEFT SQUARE BRACKET
	if (val==0x0000005C) return ON;	//# Po       REVERSE SOLIDUS
	if (val==0x0000005D) return ON;	//# Pe       RIGHT SQUARE BRACKET
	if (val==0x0000005E) return ON;	//# Sk       CIRCUMFLEX ACCENT
	if (val==0x0000005F) return ON;	//# Pc       LOW LINE
	if (val==0x00000060) return ON;	//# Sk       GRAVE ACCENT
	if (val==0x0000007B) return ON;	//# Ps       LEFT CURLY BRACKET
	if (val==0x0000007C) return ON;	//# Sm       VERTICAL LINE
	if (val==0x0000007D) return ON;	//# Pe       RIGHT CURLY BRACKET
	if (val==0x0000007E) return ON;	//# Sm       TILDE
	if (val==0x000000A1) return ON;	//# Po       INVERTED EXCLAMATION MARK
	if ((val>=0x00A6) && (val<=0x00A7)) return ON;	//# So   [2] BROKEN BAR..SECTION SIGN
	if (val==0x000000A8) return ON;	//# Sk       DIAERESIS
	if (val==0x000000A9) return ON;	//# So       COPYRIGHT SIGN
	if (val==0x000000AB) return ON;	//# Pi       LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	if (val==0x000000AC) return ON;	//# Sm       NOT SIGN
	if (val==0x000000AE) return ON;	//# So       REGISTERED SIGN
	if (val==0x000000AF) return ON;	//# Sk       MACRON
	if (val==0x000000B4) return ON;	//# Sk       ACUTE ACCENT
	if (val==0x000000B6) return ON;	//# So       PILCROW SIGN
	if (val==0x000000B7) return ON;	//# Po       MIDDLE DOT
	if (val==0x000000B8) return ON;	//# Sk       CEDILLA
	if (val==0x000000BB) return ON;	//# Pf       RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
	if ((val>=0x00BC) && (val<=0x00BE)) return ON;	//# No   [3] VULGAR FRACTION ONE QUARTER..VULGAR FRACTION THREE QUARTERS
	if (val==0x000000BF) return ON;	//# Po       INVERTED QUESTION MARK
	if (val==0x000000D7) return ON;	//# Sm       MULTIPLICATION SIGN
	if (val==0x000000F7) return ON;	//# Sm       DIVISION SIGN
	if ((val>=0x02B9) && (val<=0x02BA)) return ON;	//# Lm   [2] MODIFIER LETTER PRIME..MODIFIER LETTER DOUBLE PRIME
	if ((val>=0x02C2) && (val<=0x02C5)) return ON;	//# Sk   [4] MODIFIER LETTER LEFT ARROWHEAD..MODIFIER LETTER DOWN ARROWHEAD
	if ((val>=0x02C6) && (val<=0x02CF)) return ON;	//# Lm  [10] MODIFIER LETTER CIRCUMFLEX ACCENT..MODIFIER LETTER LOW ACUTE ACCENT
	if ((val>=0x02D2) && (val<=0x02DF)) return ON;	//# Sk  [14] MODIFIER LETTER CENTRED RIGHT HALF RING..MODIFIER LETTER CROSS ACCENT
	if ((val>=0x02E5) && (val<=0x02ED)) return ON;	//# Sk   [9] MODIFIER LETTER EXTRA-HIGH TONE BAR..MODIFIER LETTER UNASPIRATED
	if ((val>=0x02EF) && (val<=0x02FF)) return ON;	//# Sk  [17] MODIFIER LETTER LOW DOWN ARROWHEAD..MODIFIER LETTER LOW LEFT ARROW
	if ((val>=0x0374) && (val<=0x0375)) return ON;	//# Sk   [2] GREEK NUMERAL SIGN..GREEK LOWER NUMERAL SIGN
	if (val==0x0000037E) return ON;	//# Po       GREEK QUESTION MARK
	if ((val>=0x0384) && (val<=0x0385)) return ON;	//# Sk   [2] GREEK TONOS..GREEK DIALYTIKA TONOS
	if (val==0x00000387) return ON;	//# Po       GREEK ANO TELEIA
	if (val==0x000003F6) return ON;	//# Sm       GREEK REVERSED LUNATE EPSILON SYMBOL
	if (val==0x0000058A) return ON;	//# Pd       ARMENIAN HYPHEN
	if ((val>=0x060E) && (val<=0x060F)) return ON;	//# So   [2] ARABIC POETIC VERSE SIGN..ARABIC SIGN MISRA
	if (val==0x000006E9) return ON;	//# So       ARABIC PLACE OF SAJDAH
	if (val==0x000007F6) return ON;	//# So       NKO SYMBOL OO DENNEN
	if ((val>=0x07F7) && (val<=0x07F9)) return ON;	//# Po   [3] NKO SYMBOL GBAKURUNEN..NKO EXCLAMATION MARK
	if ((val>=0x0BF3) && (val<=0x0BF8)) return ON;	//# So   [6] TAMIL DAY SIGN..TAMIL AS ABOVE SIGN
	if (val==0x00000BFA) return ON;	//# So       TAMIL NUMBER SIGN
	if ((val>=0x0CF1) && (val<=0x0CF2)) return ON;	//# So   [2] KANNADA SIGN JIHVAMULIYA..KANNADA SIGN UPADHMANIYA
	if (val==0x00000F3A) return ON;	//# Ps       TIBETAN MARK GUG RTAGS GYON
	if (val==0x00000F3B) return ON;	//# Pe       TIBETAN MARK GUG RTAGS GYAS
	if (val==0x00000F3C) return ON;	//# Ps       TIBETAN MARK ANG KHANG GYON
	if (val==0x00000F3D) return ON;	//# Pe       TIBETAN MARK ANG KHANG GYAS
	if ((val>=0x1390) && (val<=0x1399)) return ON;	//# So  [10] ETHIOPIC TONAL MARK YIZET..ETHIOPIC TONAL MARK KURT
	if (val==0x0000169B) return ON;	//# Ps       OGHAM FEATHER MARK
	if (val==0x0000169C) return ON;	//# Pe       OGHAM REVERSED FEATHER MARK
	if ((val>=0x17F0) && (val<=0x17F9)) return ON;	//# No  [10] KHMER SYMBOL LEK ATTAK SON..KHMER SYMBOL LEK ATTAK PRAM-BUON
	if ((val>=0x1800) && (val<=0x1805)) return ON;	//# Po   [6] MONGOLIAN BIRGA..MONGOLIAN FOUR DOTS
	if (val==0x00001806) return ON;	//# Pd       MONGOLIAN TODO SOFT HYPHEN
	if ((val>=0x1807) && (val<=0x180A)) return ON;	//# Po   [4] MONGOLIAN SIBE SYLLABLE BOUNDARY MARKER..MONGOLIAN NIRUGU
	if (val==0x00001940) return ON;	//# So       LIMBU SIGN LOO
	if ((val>=0x1944) && (val<=0x1945)) return ON;	//# Po   [2] LIMBU EXCLAMATION MARK..LIMBU QUESTION MARK
	if ((val>=0x19DE) && (val<=0x19DF)) return ON;	//# Po   [2] NEW TAI LUE SIGN LAE..NEW TAI LUE SIGN LAEV
	if ((val>=0x19E0) && (val<=0x19FF)) return ON;	//# So  [32] KHMER SYMBOL PATHAMASAT..KHMER SYMBOL DAP-PRAM ROC
	if (val==0x00001FBD) return ON;	//# Sk       GREEK KORONIS
	if ((val>=0x1FBF) && (val<=0x1FC1)) return ON;	//# Sk   [3] GREEK PSILI..GREEK DIALYTIKA AND PERISPOMENI
	if ((val>=0x1FCD) && (val<=0x1FCF)) return ON;	//# Sk   [3] GREEK PSILI AND VARIA..GREEK PSILI AND PERISPOMENI
	if ((val>=0x1FDD) && (val<=0x1FDF)) return ON;	//# Sk   [3] GREEK DASIA AND VARIA..GREEK DASIA AND PERISPOMENI
	if ((val>=0x1FED) && (val<=0x1FEF)) return ON;	//# Sk   [3] GREEK DIALYTIKA AND VARIA..GREEK VARIA
	if ((val>=0x1FFD) && (val<=0x1FFE)) return ON;	//# Sk   [2] GREEK OXIA..GREEK DASIA
	if ((val>=0x2010) && (val<=0x2015)) return ON;	//# Pd   [6] HYPHEN..HORIZONTAL BAR
	if ((val>=0x2016) && (val<=0x2017)) return ON;	//# Po   [2] DOUBLE VERTICAL LINE..DOUBLE LOW LINE
	if (val==0x00002018) return ON;	//# Pi       LEFT SINGLE QUOTATION MARK
	if (val==0x00002019) return ON;	//# Pf       RIGHT SINGLE QUOTATION MARK
	if (val==0x0000201A) return ON;	//# Ps       SINGLE LOW-9 QUOTATION MARK
	if ((val>=0x201B) && (val<=0x201C)) return ON;	//# Pi   [2] SINGLE HIGH-REVERSED-9 QUOTATION MARK..LEFT DOUBLE QUOTATION MARK
	if (val==0x0000201D) return ON;	//# Pf       RIGHT DOUBLE QUOTATION MARK
	if (val==0x0000201E) return ON;	//# Ps       DOUBLE LOW-9 QUOTATION MARK
	if (val==0x0000201F) return ON;	//# Pi       DOUBLE HIGH-REVERSED-9 QUOTATION MARK
	if ((val>=0x2020) && (val<=0x2027)) return ON;	//# Po   [8] DAGGER..HYPHENATION POINT
	if ((val>=0x2035) && (val<=0x2038)) return ON;	//# Po   [4] REVERSED PRIME..CARET
	if (val==0x00002039) return ON;	//# Pi       SINGLE LEFT-POINTING ANGLE QUOTATION MARK
	if (val==0x0000203A) return ON;	//# Pf       SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
	if ((val>=0x203B) && (val<=0x203E)) return ON;	//# Po   [4] REFERENCE MARK..OVERLINE
	if ((val>=0x203F) && (val<=0x2040)) return ON;	//# Pc   [2] UNDERTIE..CHARACTER TIE
	if ((val>=0x2041) && (val<=0x2043)) return ON;	//# Po   [3] CARET INSERTION POINT..HYPHEN BULLET
	if (val==0x00002045) return ON;	//# Ps       LEFT SQUARE BRACKET WITH QUILL
	if (val==0x00002046) return ON;	//# Pe       RIGHT SQUARE BRACKET WITH QUILL
	if ((val>=0x2047) && (val<=0x2051)) return ON;	//# Po  [11] DOUBLE QUESTION MARK..TWO ASTERISKS ALIGNED VERTICALLY
	if (val==0x00002052) return ON;	//# Sm       COMMERCIAL MINUS SIGN
	if (val==0x00002053) return ON;	//# Po       SWUNG DASH
	if (val==0x00002054) return ON;	//# Pc       INVERTED UNDERTIE
	if ((val>=0x2055) && (val<=0x205E)) return ON;	//# Po  [10] FLOWER PUNCTUATION MARK..VERTICAL FOUR DOTS
	if (val==0x0000207C) return ON;	//# Sm       SUPERSCRIPT EQUALS SIGN
	if (val==0x0000207D) return ON;	//# Ps       SUPERSCRIPT LEFT PARENTHESIS
	if (val==0x0000207E) return ON;	//# Pe       SUPERSCRIPT RIGHT PARENTHESIS
	if (val==0x0000208C) return ON;	//# Sm       SUBSCRIPT EQUALS SIGN
	if (val==0x0000208D) return ON;	//# Ps       SUBSCRIPT LEFT PARENTHESIS
	if (val==0x0000208E) return ON;	//# Pe       SUBSCRIPT RIGHT PARENTHESIS
	if ((val>=0x2100) && (val<=0x2101)) return ON;	//# So   [2] ACCOUNT OF..ADDRESSED TO THE SUBJECT
	if ((val>=0x2103) && (val<=0x2106)) return ON;	//# So   [4] DEGREE CELSIUS..CADA UNA
	if ((val>=0x2108) && (val<=0x2109)) return ON;	//# So   [2] SCRUPLE..DEGREE FAHRENHEIT
	if (val==0x00002114) return ON;	//# So       L B BAR SYMBOL
	if ((val>=0x2116) && (val<=0x2118)) return ON;	//# So   [3] NUMERO SIGN..SCRIPT CAPITAL P
	if ((val>=0x211E) && (val<=0x2123)) return ON;	//# So   [6] PRESCRIPTION TAKE..VERSICLE
	if (val==0x00002125) return ON;	//# So       OUNCE SIGN
	if (val==0x00002127) return ON;	//# So       INVERTED OHM SIGN
	if (val==0x00002129) return ON;	//# So       TURNED GREEK SMALL LETTER IOTA
	if ((val>=0x213A) && (val<=0x213B)) return ON;	//# So   [2] ROTATED CAPITAL Q..FACSIMILE SIGN
	if ((val>=0x2140) && (val<=0x2144)) return ON;	//# Sm   [5] DOUBLE-STRUCK N-ARY SUMMATION..TURNED SANS-SERIF CAPITAL Y
	if (val==0x0000214A) return ON;	//# So       PROPERTY LINE
	if (val==0x0000214B) return ON;	//# Sm       TURNED AMPERSAND
	if ((val>=0x214C) && (val<=0x214D)) return ON;	//# So   [2] PER SIGN..AKTIESELSKAB
	if ((val>=0x2153) && (val<=0x215F)) return ON;	//# No  [13] VULGAR FRACTION ONE THIRD..FRACTION NUMERATOR ONE
	if ((val>=0x2190) && (val<=0x2194)) return ON;	//# Sm   [5] LEFTWARDS ARROW..LEFT RIGHT ARROW
	if ((val>=0x2195) && (val<=0x2199)) return ON;	//# So   [5] UP DOWN ARROW..SOUTH WEST ARROW
	if ((val>=0x219A) && (val<=0x219B)) return ON;	//# Sm   [2] LEFTWARDS ARROW WITH STROKE..RIGHTWARDS ARROW WITH STROKE
	if ((val>=0x219C) && (val<=0x219F)) return ON;	//# So   [4] LEFTWARDS WAVE ARROW..UPWARDS TWO HEADED ARROW
	if (val==0x000021A0) return ON;	//# Sm       RIGHTWARDS TWO HEADED ARROW
	if ((val>=0x21A1) && (val<=0x21A2)) return ON;	//# So   [2] DOWNWARDS TWO HEADED ARROW..LEFTWARDS ARROW WITH TAIL
	if (val==0x000021A3) return ON;	//# Sm       RIGHTWARDS ARROW WITH TAIL
	if ((val>=0x21A4) && (val<=0x21A5)) return ON;	//# So   [2] LEFTWARDS ARROW FROM BAR..UPWARDS ARROW FROM BAR
	if (val==0x000021A6) return ON;	//# Sm       RIGHTWARDS ARROW FROM BAR
	if ((val>=0x21A7) && (val<=0x21AD)) return ON;	//# So   [7] DOWNWARDS ARROW FROM BAR..LEFT RIGHT WAVE ARROW
	if (val==0x000021AE) return ON;	//# Sm       LEFT RIGHT ARROW WITH STROKE
	if ((val>=0x21AF) && (val<=0x21CD)) return ON;	//# So  [31] DOWNWARDS ZIGZAG ARROW..LEFTWARDS DOUBLE ARROW WITH STROKE
	if ((val>=0x21CE) && (val<=0x21CF)) return ON;	//# Sm   [2] LEFT RIGHT DOUBLE ARROW WITH STROKE..RIGHTWARDS DOUBLE ARROW WITH STROKE
	if ((val>=0x21D0) && (val<=0x21D1)) return ON;	//# So   [2] LEFTWARDS DOUBLE ARROW..UPWARDS DOUBLE ARROW
	if (val==0x000021D2) return ON;	//# Sm       RIGHTWARDS DOUBLE ARROW
	if (val==0x000021D3) return ON;	//# So       DOWNWARDS DOUBLE ARROW
	if (val==0x000021D4) return ON;	//# Sm       LEFT RIGHT DOUBLE ARROW
	if ((val>=0x21D5) && (val<=0x21F3)) return ON;	//# So  [31] UP DOWN DOUBLE ARROW..UP DOWN WHITE ARROW
	if ((val>=0x21F4) && (val<=0x2211)) return ON;	//# Sm  [30] RIGHT ARROW WITH SMALL CIRCLE..N-ARY SUMMATION
	if ((val>=0x2214) && (val<=0x22FF)) return ON;	//# Sm [236] DOT PLUS..Z NOTATION BAG MEMBERSHIP
	if ((val>=0x2300) && (val<=0x2307)) return ON;	//# So   [8] DIAMETER SIGN..WAVY LINE
	if ((val>=0x2308) && (val<=0x230B)) return ON;	//# Sm   [4] LEFT CEILING..RIGHT FLOOR
	if ((val>=0x230C) && (val<=0x231F)) return ON;	//# So  [20] BOTTOM RIGHT CROP..BOTTOM RIGHT CORNER
	if ((val>=0x2320) && (val<=0x2321)) return ON;	//# Sm   [2] TOP HALF INTEGRAL..BOTTOM HALF INTEGRAL
	if ((val>=0x2322) && (val<=0x2328)) return ON;	//# So   [7] FROWN..KEYBOARD
	if (val==0x00002329) return ON;	//# Ps       LEFT-POINTING ANGLE BRACKET
	if (val==0x0000232A) return ON;	//# Pe       RIGHT-POINTING ANGLE BRACKET
	if ((val>=0x232B) && (val<=0x2335)) return ON;	//# So  [11] ERASE TO THE LEFT..COUNTERSINK
	if (val==0x0000237B) return ON;	//# So       NOT CHECK MARK
	if (val==0x0000237C) return ON;	//# Sm       RIGHT ANGLE WITH DOWNWARDS ZIGZAG ARROW
	if ((val>=0x237D) && (val<=0x2394)) return ON;	//# So  [24] SHOULDERED OPEN BOX..SOFTWARE-FUNCTION SYMBOL
	if ((val>=0x2396) && (val<=0x239A)) return ON;	//# So   [5] DECIMAL SEPARATOR KEY SYMBOL..CLEAR SCREEN SYMBOL
	if ((val>=0x239B) && (val<=0x23B3)) return ON;	//# Sm  [25] LEFT PARENTHESIS UPPER HOOK..SUMMATION BOTTOM
	if ((val>=0x23B4) && (val<=0x23DB)) return ON;	//# So  [40] TOP SQUARE BRACKET..FUSE
	if ((val>=0x23DC) && (val<=0x23E1)) return ON;	//# Sm   [6] TOP PARENTHESIS..BOTTOM TORTOISE SHELL BRACKET
	if ((val>=0x23E2) && (val<=0x23E7)) return ON;	//# So   [6] WHITE TRAPEZIUM..ELECTRICAL INTERSECTION
	if ((val>=0x2400) && (val<=0x2426)) return ON;	//# So  [39] SYMBOL FOR NULL..SYMBOL FOR SUBSTITUTE FORM TWO
	if ((val>=0x2440) && (val<=0x244A)) return ON;	//# So  [11] OCR HOOK..OCR DOUBLE BACKSLASH
	if ((val>=0x2460) && (val<=0x2487)) return ON;	//# No  [40] CIRCLED DIGIT ONE..PARENTHESIZED NUMBER TWENTY
	if ((val>=0x24EA) && (val<=0x24FF)) return ON;	//# No  [22] CIRCLED DIGIT ZERO..NEGATIVE CIRCLED DIGIT ZERO
	if ((val>=0x2500) && (val<=0x25B6)) return ON;	//# So [183] BOX DRAWINGS LIGHT HORIZONTAL..BLACK RIGHT-POINTING TRIANGLE
	if (val==0x000025B7) return ON;	//# Sm       WHITE RIGHT-POINTING TRIANGLE
	if ((val>=0x25B8) && (val<=0x25C0)) return ON;	//# So   [9] BLACK RIGHT-POINTING SMALL TRIANGLE..BLACK LEFT-POINTING TRIANGLE
	if (val==0x000025C1) return ON;	//# Sm       WHITE LEFT-POINTING TRIANGLE
	if ((val>=0x25C2) && (val<=0x25F7)) return ON;	//# So  [54] BLACK LEFT-POINTING SMALL TRIANGLE..WHITE CIRCLE WITH UPPER RIGHT QUADRANT
	if ((val>=0x25F8) && (val<=0x25FF)) return ON;	//# Sm   [8] UPPER LEFT TRIANGLE..LOWER RIGHT TRIANGLE
	if ((val>=0x2600) && (val<=0x266E)) return ON;	//# So [111] BLACK SUN WITH RAYS..MUSIC NATURAL SIGN
	if (val==0x0000266F) return ON;	//# Sm       MUSIC SHARP SIGN
	if ((val>=0x2670) && (val<=0x269C)) return ON;	//# So  [45] WEST SYRIAC CROSS..FLEUR-DE-LIS
	if ((val>=0x26A0) && (val<=0x26AB)) return ON;	//# So  [12] WARNING SIGN..MEDIUM BLACK CIRCLE
	if ((val>=0x26AD) && (val<=0x26B2)) return ON;	//# So   [6] MARRIAGE SYMBOL..NEUTER
	if ((val>=0x2701) && (val<=0x2704)) return ON;	//# So   [4] UPPER BLADE SCISSORS..WHITE SCISSORS
	if ((val>=0x2706) && (val<=0x2709)) return ON;	//# So   [4] TELEPHONE LOCATION SIGN..ENVELOPE
	if ((val>=0x270C) && (val<=0x2727)) return ON;	//# So  [28] VICTORY HAND..WHITE FOUR POINTED STAR
	if ((val>=0x2729) && (val<=0x274B)) return ON;	//# So  [35] STRESS OUTLINED WHITE STAR..HEAVY EIGHT TEARDROP-SPOKED PROPELLER ASTERISK
	if (val==0x0000274D) return ON;	//# So       SHADOWED WHITE CIRCLE
	if ((val>=0x274F) && (val<=0x2752)) return ON;	//# So   [4] LOWER RIGHT DROP-SHADOWED WHITE SQUARE..UPPER RIGHT SHADOWED WHITE SQUARE
	if (val==0x00002756) return ON;	//# So       BLACK DIAMOND MINUS WHITE X
	if ((val>=0x2758) && (val<=0x275E)) return ON;	//# So   [7] LIGHT VERTICAL BAR..HEAVY DOUBLE COMMA QUOTATION MARK ORNAMENT
	if ((val>=0x2761) && (val<=0x2767)) return ON;	//# So   [7] CURVED STEM PARAGRAPH SIGN ORNAMENT..ROTATED FLORAL HEART BULLET
	if (val==0x00002768) return ON;	//# Ps       MEDIUM LEFT PARENTHESIS ORNAMENT
	if (val==0x00002769) return ON;	//# Pe       MEDIUM RIGHT PARENTHESIS ORNAMENT
	if (val==0x0000276A) return ON;	//# Ps       MEDIUM FLATTENED LEFT PARENTHESIS ORNAMENT
	if (val==0x0000276B) return ON;	//# Pe       MEDIUM FLATTENED RIGHT PARENTHESIS ORNAMENT
	if (val==0x0000276C) return ON;	//# Ps       MEDIUM LEFT-POINTING ANGLE BRACKET ORNAMENT
	if (val==0x0000276D) return ON;	//# Pe       MEDIUM RIGHT-POINTING ANGLE BRACKET ORNAMENT
	if (val==0x0000276E) return ON;	//# Ps       HEAVY LEFT-POINTING ANGLE QUOTATION MARK ORNAMENT
	if (val==0x0000276F) return ON;	//# Pe       HEAVY RIGHT-POINTING ANGLE QUOTATION MARK ORNAMENT
	if (val==0x00002770) return ON;	//# Ps       HEAVY LEFT-POINTING ANGLE BRACKET ORNAMENT
	if (val==0x00002771) return ON;	//# Pe       HEAVY RIGHT-POINTING ANGLE BRACKET ORNAMENT
	if (val==0x00002772) return ON;	//# Ps       LIGHT LEFT TORTOISE SHELL BRACKET ORNAMENT
	if (val==0x00002773) return ON;	//# Pe       LIGHT RIGHT TORTOISE SHELL BRACKET ORNAMENT
	if (val==0x00002774) return ON;	//# Ps       MEDIUM LEFT CURLY BRACKET ORNAMENT
	if (val==0x00002775) return ON;	//# Pe       MEDIUM RIGHT CURLY BRACKET ORNAMENT
	if ((val>=0x2776) && (val<=0x2793)) return ON;	//# No  [30] DINGBAT NEGATIVE CIRCLED DIGIT ONE..DINGBAT NEGATIVE CIRCLED SANS-SERIF NUMBER TEN
	if (val==0x00002794) return ON;	//# So       HEAVY WIDE-HEADED RIGHTWARDS ARROW
	if ((val>=0x2798) && (val<=0x27AF)) return ON;	//# So  [24] HEAVY SOUTH EAST ARROW..NOTCHED LOWER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW
	if ((val>=0x27B1) && (val<=0x27BE)) return ON;	//# So  [14] NOTCHED UPPER RIGHT-SHADOWED WHITE RIGHTWARDS ARROW..OPEN-OUTLINED RIGHTWARDS ARROW
	if ((val>=0x27C0) && (val<=0x27C4)) return ON;	//# Sm   [5] THREE DIMENSIONAL ANGLE..OPEN SUPERSET
	if (val==0x000027C5) return ON;	//# Ps       LEFT S-SHAPED BAG DELIMITER
	if (val==0x000027C6) return ON;	//# Pe       RIGHT S-SHAPED BAG DELIMITER
	if ((val>=0x27C7) && (val<=0x27CA)) return ON;	//# Sm   [4] OR WITH DOT INSIDE..VERTICAL BAR WITH HORIZONTAL STROKE
	if ((val>=0x27D0) && (val<=0x27E5)) return ON;	//# Sm  [22] WHITE DIAMOND WITH CENTRED DOT..WHITE SQUARE WITH RIGHTWARDS TICK
	if (val==0x000027E6) return ON;	//# Ps       MATHEMATICAL LEFT WHITE SQUARE BRACKET
	if (val==0x000027E7) return ON;	//# Pe       MATHEMATICAL RIGHT WHITE SQUARE BRACKET
	if (val==0x000027E8) return ON;	//# Ps       MATHEMATICAL LEFT ANGLE BRACKET
	if (val==0x000027E9) return ON;	//# Pe       MATHEMATICAL RIGHT ANGLE BRACKET
	if (val==0x000027EA) return ON;	//# Ps       MATHEMATICAL LEFT DOUBLE ANGLE BRACKET
	if (val==0x000027EB) return ON;	//# Pe       MATHEMATICAL RIGHT DOUBLE ANGLE BRACKET
	if ((val>=0x27F0) && (val<=0x27FF)) return ON;	//# Sm  [16] UPWARDS QUADRUPLE ARROW..LONG RIGHTWARDS SQUIGGLE ARROW
	if ((val>=0x2900) && (val<=0x2982)) return ON;	//# Sm [131] RIGHTWARDS TWO-HEADED ARROW WITH VERTICAL STROKE..Z NOTATION TYPE COLON
	if (val==0x00002983) return ON;	//# Ps       LEFT WHITE CURLY BRACKET
	if (val==0x00002984) return ON;	//# Pe       RIGHT WHITE CURLY BRACKET
	if (val==0x00002985) return ON;	//# Ps       LEFT WHITE PARENTHESIS
	if (val==0x00002986) return ON;	//# Pe       RIGHT WHITE PARENTHESIS
	if (val==0x00002987) return ON;	//# Ps       Z NOTATION LEFT IMAGE BRACKET
	if (val==0x00002988) return ON;	//# Pe       Z NOTATION RIGHT IMAGE BRACKET
	if (val==0x00002989) return ON;	//# Ps       Z NOTATION LEFT BINDING BRACKET
	if (val==0x0000298A) return ON;	//# Pe       Z NOTATION RIGHT BINDING BRACKET
	if (val==0x0000298B) return ON;	//# Ps       LEFT SQUARE BRACKET WITH UNDERBAR
	if (val==0x0000298C) return ON;	//# Pe       RIGHT SQUARE BRACKET WITH UNDERBAR
	if (val==0x0000298D) return ON;	//# Ps       LEFT SQUARE BRACKET WITH TICK IN TOP CORNER
	if (val==0x0000298E) return ON;	//# Pe       RIGHT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
	if (val==0x0000298F) return ON;	//# Ps       LEFT SQUARE BRACKET WITH TICK IN BOTTOM CORNER
	if (val==0x00002990) return ON;	//# Pe       RIGHT SQUARE BRACKET WITH TICK IN TOP CORNER
	if (val==0x00002991) return ON;	//# Ps       LEFT ANGLE BRACKET WITH DOT
	if (val==0x00002992) return ON;	//# Pe       RIGHT ANGLE BRACKET WITH DOT
	if (val==0x00002993) return ON;	//# Ps       LEFT ARC LESS-THAN BRACKET
	if (val==0x00002994) return ON;	//# Pe       RIGHT ARC GREATER-THAN BRACKET
	if (val==0x00002995) return ON;	//# Ps       DOUBLE LEFT ARC GREATER-THAN BRACKET
	if (val==0x00002996) return ON;	//# Pe       DOUBLE RIGHT ARC LESS-THAN BRACKET
	if (val==0x00002997) return ON;	//# Ps       LEFT BLACK TORTOISE SHELL BRACKET
	if (val==0x00002998) return ON;	//# Pe       RIGHT BLACK TORTOISE SHELL BRACKET
	if ((val>=0x2999) && (val<=0x29D7)) return ON;	//# Sm  [63] DOTTED FENCE..BLACK HOURGLASS
	if (val==0x000029D8) return ON;	//# Ps       LEFT WIGGLY FENCE
	if (val==0x000029D9) return ON;	//# Pe       RIGHT WIGGLY FENCE
	if (val==0x000029DA) return ON;	//# Ps       LEFT DOUBLE WIGGLY FENCE
	if (val==0x000029DB) return ON;	//# Pe       RIGHT DOUBLE WIGGLY FENCE
	if ((val>=0x29DC) && (val<=0x29FB)) return ON;	//# Sm  [32] INCOMPLETE INFINITY..TRIPLE PLUS
	if (val==0x000029FC) return ON;	//# Ps       LEFT-POINTING CURVED ANGLE BRACKET
	if (val==0x000029FD) return ON;	//# Pe       RIGHT-POINTING CURVED ANGLE BRACKET
	if ((val>=0x29FE) && (val<=0x2AFF)) return ON;	//# Sm [258] TINY..N-ARY WHITE VERTICAL BAR
	if ((val>=0x2B00) && (val<=0x2B1A)) return ON;	//# So  [27] NORTH EAST WHITE ARROW..DOTTED SQUARE
	if ((val>=0x2B20) && (val<=0x2B23)) return ON;	//# So   [4] WHITE PENTAGON..HORIZONTAL BLACK HEXAGON
	if ((val>=0x2CE5) && (val<=0x2CEA)) return ON;	//# So   [6] COPTIC SYMBOL MI RO..COPTIC SYMBOL SHIMA SIMA
	if ((val>=0x2CF9) && (val<=0x2CFC)) return ON;	//# Po   [4] COPTIC OLD NUBIAN FULL STOP..COPTIC OLD NUBIAN VERSE DIVIDER
	if (val==0x00002CFD) return ON;	//# No       COPTIC FRACTION ONE HALF
	if ((val>=0x2CFE) && (val<=0x2CFF)) return ON;	//# Po   [2] COPTIC FULL STOP..COPTIC MORPHOLOGICAL DIVIDER
	if ((val>=0x2E00) && (val<=0x2E01)) return ON;	//# Po   [2] RIGHT ANGLE SUBSTITUTION MARKER..RIGHT ANGLE DOTTED SUBSTITUTION MARKER
	if (val==0x00002E02) return ON;	//# Pi       LEFT SUBSTITUTION BRACKET
	if (val==0x00002E03) return ON;	//# Pf       RIGHT SUBSTITUTION BRACKET
	if (val==0x00002E04) return ON;	//# Pi       LEFT DOTTED SUBSTITUTION BRACKET
	if (val==0x00002E05) return ON;	//# Pf       RIGHT DOTTED SUBSTITUTION BRACKET
	if ((val>=0x2E06) && (val<=0x2E08)) return ON;	//# Po   [3] RAISED INTERPOLATION MARKER..DOTTED TRANSPOSITION MARKER
	if (val==0x00002E09) return ON;	//# Pi       LEFT TRANSPOSITION BRACKET
	if (val==0x00002E0A) return ON;	//# Pf       RIGHT TRANSPOSITION BRACKET
	if (val==0x00002E0B) return ON;	//# Po       RAISED SQUARE
	if (val==0x00002E0C) return ON;	//# Pi       LEFT RAISED OMISSION BRACKET
	if (val==0x00002E0D) return ON;	//# Pf       RIGHT RAISED OMISSION BRACKET
	if ((val>=0x2E0E) && (val<=0x2E16)) return ON;	//# Po   [9] EDITORIAL CORONIS..DOTTED RIGHT-POINTING ANGLE
	if (val==0x00002E17) return ON;	//# Pd       DOUBLE OBLIQUE HYPHEN
	if (val==0x00002E1C) return ON;	//# Pi       LEFT LOW PARAPHRASE BRACKET
	if (val==0x00002E1D) return ON;	//# Pf       RIGHT LOW PARAPHRASE BRACKET
	if ((val>=0x2E80) && (val<=0x2E99)) return ON;	//# So  [26] CJK RADICAL REPEAT..CJK RADICAL RAP
	if ((val>=0x2E9B) && (val<=0x2EF3)) return ON;	//# So  [89] CJK RADICAL CHOKE..CJK RADICAL C-SIMPLIFIED TURTLE
	if ((val>=0x2F00) && (val<=0x2FD5)) return ON;	//# So [214] KANGXI RADICAL ONE..KANGXI RADICAL FLUTE
	if ((val>=0x2FF0) && (val<=0x2FFB)) return ON;	//# So  [12] IDEOGRAPHIC DESCRIPTION CHARACTER LEFT TO RIGHT..IDEOGRAPHIC DESCRIPTION CHARACTER OVERLAID
	if ((val>=0x3001) && (val<=0x3003)) return ON;	//# Po   [3] IDEOGRAPHIC COMMA..DITTO MARK
	if (val==0x00003004) return ON;	//# So       JAPANESE INDUSTRIAL STANDARD SYMBOL
	if (val==0x00003008) return ON;	//# Ps       LEFT ANGLE BRACKET
	if (val==0x00003009) return ON;	//# Pe       RIGHT ANGLE BRACKET
	if (val==0x0000300A) return ON;	//# Ps       LEFT DOUBLE ANGLE BRACKET
	if (val==0x0000300B) return ON;	//# Pe       RIGHT DOUBLE ANGLE BRACKET
	if (val==0x0000300C) return ON;	//# Ps       LEFT CORNER BRACKET
	if (val==0x0000300D) return ON;	//# Pe       RIGHT CORNER BRACKET
	if (val==0x0000300E) return ON;	//# Ps       LEFT WHITE CORNER BRACKET
	if (val==0x0000300F) return ON;	//# Pe       RIGHT WHITE CORNER BRACKET
	if (val==0x00003010) return ON;	//# Ps       LEFT BLACK LENTICULAR BRACKET
	if (val==0x00003011) return ON;	//# Pe       RIGHT BLACK LENTICULAR BRACKET
	if ((val>=0x3012) && (val<=0x3013)) return ON;	//# So   [2] POSTAL MARK..GETA MARK
	if (val==0x00003014) return ON;	//# Ps       LEFT TORTOISE SHELL BRACKET
	if (val==0x00003015) return ON;	//# Pe       RIGHT TORTOISE SHELL BRACKET
	if (val==0x00003016) return ON;	//# Ps       LEFT WHITE LENTICULAR BRACKET
	if (val==0x00003017) return ON;	//# Pe       RIGHT WHITE LENTICULAR BRACKET
	if (val==0x00003018) return ON;	//# Ps       LEFT WHITE TORTOISE SHELL BRACKET
	if (val==0x00003019) return ON;	//# Pe       RIGHT WHITE TORTOISE SHELL BRACKET
	if (val==0x0000301A) return ON;	//# Ps       LEFT WHITE SQUARE BRACKET
	if (val==0x0000301B) return ON;	//# Pe       RIGHT WHITE SQUARE BRACKET
	if (val==0x0000301C) return ON;	//# Pd       WAVE DASH
	if (val==0x0000301D) return ON;	//# Ps       REVERSED DOUBLE PRIME QUOTATION MARK
	if ((val>=0x301E) && (val<=0x301F)) return ON;	//# Pe   [2] DOUBLE PRIME QUOTATION MARK..LOW DOUBLE PRIME QUOTATION MARK
	if (val==0x00003020) return ON;	//# So       POSTAL MARK FACE
	if (val==0x00003030) return ON;	//# Pd       WAVY DASH
	if ((val>=0x3036) && (val<=0x3037)) return ON;	//# So   [2] CIRCLED POSTAL MARK..IDEOGRAPHIC TELEGRAPH LINE FEED SEPARATOR SYMBOL
	if (val==0x0000303D) return ON;	//# Po       PART ALTERNATION MARK
	if ((val>=0x303E) && (val<=0x303F)) return ON;	//# So   [2] IDEOGRAPHIC VARIATION INDICATOR..IDEOGRAPHIC HALF FILL SPACE
	if ((val>=0x309B) && (val<=0x309C)) return ON;	//# Sk   [2] KATAKANA-HIRAGANA VOICED SOUND MARK..KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
	if (val==0x000030A0) return ON;	//# Pd       KATAKANA-HIRAGANA DOUBLE HYPHEN
	if (val==0x000030FB) return ON;	//# Po       KATAKANA MIDDLE DOT
	if ((val>=0x31C0) && (val<=0x31CF)) return ON;	//# So  [16] CJK STROKE T..CJK STROKE N
	if ((val>=0x321D) && (val<=0x321E)) return ON;	//# So   [2] PARENTHESIZED KOREAN CHARACTER OJEON..PARENTHESIZED KOREAN CHARACTER O HU
	if (val==0x00003250) return ON;	//# So       PARTNERSHIP SIGN
	if ((val>=0x3251) && (val<=0x325F)) return ON;	//# No  [15] CIRCLED NUMBER TWENTY ONE..CIRCLED NUMBER THIRTY FIVE
	if ((val>=0x327C) && (val<=0x327E)) return ON;	//# So   [3] CIRCLED KOREAN CHARACTER CHAMKO..CIRCLED HANGUL IEUNG U
	if ((val>=0x32B1) && (val<=0x32BF)) return ON;	//# No  [15] CIRCLED NUMBER THIRTY SIX..CIRCLED NUMBER FIFTY
	if ((val>=0x32CC) && (val<=0x32CF)) return ON;	//# So   [4] SQUARE HG..LIMITED LIABILITY SIGN
	if ((val>=0x3377) && (val<=0x337A)) return ON;	//# So   [4] SQUARE DM..SQUARE IU
	if ((val>=0x33DE) && (val<=0x33DF)) return ON;	//# So   [2] SQUARE V OVER M..SQUARE A OVER M
	if (val==0x000033FF) return ON;	//# So       SQUARE GAL
	if ((val>=0x4DC0) && (val<=0x4DFF)) return ON;	//# So  [64] HEXAGRAM FOR THE CREATIVE HEAVEN..HEXAGRAM FOR BEFORE COMPLETION
	if ((val>=0xA490) && (val<=0xA4C6)) return ON;	//# So  [55] YI RADICAL QOT..YI RADICAL KE
	if ((val>=0xA700) && (val<=0xA716)) return ON;	//# Sk  [23] MODIFIER LETTER CHINESE TONE YIN PING..MODIFIER LETTER EXTRA-LOW LEFT-STEM TONE BAR
	if ((val>=0xA717) && (val<=0xA71A)) return ON;	//# Lm   [4] MODIFIER LETTER DOT VERTICAL BAR..MODIFIER LETTER LOWER RIGHT CORNER ANGLE
	if ((val>=0xA720) && (val<=0xA721)) return ON;	//# Sk   [2] MODIFIER LETTER STRESS AND HIGH TONE..MODIFIER LETTER STRESS AND LOW TONE
	if ((val>=0xA828) && (val<=0xA82B)) return ON;	//# So   [4] SYLOTI NAGRI POETRY MARK-1..SYLOTI NAGRI POETRY MARK-4
	if ((val>=0xA874) && (val<=0xA877)) return ON;	//# Po   [4] PHAGS-PA SINGLE HEAD MARK..PHAGS-PA MARK DOUBLE SHAD
	if (val==0x0000FD3E) return ON;	//# Ps       ORNATE LEFT PARENTHESIS
	if (val==0x0000FD3F) return ON;	//# Pe       ORNATE RIGHT PARENTHESIS
	if (val==0x0000FDFD) return ON;	//# So       ARABIC LIGATURE BISMILLAH AR-RAHMAN AR-RAHEEM
	if ((val>=0xFE10) && (val<=0xFE16)) return ON;	//# Po   [7] PRESENTATION FORM FOR VERTICAL COMMA..PRESENTATION FORM FOR VERTICAL QUESTION MARK
	if (val==0x0000FE17) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT WHITE LENTICULAR BRACKET
	if (val==0x0000FE18) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT WHITE LENTICULAR BRAKCET
	if (val==0x0000FE19) return ON;	//# Po       PRESENTATION FORM FOR VERTICAL HORIZONTAL ELLIPSIS
	if (val==0x0000FE30) return ON;	//# Po       PRESENTATION FORM FOR VERTICAL TWO DOT LEADER
	if ((val>=0xFE31) && (val<=0xFE32)) return ON;	//# Pd   [2] PRESENTATION FORM FOR VERTICAL EM DASH..PRESENTATION FORM FOR VERTICAL EN DASH
	if ((val>=0xFE33) && (val<=0xFE34)) return ON;	//# Pc   [2] PRESENTATION FORM FOR VERTICAL LOW LINE..PRESENTATION FORM FOR VERTICAL WAVY LOW LINE
	if (val==0x0000FE35) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT PARENTHESIS
	if (val==0x0000FE36) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT PARENTHESIS
	if (val==0x0000FE37) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT CURLY BRACKET
	if (val==0x0000FE38) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT CURLY BRACKET
	if (val==0x0000FE39) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT TORTOISE SHELL BRACKET
	if (val==0x0000FE3A) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT TORTOISE SHELL BRACKET
	if (val==0x0000FE3B) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT BLACK LENTICULAR BRACKET
	if (val==0x0000FE3C) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT BLACK LENTICULAR BRACKET
	if (val==0x0000FE3D) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT DOUBLE ANGLE BRACKET
	if (val==0x0000FE3E) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT DOUBLE ANGLE BRACKET
	if (val==0x0000FE3F) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT ANGLE BRACKET
	if (val==0x0000FE40) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT ANGLE BRACKET
	if (val==0x0000FE41) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT CORNER BRACKET
	if (val==0x0000FE42) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT CORNER BRACKET
	if (val==0x0000FE43) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT WHITE CORNER BRACKET
	if (val==0x0000FE44) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT WHITE CORNER BRACKET
	if ((val>=0xFE45) && (val<=0xFE46)) return ON;	//# Po   [2] SESAME DOT..WHITE SESAME DOT
	if (val==0x0000FE47) return ON;	//# Ps       PRESENTATION FORM FOR VERTICAL LEFT SQUARE BRACKET
	if (val==0x0000FE48) return ON;	//# Pe       PRESENTATION FORM FOR VERTICAL RIGHT SQUARE BRACKET
	if ((val>=0xFE49) && (val<=0xFE4C)) return ON;	//# Po   [4] DASHED OVERLINE..DOUBLE WAVY OVERLINE
	if ((val>=0xFE4D) && (val<=0xFE4F)) return ON;	//# Pc   [3] DASHED LOW LINE..WAVY LOW LINE
	if (val==0x0000FE51) return ON;	//# Po       SMALL IDEOGRAPHIC COMMA
	if (val==0x0000FE54) return ON;	//# Po       SMALL SEMICOLON
	if ((val>=0xFE56) && (val<=0xFE57)) return ON;	//# Po   [2] SMALL QUESTION MARK..SMALL EXCLAMATION MARK
	if (val==0x0000FE58) return ON;	//# Pd       SMALL EM DASH
	if (val==0x0000FE59) return ON;	//# Ps       SMALL LEFT PARENTHESIS
	if (val==0x0000FE5A) return ON;	//# Pe       SMALL RIGHT PARENTHESIS
	if (val==0x0000FE5B) return ON;	//# Ps       SMALL LEFT CURLY BRACKET
	if (val==0x0000FE5C) return ON;	//# Pe       SMALL RIGHT CURLY BRACKET
	if (val==0x0000FE5D) return ON;	//# Ps       SMALL LEFT TORTOISE SHELL BRACKET
	if (val==0x0000FE5E) return ON;	//# Pe       SMALL RIGHT TORTOISE SHELL BRACKET
	if ((val>=0xFE60) && (val<=0xFE61)) return ON;	//# Po   [2] SMALL AMPERSAND..SMALL ASTERISK
	if ((val>=0xFE64) && (val<=0xFE66)) return ON;	//# Sm   [3] SMALL LESS-THAN SIGN..SMALL EQUALS SIGN
	if (val==0x0000FE68) return ON;	//# Po       SMALL REVERSE SOLIDUS
	if (val==0x0000FE6B) return ON;	//# Po       SMALL COMMERCIAL AT
	if ((val>=0xFF01) && (val<=0xFF02)) return ON;	//# Po   [2] FULLWIDTH EXCLAMATION MARK..FULLWIDTH QUOTATION MARK
	if ((val>=0xFF06) && (val<=0xFF07)) return ON;	//# Po   [2] FULLWIDTH AMPERSAND..FULLWIDTH APOSTROPHE
	if (val==0x0000FF08) return ON;	//# Ps       FULLWIDTH LEFT PARENTHESIS
	if (val==0x0000FF09) return ON;	//# Pe       FULLWIDTH RIGHT PARENTHESIS
	if (val==0x0000FF0A) return ON;	//# Po       FULLWIDTH ASTERISK
	if (val==0x0000FF1B) return ON;	//# Po       FULLWIDTH SEMICOLON
	if ((val>=0xFF1C) && (val<=0xFF1E)) return ON;	//# Sm   [3] FULLWIDTH LESS-THAN SIGN..FULLWIDTH GREATER-THAN SIGN
	if ((val>=0xFF1F) && (val<=0xFF20)) return ON;	//# Po   [2] FULLWIDTH QUESTION MARK..FULLWIDTH COMMERCIAL AT
	if (val==0x0000FF3B) return ON;	//# Ps       FULLWIDTH LEFT SQUARE BRACKET
	if (val==0x0000FF3C) return ON;	//# Po       FULLWIDTH REVERSE SOLIDUS
	if (val==0x0000FF3D) return ON;	//# Pe       FULLWIDTH RIGHT SQUARE BRACKET
	if (val==0x0000FF3E) return ON;	//# Sk       FULLWIDTH CIRCUMFLEX ACCENT
	if (val==0x0000FF3F) return ON;	//# Pc       FULLWIDTH LOW LINE
	if (val==0x0000FF40) return ON;	//# Sk       FULLWIDTH GRAVE ACCENT
	if (val==0x0000FF5B) return ON;	//# Ps       FULLWIDTH LEFT CURLY BRACKET
	if (val==0x0000FF5C) return ON;	//# Sm       FULLWIDTH VERTICAL LINE
	if (val==0x0000FF5D) return ON;	//# Pe       FULLWIDTH RIGHT CURLY BRACKET
	if (val==0x0000FF5E) return ON;	//# Sm       FULLWIDTH TILDE
	if (val==0x0000FF5F) return ON;	//# Ps       FULLWIDTH LEFT WHITE PARENTHESIS
	if (val==0x0000FF60) return ON;	//# Pe       FULLWIDTH RIGHT WHITE PARENTHESIS
	if (val==0x0000FF61) return ON;	//# Po       HALFWIDTH IDEOGRAPHIC FULL STOP
	if (val==0x0000FF62) return ON;	//# Ps       HALFWIDTH LEFT CORNER BRACKET
	if (val==0x0000FF63) return ON;	//# Pe       HALFWIDTH RIGHT CORNER BRACKET
	if ((val>=0xFF64) && (val<=0xFF65)) return ON;	//# Po   [2] HALFWIDTH IDEOGRAPHIC COMMA..HALFWIDTH KATAKANA MIDDLE DOT
	if (val==0x0000FFE2) return ON;	//# Sm       FULLWIDTH NOT SIGN
	if (val==0x0000FFE3) return ON;	//# Sk       FULLWIDTH MACRON
	if (val==0x0000FFE4) return ON;	//# So       FULLWIDTH BROKEN BAR
	if (val==0x0000FFE8) return ON;	//# So       HALFWIDTH FORMS LIGHT VERTICAL
	if ((val>=0xFFE9) && (val<=0xFFEC)) return ON;	//# Sm   [4] HALFWIDTH LEFTWARDS ARROW..HALFWIDTH DOWNWARDS ARROW
	if ((val>=0xFFED) && (val<=0xFFEE)) return ON;	//# So   [2] HALFWIDTH BLACK SQUARE..HALFWIDTH WHITE CIRCLE
	if ((val>=0xFFF9) && (val<=0xFFFB)) return ON;	//# Cf   [3] INTERLINEAR ANNOTATION ANCHOR..INTERLINEAR ANNOTATION TERMINATOR
	if ((val>=0xFFFC) && (val<=0xFFFD)) return ON;	//# So   [2] OBJECT REPLACEMENT CHARACTER..REPLACEMENT CHARACTER
	if (val==0x00010101) return ON;	//# Po       AEGEAN WORD SEPARATOR DOT
	if ((val>=0x10140) && (val<=0x10174)) return ON;	//# Nl  [53] GREEK ACROPHONIC ATTIC ONE QUARTER..GREEK ACROPHONIC STRATIAN FIFTY MNAS
	if ((val>=0x10175) && (val<=0x10178)) return ON;	//# No   [4] GREEK ONE HALF SIGN..GREEK THREE QUARTERS SIGN
	if ((val>=0x10179) && (val<=0x10189)) return ON;	//# So  [17] GREEK YEAR SIGN..GREEK TRYBLION BASE SIGN
	if (val==0x0001018A) return ON;	//# No       GREEK ZERO SIGN
	if (val==0x0001091F) return ON;	//# Po       PHOENICIAN WORD SEPARATOR
	if ((val>=0x1D200) && (val<=0x1D241)) return ON;	//# So  [66] GREEK VOCAL NOTATION SYMBOL-1..GREEK INSTRUMENTAL NOTATION SYMBOL-54
	if (val==0x0001D245) return ON;	//# So       GREEK MUSICAL LEIMMA
	if ((val>=0x1D300) && (val<=0x1D356)) return ON;	//# So  [87] MONOGRAM FOR EARTH..TETRAGRAM FOR FOSTERING
	if (/*(val>=0x0000) && */(val<=0x0008)) return BN;	//# Cc   [9] <control-0000>..<control-0008>
	if ((val>=0x000E) && (val<=0x001B)) return BN;	//# Cc  [14] <control-000E>..<control-001B>
	if ((val>=0x007F) && (val<=0x0084)) return BN;	//# Cc   [6] <control-007F>..<control-0084>
	if ((val>=0x0086) && (val<=0x009F)) return BN;	//# Cc  [26] <control-0086>..<control-009F>
	if (val==0x000000AD) return BN;	//# Cf       SOFT HYPHEN
	if (val==0x0000070F) return BN;	//# Cf       SYRIAC ABBREVIATION MARK
	if ((val>=0x200B) && (val<=0x200D)) return BN;	//# Cf   [3] ZERO WIDTH SPACE..ZERO WIDTH JOINER
	if ((val>=0x2060) && (val<=0x2063)) return BN;	//# Cf   [4] WORD JOINER..INVISIBLE SEPARATOR
	if ((val>=0x2064) && (val<=0x2069)) return BN;	//# Cn   [6] <reserved-2064>..<reserved-2069>
	if ((val>=0x206A) && (val<=0x206F)) return BN;	//# Cf   [6] INHIBIT SYMMETRIC SWAPPING..NOMINAL DIGIT SHAPES
	if ((val>=0xFDD0) && (val<=0xFDEF)) return BN;	//# Cn  [32] <noncharacter-FDD0>..<noncharacter-FDEF>
	if (val==0x0000FEFF) return BN;	//# Cf       ZERO WIDTH NO-BREAK SPACE
	if ((val>=0xFFF0) && (val<=0xFFF8)) return BN;	//# Cn   [9] <reserved-FFF0>..<reserved-FFF8>
	if ((val>=0xFFFE) && (val<=0xFFFF)) return BN;	//# Cn   [2] <noncharacter-FFFE>..<noncharacter-FFFF>
	if ((val>=0x1D173) && (val<=0x1D17A)) return BN;	//# Cf   [8] MUSICAL SYMBOL BEGIN BEAM..MUSICAL SYMBOL END PHRASE
	if ((val>=0x1FFFE) && (val<=0x1FFFF)) return BN;	//# Cn   [2] <noncharacter-1FFFE>..<noncharacter-1FFFF>
	if ((val>=0x2FFFE) && (val<=0x2FFFF)) return BN;	//# Cn   [2] <noncharacter-2FFFE>..<noncharacter-2FFFF>
	if ((val>=0x3FFFE) && (val<=0x3FFFF)) return BN;	//# Cn   [2] <noncharacter-3FFFE>..<noncharacter-3FFFF>
	if ((val>=0x4FFFE) && (val<=0x4FFFF)) return BN;	//# Cn   [2] <noncharacter-4FFFE>..<noncharacter-4FFFF>
	if ((val>=0x5FFFE) && (val<=0x5FFFF)) return BN;	//# Cn   [2] <noncharacter-5FFFE>..<noncharacter-5FFFF>
	if ((val>=0x6FFFE) && (val<=0x6FFFF)) return BN;	//# Cn   [2] <noncharacter-6FFFE>..<noncharacter-6FFFF>
	if ((val>=0x7FFFE) && (val<=0x7FFFF)) return BN;	//# Cn   [2] <noncharacter-7FFFE>..<noncharacter-7FFFF>
	if ((val>=0x8FFFE) && (val<=0x8FFFF)) return BN;	//# Cn   [2] <noncharacter-8FFFE>..<noncharacter-8FFFF>
	if ((val>=0x9FFFE) && (val<=0x9FFFF)) return BN;	//# Cn   [2] <noncharacter-9FFFE>..<noncharacter-9FFFF>
	if ((val>=0xAFFFE) && (val<=0xAFFFF)) return BN;	//# Cn   [2] <noncharacter-AFFFE>..<noncharacter-AFFFF>
	if ((val>=0xBFFFE) && (val<=0xBFFFF)) return BN;	//# Cn   [2] <noncharacter-BFFFE>..<noncharacter-BFFFF>
	if ((val>=0xCFFFE) && (val<=0xCFFFF)) return BN;	//# Cn   [2] <noncharacter-CFFFE>..<noncharacter-CFFFF>
	if ((val>=0xDFFFE) && (val<=0xE0000)) return BN;	//# Cn   [3] <noncharacter-DFFFE>..<reserved-E0000>
	if (val==0x000E0001) return BN;	//# Cf       LANGUAGE TAG
	if ((val>=0xE0002) && (val<=0xE001F)) return BN;	//# Cn  [30] <reserved-E0002>..<reserved-E001F>
	if ((val>=0xE0020) && (val<=0xE007F)) return BN;	//# Cf  [96] TAG SPACE..CANCEL TAG
	if ((val>=0xE0080) && (val<=0xE00FF)) return BN;	//# Cn [128] <reserved-E0080>..<reserved-E00FF>
	if ((val>=0xE01F0) && (val<=0xE0FFF)) return BN;	//# Cn [3600] <reserved-E01F0>..<reserved-E0FFF>
	if ((val>=0xEFFFE) && (val<=0xEFFFF)) return BN;	//# Cn   [2] <noncharacter-EFFFE>..<noncharacter-EFFFF>
	if ((val>=0xFFFFE) && (val<=0xFFFFF)) return BN;	//# Cn   [2] <noncharacter-FFFFE>..<noncharacter-FFFFF>
	if ((val>=0x10FFFE) && (val<=0x10FFFF)) return BN;	//# Cn   [2] <noncharacter-10FFFE>..<noncharacter-10FFFF>
	if ((val>=0x0300) && (val<=0x036F)) return NSM;	//# Mn [112] COMBINING GRAVE ACCENT..COMBINING LATIN SMALL LETTER X
	if ((val>=0x0483) && (val<=0x0486)) return NSM;	//# Mn   [4] COMBINING CYRILLIC TITLO..COMBINING CYRILLIC PSILI PNEUMATA
	if ((val>=0x0488) && (val<=0x0489)) return NSM;	//# Me   [2] COMBINING CYRILLIC HUNDRED THOUSANDS SIGN..COMBINING CYRILLIC MILLIONS SIGN
	if ((val>=0x0591) && (val<=0x05BD)) return NSM;	//# Mn  [45] HEBREW ACCENT ETNAHTA..HEBREW POINT METEG
	if (val==0x000005BF) return NSM;	//# Mn       HEBREW POINT RAFE
	if ((val>=0x05C1) && (val<=0x05C2)) return NSM;	//# Mn   [2] HEBREW POINT SHIN DOT..HEBREW POINT SIN DOT
	if ((val>=0x05C4) && (val<=0x05C5)) return NSM;	//# Mn   [2] HEBREW MARK UPPER DOT..HEBREW MARK LOWER DOT
	if (val==0x000005C7) return NSM;	//# Mn       HEBREW POINT QAMATS QATAN
	if ((val>=0x0610) && (val<=0x0615)) return NSM;	//# Mn   [6] ARABIC SIGN SALLALLAHOU ALAYHE WASSALLAM..ARABIC SMALL HIGH TAH
	if ((val>=0x064B) && (val<=0x065E)) return NSM;	//# Mn  [20] ARABIC FATHATAN..ARABIC FATHA WITH TWO DOTS
	if (val==0x00000670) return NSM;	//# Mn       ARABIC LETTER SUPERSCRIPT ALEF
	if ((val>=0x06D6) && (val<=0x06DC)) return NSM;	//# Mn   [7] ARABIC SMALL HIGH LIGATURE SAD WITH LAM WITH ALEF MAKSURA..ARABIC SMALL HIGH SEEN
	if (val==0x000006DE) return NSM;	//# Me       ARABIC START OF RUB EL HIZB
	if ((val>=0x06DF) && (val<=0x06E4)) return NSM;	//# Mn   [6] ARABIC SMALL HIGH ROUNDED ZERO..ARABIC SMALL HIGH MADDA
	if ((val>=0x06E7) && (val<=0x06E8)) return NSM;	//# Mn   [2] ARABIC SMALL HIGH YEH..ARABIC SMALL HIGH NOON
	if ((val>=0x06EA) && (val<=0x06ED)) return NSM;	//# Mn   [4] ARABIC EMPTY CENTRE LOW STOP..ARABIC SMALL LOW MEEM
	if (val==0x00000711) return NSM;	//# Mn       SYRIAC LETTER SUPERSCRIPT ALAPH
	if ((val>=0x0730) && (val<=0x074A)) return NSM;	//# Mn  [27] SYRIAC PTHAHA ABOVE..SYRIAC BARREKH
	if ((val>=0x07A6) && (val<=0x07B0)) return NSM;	//# Mn  [11] THAANA ABAFILI..THAANA SUKUN
	if ((val>=0x07EB) && (val<=0x07F3)) return NSM;	//# Mn   [9] NKO COMBINING SHORT HIGH TONE..NKO COMBINING DOUBLE DOT ABOVE
	if ((val>=0x0901) && (val<=0x0902)) return NSM;	//# Mn   [2] DEVANAGARI SIGN CANDRABINDU..DEVANAGARI SIGN ANUSVARA
	if (val==0x0000093C) return NSM;	//# Mn       DEVANAGARI SIGN NUKTA
	if ((val>=0x0941) && (val<=0x0948)) return NSM;	//# Mn   [8] DEVANAGARI VOWEL SIGN U..DEVANAGARI VOWEL SIGN AI
	if (val==0x0000094D) return NSM;	//# Mn       DEVANAGARI SIGN VIRAMA
	if ((val>=0x0951) && (val<=0x0954)) return NSM;	//# Mn   [4] DEVANAGARI STRESS SIGN UDATTA..DEVANAGARI ACUTE ACCENT
	if ((val>=0x0962) && (val<=0x0963)) return NSM;	//# Mn   [2] DEVANAGARI VOWEL SIGN VOCALIC L..DEVANAGARI VOWEL SIGN VOCALIC LL
	if (val==0x00000981) return NSM;	//# Mn       BENGALI SIGN CANDRABINDU
	if (val==0x000009BC) return NSM;	//# Mn       BENGALI SIGN NUKTA
	if ((val>=0x09C1) && (val<=0x09C4)) return NSM;	//# Mn   [4] BENGALI VOWEL SIGN U..BENGALI VOWEL SIGN VOCALIC RR
	if (val==0x000009CD) return NSM;	//# Mn       BENGALI SIGN VIRAMA
	if ((val>=0x09E2) && (val<=0x09E3)) return NSM;	//# Mn   [2] BENGALI VOWEL SIGN VOCALIC L..BENGALI VOWEL SIGN VOCALIC LL
	if ((val>=0x0A01) && (val<=0x0A02)) return NSM;	//# Mn   [2] GURMUKHI SIGN ADAK BINDI..GURMUKHI SIGN BINDI
	if (val==0x00000A3C) return NSM;	//# Mn       GURMUKHI SIGN NUKTA
	if ((val>=0x0A41) && (val<=0x0A42)) return NSM;	//# Mn   [2] GURMUKHI VOWEL SIGN U..GURMUKHI VOWEL SIGN UU
	if ((val>=0x0A47) && (val<=0x0A48)) return NSM;	//# Mn   [2] GURMUKHI VOWEL SIGN EE..GURMUKHI VOWEL SIGN AI
	if ((val>=0x0A4B) && (val<=0x0A4D)) return NSM;	//# Mn   [3] GURMUKHI VOWEL SIGN OO..GURMUKHI SIGN VIRAMA
	if ((val>=0x0A70) && (val<=0x0A71)) return NSM;	//# Mn   [2] GURMUKHI TIPPI..GURMUKHI ADDAK
	if ((val>=0x0A81) && (val<=0x0A82)) return NSM;	//# Mn   [2] GUJARATI SIGN CANDRABINDU..GUJARATI SIGN ANUSVARA
	if (val==0x00000ABC) return NSM;	//# Mn       GUJARATI SIGN NUKTA
	if ((val>=0x0AC1) && (val<=0x0AC5)) return NSM;	//# Mn   [5] GUJARATI VOWEL SIGN U..GUJARATI VOWEL SIGN CANDRA E
	if ((val>=0x0AC7) && (val<=0x0AC8)) return NSM;	//# Mn   [2] GUJARATI VOWEL SIGN E..GUJARATI VOWEL SIGN AI
	if (val==0x00000ACD) return NSM;	//# Mn       GUJARATI SIGN VIRAMA
	if ((val>=0x0AE2) && (val<=0x0AE3)) return NSM;	//# Mn   [2] GUJARATI VOWEL SIGN VOCALIC L..GUJARATI VOWEL SIGN VOCALIC LL
	if (val==0x00000B01) return NSM;	//# Mn       ORIYA SIGN CANDRABINDU
	if (val==0x00000B3C) return NSM;	//# Mn       ORIYA SIGN NUKTA
	if (val==0x00000B3F) return NSM;	//# Mn       ORIYA VOWEL SIGN I
	if ((val>=0x0B41) && (val<=0x0B43)) return NSM;	//# Mn   [3] ORIYA VOWEL SIGN U..ORIYA VOWEL SIGN VOCALIC R
	if (val==0x00000B4D) return NSM;	//# Mn       ORIYA SIGN VIRAMA
	if (val==0x00000B56) return NSM;	//# Mn       ORIYA AI LENGTH MARK
	if (val==0x00000B82) return NSM;	//# Mn       TAMIL SIGN ANUSVARA
	if (val==0x00000BC0) return NSM;	//# Mn       TAMIL VOWEL SIGN II
	if (val==0x00000BCD) return NSM;	//# Mn       TAMIL SIGN VIRAMA
	if ((val>=0x0C3E) && (val<=0x0C40)) return NSM;	//# Mn   [3] TELUGU VOWEL SIGN AA..TELUGU VOWEL SIGN II
	if ((val>=0x0C46) && (val<=0x0C48)) return NSM;	//# Mn   [3] TELUGU VOWEL SIGN E..TELUGU VOWEL SIGN AI
	if ((val>=0x0C4A) && (val<=0x0C4D)) return NSM;	//# Mn   [4] TELUGU VOWEL SIGN O..TELUGU SIGN VIRAMA
	if ((val>=0x0C55) && (val<=0x0C56)) return NSM;	//# Mn   [2] TELUGU LENGTH MARK..TELUGU AI LENGTH MARK
	if (val==0x00000CBC) return NSM;	//# Mn       KANNADA SIGN NUKTA
	if ((val>=0x0CCC) && (val<=0x0CCD)) return NSM;	//# Mn   [2] KANNADA VOWEL SIGN AU..KANNADA SIGN VIRAMA
	if ((val>=0x0CE2) && (val<=0x0CE3)) return NSM;	//# Mn   [2] KANNADA VOWEL SIGN VOCALIC L..KANNADA VOWEL SIGN VOCALIC LL
	if ((val>=0x0D41) && (val<=0x0D43)) return NSM;	//# Mn   [3] MALAYALAM VOWEL SIGN U..MALAYALAM VOWEL SIGN VOCALIC R
	if (val==0x00000D4D) return NSM;	//# Mn       MALAYALAM SIGN VIRAMA
	if (val==0x00000DCA) return NSM;	//# Mn       SINHALA SIGN AL-LAKUNA
	if ((val>=0x0DD2) && (val<=0x0DD4)) return NSM;	//# Mn   [3] SINHALA VOWEL SIGN KETTI IS-PILLA..SINHALA VOWEL SIGN KETTI PAA-PILLA
	if (val==0x00000DD6) return NSM;	//# Mn       SINHALA VOWEL SIGN DIGA PAA-PILLA
	if (val==0x00000E31) return NSM;	//# Mn       THAI CHARACTER MAI HAN-AKAT
	if ((val>=0x0E34) && (val<=0x0E3A)) return NSM;	//# Mn   [7] THAI CHARACTER SARA I..THAI CHARACTER PHINTHU
	if ((val>=0x0E47) && (val<=0x0E4E)) return NSM;	//# Mn   [8] THAI CHARACTER MAITAIKHU..THAI CHARACTER YAMAKKAN
	if (val==0x00000EB1) return NSM;	//# Mn       LAO VOWEL SIGN MAI KAN
	if ((val>=0x0EB4) && (val<=0x0EB9)) return NSM;	//# Mn   [6] LAO VOWEL SIGN I..LAO VOWEL SIGN UU
	if ((val>=0x0EBB) && (val<=0x0EBC)) return NSM;	//# Mn   [2] LAO VOWEL SIGN MAI KON..LAO SEMIVOWEL SIGN LO
	if ((val>=0x0EC8) && (val<=0x0ECD)) return NSM;	//# Mn   [6] LAO TONE MAI EK..LAO NIGGAHITA
	if ((val>=0x0F18) && (val<=0x0F19)) return NSM;	//# Mn   [2] TIBETAN ASTROLOGICAL SIGN -KHYUD PA..TIBETAN ASTROLOGICAL SIGN SDONG TSHUGS
	if (val==0x00000F35) return NSM;	//# Mn       TIBETAN MARK NGAS BZUNG NYI ZLA
	if (val==0x00000F37) return NSM;	//# Mn       TIBETAN MARK NGAS BZUNG SGOR RTAGS
	if (val==0x00000F39) return NSM;	//# Mn       TIBETAN MARK TSA -PHRU
	if ((val>=0x0F71) && (val<=0x0F7E)) return NSM;	//# Mn  [14] TIBETAN VOWEL SIGN AA..TIBETAN SIGN RJES SU NGA RO
	if ((val>=0x0F80) && (val<=0x0F84)) return NSM;	//# Mn   [5] TIBETAN VOWEL SIGN REVERSED I..TIBETAN MARK HALANTA
	if ((val>=0x0F86) && (val<=0x0F87)) return NSM;	//# Mn   [2] TIBETAN SIGN LCI RTAGS..TIBETAN SIGN YANG RTAGS
	if ((val>=0x0F90) && (val<=0x0F97)) return NSM;	//# Mn   [8] TIBETAN SUBJOINED LETTER KA..TIBETAN SUBJOINED LETTER JA
	if ((val>=0x0F99) && (val<=0x0FBC)) return NSM;	//# Mn  [36] TIBETAN SUBJOINED LETTER NYA..TIBETAN SUBJOINED LETTER FIXED-FORM RA
	if (val==0x00000FC6) return NSM;	//# Mn       TIBETAN SYMBOL PADMA GDAN
	if ((val>=0x102D) && (val<=0x1030)) return NSM;	//# Mn   [4] MYANMAR VOWEL SIGN I..MYANMAR VOWEL SIGN UU
	if (val==0x00001032) return NSM;	//# Mn       MYANMAR VOWEL SIGN AI
	if ((val>=0x1036) && (val<=0x1037)) return NSM;	//# Mn   [2] MYANMAR SIGN ANUSVARA..MYANMAR SIGN DOT BELOW
	if (val==0x00001039) return NSM;	//# Mn       MYANMAR SIGN VIRAMA
	if ((val>=0x1058) && (val<=0x1059)) return NSM;	//# Mn   [2] MYANMAR VOWEL SIGN VOCALIC L..MYANMAR VOWEL SIGN VOCALIC LL
	if (val==0x0000135F) return NSM;	//# Mn       ETHIOPIC COMBINING GEMINATION MARK
	if ((val>=0x1712) && (val<=0x1714)) return NSM;	//# Mn   [3] TAGALOG VOWEL SIGN I..TAGALOG SIGN VIRAMA
	if ((val>=0x1732) && (val<=0x1734)) return NSM;	//# Mn   [3] HANUNOO VOWEL SIGN I..HANUNOO SIGN PAMUDPOD
	if ((val>=0x1752) && (val<=0x1753)) return NSM;	//# Mn   [2] BUHID VOWEL SIGN I..BUHID VOWEL SIGN U
	if ((val>=0x1772) && (val<=0x1773)) return NSM;	//# Mn   [2] TAGBANWA VOWEL SIGN I..TAGBANWA VOWEL SIGN U
	if ((val>=0x17B7) && (val<=0x17BD)) return NSM;	//# Mn   [7] KHMER VOWEL SIGN I..KHMER VOWEL SIGN UA
	if (val==0x000017C6) return NSM;	//# Mn       KHMER SIGN NIKAHIT
	if ((val>=0x17C9) && (val<=0x17D3)) return NSM;	//# Mn  [11] KHMER SIGN MUUSIKATOAN..KHMER SIGN BATHAMASAT
	if (val==0x000017DD) return NSM;	//# Mn       KHMER SIGN ATTHACAN
	if ((val>=0x180B) && (val<=0x180D)) return NSM;	//# Mn   [3] MONGOLIAN FREE VARIATION SELECTOR ONE..MONGOLIAN FREE VARIATION SELECTOR THREE
	if (val==0x000018A9) return NSM;	//# Mn       MONGOLIAN LETTER ALI GALI DAGALGA
	if ((val>=0x1920) && (val<=0x1922)) return NSM;	//# Mn   [3] LIMBU VOWEL SIGN A..LIMBU VOWEL SIGN U
	if ((val>=0x1927) && (val<=0x1928)) return NSM;	//# Mn   [2] LIMBU VOWEL SIGN E..LIMBU VOWEL SIGN O
	if ((val>=0x1929) && (val<=0x192B)) return NSM;	//# Mc   [3] LIMBU SUBJOINED LETTER YA..LIMBU SUBJOINED LETTER WA
	if (val==0x00001932) return NSM;	//# Mn       LIMBU SMALL LETTER ANUSVARA
	if ((val>=0x1939) && (val<=0x193B)) return NSM;	//# Mn   [3] LIMBU SIGN MUKPHRENG..LIMBU SIGN SA-I
	if ((val>=0x1A17) && (val<=0x1A18)) return NSM;	//# Mn   [2] BUGINESE VOWEL SIGN I..BUGINESE VOWEL SIGN U
	if ((val>=0x1B00) && (val<=0x1B03)) return NSM;	//# Mn   [4] BALINESE SIGN ULU RICEM..BALINESE SIGN SURANG
	if (val==0x00001B34) return NSM;	//# Mn       BALINESE SIGN REREKAN
	if ((val>=0x1B36) && (val<=0x1B3A)) return NSM;	//# Mn   [5] BALINESE VOWEL SIGN ULU..BALINESE VOWEL SIGN RA REPA
	if (val==0x00001B3C) return NSM;	//# Mn       BALINESE VOWEL SIGN LA LENGA
	if (val==0x00001B42) return NSM;	//# Mn       BALINESE VOWEL SIGN PEPET
	if ((val>=0x1B6B) && (val<=0x1B73)) return NSM;	//# Mn   [9] BALINESE MUSICAL SYMBOL COMBINING TEGEH..BALINESE MUSICAL SYMBOL COMBINING GONG
	if ((val>=0x1DC0) && (val<=0x1DCA)) return NSM;	//# Mn  [11] COMBINING DOTTED GRAVE ACCENT..COMBINING LATIN SMALL LETTER R BELOW
	if ((val>=0x1DFE) && (val<=0x1DFF)) return NSM;	//# Mn   [2] COMBINING LEFT ARROWHEAD ABOVE..COMBINING RIGHT ARROWHEAD AND DOWN ARROWHEAD BELOW
	if ((val>=0x20D0) && (val<=0x20DC)) return NSM;	//# Mn  [13] COMBINING LEFT HARPOON ABOVE..COMBINING FOUR DOTS ABOVE
	if ((val>=0x20DD) && (val<=0x20E0)) return NSM;	//# Me   [4] COMBINING ENCLOSING CIRCLE..COMBINING ENCLOSING CIRCLE BACKSLASH
	if (val==0x000020E1) return NSM;	//# Mn       COMBINING LEFT RIGHT ARROW ABOVE
	if ((val>=0x20E2) && (val<=0x20E4)) return NSM;	//# Me   [3] COMBINING ENCLOSING SCREEN..COMBINING ENCLOSING UPWARD POINTING TRIANGLE
	if ((val>=0x20E5) && (val<=0x20EF)) return NSM;	//# Mn  [11] COMBINING REVERSE SOLIDUS OVERLAY..COMBINING RIGHT ARROW BELOW
	if ((val>=0x302A) && (val<=0x302F)) return NSM;	//# Mn   [6] IDEOGRAPHIC LEVEL TONE MARK..HANGUL DOUBLE DOT TONE MARK
	if ((val>=0x3099) && (val<=0x309A)) return NSM;	//# Mn   [2] COMBINING KATAKANA-HIRAGANA VOICED SOUND MARK..COMBINING KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
	if (val==0x0000A802) return NSM;	//# Mc       SYLOTI NAGRI SIGN DVISVARA
	if (val==0x0000A806) return NSM;	//# Mn       SYLOTI NAGRI SIGN HASANTA
	if (val==0x0000A80B) return NSM;	//# Mn       SYLOTI NAGRI SIGN ANUSVARA
	if ((val>=0xA825) && (val<=0xA826)) return NSM;	//# Mn   [2] SYLOTI NAGRI VOWEL SIGN U..SYLOTI NAGRI VOWEL SIGN E
	if (val==0x0000FB1E) return NSM;	//# Mn       HEBREW POINT JUDEO-SPANISH VARIKA
	if ((val>=0xFE00) && (val<=0xFE0F)) return NSM;	//# Mn  [16] VARIATION SELECTOR-1..VARIATION SELECTOR-16
	if ((val>=0xFE20) && (val<=0xFE23)) return NSM;	//# Mn   [4] COMBINING LIGATURE LEFT HALF..COMBINING DOUBLE TILDE RIGHT HALF
	if ((val>=0x10A01) && (val<=0x10A03)) return NSM;	//# Mn   [3] KHAROSHTHI VOWEL SIGN I..KHAROSHTHI VOWEL SIGN VOCALIC R
	if ((val>=0x10A05) && (val<=0x10A06)) return NSM;	//# Mn   [2] KHAROSHTHI VOWEL SIGN E..KHAROSHTHI VOWEL SIGN O
	if ((val>=0x10A0C) && (val<=0x10A0F)) return NSM;	//# Mn   [4] KHAROSHTHI VOWEL LENGTH MARK..KHAROSHTHI SIGN VISARGA
	if ((val>=0x10A38) && (val<=0x10A3A)) return NSM;	//# Mn   [3] KHAROSHTHI SIGN BAR ABOVE..KHAROSHTHI SIGN DOT BELOW
	if (val==0x00010A3F) return NSM;	//# Mn       KHAROSHTHI VIRAMA
	if ((val>=0x1D167) && (val<=0x1D169)) return NSM;	//# Mn   [3] MUSICAL SYMBOL COMBINING TREMOLO-1..MUSICAL SYMBOL COMBINING TREMOLO-3
	if ((val>=0x1D17B) && (val<=0x1D182)) return NSM;	//# Mn   [8] MUSICAL SYMBOL COMBINING ACCENT..MUSICAL SYMBOL COMBINING LOURE
	if ((val>=0x1D185) && (val<=0x1D18B)) return NSM;	//# Mn   [7] MUSICAL SYMBOL COMBINING DOIT..MUSICAL SYMBOL COMBINING TRIPLE TONGUE
	if ((val>=0x1D1AA) && (val<=0x1D1AD)) return NSM;	//# Mn   [4] MUSICAL SYMBOL COMBINING DOWN BOW..MUSICAL SYMBOL COMBINING SNAP PIZZICATO
	if ((val>=0x1D242) && (val<=0x1D244)) return NSM;	//# Mn   [3] COMBINING GREEK MUSICAL TRISEME..COMBINING GREEK MUSICAL PENTASEME
	if ((val>=0xE0100) && (val<=0xE01EF)) return NSM;	//# Mn [240] VARIATION SELECTOR-17..VARIATION SELECTOR-256
	if ((val>=0x0600) && (val<=0x0603)) return AL;	//# Cf   [4] ARABIC NUMBER SIGN..ARABIC SIGN SAFHA
	if ((val>=0x0604) && (val<=0x060A)) return AL;	//# Cn   [7] <reserved-0604>..<reserved-060A>
	if (val==0x0000060B) return AL;	//# Sc       AFGHANI SIGN
	if (val==0x0000060D) return AL;	//# Po       ARABIC DATE SEPARATOR
	if ((val>=0x0616) && (val<=0x061A)) return AL;	//# Cn   [5] <reserved-0616>..<reserved-061A>
	if (val==0x0000061B) return AL;	//# Po       ARABIC SEMICOLON
	if ((val>=0x061C) && (val<=0x061D)) return AL;	//# Cn   [2] <reserved-061C>..<reserved-061D>
	if ((val>=0x061E) && (val<=0x061F)) return AL;	//# Po   [2] ARABIC TRIPLE DOT PUNCTUATION MARK..ARABIC QUESTION MARK
	if (val==0x00000620) return AL;	//# Cn       <reserved-0620>
	if ((val>=0x0621) && (val<=0x063A)) return AL;	//# Lo  [26] ARABIC LETTER HAMZA..ARABIC LETTER GHAIN
	if ((val>=0x063B) && (val<=0x063F)) return AL;	//# Cn   [5] <reserved-063B>..<reserved-063F>
	if (val==0x00000640) return AL;	//# Lm       ARABIC TATWEEL
	if ((val>=0x0641) && (val<=0x064A)) return AL;	//# Lo  [10] ARABIC LETTER FEH..ARABIC LETTER YEH
	if (val==0x0000065F) return AL;	//# Cn       <reserved-065F>
	if (val==0x0000066D) return AL;	//# Po       ARABIC FIVE POINTED STAR
	if ((val>=0x066E) && (val<=0x066F)) return AL;	//# Lo   [2] ARABIC LETTER DOTLESS BEH..ARABIC LETTER DOTLESS QAF
	if ((val>=0x0671) && (val<=0x06D3)) return AL;	//# Lo  [99] ARABIC LETTER ALEF WASLA..ARABIC LETTER YEH BARREE WITH HAMZA ABOVE
	if (val==0x000006D4) return AL;	//# Po       ARABIC FULL STOP
	if (val==0x000006D5) return AL;	//# Lo       ARABIC LETTER AE
	if (val==0x000006DD) return AL;	//# Cf       ARABIC END OF AYAH
	if ((val>=0x06E5) && (val<=0x06E6)) return AL;	//# Lm   [2] ARABIC SMALL WAW..ARABIC SMALL YEH
	if ((val>=0x06EE) && (val<=0x06EF)) return AL;	//# Lo   [2] ARABIC LETTER DAL WITH INVERTED V..ARABIC LETTER REH WITH INVERTED V
	if ((val>=0x06FA) && (val<=0x06FC)) return AL;	//# Lo   [3] ARABIC LETTER SHEEN WITH DOT BELOW..ARABIC LETTER GHAIN WITH DOT BELOW
	if ((val>=0x06FD) && (val<=0x06FE)) return AL;	//# So   [2] ARABIC SIGN SINDHI AMPERSAND..ARABIC SIGN SINDHI POSTPOSITION MEN
	if (val==0x000006FF) return AL;	//# Lo       ARABIC LETTER HEH WITH INVERTED V
	if ((val>=0x0700) && (val<=0x070D)) return AL;	//# Po  [14] SYRIAC END OF PARAGRAPH..SYRIAC HARKLEAN ASTERISCUS
	if (val==0x0000070E) return AL;	//# Cn       <reserved-070E>
	if (val==0x00000710) return AL;	//# Lo       SYRIAC LETTER ALAPH
	if ((val>=0x0712) && (val<=0x072F)) return AL;	//# Lo  [30] SYRIAC LETTER BETH..SYRIAC LETTER PERSIAN DHALATH
	if ((val>=0x074B) && (val<=0x074C)) return AL;	//# Cn   [2] <reserved-074B>..<reserved-074C>
	if ((val>=0x074D) && (val<=0x076D)) return AL;	//# Lo  [33] SYRIAC LETTER SOGDIAN ZHAIN..ARABIC LETTER SEEN WITH TWO DOTS VERTICALLY ABOVE
	if ((val>=0x076E) && (val<=0x077F)) return AL;	//# Cn  [18] <reserved-076E>..<reserved-077F>
	if ((val>=0x0780) && (val<=0x07A5)) return AL;	//# Lo  [38] THAANA LETTER HAA..THAANA LETTER WAAVU
	if (val==0x000007B1) return AL;	//# Lo       THAANA LETTER NAA
	if ((val>=0x07B2) && (val<=0x07BF)) return AL;	//# Cn  [14] <reserved-07B2>..<reserved-07BF>
	if ((val>=0xFB50) && (val<=0xFBB1)) return AL;	//# Lo  [98] ARABIC LETTER ALEF WASLA ISOLATED FORM..ARABIC LETTER YEH BARREE WITH HAMZA ABOVE FINAL FORM
	if ((val>=0xFBB2) && (val<=0xFBD2)) return AL;	//# Cn  [33] <reserved-FBB2>..<reserved-FBD2>
	if ((val>=0xFBD3) && (val<=0xFD3D)) return AL;	//# Lo [363] ARABIC LETTER NG ISOLATED FORM..ARABIC LIGATURE ALEF WITH FATHATAN ISOLATED FORM
	if ((val>=0xFD40) && (val<=0xFD4F)) return AL;	//# Cn  [16] <reserved-FD40>..<reserved-FD4F>
	if ((val>=0xFD50) && (val<=0xFD8F)) return AL;	//# Lo  [64] ARABIC LIGATURE TEH WITH JEEM WITH MEEM INITIAL FORM..ARABIC LIGATURE MEEM WITH KHAH WITH MEEM INITIAL FORM
	if ((val>=0xFD90) && (val<=0xFD91)) return AL;	//# Cn   [2] <reserved-FD90>..<reserved-FD91>
	if ((val>=0xFD92) && (val<=0xFDC7)) return AL;	//# Lo  [54] ARABIC LIGATURE MEEM WITH JEEM WITH KHAH INITIAL FORM..ARABIC LIGATURE NOON WITH JEEM WITH YEH FINAL FORM
	if ((val>=0xFDC8) && (val<=0xFDCF)) return AL;	//# Cn   [8] <reserved-FDC8>..<reserved-FDCF>
	if ((val>=0xFDF0) && (val<=0xFDFB)) return AL;	//# Lo  [12] ARABIC LIGATURE SALLA USED AS KORANIC STOP SIGN ISOLATED FORM..ARABIC LIGATURE JALLAJALALOUHOU
	if (val==0x0000FDFC) return AL;	//# Sc       RIAL SIGN
	if ((val>=0xFDFE) && (val<=0xFDFF)) return AL;	//# Cn   [2] <reserved-FDFE>..<reserved-FDFF>
	if ((val>=0xFE70) && (val<=0xFE74)) return AL;	//# Lo   [5] ARABIC FATHATAN ISOLATED FORM..ARABIC KASRATAN ISOLATED FORM
	if (val==0x0000FE75) return AL;	//# Cn       <reserved-FE75>
	if ((val>=0xFE76) && (val<=0xFEFC)) return AL;	//# Lo [135] ARABIC FATHA ISOLATED FORM..ARABIC LIGATURE LAM WITH ALEF FINAL FORM
	if ((val>=0xFEFD) && (val<=0xFEFE)) return AL;	//# Cn   [2] <reserved-FEFD>..<reserved-FEFE>
	if (val==0x0000202D) return LRO;	//# Cf       LEFT-TO-RIGHT OVERRIDE
	if (val==0x0000202E) return RLO;	//# Cf       RIGHT-TO-LEFT OVERRIDE
	if (val==0x0000202A) return LRE;	//# Cf       LEFT-TO-RIGHT EMBEDDING
	if (val==0x0000202B) return RLE;	//# Cf       RIGHT-TO-LEFT EMBEDDING
	if (val==0x0000202C) return PDF;	//# Cf       POP DIRECTIONAL FORMATTING
	return L;
}

#endif //!defined(GPAC_DISABLE_SVG) && !defined(GPAC_DISABLE_COMPOSITOR)
