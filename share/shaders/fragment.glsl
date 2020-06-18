/**
 * Shader implementing: Clipping, Texturing (RGB and YUV), Lighting, Fog
 **/

//version defined first, at shader compilation
#ifdef GF_GL_IS_ExternalOES
#extension GL_OES_EGL_image_external : require
#endif
#if defined(GL_ES)
#if defined(GL_FRAGMENT_PRECISION_HIGH)
precision highp float;	//ES2.0 supporting highp
#else
precision mediump float;	//Default
#endif
#endif

//#pragma STDGL invariant(all)	//removed due to incompatibility with the emulator

//LIGHTS_MAX and CLIP_MAX defined at shader compilation


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

//Generic (Scene) Uniforms
#if defined(GL_ES)
uniform lowp int gfNumLights;
#else
uniform int gfNumLights;
#endif
uniform bool gfLightTwoSide;
uniform gfLight lights[LIGHTS_MAX];

//Material and Lighting Properties
uniform vec4 gfAmbientColor;
uniform vec4 gfDiffuseColor;
uniform vec4 gfSpecularColor;
uniform float gfShininess;	//a.k.a. specular exponent
uniform vec4 gfLightDiffuse;
uniform vec4 gfLightAmbient;
uniform vec4 gfLightSpecular;

//Fog
uniform bool gfFogEnabled;
uniform vec3 gfFogColor;

#endif

uniform bool hasMaterial2D;
uniform vec4 gfEmissionColor;

#ifdef GF_GL_HAS_COLOR
uniform bool hasMeshColor;
#endif

#ifdef GF_GL_HAS_CLIP
#if defined(GL_ES)
uniform lowp int gfNumClippers;
#else
uniform int gfNumClippers;
#endif
#endif

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

//Varyings
#ifdef GF_GL_HAS_LIGHT
varying vec3 m_normal;
varying vec4 gfEye;
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

//constants
const float zero_float = 0.0;
const float one_float = 1.0;


#ifdef GF_GL_HAS_LIGHT
vec4 doLighting(int i){

	vec4 lightColor = vec4(zero_float, zero_float, zero_float, zero_float);
	float att = zero_float;
	gfLight tempLight;	//we use a temp gfLight, because array of straucts in fragment are not supported in android
	vec3 lightVnorm; //normalized lightVector
	vec3 lightV; //temp lightVector
	vec3 halfVnorm; //temp lightVector

	//FIXME - couldn't we use a light[i]. something here ?
	if(i==0) {	//ES2 does not support switch() statements
		tempLight = lights[0];
		lightV = lightVector[0];
		halfVnorm = normalize( lightVector[0] + gfEye.xyz );
	} else if (i==1) {
		tempLight = lights[1];
		lightV = lightVector[1];
		halfVnorm = normalize( lightVector[1] + gfEye.xyz );
	} else if(i==2) {
		tempLight = lights[2];
		lightV = lightVector[2];
		halfVnorm = normalize( lightVector[2] + gfEye.xyz );
	}
	
	lightVnorm = normalize(lightV);
	vec3 normal = normalize(m_normal);

	if (gfLightTwoSide && (!gl_FrontFacing)){//light back face
		//originally: normal *=-1; -> Not compliant with Shading Language v1.0
		normal *= vec3(-1.0, -1.0, -1.0);
	}
	
	float light_cos = max(zero_float, dot(normal, lightVnorm));	//ndotl
	float half_cos = dot(normal, halfVnorm);

	if (tempLight.type == L_POINT) {	//we have a point
		float distance = length(lightV);	
		att = 1.0 / (tempLight.attenuation.x + tempLight.attenuation.y * distance + tempLight.attenuation.z * distance * distance);

		if (att <= 0.0)
			return lightColor;
			
		lightColor += light_cos * tempLight.color * gfDiffuseColor;
		
		if (light_cos > 0.0) {
			float dotNormHalf = max(dot(normal, halfVnorm),0.0);	//ndoth
			lightColor += (pow(dotNormHalf, gfShininess) * gfSpecularColor * tempLight.color);
			lightColor *= att;
		}
		lightColor.a = gfDiffuseColor.a;
		return lightColor;
		
	} else if (tempLight.type == L_SPOT) {	//we have a spot
		if (light_cos > 0.0) {
			float spot = dot(normalize(tempLight.direction.xyz), lightVnorm);	//it should be -direction, but we invert it before parsing
			if (spot > tempLight.cutOffAngle) {
				float distance = length(lightV);	
				float dotNormHalf = max(dot(normal, halfVnorm),0.0);	//ndoth
				spot = pow(spot, tempLight.intensity);
				att = spot / (tempLight.attenuation.x + tempLight.attenuation.y * distance + tempLight.attenuation.z * distance * distance);
				lightColor += att * (light_cos * tempLight.color * gfDiffuseColor);
				lightColor += att * (pow(dotNormHalf, gfShininess) * gfSpecularColor * tempLight.color);
			}
		}
		return lightColor;

	} else if(tempLight.position.w == zero_float || tempLight.type == L_DIRECTIONAL) { //we have a direction
		vec3 lightDirection = vec3(tempLight.position);
		lightColor = (gfDiffuseColor * gfLightDiffuse) * light_cos; 
		if (half_cos > zero_float) {
			lightColor += (gfSpecularColor * gfLightSpecular) * pow(half_cos, gfShininess);
		}
		lightColor.a = gfDiffuseColor.a;
		return lightColor;
	}

	return vec4(zero_float);
}
#endif //GF_GL_HAS_LIGHT

