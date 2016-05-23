/*
   miniunz.c
   Version 1.01e, February 12th, 2005

   Copyright (C) 1998-2005 Gilles Vollant
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef _WIN32_WCE
#include <errno.h>
#include <fcntl.h>

#ifdef WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
# include <sys/stat.h>
#endif

#endif

//hack: prevent mozilla to redifine z functions
#define MOZZCONF_H

#include "unzip.h"
#include <gpac/tools.h>



/* I've found an old Unix (a SunOS 4.1.3_U1) without all SEEK_* defined.... */

#ifndef SEEK_CUR
#define SEEK_CUR    1
#endif

#ifndef SEEK_END
#define SEEK_END    2
#endif

#ifndef SEEK_SET
#define SEEK_SET    0
#endif

voidpf ZCALLBACK fopen_file_func OF((
                                        voidpf opaque,
                                        const char* filename,
                                        int mode));

uLong ZCALLBACK fread_file_func OF((
                                       voidpf opaque,
                                       voidpf stream,
                                       void* buf,
                                       uLong size));

uLong ZCALLBACK fwrite_file_func OF((
                                        voidpf opaque,
                                        voidpf stream,
                                        const void* buf,
                                        uLong size));

long ZCALLBACK ftell_file_func OF((
                                      voidpf opaque,
                                      voidpf stream));

long ZCALLBACK fseek_file_func OF((
                                      voidpf opaque,
                                      voidpf stream,
                                      uLong offset,
                                      int origin));

int ZCALLBACK fclose_file_func OF((
                                      voidpf opaque,
                                      voidpf stream));

int ZCALLBACK ferror_file_func OF((
                                      voidpf opaque,
                                      voidpf stream));


voidpf ZCALLBACK fopen_file_func (opaque, filename, mode)
voidpf opaque;
const char* filename;
int mode;
{
	FILE* file = NULL;
	const char* mode_fopen = NULL;
	if ((mode & ZLIB_FILEFUNC_MODE_READWRITEFILTER)==ZLIB_FILEFUNC_MODE_READ)
		mode_fopen = "rb";
	else if (mode & ZLIB_FILEFUNC_MODE_EXISTING)
		mode_fopen = "r+b";
	else if (mode & ZLIB_FILEFUNC_MODE_CREATE)
		mode_fopen = "wb";

	if ((filename!=NULL) && (mode_fopen != NULL))
		file = gf_fopen(filename, mode_fopen);
	return file;
}


uLong ZCALLBACK fread_file_func (opaque, stream, buf, size)
voidpf opaque;
voidpf stream;
void* buf;
uLong size;
{
	uLong ret;
	ret = (uLong)fread(buf, 1, (size_t)size, (FILE *)stream);
	return ret;
}


uLong ZCALLBACK fwrite_file_func (opaque, stream, buf, size)
voidpf opaque;
voidpf stream;
const void* buf;
uLong size;
{
	uLong ret;
	ret = (uLong)gf_fwrite(buf, 1, (size_t)size, (FILE *)stream);
	return ret;
}

long ZCALLBACK ftell_file_func (opaque, stream)
voidpf opaque;
voidpf stream;
{
	long ret;
	ret = ftell((FILE *)stream);
	return ret;
}

long ZCALLBACK fseek_file_func (opaque, stream, offset, origin)
voidpf opaque;
voidpf stream;
uLong offset;
int origin;
{
	int fseek_origin=0;
	long ret;
	switch (origin)
	{
	case ZLIB_FILEFUNC_SEEK_CUR :
		fseek_origin = SEEK_CUR;
		break;
	case ZLIB_FILEFUNC_SEEK_END :
		fseek_origin = SEEK_END;
		break;
	case ZLIB_FILEFUNC_SEEK_SET :
		fseek_origin = SEEK_SET;
		break;
	default:
		return -1;
	}
	ret = 0;
	fseek((FILE *)stream, offset, fseek_origin);
	return ret;
}

int ZCALLBACK fclose_file_func (opaque, stream)
voidpf opaque;
voidpf stream;
{
	int ret;
	ret = gf_fclose((FILE *)stream);
	return ret;
}

int ZCALLBACK ferror_file_func (opaque, stream)
voidpf opaque;
voidpf stream;
{
	int ret;
	ret = ferror((FILE *)stream);
	return ret;
}

void fill_fopen_filefunc (pzlib_filefunc_def)
zlib_filefunc_def* pzlib_filefunc_def;
{
	pzlib_filefunc_def->zopen_file = fopen_file_func;
	pzlib_filefunc_def->zread_file = fread_file_func;
	pzlib_filefunc_def->zwrite_file = fwrite_file_func;
	pzlib_filefunc_def->ztell_file = ftell_file_func;
	pzlib_filefunc_def->zseek_file = fseek_file_func;
	pzlib_filefunc_def->zclose_file = fclose_file_func;
	pzlib_filefunc_def->zerror_file = ferror_file_func;
	pzlib_filefunc_def->opaque = NULL;
}

