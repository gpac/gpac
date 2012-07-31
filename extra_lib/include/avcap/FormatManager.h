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
 * To use it in commercial endeavors, please contact Nico Pranke <Nico.Pranke@googlemail.com>
 */

#ifndef FORMATMANAGER_H_
#define FORMATMANAGER_H_

#include <list>
#include <string>

#if !defined(_MSC_VER) && !defined(USE_PREBUILD_LIBS)
# include <stdint.h>
# include "avcap-config.h"
#endif

#include "Manager.h"
#include "avcap-export.h"

#ifdef AVCAP_LINUX
# include <linux/types.h>
# ifdef AVCAP_HAVE_V4L2
#  include <linux/videodev2.h>
# else
#  include <linux/videodev.h>
# endif
#endif // AVCAP_LINUX

#ifdef _WIN32
typedef unsigned int uint32_t;
#endif

/* taken from linux/videodev2.h but all FormatManagers use these fourcc-codes*/

#define FOURCC(a,b,c,d)\
        (((uint32_t)(a)<<0)|((uint32_t)(b)<<8)|((uint32_t)(c)<<16)|((uint32_t)(d)<<24))

#define PIX_FMT_RGB332  FOURCC('R','G','B','1') /*  8  RGB-3-3-2     */
#define PIX_FMT_RGB555  FOURCC('R','G','B','O') /* 16  RGB-5-5-5     */
#define PIX_FMT_RGB565  FOURCC('R','G','B','P') /* 16  RGB-5-6-5     */
#define PIX_FMT_RGB555X FOURCC('R','G','B','Q') /* 16  RGB-5-5-5 BE  */
#define PIX_FMT_RGB565X FOURCC('R','G','B','R') /* 16  RGB-5-6-5 BE  */
#define PIX_FMT_BGR24   FOURCC('B','G','R','3') /* 24  BGR-8-8-8     */
#define PIX_FMT_RGB24   FOURCC('R','G','B','3') /* 24  RGB-8-8-8     */
#define PIX_FMT_BGR32   FOURCC('B','G','R','4') /* 32  BGR-8-8-8-8   */
#define PIX_FMT_RGB32   FOURCC('R','G','B','4') /* 32  RGB-8-8-8-8   */
#define PIX_FMT_GREY    FOURCC('G','R','E','Y') /*  8  Greyscale     */
#define PIX_FMT_YVU410  FOURCC('Y','V','U','9') /*  9  YVU 4:1:0     */
#define PIX_FMT_YVU420  FOURCC('Y','V','1','2') /* 12  YVU 4:2:0     */
#define PIX_FMT_YUYV    FOURCC('Y','U','Y','V') /* 16  YUV 4:2:2     */
#define PIX_FMT_UYVY    FOURCC('U','Y','V','Y') /* 16  YUV 4:2:2     */
#define PIX_FMT_YUV422P FOURCC('4','2','2','P') /* 16  YVU422 planar */
#define PIX_FMT_YUV411P FOURCC('4','1','1','P') /* 16  YVU411 planar */
#define PIX_FMT_Y41P    FOURCC('Y','4','1','P') /* 12  YUV 4:1:1     */

/* two planes -- one Y, one Cr + Cb interleaved  */
#define PIX_FMT_NV12    FOURCC('N','V','1','2') /* 12  Y/CbCr 4:2:0  */
#define PIX_FMT_NV21    FOURCC('N','V','2','1') /* 12  Y/CrCb 4:2:0  */

/*  The following formats are not defined in the V4L2 specification */
#define PIX_FMT_YUV410  FOURCC('Y','U','V','9') /*  9  YUV 4:1:0     */
#define PIX_FMT_YUV420  FOURCC('Y','U','1','2') /* 12  YUV 4:2:0     */
#define PIX_FMT_I420	FOURCC('I','4','2','0') /* 12  identical to YU12 */
#define PIX_FMT_YYUV    FOURCC('Y','Y','U','V') /* 16  YUV 4:2:2     */
#define PIX_FMT_HI240   FOURCC('H','I','2','4') /*  8  8-bit color   */
#define PIX_FMT_HM12    FOURCC('H','M','1','2') /*  8  YUV 4:1:1 16x16 macroblocks */

