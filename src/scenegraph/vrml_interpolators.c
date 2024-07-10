/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / Scene Graph sub-project
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


#include <gpac/internal/scenegraph_dev.h>
#include <gpac/nodes_mpeg4.h>
#include <gpac/nodes_x3d.h>


GF_EXPORT
SFRotation gf_sg_sfrotation_interpolate(SFRotation kv1, SFRotation kv2, Fixed fraction)
{
	SFRotation res;
	Fixed newa, olda;
	Bool stzero = ( ABS(kv1.q) < FIX_EPSILON) ? 1 : 0;
	Bool endzero = ( ABS(kv2.q) < FIX_EPSILON) ? 1 : 0;
	Fixed testa = gf_mulfix(kv1.x, kv2.x) + gf_mulfix(kv1.y, kv2.y) + gf_mulfix(kv1.y, kv2.y);

	if (testa>= 0) {
		res.x = kv1.x + gf_mulfix(fraction, kv2.x-kv1.x);
		res.y = kv1.y + gf_mulfix(fraction, kv2.y-kv1.y);
		res.z = kv1.z + gf_mulfix(fraction, kv2.z-kv1.z);
		newa = kv2.q;
	} else {
		res.x = kv1.x + gf_mulfix(fraction, -kv2.x -kv1.x);
		res.y = kv1.y + gf_mulfix(fraction, -kv2.y-kv1.y);
		res.z = kv1.z + gf_mulfix(fraction, -kv2.z-kv1.z);
		newa = -kv2.q;
	}
	olda = kv1.q;
	if (stzero || endzero) {
		res.x = stzero ? kv2.x : kv1.x;
		res.y = stzero ? kv2.y : kv1.y;
		res.z = stzero ? kv2.z : kv1.z;
	}
	res.q = olda + gf_mulfix(fraction, newa - olda);
	if (res.q > GF_2PI) {
		res.q -= GF_2PI;
	} else if (res.q < GF_2PI) {
		res.q += GF_2PI;
	}
	return res;
}

#ifndef GPAC_DISABLE_VRML

static Fixed Interpolate(Fixed keyValue1, Fixed keyValue2, Fixed fraction)
{
	return gf_mulfix(keyValue2 - keyValue1, fraction) + keyValue1;
}

static Fixed GetInterpolateFraction(Fixed key1, Fixed key2, Fixed fraction)
{
	Fixed keyDiff = key2 - key1;
	gf_assert((fraction >= key1) && (fraction <= key2));
	if (ABS(keyDiff) < FIX_EPSILON) return 0;
	return gf_divfix(fraction - key1, keyDiff);
}

static void CI2D_SetFraction(GF_Node *n, GF_Route *route)
{
	Fixed frac;
	u32 numElemPerKey, i, j;
	M_CoordinateInterpolator2D *_this = (M_CoordinateInterpolator2D *) n;

	if (! _this->key.count) return;
	if (_this->keyValue.count % _this->key.count) return;

	numElemPerKey = _this->keyValue.count / _this->key.count;
	//set size
	if (_this->value_changed.count != numElemPerKey)
		gf_sg_vrml_mf_alloc(&_this->value_changed, GF_SG_VRML_MFVEC2F, numElemPerKey);


	if (_this->set_fraction < _this->key.vals[0]) {
		for (i=0; i<numElemPerKey; i++)
			_this->value_changed.vals[i] = _this->keyValue.vals[i];
	} else if (_this->set_fraction > _this->key.vals[_this->key.count - 1]) {
		for (i=0; i<numElemPerKey; i++)
			_this->value_changed.vals[i] = _this->keyValue.vals[(_this->keyValue.count) - numElemPerKey + i];
	} else {
		for (j = 1; j < _this->key.count; j++) {
			// Find the key values the fraction lies between
			if ( _this->set_fraction < _this->key.vals[j-1]) continue;
			if (_this->set_fraction >= _this->key.vals[j]) continue;

			frac = GetInterpolateFraction(_this->key.vals[j-1], _this->key.vals[j], _this->set_fraction);
			for (i=0; i<numElemPerKey; i++) {
				_this->value_changed.vals[i].x = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].x,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].x,
				                                 frac);
				_this->value_changed.vals[i].y = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].y,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].y,
				                                 frac);
			}
			break;
		}
	}
	//invalidate
	gf_node_event_out(n, 3);//"value_changed"
}