static int unzlocal_getByte(pzlib_filefunc_def,filestream,pi)
const zlib_filefunc_def* pzlib_filefunc_def;
voidpf filestream;
int *pi;
{
	unsigned char c;
	int err = (int)ZREAD(*pzlib_filefunc_def,filestream,&c,1);
	if (err==1)
	{
		*pi = (int)c;
		return UNZ_OK;
	}
	else
	{
		if (ZERROR(*pzlib_filefunc_def,filestream))
			return UNZ_ERRNO;
		else
			return UNZ_EOF;
	}
}

static int unzlocal_getShort (pzlib_filefunc_def,filestream,pX)
const zlib_filefunc_def* pzlib_filefunc_def;
voidpf filestream;
uLong *pX;
{
	uLong x ;
	int err;
	int i = 0;

	err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x = (uLong)i;

	if (err==UNZ_OK)
		err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x += ((uLong)i)<<8;

	if (err==UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}

static int unzlocal_getLong (pzlib_filefunc_def,filestream,pX)
const zlib_filefunc_def* pzlib_filefunc_def;
voidpf filestream;
uLong *pX;
{
	uLong x ;
	int err;
	int i = 0;

	err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x = (uLong)i;

	if (err==UNZ_OK)
		err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x += ((uLong)i)<<8;

	if (err==UNZ_OK)
		err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x += ((uLong)i)<<16;

	if (err==UNZ_OK)
		err = unzlocal_getByte(pzlib_filefunc_def,filestream,&i);
	x += ((uLong)i)<<24;

	if (err==UNZ_OK)
		*pX = x;
	else
		*pX = 0;
	return err;
}

static uLong unzlocal_SearchCentralDir(pzlib_filefunc_def,filestream)
const zlib_filefunc_def* pzlib_filefunc_def;
voidpf filestream;
{
	unsigned char* buf;
	uLong uSizeFile;
	uLong uBackRead;
	uLong uMaxBack=0xffff; /* maximum size of global comment */
	uLong uPosFound=0;

	if (ZSEEK(*pzlib_filefunc_def,filestream,0,ZLIB_FILEFUNC_SEEK_END) != 0)
		return 0;


	uSizeFile = ZTELL(*pzlib_filefunc_def,filestream);

	if (uMaxBack>uSizeFile)
		uMaxBack = uSizeFile;

	buf = (unsigned char*)ALLOC(BUFREADCOMMENT+4);
	if (buf==NULL)
		return 0;

	uBackRead = 4;
	while (uBackRead<uMaxBack)
	{
		uLong uReadSize,uReadPos ;
		int i;
		if (uBackRead+BUFREADCOMMENT>uMaxBack)
			uBackRead = uMaxBack;
		else
			uBackRead+=BUFREADCOMMENT;
		uReadPos = uSizeFile-uBackRead ;

		uReadSize = ((BUFREADCOMMENT+4) < (uSizeFile-uReadPos)) ?
		            (BUFREADCOMMENT+4) : (uSizeFile-uReadPos);
		if (ZSEEK(*pzlib_filefunc_def,filestream,uReadPos,ZLIB_FILEFUNC_SEEK_SET)!=0)
			break;

		if (ZREAD(*pzlib_filefunc_def,filestream,buf,uReadSize)!=uReadSize)
			break;

		for (i=(int)uReadSize-3; (i--)>0;)
			if (((*(buf+i))==0x50) && ((*(buf+i+1))==0x4b) &&
			        ((*(buf+i+2))==0x05) && ((*(buf+i+3))==0x06))
			{
				uPosFound = uReadPos+i;
				break;
			}

		if (uPosFound!=0)
			break;
	}
	TRYFREE(buf);
	return uPosFound;
}


/*
   Translate date/time from Dos format to tm_unz (readable more easilty)
*/
void unzlocal_DosDateToTmuDate (ulDosDate, ptm)
uLong ulDosDate;
tm_unz* ptm;
{
	uLong uDate;
	uDate = (uLong)(ulDosDate>>16);
	ptm->tm_mday = (uInt)(uDate&0x1f) ;
	ptm->tm_mon =  (uInt)((((uDate)&0x1E0)/0x20)-1) ;
	ptm->tm_year = (uInt)(((uDate&0x0FE00)/0x0200)+1980) ;

	ptm->tm_hour = (uInt) ((ulDosDate &0xF800)/0x800);
	ptm->tm_min =  (uInt) ((ulDosDate&0x7E0)/0x20) ;
	ptm->tm_sec =  (uInt) (2*(ulDosDate&0x1f)) ;
}

static int unzlocal_GetCurrentFileInfoInternal (file,
        pfile_info,
        pfile_info_internal,
        szFileName, fileNameBufferSize,
        extraField, extraFieldBufferSize,
        szComment,  commentBufferSize)
unzFile file;
unz_file_info *pfile_info;
unz_file_info_internal *pfile_info_internal;
char *szFileName;
uLong fileNameBufferSize;
void *extraField;
uLong extraFieldBufferSize;
char *szComment;
uLong commentBufferSize;
{
	unz_s* s;
	unz_file_info file_info;
	unz_file_info_internal file_info_internal;
	int err=UNZ_OK;
	uLong uMagic;
	long lSeek=0;

	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (ZSEEK(s->z_filefunc, s->filestream,
	          s->pos_in_central_dir+s->byte_before_the_zipfile,
	          ZLIB_FILEFUNC_SEEK_SET)!=0)
		err=UNZ_ERRNO;


	/* we check the magic */
	if (err==UNZ_OK) {
		if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uMagic) != UNZ_OK)
			err=UNZ_ERRNO;
		else if (uMagic!=0x02014b50)
			err=UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.version) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.version_needed) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.flag) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.compression_method) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.dosDate) != UNZ_OK)
		err=UNZ_ERRNO;

	unzlocal_DosDateToTmuDate(file_info.dosDate,&file_info.tmu_date);

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.crc) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.compressed_size) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.uncompressed_size) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_filename) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_file_extra) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.size_file_comment) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.disk_num_start) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&file_info.internal_fa) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info.external_fa) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&file_info_internal.offset_curfile) != UNZ_OK)
		err=UNZ_ERRNO;

	lSeek+=file_info.size_filename;
	if ((err==UNZ_OK) && (szFileName!=NULL))
	{
		uLong uSizeRead ;
		if (file_info.size_filename<fileNameBufferSize)
		{
			*(szFileName+file_info.size_filename)='\0';
			uSizeRead = file_info.size_filename;
		}
		else
			uSizeRead = fileNameBufferSize;

		if ((file_info.size_filename>0) && (fileNameBufferSize>0))
			if (ZREAD(s->z_filefunc, s->filestream,szFileName,uSizeRead)!=uSizeRead)
				err=UNZ_ERRNO;
		lSeek -= uSizeRead;
	}


	if ((err==UNZ_OK) && (extraField!=NULL))
	{
		uLong uSizeRead ;
		if (file_info.size_file_extra<extraFieldBufferSize)
			uSizeRead = file_info.size_file_extra;
		else
			uSizeRead = extraFieldBufferSize;

		if (lSeek!=0) {
			if (ZSEEK(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0)
				lSeek=0;
			else
				err=UNZ_ERRNO;
		}
		if ((file_info.size_file_extra>0) && (extraFieldBufferSize>0))
			if (ZREAD(s->z_filefunc, s->filestream,extraField,uSizeRead)!=uSizeRead)
				err=UNZ_ERRNO;
		lSeek += file_info.size_file_extra - uSizeRead;
	}
	else
		lSeek+=file_info.size_file_extra;


	if ((err==UNZ_OK) && (szComment!=NULL))
	{
		uLong uSizeRead ;
		if (file_info.size_file_comment<commentBufferSize)
		{
			*(szComment+file_info.size_file_comment)='\0';
			uSizeRead = file_info.size_file_comment;
		}
		else
			uSizeRead = commentBufferSize;

		if (lSeek!=0) {
			if (ZSEEK(s->z_filefunc, s->filestream,lSeek,ZLIB_FILEFUNC_SEEK_CUR)==0) {
				//lSeek=0;
			} else {
				err=UNZ_ERRNO;
			}
		}
		if ((file_info.size_file_comment>0) && (commentBufferSize>0))
			if (ZREAD(s->z_filefunc, s->filestream,szComment,uSizeRead)!=uSizeRead)
				err=UNZ_ERRNO;
		//lSeek+=file_info.size_file_comment - uSizeRead;
	} else {
		//lSeek+=file_info.size_file_comment;
	}

	if ((err==UNZ_OK) && (pfile_info!=NULL))
		*pfile_info=file_info;

	if ((err==UNZ_OK) && (pfile_info_internal!=NULL))
		*pfile_info_internal=file_info_internal;

	return err;
}

