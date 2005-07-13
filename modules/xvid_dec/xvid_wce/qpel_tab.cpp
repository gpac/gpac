#include "portab.h"

//----------------------------
// Quarterpel FIR definition

static const int FIR_Tab_16[17][16] = {
	{ 14, -3,  2, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ 23, 19, -6,  3, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ -7, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0,  0,  0,  0,  0 },
	{ -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0,  0,  0,  0 },
	{  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0,  0,  0 },
	{  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0,  0 },
	{  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0,  0 },
	{  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0,  0 },
	{  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0,  0 },
	{  0,  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0,  0 },
	{  0,  0,  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1,  0 },
	{  0,  0,  0,  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3, -1 },
	{  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -6,  3 },
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  3, -6, 20, 20, -7 },
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  3, -6, 19, 23 },
	{  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, -1,  2, -3, 14 }
};

static const int FIR_Tab_8[9][8] = {
	{ 14, -3,  2, -1,  0,  0,  0,  0 },
	{ 23, 19, -6,  3, -1,  0,  0,  0 },
	{ -7, 20, 20, -6,  3, -1,  0,  0 },
	{  3, -6, 20, 20, -6,  3, -1,  0 },
	{ -1,  3, -6, 20, 20, -6,  3, -1 },
	{  0, -1,  3, -6, 20, 20, -6,  3 },
	{  0,  0, -1,  3, -6, 20, 20, -7 },
	{  0,  0,  0, -1,  3, -6, 19, 23 },
	{  0,  0,  0,  0, -1,  2, -3, 14 }
};


//----------------------------

/* Implementation
 ****************************************************************************/

/* 16x? filters */

#define SIZE  16
#define TABLE FIR_Tab_16

#define STORE(d,s)  (d) = (s)
#define FUNC_H      H_Pass_16_C
#define FUNC_V      V_Pass_16_C
#define FUNC_HA     H_Pass_Avrg_16_C
#define FUNC_VA     V_Pass_Avrg_16_C
#define FUNC_HA_UP  H_Pass_Avrg_Up_16_C
#define FUNC_VA_UP  V_Pass_Avrg_Up_16_C

#include "qpel.inl"

/* note: B-frame always uses Rnd=0... */
#define STORE(d,s)  (d) = ( (s)+(d)+1 ) >> 1
#define FUNC_H      H_Pass_16_Add_C
#define FUNC_V      V_Pass_16_Add_C
#define FUNC_HA     H_Pass_Avrg_16_Add_C
#define FUNC_VA     V_Pass_Avrg_16_Add_C
#define FUNC_HA_UP  H_Pass_Avrg_Up_16_Add_C
#define FUNC_VA_UP  V_Pass_Avrg_Up_16_Add_C

#include "qpel.inl"

#undef SIZE
#undef TABLE

/* 8x? filters */

#define SIZE  8
#define TABLE FIR_Tab_8

#define STORE(d,s)  (d) = (s)
#define FUNC_H      H_Pass_8_C
#define FUNC_V      V_Pass_8_C
#define FUNC_HA     H_Pass_Avrg_8_C
#define FUNC_VA     V_Pass_Avrg_8_C
#define FUNC_HA_UP  H_Pass_Avrg_Up_8_C
#define FUNC_VA_UP  V_Pass_Avrg_Up_8_C

#include "qpel.inl"

/* note: B-frame always uses Rnd=0... */
#define STORE(d,s)  (d) = ( (s)+(d)+1 ) >> 1
#define FUNC_H      H_Pass_8_Add_C
#define FUNC_V      V_Pass_8_Add_C
#define FUNC_HA     H_Pass_Avrg_8_Add_C
#define FUNC_VA     V_Pass_Avrg_8_Add_C
#define FUNC_HA_UP  H_Pass_Avrg_Up_8_Add_C
#define FUNC_VA_UP  V_Pass_Avrg_Up_8_Add_C

#include "qpel.inl"

#undef SIZE
#undef TABLE

//----------------------------