Bool InitCoordinateInterpolator2D(M_CoordinateInterpolator2D *node)
{
	node->on_set_fraction = CI2D_SetFraction;

	if (node->key.count && !(node->keyValue.count % node->key.count) ) {
		u32 numElemPerKey, i;
		numElemPerKey = node->keyValue.count / node->key.count;
		gf_sg_vrml_mf_alloc(&node->value_changed, GF_SG_VRML_MFVEC2F, numElemPerKey);
		for (i=0; i<numElemPerKey; i++)
			node->value_changed.vals[i] = node->keyValue.vals[i];
	}
	return 1;
}

static Bool CI_SetFraction(Fixed fraction, MFVec3f *vals, MFFloat *key, MFVec3f *keyValue)
{
	Fixed frac;
	u32 numElemPerKey, i, j;

	if (! key->count) return 0;
	if (keyValue->count % key->count) return 0;

	numElemPerKey = keyValue->count / key->count;

	if (vals->count != numElemPerKey) gf_sg_vrml_mf_alloc(vals, GF_SG_VRML_MFVEC3F, numElemPerKey);

	if (fraction < key->vals[0]) {
		for (i=0; i<numElemPerKey; i++)
			vals->vals[i] = keyValue->vals[i];
	} else if (fraction > key->vals[key->count - 1]) {
		for (i=0; i<numElemPerKey; i++)
			vals->vals[i] = keyValue->vals[(keyValue->count) - numElemPerKey + i];
	} else {
		for (j = 1; j < key->count; j++) {
			// Find the key values the fraction lies between
			if (fraction < key->vals[j-1]) continue;
			if (fraction >= key->vals[j]) continue;

			frac = GetInterpolateFraction(key->vals[j-1], key->vals[j], fraction);
			for (i=0; i<numElemPerKey; i++) {
				vals->vals[i].x = Interpolate(keyValue->vals[(j-1)*numElemPerKey + i].x,
				                              keyValue->vals[(j)*numElemPerKey + i].x,
				                              frac);
				vals->vals[i].y = Interpolate(keyValue->vals[(j-1)*numElemPerKey + i].y,
				                              keyValue->vals[(j)*numElemPerKey + i].y,
				                              frac);
				vals->vals[i].z = Interpolate(keyValue->vals[(j-1)*numElemPerKey + i].z,
				                              keyValue->vals[(j)*numElemPerKey + i].z,
				                              frac);
			}
			break;
		}
	}
	return 1;
}


static void CoordInt_SetFraction(GF_Node *n, GF_Route *route)
{
	M_CoordinateInterpolator *_this = (M_CoordinateInterpolator *) n;

	if (CI_SetFraction(_this->set_fraction, &_this->value_changed, &_this->key, &_this->keyValue))
		gf_node_event_out(n, 3);//"value_changed"
}

Bool InitCoordinateInterpolator(M_CoordinateInterpolator *n)
{
	n->on_set_fraction = CoordInt_SetFraction;
	CI_SetFraction(0, &n->value_changed, &n->key, &n->keyValue);
	return 1;
}

static void NormInt_SetFraction(GF_Node *n, GF_Route *route)
{
	u32 i;
	M_NormalInterpolator *_this = (M_NormalInterpolator *) n;

	if (!CI_SetFraction(_this->set_fraction, &_this->value_changed, &_this->key, &_this->keyValue)) return;
	/*renorm*/
	for (i=0; i<_this->value_changed.count; i++) {
		gf_vec_norm(&_this->value_changed.vals[i]);
	}
	gf_node_event_out(n, 3);//"value_changed"
}

