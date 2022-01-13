	/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2021-2022
 *					All rights reserved
 *
 *  This file is part of GPAC / NodeJS module
 *
 *  GPAC is free software; you can redistribute it and/or modify
 *  it under the terfsess of the GNU Lesser General Public License as published by
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

#ifndef _GPAC_NAPI_H
#define _GPAC_NAPI_H

#include <node_api.h>

#include <gpac/tools.h>
#include <gpac/filters.h>
#include <gpac/version.h>


#define NAPI_CALL(_a) \
		status = _a ; \
		if (status != napi_ok) { \
			napi_throw_error(env, NULL, "NAPI call failed"); \
			return NULL;\
		}

#define NARG_ARGS(_N, _MIN) \
	napi_status status; \
	size_t argc = _N;\
	napi_value argv[_N];\
	status = napi_get_cb_info(env, info, &argc, argv, NULL, NULL);\
	if (status != napi_ok) {\
		napi_throw_error(env, NULL, "Failed to parse arguments");\
		return NULL;\
	}\
	if (argc<_MIN) {\
		napi_throw_error(env, NULL, "Not enough arguments");\
		return NULL;\
	}\

#define NARG_ARGS_THIS(_N, _MIN) \
	napi_status status; \
	napi_value this_val;\
	size_t argc = _N;\
	napi_value argv[_N];\
	status = napi_get_cb_info(env, info, &argc, argv, &this_val, NULL);\
	if (status != napi_ok) {\
		napi_throw_error(env, NULL, "Failed to parse arguments");\
		return NULL;\
	}\
	if (argc<_MIN) {\
		napi_throw_error(env, NULL, "Not enough arguments");\
		return NULL;\
	}\

#define NARG_ARGS_THIS_MAGIC(_N, _MIN) \
	napi_status status; \
	napi_value this_val;\
	void *magic=NULL;\
	size_t argc = _N;\
	napi_value argv[_N];\
	status = napi_get_cb_info(env, info, &argc, argv, &this_val, &magic);\
	if ((status != napi_ok) || !magic) {\
		napi_throw_error(env, NULL, "Failed to parse arguments");\
		return NULL;\
	}\
	if (argc<_MIN) {\
		napi_throw_error(env, NULL, "Not enough arguments");\
		return NULL;\
	}\
	else if (argc>_N) {\
		GF_LOG(GF_LOG_WARNING, GF_LOG_SCRIPT, ("[NodeJS] Too many arguments\n"));\
	}\

#define NARG_THIS\
	napi_status status;\
	napi_value this_val;\
	NAPI_CALL(napi_get_cb_info(env, info, NULL, NULL, &this_val, NULL) );


#define NARG_U32(_val, _argidx, def_val) \
	u32 _val;\
	if (_argidx<argc) { \
		NAPI_CALL( napi_get_value_uint32(env, argv[_argidx], &_val) ); \
	} else { \
		_val = def_val; \
	}

#define NARG_U64(_val, _argidx, def_val) \
	u64 _val;\
	if (_argidx<argc) { \
		NAPI_CALL( napi_get_value_int64(env, argv[_argidx], (s64*) &_val) ); \
	} else { \
		_val = def_val; \
	}

#define NARG_S32(_val, _argidx, def_val) \
	s32 _val;\
	if (_argidx<argc) { \
		NAPI_CALL( napi_get_value_int32(env, argv[_argidx], &_val) ); \
	} else { \
		_val = def_val; \
	}

#define NARG_BOOL(_val, _argidx, def_val) \
	Bool _val;\
	if (_argidx<argc) { \
		NAPI_CALL( napi_get_value_bool(env, argv[_argidx], (bool *) &_val) ); \
	} else { \
		_val = def_val; \
	}

#define NARG_DBL(_val, _argidx, def_val) \
	Double _val;\
	if (_argidx<argc) { \
		NAPI_CALL( napi_get_value_double(env, argv[_argidx], &_val) ); \
	} else { \
		_val = def_val; \
	}

