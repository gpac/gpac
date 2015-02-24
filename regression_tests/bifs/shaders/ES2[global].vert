/**
 * Shader implementing: Clipping, Texturing, Lighting, Fog
 * Available flags: GF_GL_IS_RECT, GF_GL_IS_YUV, GF_GL_HAS_FOG, GF_GL_HAS_CLIP
 *
 **/

//NOTE: if there is a #version directive (e.g. #version 100), it must occur before anything else in the program (including other directives)
 
 #version 100

 //For other GL versions compatibility
 #ifdef GL_FRAGMENT_PRECISION_HIGH
	precision highp float;	//Desktop (or ES2.0 supporting highp)
#elif GL_ES
	precision mediump float;	//Default
#else
	precision lowp float;	//Fallback
#endif
 
#pragma STDGL invariant(all)	//delete after testing

#define FOG_TYPE_LINEAR 0
#define FOG_TYPE_EXP    1
#define FOG_TYPE_EXP2   2

#define L_DIRECTIONAL	0
#define L_SPOT		    1
#define L_POINT			2

#define LIGHTS_MAX 		8
#define TEXTURES_MAX 	2

#define CLIPS_MAX 		8

struct gfLight{
	int type;
	vec4 position;
	vec4 direction;
	vec3 attenuation;
	vec4 color;
	float ambientIntensity;
	float intensity;	//Diffuse
	float beamWidth;
	float cutOffAngle;
};

//attributes
	attribute vec4 gfVertex;
	attribute vec3 gfNormal;
	attribute vec4 gfMultiTexCoord;
	attribute vec4 gfMeshColor;
	
//fog unifrosm
	uniform bool gfFogEnabled; 
	uniform vec3 gfFogColor; 
	uniform float gfFogDensity; 
	uniform int gfFogType; 
	uniform float gfFogVisibility; 
	
	

	uniform vec4 gfLightDiffuse; 
	uniform vec4 gfDiffuseColor; 
	uniform vec4 gfLightAmbient; 
	uniform int gfNumTextures;
	uniform mat4 gfModelViewMatrix;
	uniform mat4 gfProjectionMatrix; 
	uniform mat4 gfNormalMatrix; 
	uniform vec4 gfEmissionColor; 
    uniform vec4 gfAmbientColor;
	uniform int gfNumLights;
    uniform vec4 gfLightPosition; 
    uniform vec4 gfLightAmbiant;
	uniform mat4 gfTextureMatrix;
	uniform gfLight lights[LIGHTS_MAX];
	uniform vec4 clipPlane[CLIPS_MAX];	//we can put these two in a struct
	uniform bool clipActive[CLIPS_MAX];
	uniform bool hasClip;
	uniform bool hasMeshColor;	//MESH_HAS_COLOR replaces the diffuse colour value of the material
	uniform bool enableLights;

	varying vec3 n;
	varying vec4 gfEye;
	//varying vec3 lightVector;
	//varying vec3 halfVector;
	varying vec2 TexCoord;
	varying float clipDistance[CLIPS_MAX];
	varying vec3 lightVector[8];
	varying vec3 halfVector[8];
	varying float gfFogFactor;
	
	//testing material
	varying vec4 m_ambientC;
	varying vec4 m_diffuseC;
	varying vec4 m_specularC;
	varying vec4 m_emissionC;
	varying float m_shininess;	//a.ka. specular exponent
	varying vec4 m_color;
//+ uniform bool hasMeshColour;
	
	
float fog() {

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
		//alternative to normal
		/*
		mat3 normM;
		normM[0] = gfNormalMatrix[0].xyz;
		normM[1] = gfNormalMatrix[1].xyz;
		normM[2] = gfNormalMatrix[2].xyz;
		n = normM * gfNormal;
		n = normalize(n);
		*/
		
		if(hasMeshColor){
			m_color = gfMeshColor;	//we use the m_color as a container for fragment parsing, since gfMeshColor is an attribute
		}
		
	if (gfNumLights > 0) {
		for(int i=0; i<LIGHTS_MAX; i++){
			if(i>=gfNumLights) break;

			if ( lights[i].type == L_SPOT || lights[i].type == L_POINT ) {
				lightVector[i] = lights[i].position.xyz - gfEye.xyz;	//do NOT do it here (dunno know why)
			} else {
			//if it is a directional light, position SHOULD indicate direction (modified implementation - check before commiting)
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