Bool InitNormalInterpolator(M_NormalInterpolator *n)
{
	n->on_set_fraction = NormInt_SetFraction;
	CI_SetFraction(0, &n->value_changed, &n->key, &n->keyValue);
	return 1;
}

static void ColorInt_SetFraction(GF_Node *node, GF_Route *route)
{
	u32 i;
	Fixed frac;
	M_ColorInterpolator *_this = (M_ColorInterpolator *)node;


	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed.red = Interpolate(_this->keyValue.vals[i-1].red,
			                                       _this->keyValue.vals[i].red,
			                                       frac);
			_this->value_changed.green = Interpolate(_this->keyValue.vals[i-1].green,
			                             _this->keyValue.vals[i].green,
			                             frac);
			_this->value_changed.blue = Interpolate(_this->keyValue.vals[i-1].blue,
			                                        _this->keyValue.vals[i].blue,
			                                        frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}

Bool InitColorInterpolator(M_ColorInterpolator *node)
{
	node->on_set_fraction = ColorInt_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}


static void PosInt2D_SetFraction(GF_Node *node, GF_Route *route)
{
	M_PositionInterpolator2D *_this = (M_PositionInterpolator2D *)node;
	u32 i;
	Fixed frac;

	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed.x = Interpolate(_this->keyValue.vals[i-1].x, _this->keyValue.vals[i].x, frac);
			_this->value_changed.y = Interpolate(_this->keyValue.vals[i-1].y, _this->keyValue.vals[i].y, frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}

Bool InitPositionInterpolator2D(M_PositionInterpolator2D *node)
{
	node->on_set_fraction = PosInt2D_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}

static void PosInt_SetFraction(GF_Node *node, GF_Route *route)
{
	u32 i;
	Fixed frac;
	M_PositionInterpolator *_this = (M_PositionInterpolator *)node;

	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed.x = Interpolate(_this->keyValue.vals[i-1].x, _this->keyValue.vals[i].x, frac);
			_this->value_changed.y = Interpolate(_this->keyValue.vals[i-1].y, _this->keyValue.vals[i].y, frac);
			_this->value_changed.z = Interpolate(_this->keyValue.vals[i-1].z, _this->keyValue.vals[i].z, frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}

Bool InitPositionInterpolator(M_PositionInterpolator *node)
{
	node->on_set_fraction = PosInt_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}

static void ScalarInt_SetFraction(GF_Node *node, GF_Route *route)
{
	M_ScalarInterpolator *_this = (M_ScalarInterpolator *)node;
	u32 i;
	Fixed frac;

	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed = Interpolate(_this->keyValue.vals[i-1], _this->keyValue.vals[i], frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}
Bool InitScalarInterpolator(M_ScalarInterpolator *node)
{
	node->on_set_fraction = ScalarInt_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}


static void OrientInt_SetFraction(GF_Node *node, GF_Route *route)
{
	u32 i;
	Fixed frac;
	M_OrientationInterpolator *_this = (M_OrientationInterpolator *)node;

	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed = gf_sg_sfrotation_interpolate(_this->keyValue.vals[i-1], _this->keyValue.vals[i], frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}

Bool InitOrientationInterpolator(M_OrientationInterpolator *node)
{
	node->on_set_fraction = OrientInt_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}

static void CI4D_SetFraction(GF_Node *n, GF_Route *route)
{
	Fixed frac;
	u32 numElemPerKey, i, j;
	M_CoordinateInterpolator4D *_this = (M_CoordinateInterpolator4D *) n;

	if (! _this->key.count) return;
	if (_this->keyValue.count % _this->key.count) return;

	numElemPerKey = _this->keyValue.count / _this->key.count;
	//set size
	if (_this->value_changed.count != numElemPerKey)
		gf_sg_vrml_mf_alloc(&_this->value_changed, GF_SG_VRML_MFVEC4F, numElemPerKey);


	if (_this->set_fraction < _this->key.vals[0]) {
		for (i=0; i<numElemPerKey; i++)
			_this->value_changed.vals[i] = _this->keyValue.vals[i];
	} else if (_this->set_fraction > _this->key.vals[_this->key.count - 1]) {
		for (i=0; i<numElemPerKey; i++)
			_this->value_changed.vals[i] = _this->keyValue.vals[(_this->keyValue.count) - numElemPerKey + i];
	} else {
		for (j = 1; j < _this->key.count; j++) {
			// Find the key values the fraction lies between
			if ( _this->set_fraction < _this->key.vals[j-1]) continue;
			if (_this->set_fraction >= _this->key.vals[j]) continue;

			frac = GetInterpolateFraction(_this->key.vals[j-1], _this->key.vals[j], _this->set_fraction);
			for (i=0; i<numElemPerKey; i++) {
				_this->value_changed.vals[i].x = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].x,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].x,
				                                 frac);
				_this->value_changed.vals[i].y = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].y,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].y,
				                                 frac);
				_this->value_changed.vals[i].z = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].z,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].z,
				                                 frac);
				_this->value_changed.vals[i].q = Interpolate(_this->keyValue.vals[(j-1)*numElemPerKey + i].q,
				                                 _this->keyValue.vals[(j)*numElemPerKey + i].q,
				                                 frac);
			}
			break;
		}
	}
	//invalidate
	gf_node_event_out(n, 3);//"value_changed"
}