#define NARG_STR(_val, _argidx, _def_val) \
	char *_val;\
	if (_argidx<argc) { \
		GPAC_NAPI *gpac; \
		NAPI_CALL( napi_get_instance_data(env, (void **) &gpac) ); \
		size_t s;\
		napi_valuetype vtype;\
		NAPI_CALL( napi_typeof(env, argv[_argidx], &vtype) );\
  		if (vtype == napi_null) {\
			_val = NULL;\
		} else {\
			NAPI_CALL( napi_get_value_string_utf8(env, argv[_argidx], NULL, 0, &s) ); \
			if (s+1 > gpac->str_buf_alloc) {\
				gpac->str_buf = gf_realloc(gpac->str_buf, sizeof(char) * (s+1)); \
				if (!gpac->str_buf) {\
					napi_throw_error(env, NULL, "Out of memory");\
					return NULL;\
				}\
				gpac->str_buf_alloc = s+1;\
			}\
			NAPI_CALL( napi_get_value_string_utf8(env, argv[_argidx], gpac->str_buf, gpac->str_buf_alloc, &s) ); \
			gpac->str_buf[s]=0;\
			_val = gpac->str_buf; \
		}\
	} else { \
		_val = _def_val; \
	}

#define NARG_STR_ALLOC(_val, _argidx, _def_val) \
	char *_val;\
	if (_argidx<argc) { \
		GPAC_NAPI *gpac; \
		NAPI_CALL( napi_get_instance_data(env, (void **) &gpac) ); \
		size_t s;\
		napi_valuetype vtype;\
		NAPI_CALL( napi_typeof(env, argv[_argidx], &vtype) );\
  		if (vtype == napi_null) {\
			_val = NULL;\
		} else {\
			NAPI_CALL( napi_get_value_string_utf8(env, argv[_argidx], NULL, 0, &s) ); \
			_val = gf_malloc(sizeof(char) + (s+1)); \
			if (!_val) {\
				napi_throw_error(env, NULL, "Out of memory");\
				return NULL;\
			}\
			status = napi_get_value_string_utf8(env, argv[_argidx], _val, s+1, &s); \
			_val[s]=0;\
		}\
	} else { \
		_val = _def_val ? gf_strdup(_def_val) : NULL; \
	}

#define GET_PROP_FIELD_S32(_name, _class, _target) \
		status = napi_has_named_property(env, prop, _name, (bool *) &has_p); \
		if (has_p) status = napi_get_named_property(env, prop, _name, &v); \
		if (status==napi_ok) { \
			status = napi_get_value_int32(env, v, &_target); \
			if (status) return status;\
		} else { \
			napi_throw_error(env, NULL, _class" missing "_name" property"); \
			return status; \
		}

#define GET_PROP_FIELD_U32(_name, _class, _target) \
		status = napi_has_named_property(env, prop, _name, (bool *) &has_p); \
		if (has_p) status = napi_get_named_property(env, prop, _name, &v); \
		if (status==napi_ok) { \
			status = napi_get_value_uint32(env, v, &_target); \
			if (status) return status;\
		} else { \
			napi_throw_error(env, NULL, _class" missing "_name" property"); \
			return status; \
		}

#define GET_PROP_FIELD_S64(_name, _class, _target) \
		status = napi_has_named_property(env, prop, _name, (bool *) &has_p); \
		if (has_p) status = napi_get_named_property(env, prop, _name, &v); \
		if (status==napi_ok) { \
			status = napi_get_value_int64(env, v, &_target); \
			if (status) return status;\
		} else { \
			napi_throw_error(env, NULL, _class" missing "_name" property"); \
			return status; \
		}

#define GET_PROP_FIELD_U64(_name, _class, _target) \
		status = napi_has_named_property(env, prop, _name, (bool *) &has_p); \
		if (has_p) status = napi_get_named_property(env, prop, _name, &v); \
		if (status==napi_ok) { \
			status = napi_get_value_int64(env, v, (s64 *) &_target); \
			if (status) return status;\
		} else { \
			napi_throw_error(env, NULL, _class" missing "_name" property"); \
			return status; \
		}

#define GET_PROP_FIELD_FLT(_name, _class, _target) \
		status = napi_has_named_property(env, prop, _name, (bool *) &has_p); \
		if (has_p) status = napi_get_named_property(env, prop, _name, &v); \
		if (status==napi_ok) { \
			Double n;\
			status = napi_get_value_double(env, v, &n); \
			if (status) return status;\
			_target = FLT2FIX(n);\
		} else { \
			napi_throw_error(env, NULL, _class" missing "_name" property"); \
			return status; \
		}

#endif //_GPAC_NAPI_H
