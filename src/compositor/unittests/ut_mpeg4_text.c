#include "tests.h"
#include "../nodes_stacks.h"

unittest(compositor_font_style_type_check)
{
#ifndef GPAC_DISABLE_VRML
	GF_SceneGraph *scenegraph = gf_sg_new();
	GF_Node *mpeg4_font_style = gf_node_new(scenegraph, TAG_MPEG4_FontStyle);
	GF_Node *face = gf_node_new(scenegraph, TAG_MPEG4_Face);
#ifndef GPAC_DISABLE_X3D
	GF_Node *x3d_font_style = gf_node_new(scenegraph, TAG_X3D_FontStyle);
#endif

	gf_node_register(mpeg4_font_style, NULL);
	gf_node_register(face, NULL);
#ifndef GPAC_DISABLE_X3D
	gf_node_register(x3d_font_style, NULL);
#endif

	assert_true(compositor_get_font_style(NULL) == NULL);
	assert_true(compositor_get_font_style(face) == NULL);
	assert_true(compositor_get_font_style(mpeg4_font_style) == (M_FontStyle *) mpeg4_font_style);
#ifndef GPAC_DISABLE_X3D
	assert_true(compositor_get_font_style(x3d_font_style) == (M_FontStyle *) x3d_font_style);
#endif

	gf_node_unregister(mpeg4_font_style, NULL);
	gf_node_unregister(face, NULL);
#ifndef GPAC_DISABLE_X3D
	gf_node_unregister(x3d_font_style, NULL);
#endif
	gf_sg_del(scenegraph);
#endif
}
