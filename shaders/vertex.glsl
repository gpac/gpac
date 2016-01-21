/**
 * Shader implementing: Clipping, Texturing (RGB and YUV), Lighting, Fog
 **/

//version defined first, at shader compilation

#if defined(GL_ES)
#if defined(GL_FRAGMENT_PRECISION_HIGH)
precision highp float;	//ES2.0 supporting highp
#else
precision mediump float;	//Default
#endif
#endif

//#pragma STDGL invariant(all)	//removed due to incompatibility with the emulator

//LIGHTS_MAX and CLIP_MAX defined at shader compilation


//lights definitions
#ifdef GF_GL_HAS_LIGHT

#define FOG_TYPE_LINEAR 0
#define FOG_TYPE_EXP    1
#define FOG_TYPE_EXP2   2

#define L_DIRECTIONAL	0
#define L_SPOT		    1
#define L_POINT			2


//Light Structure
struct gfLight{
#if defined(GL_ES)
	lowp int type;
#else
	int type;
#endif
	vec4 position;
	vec4 direction;
	vec3 attenuation;
	vec4 color;
	float ambientIntensity;	//it is not used - we calculate it inside the shader
	float intensity;
	float beamWidth;	//it is not used - we calculate it inside the shader
	float cutOffAngle;
};
#endif


//Attributes
attribute vec4 gfVertex;

#ifdef GF_GL_HAS_LIGHT
attribute vec3 gfNormal;
#endif

#ifdef GF_GL_HAS_TEXTURE
attribute vec4 gfMultiTexCoord;
#endif

#ifdef GF_GL_HAS_COLOR
attribute vec4 gfMeshColor;
#endif


#ifdef GF_GL_HAS_LIGHT
//Generic (Scene) uniforms
uniform gfLight lights[LIGHTS_MAX];
#if defined(GL_ES)
uniform lowp int gfNumLights;
#else
uniform int gfNumLights;
#endif

//Fog
uniform bool gfFogEnabled; 
uniform vec3 gfFogColor; 
uniform float gfFogDensity; 
uniform int gfFogType; 
uniform float gfFogVisibility; 

#endif

//Matrices
uniform mat4 gfModelViewMatrix;
uniform mat4 gfProjectionMatrix;

#ifdef GF_GL_HAS_LIGHT
uniform mat4 gfNormalMatrix;
#endif

#ifdef GF_GL_HAS_TEXTURE
uniform mat4 gfTextureMatrix;
uniform bool hasTextureMatrix;
#endif

//Clipping
#ifdef GF_GL_HAS_CLIP
#if defined(GL_ES)
uniform lowp int gfNumClippers;
#else
uniform int gfNumClippers;
#endif
uniform vec4 clipPlane[CLIPS_MAX];
#endif

//Varyings
#ifdef GF_GL_HAS_LIGHT

varying vec4 gfEye;	//camera

varying vec3 m_normal;		//normal
varying vec3 lightVector[LIGHTS_MAX];
varying float gfFogFactor;
#endif

#ifdef GF_GL_HAS_TEXTURE
varying vec2 TexCoord;
#endif

#ifdef GF_GL_HAS_COLOR
varying vec4 m_color;
#endif

#ifdef GF_GL_HAS_CLIP
varying float clipDistance[CLIPS_MAX];
#endif


#ifdef GF_GL_HAS_LIGHT
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
#endif

void main(void)
{
#ifndef GF_GL_HAS_LIGHT
	vec4 gfEye;
#endif
	
	gfEye = gfModelViewMatrix * gfVertex;
	
#ifdef GF_GL_HAS_COLOR
	m_color = gfMeshColor;
#endif
	
#ifdef GF_GL_HAS_LIGHT
	m_normal = normalize( vec3(gfNormalMatrix * vec4(gfNormal, 0.0)) );
	
	for(int i=0; i<LIGHTS_MAX; i++){
		if (i==gfNumLights) break;
		
		if ( lights[i].type == L_SPOT || lights[i].type == L_POINT ) {
			lightVector[i] = lights[i].position.xyz - gfEye.xyz;
		} else {
			//if it is a directional light, position SHOULD indicate direction (modified implementation - check before committing)
			lightVector[i] = lights[i].direction.xyz;
		}
	}
	gfFogFactor = gfFogEnabled ? fog() : 1.0;
	
#endif
	
#ifdef GF_GL_HAS_TEXTURE
	if (hasTextureMatrix) {
		TexCoord = vec2(gfTextureMatrix * gfMultiTexCoord);
	} else {
		TexCoord = vec2(gfMultiTexCoord);
	}
#endif
	
	
#ifdef GF_GL_HAS_CLIP
	//clipPlane are given in eye coordinate
	for (int i=0; i<CLIPS_MAX; i++) {
		if (i==gfNumClippers) break;
		clipDistance[i] = dot(gfEye.xyz, clipPlane[i].xyz) + clipPlane[i].w;
	}
#endif
	
	gl_Position = gfProjectionMatrix * gfEye;
}
