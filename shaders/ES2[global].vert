#version 100
/**
 * Shader implementing: Clipping, Texturing, Lighting, Fog
 * Available flags: GF_GL_IS_RECT, GF_GL_IS_YUV, GF_GL_HAS_FOG, GF_GL_HAS_CLIP
 *
 **/
/*
//NOTE: if there is a #version directive (e.g. #version 100), it must occur before anything else in the program (including other directives)
*/

//For other GL versions compatibility
#ifdef GL_FRAGMENT_PRECISION_HIGH
	precision highp float;	//Desktop (or ES2.0 supporting highp)
#elif GL_ES
	precision mediump float;	//Default
#else
	precision lowp float;	//Fallback
#endif
 
//#pragma STDGL invariant(all)	//removed due to incompatibility with the emulator

#define FOG_TYPE_LINEAR 0
#define FOG_TYPE_EXP    1
#define FOG_TYPE_EXP2   2

#define L_DIRECTIONAL	0
#define L_SPOT		    1
#define L_POINT			2

#define LIGHTS_MAX 		3
#define TEXTURES_MAX 	2

#define CLIPS_MAX 		4

//Light Structure
struct gfLight{
	int type;
	vec4 position;
	vec4 direction;
	vec3 attenuation;
	vec4 color;
	float ambientIntensity;	//it is not used - we calculate it inside the shader
	float intensity;
	float beamWidth;	//it is not used - we calculate it inside the shader
	float cutOffAngle;
};

//Attributes
attribute vec4 gfVertex;
attribute vec3 gfNormal;
attribute vec4 gfMultiTexCoord;
attribute vec4 gfMeshColor;

//Generic (Scene) uniforms
uniform gfLight lights[LIGHTS_MAX];
uniform int gfNumLights;
uniform int gfNumTextures;
uniform bool hasMeshColor;	//MESH_HAS_COLOR replaces the diffuse colour value of the material

//Fog
uniform bool gfFogEnabled; 
uniform vec3 gfFogColor; 
uniform float gfFogDensity; 
uniform int gfFogType; 
uniform float gfFogVisibility; 
	
//Material Properties	-	used in fragment shader
/*
uniform vec4 gfAmbientColor;
uniform vec4 gfDiffuseColor; 
uniform vec4 gfSpecularColor; 
uniform vec4 gfEmissionColor;
uniform float gfShininess;
*/
//part 2
/*
uniform vec4 gfLightDiffuse; 
uniform vec4 gfLightAmbient; 
uniform vec4 gfLightPosition; 
uniform vec4 gfLightAmbiant;
*/

//Matrices
uniform mat4 gfModelViewMatrix;
uniform mat4 gfProjectionMatrix;
uniform mat4 gfNormalMatrix;
uniform mat4 gfTextureMatrix;

//Clipping
uniform bool hasClip;
uniform vec4 clipPlane[CLIPS_MAX];	//we can put these two in a struct
uniform bool clipActive[CLIPS_MAX];

//Varyings
varying vec3 n;		//normal
varying vec4 gfEye;	//camera
varying vec2 TexCoord;
varying float clipDistance[CLIPS_MAX];
varying vec3 lightVector[LIGHTS_MAX];
varying vec3 halfVector[LIGHTS_MAX];
varying float gfFogFactor;
	
//testing material
varying vec4 m_color;
	

float fog()
{

	float fog, eyeDist = length(gfEye-gfVertex);

	if(gfFogType==FOG_TYPE_LINEAR){
		fog= (gfFogVisibility-eyeDist)/gfFogVisibility;
	}else if(gfFogType==FOG_TYPE_EXP){
		fog= exp(-(gfEye.z * gfFogDensity));
	}else if(gfFogType==FOG_TYPE_EXP2){
		fog= (gfEye.z * gfFogDensity);
		fog = exp(-(fog * fog));
	}
	return clamp(fog, 0.0, 1.0);
}
	
	
void main(void)
{

	gfEye = gfModelViewMatrix * gfVertex;
	
	n = normalize( vec3(gfNormalMatrix * vec4(gfNormal, 0.0)) );

	if(hasMeshColor){
		m_color = gfMeshColor;	//we use the m_color as a container for fragment parsing
	}
		
	if (gfNumLights > 0) {
		for(int i=0; i<LIGHTS_MAX; i++){
		
			if(i>=gfNumLights) break;

			if ( lights[i].type == L_SPOT || lights[i].type == L_POINT ) {
				lightVector[i] = lights[i].position.xyz - gfEye.xyz;
			} else {
				//if it is a directional light, position SHOULD indicate direction (modified implementation - check before committing)
				lightVector[i] = lights[i].direction.xyz;
			}
			halfVector[i] = lightVector[i] + gfEye.xyz; 
		}
	}

	gfFogFactor = gfFogEnabled ? fog() : 1.0;

	if(gfNumTextures>0)
		TexCoord = vec2(gfTextureMatrix * gfMultiTexCoord);

	if(hasClip){
		for(int i=0;i<CLIPS_MAX;i++){
				clipDistance[i] = clipActive[i] ? dot(gfVertex.xyz, clipPlane[i].xyz) + clipPlane[i].w : 1.0;
			}
	}

	gl_Position = gfProjectionMatrix * gfEye;

}