int unzGetCurrentFileInfo(unzFile file,
                          unz_file_info *pfile_info,
                          char *szFileName,
                          uLong fileNameBufferSize,
                          void *extraField,
                          uLong extraFieldBufferSize,
                          char *szComment,
                          uLong commentBufferSize)
{
	return unzlocal_GetCurrentFileInfoInternal(file, pfile_info, NULL, szFileName, fileNameBufferSize, extraField, extraFieldBufferSize, szComment, commentBufferSize);
}

/*
  Set the current file of the zipfile to the next file.
  return UNZ_OK if there is no problem
  return UNZ_END_OF_LIST_OF_FILE if the actual file was the latest.
*/
int unzGoToNextFile (file)
unzFile file;
{
	unz_s* s;
	int err;

	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (!s->current_file_ok)
		return UNZ_END_OF_LIST_OF_FILE;
	if (s->gi.number_entry != 0xffff)    /* 2^16 files overflow hack */
		if (s->num_file+1==s->gi.number_entry)
			return UNZ_END_OF_LIST_OF_FILE;

	s->pos_in_central_dir += SIZECENTRALDIRITEM + s->cur_file_info.size_filename +
	                         s->cur_file_info.size_file_extra + s->cur_file_info.size_file_comment ;
	s->num_file++;
	err = unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
	        &s->cur_file_info_internal,
	        NULL,0,NULL,0,NULL,0);
	s->current_file_ok = (err == UNZ_OK);
	return err;
}

/*
  Set the current file of the zipfile to the first file.
  return UNZ_OK if there is no problem
*/
int unzGoToFirstFile (file)
unzFile file;
{
	int err=UNZ_OK;
	unz_s* s;
	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	s->pos_in_central_dir=s->offset_central_dir;
	s->num_file=0;
	err=unzlocal_GetCurrentFileInfoInternal(file,&s->cur_file_info,
	                                        &s->cur_file_info_internal,
	                                        NULL,0,NULL,0,NULL,0);
	s->current_file_ok = (err == UNZ_OK);
	return err;
}