Bool InitCoordinateInterpolator4D(M_CoordinateInterpolator4D *node)
{
	node->on_set_fraction = CI4D_SetFraction;

	if (node->key.count && !(node->keyValue.count % node->key.count)) {
		u32 i, numElemPerKey = node->keyValue.count / node->key.count;
		gf_sg_vrml_mf_alloc(&node->value_changed, GF_SG_VRML_MFVEC4F, numElemPerKey);
		for (i=0; i<numElemPerKey; i++)
			node->value_changed.vals[i] = node->keyValue.vals[i];
	}
	return 1;
}

static void PI4D_SetFraction(GF_Node *node, GF_Route *route)
{
	u32 i;
	Fixed frac;
	M_PositionInterpolator4D *_this = (M_PositionInterpolator4D *)node;

	if (! _this->key.count) return;
	if (_this->keyValue.count != _this->key.count) return;

	// The given fraction is less than the specified range
	if (_this->set_fraction < _this->key.vals[0]) {
		_this->value_changed = _this->keyValue.vals[0];
	} else if (_this->set_fraction >= _this->key.vals[_this->key.count-1]) {
		_this->value_changed = _this->keyValue.vals[_this->keyValue.count-1];
	} else {
		for (i=1; i<_this->key.count; i++) {
			// Find the key values the fraction lies between
			if (_this->set_fraction < _this->key.vals[i-1]) continue;
			if (_this->set_fraction >= _this->key.vals[i]) continue;

			frac = GetInterpolateFraction(_this->key.vals[i-1], _this->key.vals[i], _this->set_fraction);
			_this->value_changed.x = Interpolate(_this->keyValue.vals[i-1].x, _this->keyValue.vals[i].x, frac);
			_this->value_changed.y = Interpolate(_this->keyValue.vals[i-1].y, _this->keyValue.vals[i].y, frac);
			_this->value_changed.z = Interpolate(_this->keyValue.vals[i-1].z, _this->keyValue.vals[i].z, frac);
			_this->value_changed.q = Interpolate(_this->keyValue.vals[i-1].q, _this->keyValue.vals[i].q, frac);
			break;
		}
	}
	gf_node_event_out(node, 3);//"value_changed"
}

Bool InitPositionInterpolator4D(M_PositionInterpolator4D *node)
{
	node->on_set_fraction = PI4D_SetFraction;
	if (node->keyValue.count) node->value_changed = node->keyValue.vals[0];
	return 1;
}



#ifndef GPAC_DISABLE_X3D

