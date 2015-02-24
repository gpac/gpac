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

#extension GL_ARB_texture_rectangle : enable

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
	float intensity;
	float beamWidth;
	float cutOffAngle;
};
	uniform bool gfFogEnabled; 
	uniform vec3 gfFogColor;

uniform vec4 gfAmbientColor;
uniform vec4 gfDiffuseColor; 
uniform vec4 gfSpecularColor; 
uniform vec4 gfEmissionColor;
uniform float gfShininess; 
uniform int gfNumLights;
uniform int isYUV;
uniform int isRect;
uniform vec4 gfLightPosition; 
uniform vec4 gfLightDiffuse;
uniform vec4 gfLightAmbient; 
uniform vec4 gfLightSpecular;
uniform int gfNumTextures;
uniform gfLight lights[LIGHTS_MAX];
uniform bool hasClip;
uniform bool hasMeshColor;
uniform bool enableLights;

//test
uniform sampler2D y_plane;
uniform sampler2D u_plane;
uniform sampler2D v_plane;
uniform sampler2DRect y_planeRect;
uniform sampler2DRect u_planeRect;
uniform sampler2DRect v_planeRect;
uniform float width;
uniform float height;
uniform float alpha;
//const float width=128.0;
//const float height=128.0;
//const float alpha=1.0;
const vec3 offset = vec3(-0.0625, -0.5, -0.5);
const vec3 R_mul = vec3(1.164,  0.000,  1.596);
const vec3 G_mul = vec3(1.164, -0.391, -0.813);
const vec3 B_mul = vec3(1.164,  2.018,  0.000);

varying vec3 n;
varying vec4 gfEye;
//varying vec3 lightVector;
//varying vec3 halfVector;
varying vec2 TexCoord;
varying vec3 lightVector[8];
varying vec3 halfVector[8];
varying float clipDistance[CLIPS_MAX];
varying float gfFogFactor;

//testing material
varying vec4 m_ambientC;
varying vec4 m_diffuseC;
varying vec4 m_specularC;
varying vec4 m_emissionC;
varying float m_shininess;	//a.ka. specular exponent
varying vec4 m_color;

const float zero_float = 0.0;
const float one_float = 1.0;


vec4 doLighting(int i){

	vec4 lightColor = vec4(zero_float, zero_float, zero_float, zero_float);
	float att = zero_float;
	vec3 lightVnorm = normalize(lightVector[i]);
	vec3 normal = normalize(n);
	float light_cos = max(zero_float, dot(normal, lightVnorm));	//ndotl
	float half_cos = dot(normal, normalize(halfVector[i]));

	if(lights[i].type == 2){	//we have a point
		//vec3 lightDirection = vec3(lights[i].position-gfEye);
		float distance = length(lightVector[i]);	
		att = 1.0 / (lights[i].attenuation.x + lights[i].attenuation.y * distance + lights[i].attenuation.z * distance * distance);

		if (att <= 0.0)
			return lightColor;
			
		lightColor += light_cos * lights[i].color * gfDiffuseColor;
		
		//startof method 1
		if(light_cos > 0.0){
			float dotNormHalf = max(dot(normal, normalize(halfVector[i])),0.0);	//ndoth
			lightColor += (pow(dotNormHalf, gfShininess) * gfSpecularColor * lights[i].color);
			lightColor *= att;
		}
		lightColor.a = gfDiffuseColor.a;
		return lightColor;
		
		
		
	} else if(lights[i].type == 1){	//we have a point
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

	}else if(lights[i].position.w == zero_float || lights[i].type == 0){
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

void main() {
	//texturing vars
	vec2 texc;
	vec3 yuv, rgb;
	//endof

	//lighting
	int i;
	vec3 attemp;
	float distance = zero_float;
	vec4 lightColors[8];
	vec4 fragColor = vec4(0.0, 0.0, 0.0, 0.0);
	
	if(hasMeshColor){	//
		fragColor = m_color;
	}else if(gfNumLights==0){	//is material2D => no lights - only colour (stored in gfEmissionColor)
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


	if (gfNumLights > 0) {

		for(i=0; i<8; i++){

			if(i>=gfNumLights) break;
			
			fragColor += doLighting(i);
		}
		fragColor.a = gfDiffuseColor.a;
	}

	fragColor = clamp(fragColor, zero_float, one_float);
	
	if(gfNumTextures>0){	//currently supporting 1 texture
#ifdef GF_GL_IS_RECT
		if(isRect==1){
			if(isYUV==1){
				texc = TexCoord.st;
				texc.x *= width;
				texc.y *= height;
				yuv.x = texture2DRect(y_planeRect, texc).r;
				texc.x /= 2.0;
				texc.y /= 2.0;
				yuv.y = texture2DRect(u_planeRect, texc).r;
				yuv.z = texture2DRect(v_planeRect, texc).r;
				yuv += offset;
				rgb.r = dot(yuv, R_mul);
				rgb.g = dot(yuv, G_mul);
				rgb.b = dot(yuv, B_mul);
				if(gfNumLights>0){
					fragColor *= vec4(rgb, alpha);
				}else{
					fragColor = vec4(rgb, alpha);
				}
			}else if(gfNumLights>0){	//RGB texture
				fragColor *= texture2DRect(y_planeRect, TexCoord);
			}else if(gfNumLights==0){	//RGB texture with material 2D
				fragColor = texture2DRect(y_planeRect, TexCoord);
			}
		}
		else
#endif
		{
			if(isYUV==1){
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
			}else if(gfNumLights>0){	//RGB texture
				fragColor *= texture2D(y_plane, TexCoord);
			}else if(gfNumLights==0){	//RGB texture with material 2D [TODO: check]
				fragColor *= texture2D(y_plane, TexCoord);
			}
		}
	}
	
	if(gfFogEnabled)
		fragColor = fragColor * gfFogFactor + vec4(gfFogColor, zero_float) * (one_float - gfFogFactor);
	
	gl_FragColor = fragColor;
}