int unzGetGlobalInfo (file,pglobal_info)
unzFile file;
unz_global_info *pglobal_info;
{
	unz_s* s;
	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	*pglobal_info=s->gi;
	return UNZ_OK;
}

/*
  Close the file in zip opened with unzipOpenCurrentFile
  Return UNZ_CRCERROR if all the file was read but the CRC is not good
*/
int unzCloseCurrentFile (file)
unzFile file;
{
	int err=UNZ_OK;

	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (pfile_in_zip_read_info==NULL)
		return UNZ_PARAMERROR;


	if ((pfile_in_zip_read_info->rest_read_uncompressed == 0) &&
	        (!pfile_in_zip_read_info->raw))
	{
		if (pfile_in_zip_read_info->crc32 != pfile_in_zip_read_info->crc32_wait)
			err=UNZ_CRCERROR;
	}


	TRYFREE(pfile_in_zip_read_info->read_buffer);
	pfile_in_zip_read_info->read_buffer = NULL;
	if (pfile_in_zip_read_info->stream_initialised)
		inflateEnd(&pfile_in_zip_read_info->stream);

	pfile_in_zip_read_info->stream_initialised = 0;
	TRYFREE(pfile_in_zip_read_info);

	s->pfile_in_zip_read=NULL;

	return err;
}


