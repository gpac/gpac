/*
 *			GPAC - Multimedia Framework C SDK
 *
 *    Copyright (c)2006-201X ENST - All rights reserved
 *
 *  This file is part of GPAC / MPEG2-TS sub-project
 *
 *  GPAC is gf_free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the gf_free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  GPAC is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; see the file COPYING.  If not, write to
 *  the gf_free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */


#include <gpac/tools.h>
#include <time.h>


/* decodes an Modified Julian Date (MJD) into a Co-ordinated Universal Time (UTC)
See annex C of DVB-SI ETSI EN 300468 */
void dvb_decode_mjd_date(u32 date, u16 *year, u8 *month, u8 *day)
{
    u32 yp, mp, k;
    yp = (u32)((date - 15078.2)/365.25);
    mp = (u32)((date - 14956.1 - (u32)(yp * 365.25))/30.6001);
    *day = (u32)(date - 14956 - (u32)(yp * 365.25) - (u32)(mp * 30.6001));
    if (mp == 14 || mp == 15) k = 1;
    else k = 0;
    *year = yp + k + 1900;
    *month = mp - 1 - k*12;
	assert(*year>=1900 && *year<=2100 && *month && *month<=12 && *day && *day<=31);
}

#if 0 /*disabled since mktime doesn't exist on Windows Mobile*/
/* decodes an Modified Julian Date (MJD) into a unix time (i.e. seconds since Jan 1st 1970) */
void dvb_decode_mjd_to_unix_time(u8 *data, time_t *unix_time) {
    struct tm time;
    char tmp_time[10];
    u16 year = (u16)time.tm_year;
    u8 month = (u8) time.tm_mon;
    u8 day   = (u8) time.tm_mday;

    memset(&time, 0, sizeof(struct tm));
    dvb_decode_mjd_date((data[0] << 8) | data[1], &year, &month, &day);
    time.tm_year = year - 1900;
    time.tm_mon = month-1; /* months are 0-based in time_t */
    time.tm_mday = day;
    time.tm_isdst = -1; /* we don't want to apply Daylight Saving Time */

    sprintf(tmp_time, "%02x", data[2]);
    time.tm_hour = atoi(tmp_time);
    sprintf(tmp_time, "%02x", data[3]);
    time.tm_min = atoi(tmp_time);
    sprintf(tmp_time, "%02x", data[4]);
    time.tm_sec = atoi(tmp_time);
    *unix_time = mktime(&time);
}
#endif
