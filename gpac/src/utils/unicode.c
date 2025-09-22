#include <gpac/utf.h>
/**
 * This code has been adapted from http://www.ietf.org/rfc/rfc2640.txt
 * Full Copyright Statement

   Copyright (C) The Internet Society (1999).  All Rights Reserved.

   This document and translations of it may be copied and furnished to
   others, and derivative works that comment on or otherwise explain it
   or assist in its implementation may be prepared, copied, published
   and distributed, in whole or in part, without restriction of any
   kind, provided that the above copyright notice and this paragraph are
   included on all such copies and derivative works.  However, this
   document itself may not be modified in any way, such as by removing
   the copyright notice or references to the Internet Society or other
   Internet organizations, except as needed for the purpose of
   developing Internet standards in which case the procedures for
   copyrights defined in the Internet Standards process must be
   followed, or as required to translate it into languages other than
   English.

   The limited permissions granted above are perpetual and will not be
   revoked by the Internet Society or its successors or assigns.

   This document and the information contained herein is provided on an
   "AS IS" basis and THE INTERNET SOCIETY AND THE INTERNET ENGINEERING
   TASK FORCE DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, INCLUDING
   BUT NOT LIMITED TO ANY WARRANTY THAT THE USE OF THE INFORMATION
   HEREIN WILL NOT INFRINGE ANY RIGHTS OR ANY IMPLIED WARRANTIES OF
   MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.

Acknowledgement

   Funding for the RFC Editor function is currently provided by the
   Internet Society.
 */

GF_EXPORT
u32 utf8_to_ucs4(u32 * ucs4_buf, u32 utf8_len, unsigned char *utf8_buf)
{
	if (!ucs4_buf || !utf8_buf) return 0;
	const unsigned char *utf8_endbuf = utf8_buf + utf8_len;
	u32             ucs_len = 0;

	while (utf8_buf != utf8_endbuf) {

		if ((*utf8_buf & 0x80) == 0x00) {
			/* ASCII chars no conversion needed */
			*ucs4_buf++ = (u32) * utf8_buf;
			utf8_buf++;
			ucs_len++;
		} else if ((*utf8_buf & 0xE0) == 0xC0)
			//In the 2 byte utf - 8 range
		{
			*ucs4_buf++ = (u32) (((*utf8_buf - 0xC0) * 0x40)
			                     + (*(utf8_buf + 1) - 0x80));
			utf8_buf += 2;
			ucs_len++;
		} else if ((*utf8_buf & 0xF0) == 0xE0) {
			/* In the 3 byte utf-8 range */
			*ucs4_buf++ = (u32) (((*utf8_buf - 0xE0) * 0x1000)
			                     + ((*(utf8_buf + 1) - 0x80) * 0x40)
			                     + (*(utf8_buf + 2) - 0x80));

			utf8_buf += 3;
			ucs_len++;
		} else if ((*utf8_buf & 0xF8) == 0xF0) {
			/* In the 4 byte utf-8 range */
			*ucs4_buf++ = (u32)
			              (((*utf8_buf - 0xF0) * 0x040000)
			               + ((*(utf8_buf + 1) - 0x80) * 0x1000)
			               + ((*(utf8_buf + 2) - 0x80) * 0x40)
			               + (*(utf8_buf + 3) - 0x80));
			utf8_buf += 4;
			ucs_len++;
		} else if ((*utf8_buf & 0xFC) == 0xF8) {
			/* In the 5 byte utf-8 range */
			*ucs4_buf++ = (u32)
			              (((*utf8_buf - 0xF8) * 0x01000000)
			               + ((*(utf8_buf + 1) - 0x80) * 0x040000)
			               + ((*(utf8_buf + 2) - 0x80) * 0x1000)
			               + ((*(utf8_buf + 3) - 0x80) * 0x40)
			               + (*(utf8_buf + 4) - 0x80));
			utf8_buf += 5;
			ucs_len++;
		} else if ((*utf8_buf & 0xFE) == 0xFC) {
			/* In the 6 byte utf-8  range */
			*ucs4_buf++ = (u32)
			              (((*utf8_buf - 0xFC) * 0x40000000)
			               + ((*(utf8_buf + 1) - 0x80) * 0x010000000)
			               + ((*(utf8_buf + 2) - 0x80) * 0x040000)
			               + ((*(utf8_buf + 3) - 0x80) * 0x1000)
			               + ((*(utf8_buf + 4) - 0x80) * 0x40)
			               + (*(utf8_buf + 5) - 0x80));
			utf8_buf += 6;
			ucs_len++;
		} else {
			return 0;
		}
	}
	return (ucs_len);
}