/*
  Open a Zip file. path contain the full pathname (by example,
     on a Windows NT computer "c:\\test\\zlib114.zip" or on an Unix computer
     "zlib/zlib114.zip".
     If the zipfile cannot be opened (file doesn't exist or in not valid), the
       return value is NULL.
     Else, the return value is a unzFile Handle, usable with other function
       of this unzip package.
*/
extern unzFile ZEXPORT unzOpen2 (path, pzlib_filefunc_def)
const char *path;
zlib_filefunc_def* pzlib_filefunc_def;
{
	unz_s us;
	unz_s *s;
	uLong central_pos,uL;

	uLong number_disk;          /* number of the current dist, used for
                                   spaning ZIP, unsupported, always 0*/
	uLong number_disk_with_CD;  /* number the the disk with central dir, used
                                   for spaning ZIP, unsupported, always 0*/
	uLong number_entry_CD;      /* total number of entries in
                                   the central dir
                                   (same than number_entry on nospan) */

	int err=UNZ_OK;

	if (pzlib_filefunc_def==NULL)
		fill_fopen_filefunc(&us.z_filefunc);
	else
		us.z_filefunc = *pzlib_filefunc_def;

	us.filestream= (*(us.z_filefunc.zopen_file))(us.z_filefunc.opaque,
	               path,
	               ZLIB_FILEFUNC_MODE_READ |
	               ZLIB_FILEFUNC_MODE_EXISTING);
	if (us.filestream==NULL)
		return NULL;

	central_pos = unzlocal_SearchCentralDir(&us.z_filefunc,us.filestream);
	if (central_pos==0)
		err=UNZ_ERRNO;

	if (ZSEEK(us.z_filefunc, us.filestream,
	          central_pos,ZLIB_FILEFUNC_SEEK_SET)!=0)
		err=UNZ_ERRNO;

	/* the signature, already checked */
	if (unzlocal_getLong(&us.z_filefunc, us.filestream,&uL)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* number of this disk */
	if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_disk)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* number of the disk with the start of the central directory */
	if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_disk_with_CD)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* total number of entries in the central dir on this disk */
	if (unzlocal_getShort(&us.z_filefunc, us.filestream,&us.gi.number_entry)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* total number of entries in the central dir */
	if (unzlocal_getShort(&us.z_filefunc, us.filestream,&number_entry_CD)!=UNZ_OK)
		err=UNZ_ERRNO;

	if ((number_entry_CD!=us.gi.number_entry) ||
	        (number_disk_with_CD!=0) ||
	        (number_disk!=0))
		err=UNZ_BADZIPFILE;

	/* size of the central directory */
	if (unzlocal_getLong(&us.z_filefunc, us.filestream,&us.size_central_dir)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* offset of start of central directory with respect to the
	      starting disk number */
	if (unzlocal_getLong(&us.z_filefunc, us.filestream,&us.offset_central_dir)!=UNZ_OK)
		err=UNZ_ERRNO;

	/* zipfile comment length */
	if (unzlocal_getShort(&us.z_filefunc, us.filestream,&us.gi.size_comment)!=UNZ_OK)
		err=UNZ_ERRNO;

	if ((central_pos<us.offset_central_dir+us.size_central_dir) &&
	        (err==UNZ_OK))
		err=UNZ_BADZIPFILE;

	if (err!=UNZ_OK)
	{
		ZCLOSE(us.z_filefunc, us.filestream);
		return NULL;
	}

	us.byte_before_the_zipfile = central_pos -
	                             (us.offset_central_dir+us.size_central_dir);
	us.central_pos = central_pos;
	us.pfile_in_zip_read = NULL;
	us.encrypted = 0;


	s=(unz_s*)ALLOC(sizeof(unz_s));
	*s=us;
	unzGoToFirstFile((unzFile)s);
	return (unzFile)s;
}


/*
  Read bytes from the current file.
  buf contain buffer where data must be copied
  len the size of buf.

  return the number of byte copied if somes bytes are copied
  return 0 if the end of file was reached
  return <0 with error code if there is an error
    (UNZ_ERRNO for IO error, or zLib error for uncompress error)
*/
int unzReadCurrentFile  (file, buf, len)
unzFile file;
voidp buf;
unsigned len;
{
	int err=UNZ_OK;
	uInt iRead = 0;
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	pfile_in_zip_read_info=s->pfile_in_zip_read;

	if (pfile_in_zip_read_info==NULL)
		return UNZ_PARAMERROR;


	if (pfile_in_zip_read_info->read_buffer == NULL)
		return UNZ_END_OF_LIST_OF_FILE;
	if (len==0)
		return 0;

	pfile_in_zip_read_info->stream.next_out = (Bytef*)buf;

	pfile_in_zip_read_info->stream.avail_out = (uInt)len;

	if ((len>pfile_in_zip_read_info->rest_read_uncompressed) &&
	        (!(pfile_in_zip_read_info->raw)))
		pfile_in_zip_read_info->stream.avail_out =
		    (uInt)pfile_in_zip_read_info->rest_read_uncompressed;

	if ((len>pfile_in_zip_read_info->rest_read_compressed+
	        pfile_in_zip_read_info->stream.avail_in) &&
	        (pfile_in_zip_read_info->raw))
		pfile_in_zip_read_info->stream.avail_out =
		    (uInt)pfile_in_zip_read_info->rest_read_compressed+
		    pfile_in_zip_read_info->stream.avail_in;

	while (pfile_in_zip_read_info->stream.avail_out>0)
	{
		if ((pfile_in_zip_read_info->stream.avail_in==0) &&
		        (pfile_in_zip_read_info->rest_read_compressed>0))
		{
			uInt uReadThis = UNZ_BUFSIZE;
			if (pfile_in_zip_read_info->rest_read_compressed<uReadThis)
				uReadThis = (uInt)pfile_in_zip_read_info->rest_read_compressed;
			if (uReadThis == 0)
				return UNZ_EOF;
			if (ZSEEK(pfile_in_zip_read_info->z_filefunc,
			          pfile_in_zip_read_info->filestream,
			          pfile_in_zip_read_info->pos_in_zipfile +
			          pfile_in_zip_read_info->byte_before_the_zipfile,
			          ZLIB_FILEFUNC_SEEK_SET)!=0)
				return UNZ_ERRNO;
			if (ZREAD(pfile_in_zip_read_info->z_filefunc,
			          pfile_in_zip_read_info->filestream,
			          pfile_in_zip_read_info->read_buffer,
			          uReadThis)!=uReadThis)
				return UNZ_ERRNO;

			pfile_in_zip_read_info->pos_in_zipfile += uReadThis;

			pfile_in_zip_read_info->rest_read_compressed-=uReadThis;

			pfile_in_zip_read_info->stream.next_in =
			    (Bytef*)pfile_in_zip_read_info->read_buffer;
			pfile_in_zip_read_info->stream.avail_in = (uInt)uReadThis;
		}

		if ((pfile_in_zip_read_info->compression_method==0) || (pfile_in_zip_read_info->raw))
		{
			uInt uDoCopy,i ;

			if ((pfile_in_zip_read_info->stream.avail_in == 0) &&
			        (pfile_in_zip_read_info->rest_read_compressed == 0))
				return (iRead==0) ? UNZ_EOF : iRead;

			if (pfile_in_zip_read_info->stream.avail_out <
			        pfile_in_zip_read_info->stream.avail_in)
				uDoCopy = pfile_in_zip_read_info->stream.avail_out ;
			else
				uDoCopy = pfile_in_zip_read_info->stream.avail_in ;

			for (i=0; i<uDoCopy; i++)
				*(pfile_in_zip_read_info->stream.next_out+i) =
				    *(pfile_in_zip_read_info->stream.next_in+i);

			pfile_in_zip_read_info->crc32 = crc32(pfile_in_zip_read_info->crc32,
			                                      pfile_in_zip_read_info->stream.next_out,
			                                      uDoCopy);
			pfile_in_zip_read_info->rest_read_uncompressed-=uDoCopy;
			pfile_in_zip_read_info->stream.avail_in -= uDoCopy;
			pfile_in_zip_read_info->stream.avail_out -= uDoCopy;
			pfile_in_zip_read_info->stream.next_out += uDoCopy;
			pfile_in_zip_read_info->stream.next_in += uDoCopy;
			pfile_in_zip_read_info->stream.total_out += uDoCopy;
			iRead += uDoCopy;
		}
		else
		{
			uLong uTotalOutBefore,uTotalOutAfter;
			const Bytef *bufBefore;
			uLong uOutThis;
			int flush=Z_SYNC_FLUSH;

			uTotalOutBefore = pfile_in_zip_read_info->stream.total_out;
			bufBefore = pfile_in_zip_read_info->stream.next_out;

			/*
			if ((pfile_in_zip_read_info->rest_read_uncompressed ==
			         pfile_in_zip_read_info->stream.avail_out) &&
			    (pfile_in_zip_read_info->rest_read_compressed == 0))
			    flush = Z_FINISH;
			*/
			err=inflate(&pfile_in_zip_read_info->stream,flush);

			if ((err>=0) && (pfile_in_zip_read_info->stream.msg!=NULL))
				err = Z_DATA_ERROR;

			uTotalOutAfter = pfile_in_zip_read_info->stream.total_out;
			uOutThis = uTotalOutAfter-uTotalOutBefore;

			pfile_in_zip_read_info->crc32 =
			    crc32(pfile_in_zip_read_info->crc32,bufBefore,
			          (uInt)(uOutThis));

			pfile_in_zip_read_info->rest_read_uncompressed -=
			    uOutThis;

			iRead += (uInt)(uTotalOutAfter - uTotalOutBefore);

			if (err==Z_STREAM_END)
				return (iRead==0) ? UNZ_EOF : iRead;
			if (err!=Z_OK)
				break;
		}
	}

	if (err==Z_OK)
		return iRead;
	return err;
}

/*
  Read the local header of the current zipfile
  Check the coherency of the local header and info in the end of central
        directory about this file
  store in *piSizeVar the size of extra info in local header
        (filename and size of extra field data)
*/
int unzlocal_CheckCurrentFileCoherencyHeader (s,piSizeVar,
        poffset_local_extrafield,
        psize_local_extrafield)
unz_s* s;
uInt* piSizeVar;
uLong *poffset_local_extrafield;
uInt  *psize_local_extrafield;
{
	uLong uMagic,uData,uFlags;
	uLong size_filename;
	uLong size_extra_field;
	int err=UNZ_OK;

	*piSizeVar = 0;
	*poffset_local_extrafield = 0;
	*psize_local_extrafield = 0;

	if (ZSEEK(s->z_filefunc, s->filestream,s->cur_file_info_internal.offset_curfile +
	          s->byte_before_the_zipfile,ZLIB_FILEFUNC_SEEK_SET)!=0)
		return UNZ_ERRNO;


	if (err==UNZ_OK) {
		if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uMagic) != UNZ_OK)
			err=UNZ_ERRNO;
		else if (uMagic!=0x04034b50)
			err=UNZ_BADZIPFILE;
	}

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uData) != UNZ_OK)
		err=UNZ_ERRNO;
	/*
	    else if ((err==UNZ_OK) && (uData!=s->cur_file_info.wVersion))
	        err=UNZ_BADZIPFILE;
	*/
	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uFlags) != UNZ_OK)
		err=UNZ_ERRNO;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&uData) != UNZ_OK)
		err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compression_method))
		err=UNZ_BADZIPFILE;

	if ((err==UNZ_OK) && (s->cur_file_info.compression_method!=0) &&
	        (s->cur_file_info.compression_method!=Z_DEFLATED))
		err=UNZ_BADZIPFILE;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* date/time */
		err=UNZ_ERRNO;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* crc */
		err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.crc) &&
	         ((uFlags & 8)==0))
		err=UNZ_BADZIPFILE;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* size compr */
		err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.compressed_size) &&
	         ((uFlags & 8)==0))
		err=UNZ_BADZIPFILE;

	if (unzlocal_getLong(&s->z_filefunc, s->filestream,&uData) != UNZ_OK) /* size uncompr */
		err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (uData!=s->cur_file_info.uncompressed_size) &&
	         ((uFlags & 8)==0))
		err=UNZ_BADZIPFILE;


	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&size_filename) != UNZ_OK)
		err=UNZ_ERRNO;
	else if ((err==UNZ_OK) && (size_filename!=s->cur_file_info.size_filename))
		err=UNZ_BADZIPFILE;

	*piSizeVar += (uInt)size_filename;

	if (unzlocal_getShort(&s->z_filefunc, s->filestream,&size_extra_field) != UNZ_OK)
		err=UNZ_ERRNO;
	*poffset_local_extrafield= s->cur_file_info_internal.offset_curfile +
	                           SIZEZIPLOCALHEADER + size_filename;
	*psize_local_extrafield = (uInt)size_extra_field;

	*piSizeVar += (uInt)size_extra_field;

	return err;
}