/* see http://www.siliconimaging.com/RGB%20Bayer.htm */
#define PIX_FMT_SBGGR8  FOURCC('B','A','8','1') /*  8  BGBG.. GRGR.. */

/* compressed formats */
#define PIX_FMT_MJPEG    FOURCC('M','J','P','G') /* Motion-JPEG   */
#define PIX_FMT_JPEG     FOURCC('J','P','E','G') /* JFIF JPEG     */
#define PIX_FMT_DV       FOURCC('d','v','s','d') /* 1394          */
#define PIX_FMT_MPEG     FOURCC('M','P','E','G') /* MPEG-1/2/4    */

/*  Vendor-specific formats   */
#define PIX_FMT_WNVA     FOURCC('W','N','V','A') /* Winnov hw compress */
#define PIX_FMT_SN9C10X  FOURCC('S','9','1','0') /* SN9C10x compression */
#define PIX_FMT_PWC1     FOURCC('P','W','C','1') /* pwc older webcam */
#define PIX_FMT_PWC2     FOURCC('P','W','C','2') /* pwc newer webcam */
#define PIX_FMT_ET61X251 FOURCC('E','6','2','5') /* ET61X251 compression */

namespace avcap
{
class DeviceDescriptor;

	//! Description of the video standard.
	struct AVCAP_Export VideoStandard
	{
#ifdef AVCAP_LINUX
		typedef v4l2_std_id	STANDARD_ID_T;
		enum {
			PAL = V4L2_STD_PAL_B,
			NTSC = V4L2_STD_NTSC_M,
			SECAM = V4L2_STD_SECAM_B
		};
#endif

#if defined (_WIN32) || defined (AVCAP_OSX)
		typedef unsigned int STANDARD_ID_T;
#endif


		std::string		name; 	//!< The name of the standard.
		STANDARD_ID_T	id;		//!< A unique identifier.

		//! Constructor
		VideoStandard(const std::string& n, STANDARD_ID_T i):
			name(n),
			id(i)
			{};
	};

	//! The Resolution consists of a width and a height.
	struct AVCAP_Export Resolution {
		int width, height;

		Resolution(int w, int h):
			width(w),
			height(h)
			{}
	};

	//! Description of a video format.
	class AVCAP_Export Format
	{
	public:
		typedef std::list<Resolution*> ResolutionList_t;
	private:
		std::string mName;		// A textual description.
		uint32_t mFourcc;		// The Four Character Code of the format.
		ResolutionList_t	mResList;

#ifdef _WIN32
		void *mediatype;		/* stores DirectShow-specific format description (only used internaly).
								 *< It is always casted to a AM_MEDIA_TYPE
								 * DirectShow structure; see DirectShow documentation */
#endif

	public:
		//! Constructor
		inline Format(const std::string& n, uint32_t f):
			mName(n), mFourcc(f)
			{}

		//! Destructor
		virtual ~Format();

		//! Get the name of the format.
		/*! \return name */
		inline const std::string& getName() const
			{ return mName; }

		//! Get the four character code of the format (see: www.fourcc.org).
		/*! \return fourcc */
		inline uint32_t getFourcc() const
			{ return mFourcc; }

		//! Return a list of resolutions that are supported for this format.
		/*! \return the resolutions.*/
		inline const ResolutionList_t& getResolutionList() const
			{ return mResList; }

		void addResolution(int w, int h);

#ifdef _WIN32
		void* getMediaType() { return mediatype; }

		void setMediaType(void* mt) { mediatype = mt; }
#endif
	};

	//! Abstract base for classes that query and manage available formats, video-standards and resolutions of a capture device.

