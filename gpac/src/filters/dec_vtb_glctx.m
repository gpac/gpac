
#include <OpenGLES/EAGL.h>

void* myGetGLContext() {
	return (__bridge void*)[EAGLContext currentContext];
}