/*
  Open for reading data the current file in the zipfile.
  If there is no error and the file is opened, the return value is UNZ_OK.
*/
int unzOpenCurrentFile3 (file, method, level, raw, password)
unzFile file;
int* method;
int* level;
int raw;
const char* password;
{
	int err=UNZ_OK;
	uInt iSizeVar;
	unz_s* s;
	file_in_zip_read_info_s* pfile_in_zip_read_info;
	uLong offset_local_extrafield;  /* offset of the local extra field */
	uInt  size_local_extrafield;    /* size of the local extra field */

	if (password != NULL)
		return UNZ_PARAMERROR;

	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;
	if (!s->current_file_ok)
		return UNZ_PARAMERROR;

	if (s->pfile_in_zip_read != NULL)
		unzCloseCurrentFile(file);

	if (unzlocal_CheckCurrentFileCoherencyHeader(s,&iSizeVar,
	        &offset_local_extrafield,&size_local_extrafield)!=UNZ_OK)
		return UNZ_BADZIPFILE;

	pfile_in_zip_read_info = (file_in_zip_read_info_s*)
	                         ALLOC(sizeof(file_in_zip_read_info_s));
	if (pfile_in_zip_read_info==NULL)
		return UNZ_INTERNALERROR;

	pfile_in_zip_read_info->read_buffer=(char*)ALLOC(UNZ_BUFSIZE);
	pfile_in_zip_read_info->offset_local_extrafield = offset_local_extrafield;
	pfile_in_zip_read_info->size_local_extrafield = size_local_extrafield;
	pfile_in_zip_read_info->pos_local_extrafield=0;
	pfile_in_zip_read_info->raw=raw;

	if (pfile_in_zip_read_info->read_buffer==NULL)
	{
		TRYFREE(pfile_in_zip_read_info);
		return UNZ_INTERNALERROR;
	}

	pfile_in_zip_read_info->stream_initialised=0;

	if (method!=NULL)
		*method = (int)s->cur_file_info.compression_method;

	if (level!=NULL)
	{
		*level = 6;
		switch (s->cur_file_info.flag & 0x06)
		{
		case 6 :
			*level = 1;
			break;
		case 4 :
			*level = 2;
			break;
		case 2 :
			*level = 9;
			break;
		}
	}

	if ((s->cur_file_info.compression_method!=0) && (s->cur_file_info.compression_method!=Z_DEFLATED)) {
		TRYFREE(pfile_in_zip_read_info);
		return UNZ_BADZIPFILE;
	}

	pfile_in_zip_read_info->crc32_wait=s->cur_file_info.crc;
	pfile_in_zip_read_info->crc32=0;
	pfile_in_zip_read_info->compression_method =
	    s->cur_file_info.compression_method;
	pfile_in_zip_read_info->filestream=s->filestream;
	pfile_in_zip_read_info->z_filefunc=s->z_filefunc;
	pfile_in_zip_read_info->byte_before_the_zipfile=s->byte_before_the_zipfile;

	pfile_in_zip_read_info->stream.total_out = 0;

	if ((s->cur_file_info.compression_method==Z_DEFLATED) &&
	        (!raw))
	{
		pfile_in_zip_read_info->stream.zalloc = (alloc_func)0;
		pfile_in_zip_read_info->stream.zfree = (free_func)0;
		pfile_in_zip_read_info->stream.opaque = (voidpf)0;
		pfile_in_zip_read_info->stream.next_in = (voidpf)0;
		pfile_in_zip_read_info->stream.avail_in = 0;

		err=inflateInit2(&pfile_in_zip_read_info->stream, -MAX_WBITS);
		if (err == Z_OK)
			pfile_in_zip_read_info->stream_initialised=1;
		else
		{
			TRYFREE(pfile_in_zip_read_info);
			return err;
		}
		/* windowBits is passed < 0 to tell that there is no zlib header.
		 * Note that in this case inflate *requires* an extra "dummy" byte
		 * after the compressed stream in order to complete decompression and
		 * return Z_STREAM_END.
		 * In unzip, i don't wait absolutely Z_STREAM_END because I known the
		 * size of both compressed and uncompressed data
		 */
	}
	pfile_in_zip_read_info->rest_read_compressed =
	    s->cur_file_info.compressed_size ;
	pfile_in_zip_read_info->rest_read_uncompressed =
	    s->cur_file_info.uncompressed_size ;


	pfile_in_zip_read_info->pos_in_zipfile =
	    s->cur_file_info_internal.offset_curfile + SIZEZIPLOCALHEADER +
	    iSizeVar;

	pfile_in_zip_read_info->stream.avail_in = (uInt)0;

	s->pfile_in_zip_read = pfile_in_zip_read_info;

	return UNZ_OK;
}

