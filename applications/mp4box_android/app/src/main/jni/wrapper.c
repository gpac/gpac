#ifdef __cplusplus
extern "C" {
#endif

#include <android/log.h>
#include <jni.h>
#include "stdlib.h"
#include "stdio.h"
#include "string.h"


#define jniTAG "WRAPPER_JNI"

#define jniLOGV(X...)  __android_log_print(ANDROID_LOG_VERBOSE, jniTAG, X)
#define jniLOGI(X)  __android_log_print(ANDROID_LOG_INFO, jniTAG, X)
#define jniLOGE(X)  __android_log_print(ANDROID_LOG_ERROR, jniTAG, X)

/*#define CAST_HANDLE(wr) jclass c = env->GetObjectClass(obj);\
                        if (!c) return;\
                        jfieldID fid = env->GetFieldID(c, "handle", "J");\
                        if (!fid){\
                          __android_log_print(ANDROID_LOG_ERROR, jniTAG, "No Field ID, ERROR");\
                          return;\
                        }\
                        jlong h = env->GetLongField(obj, fid);
                        //CNativeWrapper* wr = (CNativeWrapper*) h;*/

char ** ConvertCommandLine( const char* sCommand, int* iNbArg );
int mp4boxMain(int argc, char **argv);

JNIEXPORT void JNICALL Java_com_gpac_mp4box_mp4terminal_run(JNIEnv * env, jobject obj, jstring sCommand)
{
	//CAST_HANDLE(wr);
	jniLOGV("mp4terminal::start");
	/*if (!wr)
	{
		jniLOGV("mp4terminal::end : aborted");
	    return;
	}*/
	jboolean isCopy;
	const char * sOriginalCommand = (*env)->GetStringUTFChars(env, sCommand, &isCopy);

	jniLOGV("mp4terminal::command get back ok");
	jniLOGV("%s\n", sOriginalCommand);

	int i = 0;
	char** sConvertedCommandLine;
	sConvertedCommandLine = ConvertCommandLine( sOriginalCommand, &i );
	jniLOGV("Convert command line done");

	FILE* ferr = freopen( "/mnt/sdcard/mp4box.stderrout.txt", "w", stderr );
	FILE* fout = freopen( "/mnt/sdcard/mp4box.stderrout.txt", "w", stdout );

	mp4boxMain(i, sConvertedCommandLine);

	(*env)->ReleaseStringUTFChars(env, sCommand, sOriginalCommand);
	jniLOGV("mp4terminal::end");
	fclose(ferr);
	fclose(fout);
}

char ** ConvertCommandLine( const char* sCommand, int* iNbArg )
{
	int iLength = strlen( sCommand );
	int iArgNumber = 1;
	int i = 0;
	char** pReturn;
	int iPreviousPos = 0;
	int k = 1;//begin at character position 1 as the 0 position will be held by the process name , i.e. mp4box
	int l = 0;

	for ( i = 0; i < iLength ; i++ )
	{
		if( sCommand[i] == ' '  )
		{
			iArgNumber++;
		}
	}
	iArgNumber++; // last argument will not be detected as it is not followed by a space character
	pReturn = (char**)malloc(sizeof(char*)*iArgNumber);
	pReturn[0] = (char*)malloc(sizeof( char) * ( 7 ));
	strcpy( pReturn[0], "MP4Box" );//just a place holder , will never be read.
	pReturn[0][6] = '\0';

	for ( l = 0; l <= iLength ; l++ )
	{
		if( sCommand[l] ==  ' ' ||  l == ( iLength ) )
		{
			pReturn[k] = (char*)malloc(sizeof( char) * ( l - iPreviousPos + 1));
			strncpy( pReturn[k], sCommand + iPreviousPos, l - iPreviousPos );
			pReturn[k][l-iPreviousPos] = '\0';
			iPreviousPos = l + 1;
			k++;
		}
	}
	*iNbArg = iArgNumber;
	return pReturn;
}
#ifdef __cplusplus
}
#endif
