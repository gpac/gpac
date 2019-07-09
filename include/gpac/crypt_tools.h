/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / Media Tools sub-project
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

#ifndef _GF_CRYPT_TOOLS_H_
#define _GF_CRYPT_TOOLS_H_


#ifdef __cplusplus
extern "C" {
#endif

/*!
 *	\file <gpac/crypt_tools.h>
 *	\brief Utility tools for ISMA and Common Encryption.
 */
	
/*!
 *	\addtogroup crypt_grp
 *	\ingroup media_grp
 *	\brief Utility tools for ISMA and Common Encryption.
 *
 *This section documents the helper tools (mostly crypt info file) used in ISMA and CENC encryption.
 *	@{
 */

#include <gpac/isomedia.h>

/*! Supported encryption scheme types */
enum
{
	/*! ISMA E&A encryption*/
	GF_CRYPT_TYPE_ISMA	= GF_4CC( 'i', 'A', 'E', 'C' ),
	/*! OMA DRM encryption*/
	GF_CRYPT_TYPE_OMA = GF_4CC( 'o', 'd', 'k', 'm' ),
	/*! CENC CTR-128 encryption*/
	GF_CRYPT_TYPE_CENC	= GF_4CC('c','e','n','c'),
	/*! CENC CBC-128 encryption*/
	GF_CRYPT_TYPE_CBC1	= GF_4CC('c','b','c','1'),
	/*! CENC CTR-128 pattern encryption*/
	GF_CRYPT_TYPE_CENS	= GF_4CC('c','e','n','s'),
	/*! CENC CBC-128 pattern encryption*/
	GF_CRYPT_TYPE_CBCS	= GF_4CC('c','b','c','s'),
	/*! Adobe CBC-128 encryption*/
	GF_CRYPT_TYPE_ADOBE	= GF_4CC('a','d','k','m')
};

/*! Selective encryption modes */
enum
{
	/*! no selective encryption*/
	GF_CRYPT_SELENC_NONE = 0,
	/*! only encrypts RAP samples*/
	GF_CRYPT_SELENC_RAP,
	/*! only encrypts non-RAP samples*/
	GF_CRYPT_SELENC_NON_RAP,
	/*! selective encryption of random samples*/
	GF_CRYPT_SELENC_RAND,
	/*! selective encryption of a random sample in given range*/
	GF_CRYPT_SELENC_RAND_RANGE,
	/*! selective encryption of first sample in given range*/
	GF_CRYPT_SELENC_RANGE,
	/*! encryption of all samples but the preview range*/
	GF_CRYPT_SELENC_PREVIEW,
	/*!no encryption of samples, signaled with sample group*/
	GF_CRYPT_SELENC_CLEAR,
	/*!no encryption of samples, signaled without sample group but bytesOfEncrypted = 0*/
	GF_CRYPT_SELENC_CLEAR_FORCED,
};

typedef struct
{
	/*! scheme type used for encryptio of the track*/
	u32 scheme_type;
	/*! ID of track / PID / ... to be encrypted*/
	u32 trackID;
	/*! Initialization vector of the first sample/AU in track/media for CENC. For IMA/OMA, the first 8 bytes contain the salt data.*/
	unsigned char first_IV[16];
	/*! URI of key management system / rightsIssuerURL*/
	char *KMS_URI;
	/*! Scheme URI or contentID for OMA*/
	char *Scheme_URI;
	/*! selective encryption type*/
	u32 sel_enc_type;
	/*! for OMA, sets preview range in samples. Otherwise sets encryption AU frequency (encrypts 1 AU every sel_enc_range ones)*/
	u32 sel_enc_range;
	/*! IPMP signaling: 0: none, 1: IPMP, 2: IPMPX
	when IPMP signaling is enabled, the OD stream will be updated with IPMP Update commands
		THIS IS DEPRECATED IN GPAC
	*/
	u32 ipmp_type;
	/*! if not set and IPMP enabled, defaults to TrackID
		THIS IS DEPRECATED IN GPAC
	*/
	u32 ipmp_desc_id;
	/*! type of box where sample auxiliary informations is saved, either "senc" or "PSEC" (PIFF)*/
	u32 sai_saved_box_type;

	/*! OMA encryption type: 0: none, 1: AES CBC, 2: AES CTR*/
	u8 encryption;
	/*! OMA textual headers*/
	char *TextualHeaders;
	/*! OMA transaction ID*/
	char TransactionID[17];

	/*! CENC extensions - TODO, we could extend the support to allow per key patterns and selective encryption modes
	and also add support for multiple keys in ISMA ?*/
	/*! default encryption state for samples*/
	u32 IsEncrypted;
	/*! size of init vector: 0, 8 or 16*/
	u8 IV_size;
	/*! number of KEY IDs and Keys defined*/
	u32 KID_count;
	/*! KEY IDs defined*/
	bin128 *KIDs;
	/*! keys defined*/
	bin128 *keys;
	/*! default key index to use*/
	u32 defaultKeyIdx;
	/*! roll period of keys (change keys every keyRoll AUs)*/
	u32 keyRoll;
	/*! number of bytes to leave in the clear for non NAL-based tracks. Only used in cbcs mode*/
	u32 clear_bytes;
	/*! CENS/CBCS pattern */
	u8 crypt_byte_block, skip_byte_block;
	/*! cponstant IV size */
	u8 constant_IV_size;
	/*! constant IV */
	unsigned char constant_IV[16];

	/* ! for avc1 ctr CENC edition 1 */
	Bool allow_encrypted_slice_header;
	/*! force cenc and cbc1: 0: default, 1: no block alignment of encrypted data, 2: always block align even if producing non encrypted samples*/
	u32 block_align;

	/*! adobe metadata in base64*/
	char *metadata;
} GF_TrackCryptInfo;

typedef struct
{
	/*! list of track infos*/
	GF_List *tcis;
	/*! global for all tracks unless overriden*/
	u32 def_crypt_type;
	/*! indicates a common key is used*/
	Bool has_common_key;
	/*! intern to parser*/
	Bool in_text_header;
} GF_CryptInfo;

/*! loads Crypt config file. Full doc is available at https://gpac.wp.imt.fr/mp4box/encryption/common-encryption/
\param file name of the crypt XML file
\return the crypt info
*/
GF_CryptInfo *gf_crypt_info_load(const char *file);

/*! deletes crypt config file.
\param file name of the crypt XML file
*/
void gf_crypt_info_del(GF_CryptInfo *info);

#if !defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_ISOM_WRITE)

/*! decrypts a file
\param infile source MP4 file to decrypt
\param drm_file location of crypt info data
\param outname location of destination file
\param interleave_time interleave time of the destination file - 0 means flat storage
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_decrypt_file(GF_ISOFile *infile, const char *drm_file, const char *outname, Double interleave_time, u32 fs_dump_flags);

/*! encrypts a file
\param infile source MP4 file to encrypt
\param drm_file location of crypt info data
\param outname location of destination file
\param interleave_time interleave time of the destination file - 0 means flat storage
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_crypt_file(GF_ISOFile *infile, const char *drm_file, const char *outname, Double interleave_time, u32 fs_dump_flags);

/*! encrypts a fragment
\param infile init segment of the MP4 file to encrypt - this SHALL NOT be encrypted
\param drm_file location of crypt info data
\param outname location of destination file
\param frag_name name of fragment to encrypt
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_crypt_fragment(GF_ISOFile *mp4, const char *drm_file, const char *dst_file, const char *frag_name, u32 fs_dump_flags);

#endif /*!defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_ISOM_WRITE)*/

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_CRYPT_TOOLS_H_*/