/*
  Close a ZipFile opened with unzipOpen.
  If there is files inside the .Zip opened with unzipOpenCurrentFile (see later),
    these files MUST be closed with unzipCloseCurrentFile before call unzipClose.
  return UNZ_OK if there is no problem. */
extern int ZEXPORT unzClose (file)
unzFile file;
{
	unz_s* s;
	if (file==NULL)
		return UNZ_PARAMERROR;
	s=(unz_s*)file;

	if (s->pfile_in_zip_read!=NULL)
		unzCloseCurrentFile(file);

	ZCLOSE(s->z_filefunc, s->filestream);
	TRYFREE(s);
	return UNZ_OK;
}

#ifndef _WIN32_WCE

/* mymkdir and change_file_date are not 100 % portable
   As I don't know well Unix, I wait feedback for the unix portion */

int mymkdir(dirname)
const char* dirname;
{
	int ret=0;
#if defined(WIN32) || defined(_WIN32_WCE)
	return mkdir(dirname);
#else
	return mkdir (dirname, 700);
#endif
	return ret;
}

int makedir (newdir)
const char *newdir;
{
	char *buffer ;
	char *p;
	int  len = (int)strlen(newdir);

	if (len <= 0)
		return 0;

	buffer = (char*)gf_malloc(len+1);
	strcpy(buffer,newdir);

	if (buffer[len-1] == '/') {
		buffer[len-1] = '\0';
	}
	if (mymkdir(buffer) == 0)
	{
		gf_free(buffer);
		return 1;
	}

	p = buffer+1;
	while (1)
	{
		char hold;

		while(*p && *p != '\\' && *p != '/')
			p++;
		hold = *p;
		*p = 0;
		if ((mymkdir(buffer) == -1) && (errno == ENOENT))
		{
			fprintf(stderr, "couldn't create directory %s\n",buffer);
			gf_free(buffer);
			return 0;
		}
		if (hold == 0)
			break;
		*p++ = hold;
	}
	gf_free(buffer);
	return 1;
}
#else
int makedir (newdir)
{
	return 0;
}
#endif


