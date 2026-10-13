#include "tests.h"

#include "../crypt_tools.c"

char gf_prog_lf = '\r';

unittest(crypt_info_load_cpix)
{
	static const char cpix[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<cpix:CPIX version=\"2.3\" xmlns:cpix=\"urn:dashif:org:cpix\" xmlns:pskc=\"urn:ietf:params:xml:ns:keyprov:pskc\">"
		"<cpix:ContentKeyList><cpix:ContentKey kid=\"01234567-8901-2345-6789-012345678901\" explicitIV=\"ASNFZ4kBI0VniQEjRWeJAQ==\" commonEncryptionScheme=\"cenc\">"
		"<cpix:Data><pskc:Secret><pskc:PlainValue>Fzsa6fC/yLr6IPmeug0H2Q==</pskc:PlainValue></pskc:Secret></cpix:Data>"
		"</cpix:ContentKey></cpix:ContentKeyList>"
		"<cpix:DRMSystemList>"
		"<cpix:DRMSystem kid=\"01234567-8901-2345-6789-012345678901\" systemId=\"edef8ba9-79d6-4ace-a3c8-27dcd51d21ed\">"
		"<cpix:PSSH>AAAAP3Bzc2gAAAAA7e+LqXnWSs6jyCfc1R0h7QAAAB8SEAEjRWeJASNFZ4kBI0VniQEaBWV6ZHJtSOPclZsG</cpix:PSSH>"
		"</cpix:DRMSystem>"
		"<cpix:DRMSystem kid=\"01234567-8901-2345-6789-012345678901\" systemId=\"94ce86fb-07ff-4f43-adb8-93d2fa968ca2\">"
		"<cpix:HLSSignalingData playlist=\"media\">I0VYVC1YLUtFWTpNRVRIT0Q9U0FNUExFLUFFUyxVUkk9InNrZDovLzAxMjM0NTY3LTg5MDEtMjM0NS02Nzg5LTAxMjM0NTY3ODkwMTowMTIzNDU2Nzg5MDEyMzQ1Njc4OTAxMjM0NTY3ODkwMSIsS0VZRk9STUFUPSJjb20uYXBwbGUuc3RyZWFtaW5na2V5ZGVsaXZlcnkiLEtFWUZPUk1BVFZFUlNJT05TPSIxIg==</cpix:HLSSignalingData>"
		"</cpix:DRMSystem>"
		"</cpix:DRMSystemList>"
		"<cpix:ContentKeyUsageRuleList><cpix:ContentKeyUsageRule kid=\"01234567-8901-2345-6789-012345678901\"/></cpix:ContentKeyUsageRuleList>"
		"</cpix:CPIX>";
	GF_Blob blob;
	char *blob_url = NULL;
	GF_CryptInfo *info;
	GF_TrackCryptInfo *tci;
	GF_CryptDRMInfo *drm;
	GF_Err e;
	static const u8 expected_key[16] = {0x17,0x3b,0x1a,0xe9,0xf0,0xbf,0xc8,0xba,0xfa,0x20,0xf9,0x9e,0xba,0x0d,0x07,0xd9};
	static const u8 expected_system[16] = {0xed,0xef,0x8b,0xa9,0x79,0xd6,0x4a,0xce,0xa3,0xc8,0x27,0xdc,0xd5,0x1d,0x21,0xed};
	
	/* gf_file_temp() is allowed to use an unnamed system temporary file, so
	 * there is no portable path to pass back to gf_crypt_info_load(). */
	memset(&blob, 0, sizeof(blob));
	blob.data = (u8 *) cpix;
	blob.size = sizeof(cpix) - 1;
	blob_url = gf_blob_register(&blob);
	assert_not_null(blob_url);
	if (!blob_url) return;
	info = gf_crypt_info_load(blob_url, &e);
	assert_equal(e, GF_OK, "%d");
	assert_not_null(info);
	assert_true(info->is_cpix);
	assert_equal(gf_list_count(info->tcis), 1, "%u");
	assert_equal(gf_list_count(info->drm_infos), 2, "%u");
	tci = (GF_TrackCryptInfo *) gf_list_get(info->tcis, 0);
	assert_equal(tci->scheme_type, GF_CRYPT_TYPE_CENC, "%u");
	assert_equal_mem(tci->keys[0].key, expected_key, 16);
	assert_equal(tci->keys[0].IV_size, 16, "%u");
	assert_equal_str(tci->keys[0].hls_info, "URI=\"skd://01234567-8901-2345-6789-012345678901:01234567890123456789012345678901\",KEYFORMAT=\"com.apple.streamingkeydelivery\",KEYFORMATVERSIONS=\"1\"");
	drm = (GF_CryptDRMInfo *) gf_list_get(info->drm_infos, 0);
	assert_equal(drm->version, 0, "%u");
	assert_equal(drm->private_data_size, 31, "%u");
	assert_equal_mem(drm->systemID, expected_system, 16);
	gf_crypt_info_del(info);
	gf_blob_unregister(&blob);
	if (blob_url) gf_free(blob_url);
}
