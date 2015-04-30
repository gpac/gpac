/**
 * Shader implementing: Clipping, Texturing, Lighting, Fog
 * Available flags: GF_GL_IS_RECT, GF_GL_IS_YUV, GF_GL_HAS_FOG, GF_GL_HAS_CLIP
 *
 **/
 /*
 //NOTE: if there is a #version directive (e.g. #version 100), it must occur before anything else in the program (including other directives)
 #version 100	//it is set the same time as the flags
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

#define LIGHTS_MAX 		8
#define TEXTURES_MAX 	2

#define CLIPS_MAX 		8

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

//Generic (Scene) Uniforms
uniform int gfNumLights;
uniform bool gfLightTwoSide;
uniform int gfNumTextures;
uniform gfLight lights[LIGHTS_MAX];
uniform bool hasClip;
uniform bool hasMeshColor;

//Material and Lighting Properties	
uniform vec4 gfAmbientColor;
uniform vec4 gfDiffuseColor; 
uniform vec4 gfSpecularColor; 
uniform vec4 gfEmissionColor;
uniform float gfShininess;	//a.k.a. specular exponent
uniform vec4 gfLightDiffuse;
uniform vec4 gfLightAmbient; 
uniform vec4 gfLightSpecular;

//Fog
uniform bool gfFogEnabled; 
uniform vec3 gfFogColor;

//Color Matrix
uniform mat4 gfColorMatrix;
uniform bool hasColorMatrix;
uniform vec4 gfTranslationVector;
	
//Color Key
uniform vec3 gfKeyColor;
uniform float gfKeyAlpha;
uniform float gfKeyLow;
uniform float gfKeyHigh;
uniform bool hasColorKey;

//Texture samplers
uniform sampler2D y_plane;
uniform sampler2D u_plane;
uniform sampler2D v_plane;

//Texture other
uniform float alpha;
const vec3 offset = vec3(-0.0625, -0.5, -0.5);
const vec3 R_mul = vec3(1.164,  0.000,  1.596);
const vec3 G_mul = vec3(1.164, -0.391, -0.813);
const vec3 B_mul = vec3(1.164,  2.018,  0.000);

//Varyings
varying vec3 n;
varying vec4 gfEye;
varying vec2 TexCoord;
varying vec3 lightVector[8];
varying vec3 halfVector[8];
varying float clipDistance[CLIPS_MAX];
varying float gfFogFactor;

//testing material
varying vec4 m_color;

//constants
const float zero_float = 0.0;
const float one_float = 1.0;


vec4 doLighting(int i){

	vec4 lightColor = vec4(zero_float, zero_float, zero_float, zero_float);
	float att = zero_float;

	vec3 lightVnorm = normalize(lightVector[i]);
	vec3 normal = normalize(n);

	if(gfLightTwoSide && (!gl_FrontFacing)){//light back face
		//originally: normal *=-1; -> Not compliant with Shading Language v1.0
		normal *= vec3(-1.0, -1.0, -1.0);
	}
	
	float light_cos = max(zero_float, dot(normal, lightVnorm));	//ndotl
	float half_cos = dot(normal, normalize(halfVector[i]));

	if(lights[i].type == L_POINT){	//we have a point
		float distance = length(lightVector[i]);	
		att = 1.0 / (lights[i].attenuation.x + lights[i].attenuation.y * distance + lights[i].attenuation.z * distance * distance);

		if (att <= 0.0)
			return lightColor;
			
		lightColor += light_cos * lights[i].color * gfDiffuseColor;
		
		if(light_cos > 0.0){
			float dotNormHalf = max(dot(normal, normalize(halfVector[i])),0.0);	//ndoth
			lightColor += (pow(dotNormHalf, gfShininess) * gfSpecularColor * lights[i].color);
			lightColor *= att;
		}
		lightColor.a = gfDiffuseColor.a;
		return lightColor;
		
	}else if(lights[i].type == L_SPOT){	//we have a spot
		if(light_cos > 0.0){
			float spot = dot(normalize(lights[i].direction.xyz), normalize(lightVector[i]));	//it should be -direction, but we invert it before parsing
			if (spot > lights[i].cutOffAngle){
				float distance = length(lightVector[i]);	
				float dotNormHalf = max(dot(normal, normalize(halfVector[i])),0.0);	//ndoth
				spot = pow(spot, lights[i].intensity);
				att = spot / (lights[i].attenuation.x + lights[i].attenuation.y * distance + lights[i].attenuation.z * distance * distance);
				lightColor += att * (light_cos * lights[i].color * gfDiffuseColor);
				lightColor += att * (pow(dotNormHalf, gfShininess) * gfSpecularColor * lights[i].color);
			}
		}
		return lightColor;

	}else if(lights[i].position.w == zero_float || lights[i].type == L_DIRECTIONAL){ //we have a direction
		vec3 lightDirection = vec3(lights[i].position);
		lightColor = (gfDiffuseColor * gfLightDiffuse) * light_cos; 
		if (half_cos > zero_float){ 
			lightColor += (gfSpecularColor * gfLightSpecular) * pow(half_cos, gfShininess);
		}
		lightColor.a = gfDiffuseColor.a;
		return lightColor;
	}

	return vec4(zero_float);
}


void main()
{
	int i;
	vec2 texc;
	vec3 yuv, rgb;
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 0.0);
	
	if(hasMeshColor){	//
		fragColor = m_color;
	}else if(gfNumLights==0){
	//is material2D => no lights - only colour (stored in gfEmissionColor)
		/* Normally here there should be only the material 2D
		 * BUT we might have a case of normal material with no lights
		 * we treat it the same way
		 */
		fragColor = gfEmissionColor;
	}else if(gfNumLights>0){
		//default material (visual->has_material) -> handled in doLighting()
	}

	//clippin
	if(hasClip){
		for(int i=0;i<CLIPS_MAX;i++){
			if(clipDistance[i]<0.0) discard;
		}
	}

