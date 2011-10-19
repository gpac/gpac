/*
 * (c) 2005, 2008 Nico Pranke <Nico.Pranke@googlemail.com>, Robin Luedtke <RobinLu@gmx.de> 
 *
 * This file is part of avcap.
 *
 * avcap is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * avcap is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with avcap.  If not, see <http://www.gnu.org/licenses/>.
 */

/* avcap is free for non-commercial use.
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>.
 */


#ifndef FORMATNAMES_H_
#define FORMATNAMES_H_

#include <string>
#include <uuids.h>

void GetVideoFormatName(const GUID* guid, std::string& FormatName)
{
	// list creation date:05/27/2005 - taken from DirectShow documentation

	// Uncompressed RGB video subtypes
	if (*guid==MEDIASUBTYPE_ARGB1555) {
		FormatName="RGB 555 with alpha channel";
		return;
	}
	if (*guid==MEDIASUBTYPE_ARGB4444) {
		FormatName="16-bit RGB with alpha channel; 4 bits per channel";
		return;
	}
	if (*guid==MEDIASUBTYPE_ARGB32) {
		FormatName="RGB 32 with alpha channel";
		return;
	}
	if (*guid==MEDIASUBTYPE_A2R10G10B10) {
		FormatName
				="32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha";
		return;
	}
	if (*guid==MEDIASUBTYPE_A2B10G10R10) {
		FormatName
				="32-bit RGB with alpha channel; 10 bits per RGB channel plus 2 bits for alpha";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB1) {
		FormatName="RGB, 1 bit per pixel (bpp), palettized";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB4) {
		FormatName="RGB, 4 bpp, palettized";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB8) {
		FormatName="RGB, 8 bpp";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB555) {
		FormatName="RGB 555, 16 bpp";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB565) {
		FormatName="RGB 565, 16 bpp";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB24) {
		FormatName="RGB, 24 bpp";
		return;
	}
	if (*guid==MEDIASUBTYPE_RGB32) {
		FormatName="RGB, 32 bpp";
		return;
	}

	// DV video subtypes
	if (*guid==MEDIASUBTYPE_dvsl) {
		FormatName="DV, 12.5 Mbps SD-DVCR 525-60 or SD-DVCR 625-50";
		return;
	}
	if (*guid==MEDIASUBTYPE_dvsd) {
		FormatName="DV, 25 Mbps SDL-DVCR 525-60 or SDL-DVCR 625-50";
		return;
	}
	if (*guid==MEDIASUBTYPE_dvhd) {
		FormatName="DV, 50 Mbps HD-DVCR 1125-60 or HD-DVCR 1250-50";
		return;
	}
	if (*guid==MEDIASUBTYPE_dv25) {
		FormatName="DV, 25 Mbps DVCPRO 25 (525-60 or 625-50)";
		return;
	}
	if (*guid==MEDIASUBTYPE_dv50) {
		FormatName="DV, 50 Mbps DVCPRO 50 (525-60 or 625-50)";
		return;
	}
	if (*guid==MEDIASUBTYPE_dvh1) {
		FormatName="DV, 100 Mbps DVCPRO 100 (1080/60i, 1080/50i, or 720/60P)";
		return;
	}

	// YUV video subtypes
	if (*guid==MEDIASUBTYPE_AYUV) {
		FormatName="4:4:4 YUV formats";
		return;
	}
	if (*guid==MEDIASUBTYPE_UYVY) {
		FormatName="UYVY (packed 4:2:2)";
		return;
	}
	if (*guid==MEDIASUBTYPE_Y411) {
		FormatName="Y411 (packed 4:1:1)";
		return;
	}
	if (*guid==MEDIASUBTYPE_Y41P) {
		FormatName="Y41P (packed 4:1:1)";
		return;
	}
	if (*guid==MEDIASUBTYPE_Y211) {
		FormatName="Y211";
		return;
	}
	if (*guid==MEDIASUBTYPE_YUY2) {
		FormatName="YUYV (packed 4:2:2)";
		return;
	}
	if (*guid==MEDIASUBTYPE_YVYU) {
		FormatName="YVYU (packed 4:2:2)";
		return;
	}
	if (*guid==MEDIASUBTYPE_YUYV) {
		FormatName="YUYV (packed 4:2:2)(Used by Canopus; FOURCC 'YUYV')";
		return;
	}
	if (*guid==MEDIASUBTYPE_IF09) {
		FormatName="Indeo YVU9";
		return;
	}
	if (*guid==MEDIASUBTYPE_IYUV) {
		FormatName="IYUV";
		return;
	}
	/*
	if (*guid==MEDIASUBTYPE_I420) {
		FormatName="I420";
		return;
	}
	*/
	if (*guid==MEDIASUBTYPE_YV12) {
		FormatName="YV12";
		return;
	}
	if (*guid==MEDIASUBTYPE_YVU9) {
		FormatName="YVU9";
		return;
	}

	// Miscellaneous video subtypes
	if (*guid==MEDIASUBTYPE_CFCC) {
		FormatName="MJPG format produced by some cards. (FOURCC 'CFCC')";
		return;
	}
	if (*guid==MEDIASUBTYPE_CLJR) {
		FormatName="Cirrus Logic CLJR format. (FOURCC 'CLJR')";
		return;
	}
	if (*guid==MEDIASUBTYPE_CPLA) {
		FormatName="Cinepak UYVY format. (FOURCC 'CPLA')";
		return;
	}
	if (*guid==MEDIASUBTYPE_CLPL) {
		FormatName
				="A YUV format supported by some Cirrus Logic drivers. (FOURCC 'CLPL')";
		return;
	}
	if (*guid==MEDIASUBTYPE_IJPG) {
		FormatName="Intergraph JPEG format. (FOURCC 'IJPG')";
		return;
	}
	if (*guid==MEDIASUBTYPE_MDVF) {
		FormatName="A DV encoding format. (FOURCC 'MDVF')";
		return;
	}
	if (*guid==MEDIASUBTYPE_MJPG) {
		FormatName="Motion JPEG (MJPG) compressed video. (FOURCC 'MJPG')";
		return;
	}
	if (*guid==MEDIASUBTYPE_MPEG1Packet) {
		FormatName="MPEG1 Video Packet";
		return;
	}
	if (*guid==MEDIASUBTYPE_MPEG1Payload) {
		FormatName="MPEG1 Video Payload";
		return;
	}
	if (*guid==MEDIASUBTYPE_Overlay) {
		FormatName="Video delivered using hardware overlay";
		return;
	}
	if (*guid==MEDIASUBTYPE_Plum) {
		FormatName="Plum MJPG format. (FOURCC 'Plum')";
		return;
	}
	if (*guid==MEDIASUBTYPE_QTJpeg) {
		FormatName="QuickTime JPEG compressed data";
		return;
	}
	if (*guid==MEDIASUBTYPE_QTMovie) {
		FormatName="Apple® QuickTime® compression";
		return;
	}
	if (*guid==MEDIASUBTYPE_QTRle) {
		FormatName="QuickTime RLE compressed data";
		return;
	}
	if (*guid==MEDIASUBTYPE_QTRpza) {
		FormatName="QuickTime RPZA compressed data";
		return;
	}
	if (*guid==MEDIASUBTYPE_QTSmc) {
		FormatName="QuickTime SMC compressed data";
		return;
	}
	if (*guid==MEDIASUBTYPE_TVMJ) {
		FormatName="TrueVision MJPG format. (FOURCC 'TVMJ')";
		return;
	}
	if (*guid==MEDIASUBTYPE_VPVBI) {
		FormatName="Video port vertical blanking interval (VBI) data";
		return;
	}
	if (*guid==MEDIASUBTYPE_VPVideo) {
		FormatName="Video port video data";
		return;
	}
	if (*guid==MEDIASUBTYPE_WAKE) {
		FormatName="MJPG format produced by some cards. (FOURCC 'WAKE')";
		return;
	}

	if (*guid==MEDIASUBTYPE_MPEG1Video) {
		FormatName="MPEG1 Video";
		return;
	}
	if (*guid==MEDIASUBTYPE_MPEG2_VIDEO) {
		FormatName="MPEG2 Video";
		return;
	}

	FormatName="Unknown format type!";
	return;
}