static void BooleanFilter_setValue(GF_Node *n, GF_Route *route)
{
	X_BooleanFilter *bf = (X_BooleanFilter *)n;
	if (!bf->set_boolean) {
		bf->inputFalse = 1;
		gf_node_event_out(n, 1/*"inputFalse"*/);
	}
	if (bf->set_boolean) {
		bf->inputTrue = 1;
		gf_node_event_out(n, 3/*"inputTrue"*/);
	}
	bf->inputNegate = bf->set_boolean ? 0 : 1;
	gf_node_event_out(n, 2/*"inputNegate"*/);
}

void InitBooleanFilter(GF_Node *n)
{
	X_BooleanFilter *bf = (X_BooleanFilter *)n;
	bf->on_set_boolean = BooleanFilter_setValue;
}

static void BooleanSequencer_setFraction(GF_Node *n, GF_Route *route)
{
	u32 i;
	X_BooleanSequencer *bs = (X_BooleanSequencer*)n;
	if (! bs->key.count) return;
	if (bs->keyValue.count != bs->key.count) return;

	if (bs->set_fraction < bs->key.vals[0]) {
		bs->value_changed = bs->keyValue.vals[0];
	} else if (bs->set_fraction >= bs->key.vals[bs->key.count-1]) {
		bs->value_changed = bs->keyValue.vals[bs->keyValue.count-1];
	} else {
		for (i=1; i<bs->key.count; i++) {
			if (bs->set_fraction < bs->key.vals[i-1]) continue;
			if (bs->set_fraction >= bs->key.vals[i]) continue;
			bs->value_changed = bs->keyValue.vals[i-1];
			break;
		}
	}
	gf_node_event_out(n, 3);//"value_changed"
}

static void BooleanSequencer_setNext(GF_Node *n, GF_Route *route)
{
	s32 *prev_val, val;
	X_BooleanSequencer *bs = (X_BooleanSequencer*)n;
	if (!bs->next) return;

	prev_val = (s32 *)n->sgprivate->UserPrivate;
	val = (*prev_val + 1) % bs->keyValue.count;
	*prev_val = val;
	bs->value_changed = bs->keyValue.vals[val];
	gf_node_event_out(n, 3);//"value_changed"
}

static void BooleanSequencer_setPrevious(GF_Node *n, GF_Route *route)
{
	s32 *prev_val, val;
	X_BooleanSequencer *bs = (X_BooleanSequencer*)n;
	if (!bs->previous) return;

	prev_val = (s32 *)n->sgprivate->UserPrivate;
	val = (*prev_val - 1);
	if (val<0) val += bs->keyValue.count;
	val %= bs->keyValue.count;
	*prev_val = val;
	bs->value_changed = bs->keyValue.vals[val];
	gf_node_event_out(n, 3);//"value_changed"
}
static void DestroyBooleanSequencer(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		s32 *st = (s32 *) gf_node_get_private(n);
		gf_free(st);
	}
}
void InitBooleanSequencer(GF_Node *n)
{
	X_BooleanSequencer *bs = (X_BooleanSequencer*)n;
	bs->on_next = BooleanSequencer_setNext;
	bs->on_previous = BooleanSequencer_setPrevious;
	bs->on_set_fraction = BooleanSequencer_setFraction;
	n->sgprivate->UserPrivate = gf_malloc(sizeof(s32));
	*(s32 *)n->sgprivate->UserPrivate = 0;
	n->sgprivate->UserCallback = DestroyBooleanSequencer;
}

static void BooleanToggle_setValue(GF_Node *n, GF_Route *route)
{
	X_BooleanToggle *bt = (X_BooleanToggle *)n;
	if (bt->set_boolean) {
		bt->toggle = !bt->toggle;
		gf_node_event_out(n, 1/*"toggle"*/);
	}
}
void InitBooleanToggle(GF_Node *n)
{
	X_BooleanToggle *bt = (X_BooleanToggle *)n;
	bt->on_set_boolean = BooleanToggle_setValue;
}

static void BooleanTrigger_setTime(GF_Node *n, GF_Route *route)
{
	X_BooleanTrigger *bt = (X_BooleanTrigger *)n;
	bt->triggerTrue = 1;
	gf_node_event_out(n, 1/*"triggerTrue"*/);
}
void InitBooleanTrigger(GF_Node *n)
{
	X_BooleanTrigger *bt = (X_BooleanTrigger *)n;
	bt->on_set_triggerTime = BooleanTrigger_setTime;
}

