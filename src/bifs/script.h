/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre 
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
 *
 *  This file is part of GPAC / BIFS codec sub-project
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

#ifndef _SFSCRIPT_H
#define _SFSCRIPT_H

#include <gpac/internal/bifs_dev.h>

#if !defined(GPAC_DISABLE_BIFS) && defined(GPAC_HAS_SPIDERMONKEY)

#define NUMBITS_STATEMENT	3
#define NUMBITS_EXPR_TYPE	6
#define MAX_NUM_EXPR		100

enum 
{
	ST_IF=0, 
	ST_FOR=1, 
	ST_WHILE=2, 
	ST_RETURN=3, 
	ST_BREAK=5, 
	ST_CONTINUE=6, 
	ST_COMPOUND_EXPR=4,
	ST_SWITCH=7
};

enum
{
	ET_CURVED_EXPR=0,
	ET_NEGATIVE=1,
	ET_NOT=2,
	ET_ONESCOMP=3,
	ET_INCREMENT=4,
	ET_DECREMENT=5,
	ET_POST_INCREMENT=6,
	ET_POST_DECREMENT=7,
	ET_CONDTEST=8,
	ET_STRING=9,
	ET_NUMBER=10,
	ET_IDENTIFIER=11,
	ET_FUNCTION_CALL=12,
	ET_NEW=13, ET_OBJECTCONSTRUCT=13,
	ET_OBJECT_MEMBER_ACCESS=14,
	ET_OBJECT_METHOD_CALL=15,
	ET_ARRAY_DEREFERENCE=16,
	ET_ASSIGN=17,
	ET_PLUSEQ=18,
	ET_MINUSEQ=19,
	ET_MULTIPLYEQ=20,
	ET_DIVIDEEQ=21,
	ET_MODEQ=22,
	ET_ANDEQ=23,
	ET_OREQ=24,
	ET_XOREQ=25,
	ET_LSHIFTEQ=26,
	ET_RSHIFTEQ=27,
	ET_RSHIFTFILLEQ=28,
	ET_EQ=29,
	ET_NE=30,
	ET_LT=31,
	ET_LE=32,
	ET_GT=33,
	ET_GE=34,
	ET_PLUS=35,
	ET_MINUS=36,
	ET_MULTIPLY=37,
	ET_DIVIDE=38,
	ET_MOD=39,
	ET_LAND=40,
	ET_LOR=41,
    ET_AND=42,
	ET_OR=43,
	ET_XOR=44,
	ET_LSHIFT=45,
	ET_RSHIFT=46,
	ET_RSHIFTFILL=47,
	ET_BOOLEAN=48,
	ET_VAR=49,
	ET_FUNCTION_ASSIGN=50,
	NUMBER_OF_EXPR_TYPE=51
};

GF_Err SFScript_Parse(GF_BifsDecoder *codec, SFScript *script_field, GF_BitStream *bs, GF_Node *n);
#ifndef GPAC_DISABLE_BIFS_ENC
GF_Err SFScript_Encode(GF_BifsEncoder *codec, SFScript *script_field, GF_BitStream *bs, GF_Node *n);
#endif

#endif /* !defined(GPAC_DISABLE_BIFS) && defined(GPAC_HAS_SPIDERMONKEY) */

#endif