void GetVideoStandardName(ULONG VideoStandard,
		std::list<avcap::VideoStandard*>& VidStandard)
{
	// list creation date:05/27/2005 - taken from DirectShow documentation

	if (VideoStandard & AnalogVideo_None) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("Digital sensor", 0);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_NTSC_M) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("NTSC (M) standard, 7.5 IRE black", 0x1);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_NTSC_M_J) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("NTSC (M) standard, 0 IRE black (Japan)", 0x2);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_NTSC_433) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("NTSC-433", 0x4);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_B) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL-B standard", 0x10);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_D) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (D) standard", 0x20);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_G) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (G) standard", 0x40);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_H) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (H) standard", 0x80);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_I) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (I) standard", 0x100);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_M) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (M) standard", 0x200);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_N) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL (N) standard", 0x400);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_60) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("PAL-60 standard", 0x800);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_B) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (B) standard", 0x1000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_D) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (D) standard", 0x2000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_G) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (G) standard", 0x4000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_H) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (H) standard", 0x8000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_K) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (K) standard", 0x10000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_K1) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (K1) standard", 0x20000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_L) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (L) standard", 0x40000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_SECAM_L1) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("SECAM (L1) standard", 0x80000);
		VidStandard.push_back(vidSt);
	}
	if (VideoStandard & AnalogVideo_PAL_N_COMBO) {
		avcap::VideoStandard *vidSt=new avcap::VideoStandard("Combination (N) PAL standard (Argentina)", 0x100000);
		VidStandard.push_back(vidSt);
	}
}

#endif // FORMATNAMES_H_