void main()
{
#if defined(GF_GL_HAS_CLIP) && defined(GL_ES)
	bool do_clip=false;
#endif
	int i;
	vec2 texc;
	vec3 yuv, rgb;
	vec4 rgba, fragColor;

#ifdef GF_GL_HAS_CLIP
	//clipping
	for (int i=0; i<CLIPS_MAX; i++) {
		if (i==gfNumClippers) break;
		if (clipDistance[i]<0.0) {
			//do not discard on GLES too slow on most devices
#if defined(GL_ES)
			do_clip=true;
			break;
#else
			discard;
#endif
		}
	}
#endif
	
#if defined(GF_GL_HAS_CLIP) && defined(GL_ES)
	if (do_clip) {
		gl_FragColor = vec4(zero_float);
	} else {
#endif
		
		
#ifdef GF_GL_HAS_COLOR
	fragColor = m_color;
#else
	fragColor = vec4(zero_float);
#endif

	if (hasMaterial2D) {
		fragColor = gfEmissionColor;
	}

#if defined (GF_GL_HAS_LIGHT)
	if (gfNumLights>0) {
#ifdef GF_GL_HAS_COLOR
		fragColor += (gfAmbientColor * gfLightAmbient);
#else
		fragColor = gfEmissionColor + (gfAmbientColor * gfLightAmbient);
#endif
	}
#endif

#ifdef GF_GL_HAS_LIGHT
	if (gfNumLights > 0) {
		for (int i=0; i<LIGHTS_MAX; i++) {
			if (i==gfNumLights) break;
			fragColor += doLighting(i);
		}
		fragColor.a = gfDiffuseColor.a;
	}
#endif
	
	fragColor = clamp(fragColor, zero_float, one_float);

#ifdef GF_GL_HAS_TEXTURE
	
	//currently supporting 1 texture
	rgba = maintx_sample(TexCoord);

#ifdef GF_GL_HAS_LIGHT
	if (gfNumLights>0) {	//RGB texture
		fragColor *= rgba;
	}
	//RGB texture with material 2D [TODO: check]
	else if(gfNumLights==0)
#endif
	{
		fragColor = rgba;
	}

		//we have mat 2D + texture
#ifndef GF_GL_IS_ExternalOES
	if (hasMaterial2D) {
		if (gfEmissionColor.a > 0.0) {
			fragColor *= gfEmissionColor;
		}
		//hack - if full transparency on texture with material2D, use material color
		else if (fragColor.rgb == vec3(0.0, 0.0, 0.0)){
			fragColor.rgb = gfEmissionColor.rgb;
		}
	}
#endif // GF_GL_IS_ExternalOES

	
#endif // GF_GL_HAS_TEXTURE
	
	
	if (hasColorMatrix) {
		fragColor = gfColorMatrix * fragColor;
		fragColor += gfTranslationVector;
		fragColor = clamp(fragColor, zero_float, one_float);
	}
	
	if (hasColorKey) {
		vec3 tempColour = vec3(0.0, 0.0, 0.0);
		float mean = 0.0;
		
		tempColour.r = abs(gfKeyColor.r-fragColor.r);
		tempColour.g = abs(gfKeyColor.g-fragColor.g);
		tempColour.b = abs(gfKeyColor.b-fragColor.b);
		mean = (tempColour.r + tempColour.g + tempColour.b)/3.0;
		
		if (mean<gfKeyLow) {
			fragColor.a =0.0;
		} else if(mean<=gfKeyHigh) {
			fragColor.a = (mean-gfKeyLow) * gfKeyAlpha / (gfKeyHigh - gfKeyLow);
		}
	}
	
#ifdef GF_GL_HAS_LIGHT
	if (gfFogEnabled)
		fragColor = fragColor * gfFogFactor + vec4(gfFogColor, zero_float) * (one_float - gfFogFactor);
#endif
	gl_FragColor = fragColor;

#if defined(GF_GL_HAS_CLIP) && defined(GL_ES)
	}
#endif
}