#ifdef GF_GL_HAS_LIGHT
	if (gfNumLights > 0) {
	
		fragColor = gfEmissionColor + (gfAmbientColor * gfLightAmbient);

		for(i=0; i<8; i++){

			if(i>=gfNumLights) break;
			
			fragColor += doLighting(i);
		}
		fragColor.a = gfDiffuseColor.a;
	}
#endif
	
	fragColor = clamp(fragColor, zero_float, one_float);
	
	if(gfNumTextures>0){	//currently supporting 1 texture
#ifdef GF_GL_IS_YUV
			texc = TexCoord.st;
			yuv.x = texture2D(y_plane, texc).r;
			yuv.y = texture2D(u_plane, texc).r;
			yuv.z = texture2D(v_plane, texc).r;
			yuv += offset;
			rgb.r = dot(yuv, R_mul);
			rgb.g = dot(yuv, G_mul);
			rgb.b = dot(yuv, B_mul);
			if(gfNumLights>0){
				fragColor *= vec4(rgb, alpha);
			}else{
				fragColor = vec4(rgb, alpha);
			}
#else
		if(gfNumLights>0){	//RGB texture
			fragColor *= texture2D(y_plane, TexCoord);
		}else if(gfNumLights==0){	//RGB texture with material 2D [TODO: check]
			fragColor = texture2D(y_plane, TexCoord);
		}
#endif

#ifdef GF_GL_HAS_MAT_2D	//we have mat 2 + texture
		if(gfEmissionColor.a > 0.0 && gfEmissionColor.a <1.0){
			fragColor *= gfEmissionColor;
		}else if(fragColor.rgb == vec3(0.0, 0.0, 0.0)){
			fragColor.rgb = gfEmissionColor.rgb;
		}
#endif
	}
	
	if(hasColorMatrix){
		fragColor = gfColorMatrix * fragColor;
		fragColor += gfTranslationVector;
		fragColor = clamp(fragColor, zero_float, one_float);
	}
	
	if(hasColorKey){
		vec3 tempColour = vec3(0.0, 0.0, 0.0);
		float mean = 0.0;
		
		tempColour.r = abs(gfKeyColor.r-fragColor.r);
		tempColour.g = abs(gfKeyColor.g-fragColor.g);
		tempColour.b = abs(gfKeyColor.b-fragColor.b);
		mean = (tempColour.r + tempColour.g + tempColour.b)/3.0;
		
		if(mean<gfKeyLow){
			fragColor.a =0.0;
		}else if(mean<=gfKeyHigh){
			fragColor.a = (mean-gfKeyLow) * gfKeyAlpha / (gfKeyHigh - gfKeyLow);
		}
	}
	
	if(gfFogEnabled)
		fragColor = fragColor * gfFogFactor + vec4(gfFogColor, zero_float) * (one_float - gfFogFactor);
	
	gl_FragColor = fragColor;
}