static void IntegerSequencer_setFraction(GF_Node *n, GF_Route *route)
{
	u32 i;
	X_IntegerSequencer *is = (X_IntegerSequencer *)n;
	if (! is->key.count) return;
	if (is->keyValue.count != is->key.count) return;

	if (is->set_fraction < is->key.vals[0]) {
		is->value_changed = is->keyValue.vals[0];
	} else if (is->set_fraction >= is->key.vals[is->key.count-1]) {
		is->value_changed = is->keyValue.vals[is->keyValue.count-1];
	} else {
		for (i=1; i<is->key.count; i++) {
			if (is->set_fraction < is->key.vals[i-1]) continue;
			if (is->set_fraction >= is->key.vals[i]) continue;
			is->value_changed = is->keyValue.vals[i-1];
			break;
		}
	}
	gf_node_event_out(n, 3);//"value_changed"
}

static void IntegerSequencer_setNext(GF_Node *n, GF_Route *route)
{
	s32 *prev_val, val;
	X_IntegerSequencer *is = (X_IntegerSequencer*)n;
	if (!is->next) return;

	prev_val = (s32 *)n->sgprivate->UserPrivate;
	val = (*prev_val + 1) % is->keyValue.count;
	*prev_val = val;
	is->value_changed = is->keyValue.vals[val];
	gf_node_event_out(n, 3);//"value_changed"
}

static void IntegerSequencer_setPrevious(GF_Node *n, GF_Route *route)
{
	s32 *prev_val, val;
	X_IntegerSequencer *is = (X_IntegerSequencer *)n;
	if (!is->previous) return;

	prev_val = (s32 *)n->sgprivate->UserPrivate;
	val = (*prev_val - 1);
	if (val<0) val += is->keyValue.count;
	val %= is->keyValue.count;
	*prev_val = val;
	is->value_changed = is->keyValue.vals[val];
	gf_node_event_out(n, 3);//"value_changed"
}
static void DestroyIntegerSequencer(GF_Node *n, void *eff, Bool is_destroy)
{
	if (is_destroy) {
		s32 *st = (s32 *)gf_node_get_private(n);
		gf_free(st);
	}
}
void InitIntegerSequencer(GF_Node *n)
{
	X_IntegerSequencer *bs = (X_IntegerSequencer *)n;
	bs->on_next = IntegerSequencer_setNext;
	bs->on_previous = IntegerSequencer_setPrevious;
	bs->on_set_fraction = IntegerSequencer_setFraction;
	n->sgprivate->UserPrivate = gf_malloc(sizeof(s32));
	*(s32 *)n->sgprivate->UserPrivate = 0;
	n->sgprivate->UserCallback = DestroyIntegerSequencer;
}

static void IntegerTrigger_setTrigger(GF_Node *n, GF_Route *route)
{
	X_IntegerTrigger *it = (X_IntegerTrigger *)n;
	if (it->set_boolean) {
		it->triggerValue = it->integerKey;
		gf_node_event_out(n, 2/*"triggerValue"*/);
	}
}
void InitIntegerTrigger(GF_Node *n)
{
	X_IntegerTrigger *it = (X_IntegerTrigger *)n;
	it->on_set_boolean = IntegerTrigger_setTrigger;
}

static void TimeTrigger_setTrigger(GF_Node *n, GF_Route *route)
{
	X_TimeTrigger *tt = (X_TimeTrigger *)n;
	tt->triggerTime = gf_node_get_scene_time(n);
	gf_node_event_out(n, 1/*"triggerTime"*/);
}
void InitTimeTrigger(GF_Node *n)
{
	X_TimeTrigger *tt = (X_TimeTrigger*)n;
	tt->on_set_boolean = TimeTrigger_setTrigger;
}

#endif /*GPAC_DISABLE_X3D*/


#endif /*GPAC_DISABLE_VRML*/
