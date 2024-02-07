/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
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
\file <gpac/crypt_tools.h>
\brief Utility tools for ISMA and Common Encryption.
*/
	
/*!
\addtogroup crypt_grp
\ingroup media_grp
\brief Utility tools for ISMA and Common Encryption.

This section documents the helper tools (mostly crypt info file) used in ISMA and CENC encryption.

@{
 */

#include <gpac/list.h>
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
	GF_CRYPT_TYPE_ADOBE	= GF_4CC('a','d','k','m'),
	/*! PIFF CTR-128 encryption*/
	GF_CRYPT_TYPE_PIFF	= GF_4CC('p','i','f','f'),
	/*! HLS Sample encryption*/
	GF_CRYPT_TYPE_SAES	= GF_4CC('s','a','e','s'),

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

/*! Key info structure, one per defined key in the DRM XML doc*/
typedef struct
{
	//keep first 3 bin128 at the beginning for data alignment
	/*! KEY ID*/
	bin128 KID;
	/*! key value*/
	bin128 key;
	/*! constant IV or initial IV if not constant*/
	u8 IV[16];

	/*! hls_info defined*/
	char *hls_info;
	/*!IV size */
	u8 IV_size;
	/*! constant IV size */
	u8 constant_IV_size;


	/*! DASH representation ID or NULL */
	char *repID;
	/*! DASH period ID or NULL */
	char *periodID;
	/*! DASH adaptationSet ID or NULL */
	u32 ASID;
} GF_CryptKeyInfo;

/*! key roll modes*/
typedef enum
{
	/*! no key roll*/
	GF_KEYROLL_NONE = 0,
	/*! change keys every keyRoll AUs*/
	GF_KEYROLL_SAMPLES,
	/*! roll keys at each SAP type 1 or 2 for streams with SAPs*/
	GF_KEYROLL_SAPS,
	/*! change keys every keyRoll DASH segments*/
	GF_KEYROLL_SEGMENTS,
	/*! change keys every keyRoll periods*/
	GF_KEYROLL_PERIODS,
} GF_KeyRollType;

/*! Crypto information for one media stream*/
typedef struct
{
	/*! scheme type used for encryptio of the track*/
	u32 scheme_type;
	/*! ID of track / PID / ... to be encrypted*/
	u32 trackID;
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
	/*! type of box where sample auxiliary information is saved, either "senc" or "PSEC" (PIFF)*/
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
	/*! number of defined keys*/
	u32 nb_keys;
	/*! keys defined*/
	GF_CryptKeyInfo *keys;

	/*! default key index to use*/
	u32 defaultKeyIdx;
	/*! roll period of keys*/
	u32 keyRoll;
	/*! roll type */
	GF_KeyRollType roll_type;
	/*! number of bytes to leave in the clear for non NAL-based tracks. Only used in cbcs mode*/
	u32 clear_bytes;

	/*! CENS/CBCS pattern */
	u32 crypt_byte_block, skip_byte_block;

	/* ! for avc1 ctr CENC edition 1 */
	Bool allow_encrypted_slice_header;
	/*! force cenc and cbc1: 0: default, 1: no block alignment of encrypted data, 2: always block align even if producing non encrypted samples*/
	u32 block_align;

	/*0: same stsd for clear samples
	1: dedicated stsd entry for clear samples, placed before the crypted entry in stsd,
	2: dedicated stsd entry for clear samples, placed after the crypted entry in stsd,
	*/
	u32 force_clear_stsd_idx;

	/*! adobe metadata in base64*/
	char *metadata;

	/*! force using type set in XML rather than type indicated in file when decrypting*/
	Bool force_type;

	/*! generate random keys and key values*/
	Bool rand_keys;

	/*! randomly encrypts subsample if rand() % subs_rand is 0*/
	u32 subs_rand;
	/*! list of VCL NAL/OBU indices to encrypt, 1-based*/
	char *subs_crypt;
	/*! use multiple keys per sample*/
	Bool multi_key;
	/*! roll key over subsamples. If 0, roll by 1 every encrypted sample. If 1 (-1==0) disable key roll*/
	u32 mkey_roll_plus_one;
	/*!coma-separated list of indices of keys to use per subsample. Value 0 means keep clear. If less indices than subsamples, keep subsamples in clear*/
	char *mkey_subs;
} GF_TrackCryptInfo;

/*! Crypto information*/
typedef struct
{
	/*! list of track infos*/
	GF_List *tcis;
	/*! global for all tracks unless overridden*/
	u32 def_crypt_type;
	/*! indicates a common key is used*/
	Bool has_common_key;
	/*! intern to parser*/
	Bool in_text_header;
	/*! intern to parser*/
	GF_Err last_parse_error;
} GF_CryptInfo;

/*! loads a given crypto configuration file. Full doc is available at https://gpac.io/mp4box/encryption/common-encryption/
\param file name of the crypt XML file
\param out_err set to return error
\return the crypt info
*/
GF_CryptInfo *gf_crypt_info_load(const char *file, GF_Err *out_err);

/*! deletes crypto configuration file.
\param info the target crypt info
*/
void gf_crypt_info_del(GF_CryptInfo *info);

#if !defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_ISOM_WRITE)

/*! decrypts a file
\param infile source MP4 file to decrypt
\param drm_file location of crypto configuration file
\param outname location of destination file
\param interleave_time interleave time of the destination file - 0 means flat storage
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_decrypt_file(GF_ISOFile *infile, const char *drm_file, const char *outname, Double interleave_time, u32 fs_dump_flags);

/*! decrypts a fragment
\param infile source MP4 file to decrypt
\param drm_file location of crypto configuration file
\param dst_file location of destination file
\param frag_name name of fragment to decrypt
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_decrypt_fragment(GF_ISOFile *infile, const char *drm_file, const char *dst_file, const char *frag_name, u32 fs_dump_flags);

/*! encrypts a file
\param infile source MP4 file to encrypt
\param drm_file location of crypto configuration file
\param outname location of destination file
\param interleave_time interleave time of the destination file - 0 means flat storage
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_crypt_file(GF_ISOFile *infile, const char *drm_file, const char *outname, Double interleave_time, u32 fs_dump_flags);

/*! encrypts a fragment
\param infile init segment of the MP4 file to encrypt - this SHALL NOT be encrypted
\param drm_file location of crypto configuration file
\param dst_file location of destination file
\param frag_name name of fragment to encrypt
\param fs_dump_flags flags for session stats (1) and session graph (1<<1) dumping
\return error code if any
*/
GF_Err gf_crypt_fragment(GF_ISOFile *infile, const char *drm_file, const char *dst_file, const char *frag_name, u32 fs_dump_flags);

#endif /*!defined(GPAC_DISABLE_CRYPTO) && !defined(GPAC_DISABLE_ISOM_WRITE)*/

/*! @} */

#ifdef __cplusplus
}
#endif


#endif	/*_GF_CRYPT_TOOLS_H_*/