	/*! This class queries the formats, video-standards and resolutions provided by the device
	 * and allows applications to set them.
	 * The class provides a STL-list of Format-objects.
	 * Actualy changing the format may be deferred by the concrete implementation until it
	 * is really necessary, e.g. the capture begins, because advising the driver
	 * to change the format can be a quite time-consuming operation.
	 * Most of the methods in this class are implemented as a noop and are
	 * reimplemented by the derived class for a concrete capture API/OS, if the method is applicable.
	 */

	class AVCAP_Export FormatManager: public Manager<Format>
	{
	public:
		typedef std::list<VideoStandard*>	VideoStandardList;

	protected:
		ListType		mFormats;
		int				mWidth;
		int				mHeight;
		int 			mBytesPerLine;
#if defined(AVCAP_LINUX) || defined (AVCAP_OSX)
		unsigned int	mCurrentFormat;
#endif
#ifdef _WIN32
		void			*mCurrentFormat;		// Always casted to a AM_MEDIA_TYPE DirectShow structure
#endif
		unsigned long	mImageSize;
		bool			mModified;
		VideoStandardList	mStandards;

	public:
		//! The constructor. */
		FormatManager(DeviceDescriptor *dd);

		//! The destructor. */
		virtual ~FormatManager();

		//! Returns the STL-list of Format objects describing the available formats.
		/*! \return The format list.*/
		virtual inline const ListType& getFormatList() const
			{ return (const ListType&) mFormats; }

		//! Set the format to capture.
		/*! \param fmt The new format.
		 * \return 0, if successful, -1 else */
		virtual int setFormat(Format *fmt);

		//! Set the format to capture.
		/*! \param fourcc The four character code of the new format.
		 * \return 0, if successful, -1 else */
		virtual int setFormat(uint32_t fourcc);

		//! Get the current format.
		/*! \return The format. */
		virtual Format* getFormat();

		//! Set the image with and height.
		/*! \param w : width
		 * \param h : height
		 * \return 0, if successful, -1 else */
		virtual int setResolution(int w, int h);

		//! Set the number of used bytes per scanline, if possible.
		/*! \param bpl
		 * \return 0, if successful, -1 else */
		virtual int setBytesPerLine(int bpl);

		//! Returns the image with.
		/*! \return width*/
		virtual int getWidth();

		//! Returns the image height.
		/*! \return height*/
		virtual int getHeight();

		//! Returns the bytes per line.
		/*! \return bpl*/
		virtual int getBytesPerLine();

		//! Flushes the format, i.e. the driver is advised to apply the current format settings.
		/*! \return  0, if successful, -1 else */
		virtual int flush();

		//! The number of bytes that an image of the current size requires to be stored in memory, including padding.
		/*! \return size */
		virtual size_t getImageSize();

		//! Set the framerate.
		/*! The default implementation returns -1
		 * \param fps : the number of frames per second.
		 * \return 0 if successful, -1 on failure */
		virtual int setFramerate(int fps);

		//! Get the current framerate.
		/*! The default implementation returns -1
		 *! \return the frames per second */
		virtual int getFramerate();

		//! Get the STL-list of avaliable video standards described by VideoStandard objects.
		/*! \return standards list*/
		virtual inline const VideoStandardList& getVideoStandardList() const
			{ return (const VideoStandardList&) mStandards; }

		//! Get the currently used video standard.
		/*! The default implementation returns 0
		 * \return the current standard or 0, if not applicable */
		virtual const VideoStandard* getVideoStandard();

		//! Set the video standard to use.
		/*! Attention: not all video standards can be set in conjunction with each connector and format.
		 * The default implementation returns -1
		 * \param std The new video standard.
		 * \return 0, if successful, -1 else */
		virtual int setVideoStandard(const VideoStandard* std);

		virtual void query() = 0;
	};
};
#endif //FORMATMANAGER_H_