int do_extract_currentfile(uf)
unzFile uf;
{
	char filename_inzip[256];
	char* filename_withoutpath;
	char* p;
	int err=UNZ_OK;
	FILE *fout=NULL;
	void* buf;
	uInt size_buf;

	unz_file_info file_info;
	err = unzlocal_GetCurrentFileInfoInternal(uf,&file_info,NULL,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

	if (err!=UNZ_OK)
	{
		fprintf(stderr, "error %d with zipfile in unzGetCurrentFileInfo\n",err);
		return err;
	}

	size_buf = WRITEBUFFERSIZE;
	buf = (void*)gf_malloc(size_buf);
	if (buf==NULL)
	{
		fprintf(stderr, "Error allocating memory\n");
		return UNZ_INTERNALERROR;
	}

	p = filename_withoutpath = filename_inzip;
	while ((*p) != '\0')
	{
		if (((*p)=='/') || ((*p)=='\\'))
			filename_withoutpath = p+1;
		p++;
	}

	if ((*filename_withoutpath)=='\0')
	{
#ifndef _WIN32_WCE
		fprintf(stderr, "creating directory: %s\n",filename_inzip);
		mymkdir(filename_inzip);
#endif
	}
	else
	{
		const char* write_filename;
		int skip=0;

		write_filename = filename_inzip;

		err = unzOpenCurrentFile3(uf, NULL, NULL, 0, NULL/*password*/);
		if (err!=UNZ_OK)
		{
			fprintf(stderr, "error %d with zipfile in unzOpenCurrentFilePassword\n",err);
		}

		if ((skip==0) && (err==UNZ_OK))
		{
			fout = gf_fopen(write_filename,"wb");

			/* some zipfile don't contain directory alone before file */
			if ((fout==NULL) && (filename_withoutpath!=(char*)filename_inzip))
			{
				char c=*(filename_withoutpath-1);
				*(filename_withoutpath-1)='\0';
				makedir(write_filename);
				*(filename_withoutpath-1)=c;
				fout = gf_fopen(write_filename,"wb");
			}

			if (fout==NULL)
			{
				fprintf(stderr, "error opening %s\n",write_filename);
			}
		}

		if (fout!=NULL)
		{
			fprintf(stderr, " extracting: %s\n",write_filename);

			do
			{
				err = unzReadCurrentFile(uf,buf,size_buf);
				if (err<0)
				{
					fprintf(stderr, "error %d with zipfile in unzReadCurrentFile\n",err);
					break;
				}
				if (err>0)
					if (gf_fwrite(buf,err,1,fout)!=1)
					{
						fprintf(stderr, "error in writing extracted file\n");
						err=UNZ_ERRNO;
						break;
					}
			}
			while (err>0);
			if (fout)
				gf_fclose(fout);
		}

		if (err==UNZ_OK)
		{
			err = unzCloseCurrentFile (uf);
			if (err!=UNZ_OK)
			{
				fprintf(stderr, "error %d with zipfile in unzCloseCurrentFile\n",err);
			}
		}
		else
			unzCloseCurrentFile(uf); /* don't lose the error */
	}

	gf_free(buf);
	return err;
}


int gf_unzip_archive(const char *zipfilename, const char *dirname)
{
	uLong i;
	unz_global_info gi;
	int err;

	unzFile uf=NULL;

	uf = unzOpen2(zipfilename, NULL);
	if (uf==NULL)
	{
		fprintf(stderr, "Cannot open %s\n", zipfilename);
		return 1;
	}
#ifndef _WIN32_WCE
	if (chdir(dirname))
	{
		fprintf(stderr, "Error changing into %s, aborting\n", dirname);
		exit(-1);
	}
#endif

	err = unzGetGlobalInfo (uf,&gi);
	if (err!=UNZ_OK)
		fprintf(stderr, "error %d with zipfile in unzGetGlobalInfo \n",err);

	for (i=0; i<gi.number_entry; i++)
	{
		if (do_extract_currentfile(uf) != UNZ_OK)
			break;

		if ((i+1)<gi.number_entry)
		{
			err = unzGoToNextFile(uf);
			if (err!=UNZ_OK)
			{
				fprintf(stderr, "error %d with zipfile in unzGoToNextFile\n",err);
				break;
			}
		}
	}
	unzClose(uf);

	return 0;
}

int gf_unzip_probe(const char *zipfilename)
{
	int ret = 0;
	FILE *f = gf_fopen(zipfilename, "r");
	if (!f) return 0;
	if (fgetc(f)=='P')
		if (fgetc(f)=='K')
			if (fgetc(f)==3)
				if (fgetc(f)==4)
					ret = 1;
	gf_fclose(f);
	return ret;
}
