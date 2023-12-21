/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */


// With these flags defined, this source file will dynamically
// load the corresponding functions.  Disabled by default.
#define __CUDA_API_VERSION 4000

#include <stdio.h>
#include <string.h>

#include <gpac/tools.h>


#if ((defined(WIN32) || defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_DARWIN)) && !defined(GPAC_DISABLE_NVDEC))

#include "dec_nvdec_sdk.h"

tcuInit                               *_cuInit;
tcuDriverGetVersion                   *cuDriverGetVersion;
tcuDeviceGet                          *cuDeviceGet;
tcuDeviceGetCount                     *cuDeviceGetCount;
tcuDeviceGetName                      *cuDeviceGetName;
tcuDeviceComputeCapability            *cuDeviceComputeCapability;
tcuDeviceTotalMem                     *cuDeviceTotalMem;
tcuDeviceGetProperties                *cuDeviceGetProperties;
tcuDeviceGetAttribute                 *cuDeviceGetAttribute;
tcuCtxCreate                          *cuCtxCreate;
tcuCtxDestroy                         *cuCtxDestroy;
tcuCtxAttach                          *cuCtxAttach;
tcuCtxDetach                          *cuCtxDetach;
tcuCtxPushCurrent                     *cuCtxPushCurrent;
tcuCtxPopCurrent                      *cuCtxPopCurrent;
tcuCtxGetCurrent                      *cuCtxGetCurrent;
tcuCtxSetCurrent                      *cuCtxSetCurrent;
tcuCtxGetDevice                       *cuCtxGetDevice;
tcuCtxSynchronize                     *cuCtxSynchronize;
tcuModuleLoad                         *cuModuleLoad;
tcuModuleLoadData                     *cuModuleLoadData;
tcuModuleLoadDataEx                   *cuModuleLoadDataEx;
tcuModuleLoadFatBinary                *cuModuleLoadFatBinary;
tcuModuleUnload                       *cuModuleUnload;
tcuModuleGetFunction                  *cuModuleGetFunction;
tcuModuleGetGlobal                    *cuModuleGetGlobal;
tcuModuleGetTexRef                    *cuModuleGetTexRef;
tcuModuleGetSurfRef                   *cuModuleGetSurfRef;
tcuMemGetInfo                         *cuMemGetInfo;
tcuMemAlloc                           *cuMemAlloc;
tcuMemAllocPitch                      *cuMemAllocPitch;
tcuMemFree                            *cuMemFree;
tcuMemGetAddressRange                 *cuMemGetAddressRange;
tcuMemAllocHost                       *cuMemAllocHost;
tcuMemFreeHost                        *cuMemFreeHost;
tcuMemHostAlloc                       *cuMemHostAlloc;
tcuMemHostGetDevicePointer            *cuMemHostGetDevicePointer;
tcuMemHostRegister                    *cuMemHostRegister;
tcuMemHostUnregister                  *cuMemHostUnregister;
tcuMemcpyHtoD                         *cuMemcpyHtoD;
tcuMemcpyDtoH                         *cuMemcpyDtoH;
tcuMemcpyDtoD                         *cuMemcpyDtoD;
tcuMemcpyDtoA                         *cuMemcpyDtoA;
tcuMemcpyAtoD                         *cuMemcpyAtoD;
tcuMemcpyHtoA                         *cuMemcpyHtoA;
tcuMemcpyAtoH                         *cuMemcpyAtoH;
tcuMemcpyAtoA                         *cuMemcpyAtoA;
tcuMemcpy2D                           *cuMemcpy2D;
tcuMemcpy2DUnaligned                  *cuMemcpy2DUnaligned;
tcuMemcpy3D                           *cuMemcpy3D;
tcuMemcpyHtoDAsync                    *cuMemcpyHtoDAsync;
tcuMemcpyDtoHAsync                    *cuMemcpyDtoHAsync;
tcuMemcpyDtoDAsync                    *cuMemcpyDtoDAsync;
tcuMemcpyHtoAAsync                    *cuMemcpyHtoAAsync;
tcuMemcpyAtoHAsync                    *cuMemcpyAtoHAsync;
tcuMemcpy2DAsync                      *cuMemcpy2DAsync;
tcuMemcpy3DAsync                      *cuMemcpy3DAsync;
tcuMemcpy                             *cuMemcpy;
tcuMemcpyPeer                         *cuMemcpyPeer;
tcuMemsetD8                           *cuMemsetD8;
tcuMemsetD16                          *cuMemsetD16;
tcuMemsetD32                          *cuMemsetD32;
tcuMemsetD2D8                         *cuMemsetD2D8;
tcuMemsetD2D16                        *cuMemsetD2D16;
tcuMemsetD2D32                        *cuMemsetD2D32;
tcuFuncSetBlockShape                  *cuFuncSetBlockShape;
tcuFuncSetSharedSize                  *cuFuncSetSharedSize;
tcuFuncGetAttribute                   *cuFuncGetAttribute;
tcuFuncSetCacheConfig                 *cuFuncSetCacheConfig;
tcuLaunchKernel                       *cuLaunchKernel;
tcuArrayCreate                        *cuArrayCreate;
tcuArrayGetDescriptor                 *cuArrayGetDescriptor;
tcuArrayDestroy                       *cuArrayDestroy;
tcuArray3DCreate                      *cuArray3DCreate;
tcuArray3DGetDescriptor               *cuArray3DGetDescriptor;
tcuTexRefCreate                       *cuTexRefCreate;
tcuTexRefDestroy                      *cuTexRefDestroy;
tcuTexRefSetArray                     *cuTexRefSetArray;
tcuTexRefSetAddress                   *cuTexRefSetAddress;
tcuTexRefSetAddress2D                 *cuTexRefSetAddress2D;
tcuTexRefSetFormat                    *cuTexRefSetFormat;
tcuTexRefSetAddressMode               *cuTexRefSetAddressMode;
tcuTexRefSetFilterMode                *cuTexRefSetFilterMode;
tcuTexRefSetFlags                     *cuTexRefSetFlags;
tcuTexRefGetAddress                   *cuTexRefGetAddress;
tcuTexRefGetArray                     *cuTexRefGetArray;
tcuTexRefGetAddressMode               *cuTexRefGetAddressMode;
tcuTexRefGetFilterMode                *cuTexRefGetFilterMode;
tcuTexRefGetFormat                    *cuTexRefGetFormat;
tcuTexRefGetFlags                     *cuTexRefGetFlags;
tcuSurfRefSetArray                    *cuSurfRefSetArray;
tcuSurfRefGetArray                    *cuSurfRefGetArray;
tcuParamSetSize                       *cuParamSetSize;
tcuParamSeti                          *cuParamSeti;
tcuParamSetf                          *cuParamSetf;
tcuParamSetv                          *cuParamSetv;
tcuParamSetTexRef                     *cuParamSetTexRef;
tcuLaunch                             *cuLaunch;
tcuLaunchGrid                         *cuLaunchGrid;
tcuLaunchGridAsync                    *cuLaunchGridAsync;
tcuEventCreate                        *cuEventCreate;
tcuEventRecord                        *cuEventRecord;
tcuEventQuery                         *cuEventQuery;
tcuEventSynchronize                   *cuEventSynchronize;
tcuEventDestroy                       *cuEventDestroy;
tcuEventElapsedTime                   *cuEventElapsedTime;
tcuStreamCreate                       *cuStreamCreate;
tcuStreamQuery                        *cuStreamQuery;
tcuStreamSynchronize                  *cuStreamSynchronize;
tcuStreamDestroy                      *cuStreamDestroy;
tcuGraphicsUnregisterResource         *cuGraphicsUnregisterResource;
tcuGraphicsSubResourceGetMappedArray  *cuGraphicsSubResourceGetMappedArray;
tcuGraphicsResourceGetMappedPointer   *cuGraphicsResourceGetMappedPointer;
tcuGraphicsResourceSetMapFlags        *cuGraphicsResourceSetMapFlags;
tcuGraphicsMapResources               *cuGraphicsMapResources;
tcuGraphicsUnmapResources             *cuGraphicsUnmapResources;
tcuGetExportTable                     *cuGetExportTable;
tcuCtxSetLimit                        *cuCtxSetLimit;
tcuCtxGetLimit                        *cuCtxGetLimit;
tcuMemHostGetFlags                    *cuMemHostGetFlags;

    // GL/CUDA interop
    #if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    tcuWGLGetDevice                       *cuWGLGetDevice;
    #endif

    //#if __CUDA_API_VERSION >= 3020
    tcuGLCtxCreate                        *cuGLCtxCreate;
    tcuGLCtxCreate                        *cuGLCtxCreate_v2;
    tcuGLMapBufferObject                  *cuGLMapBufferObject;
    tcuGLMapBufferObject                  *cuGLMapBufferObject_v2;
    tcuGLMapBufferObjectAsync             *cuGLMapBufferObjectAsync;
    //#endif

#if __CUDA_API_VERSION >= 6050
    tcuGLGetDevices                       *cuGLGetDevices;
#endif

    tcuGLInit                             *cuGLInit; // deprecated in CUDA 3.0
    tcuGraphicsGLRegisterBuffer           *cuGraphicsGLRegisterBuffer;
    tcuGraphicsGLRegisterImage            *cuGraphicsGLRegisterImage;
    tcuGLSetBufferObjectMapFlags          *cuGLSetBufferObjectMapFlags;
    tcuGLRegisterBufferObject             *cuGLRegisterBufferObject;

    tcuGLUnmapBufferObject                *cuGLUnmapBufferObject;
    tcuGLUnmapBufferObjectAsync           *cuGLUnmapBufferObjectAsync;

    tcuGLUnregisterBufferObject           *cuGLUnregisterBufferObject;
    tcuGLGetDevices                       *cuGLGetDevices; // CUDA 6.5 only



#if !defined(__APPLE__)
tcuvidCreateVideoSource               *cuvidCreateVideoSource;
tcuvidCreateVideoSourceW              *cuvidCreateVideoSourceW;
tcuvidDestroyVideoSource              *cuvidDestroyVideoSource;
tcuvidSetVideoSourceState             *cuvidSetVideoSourceState;
tcuvidGetVideoSourceState             *cuvidGetVideoSourceState;
tcuvidGetSourceVideoFormat            *cuvidGetSourceVideoFormat;
tcuvidGetSourceAudioFormat            *cuvidGetSourceAudioFormat;
#endif

tcuvidCreateVideoParser               *cuvidCreateVideoParser;
tcuvidParseVideoData                  *cuvidParseVideoData;
tcuvidDestroyVideoParser              *cuvidDestroyVideoParser;

tcuvidCreateDecoder                   *cuvidCreateDecoder;
tcuvidDestroyDecoder                  *cuvidDestroyDecoder;
tcuvidDecodePicture                   *cuvidDecodePicture;

tcuvidMapVideoFrame                   *cuvidMapVideoFrame;
tcuvidUnmapVideoFrame                 *cuvidUnmapVideoFrame;

#if defined(WIN64) || defined(_WIN64) || defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
tcuvidMapVideoFrame64                 *cuvidMapVideoFrame64;
tcuvidUnmapVideoFrame64               *cuvidUnmapVideoFrame64;
#endif

//tcuvidGetVideoFrameSurface            *cuvidGetVideoFrameSurface;
tcuvidCtxLockCreate                   *cuvidCtxLockCreate;
tcuvidCtxLockDestroy                  *cuvidCtxLockDestroy;
tcuvidCtxLock                         *cuvidCtxLock;
tcuvidCtxUnlock                       *cuvidCtxUnlock;



#define STRINGIFY(X) #X

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>

#ifdef UNICODE
static LPCWSTR __CudaLibName = L"nvcuda.dll";
static LPCWSTR __CuvidLibName = L"nvcuvid.dll";
#else
static LPCSTR __CudaLibName = "nvcuda.dll";
static LPCSTR __CuvidLibName = "nvcuvid.dll";
#endif

typedef HMODULE CUDADRIVER;
typedef HMODULE CUVIDDRIVER;

static CUresult LOAD_LIBRARY_CUDA(CUDADRIVER *pInstance)
{
    *pInstance = LoadLibrary(__CudaLibName);
    if (*pInstance == NULL) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] LoadLibrary \"%s\" failed!\n", __CudaLibName));
        return CUDA_ERROR_SHARED_OBJECT_INIT_FAILED;
    }
    return CUDA_SUCCESS;
}

static CUresult LOAD_LIBRARY_CUVID(CUVIDDRIVER *pInstance)
{
    *pInstance = LoadLibrary(__CuvidLibName);
    if (*pInstance == NULL) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] LoadLibrary \"%s\" failed!\n", __CuvidLibName));
        return CUDA_ERROR_UNKNOWN;
    }
    return CUDA_SUCCESS;
}

#define GET_PROC_EX(name, alias, required)                     \
    alias = (t##name *)GetProcAddress(curr_lib, #name);               \
    if (alias == NULL && required) {                                    \
        GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] Failed to find required function \"%s\" \n",       \
               #name));                                  \
        }

#define GET_PROC_EX_V2(name, alias, required)                           \
    alias = (t##name *)GetProcAddress(curr_lib, STRINGIFY(name##_v2));\
    if (alias == NULL && required) {                                    \
        GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] Failed to find required function \"%s\" \n",       \
               STRINGIFY(name##_v2)));                       \
        }

#elif defined(__unix__) || defined(__APPLE__) || defined(__MACOSX)

#include <dlfcn.h>

#if defined(__APPLE__) || defined(__MACOSX)
static char __CudaLibName[] = "/usr/local/cuda/lib/libcuda.dylib";
#else
static char __CudaLibName[] = "libcuda.so";
#endif

static char __CuvidLibName[] = "libnvcuvid.so";

typedef void *CUVIDDRIVER;

static CUresult LOAD_LIBRARY_CUDA(CUDADRIVER *pInstance)
{
    *pInstance = dlopen(__CudaLibName, RTLD_NOW);
    if (*pInstance == NULL) {
        GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec] dlopen \"%s\" failed!\n", __CudaLibName));
        return CUDA_ERROR_SHARED_OBJECT_INIT_FAILED;
    }
    return CUDA_SUCCESS;
}

static CUresult LOAD_LIBRARY_CUVID(CUVIDDRIVER *pInstance)
{
    *pInstance = dlopen(__CuvidLibName, RTLD_NOW);
    if (*pInstance == NULL) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CODEC, ("[NVDec]dlopen \"%s\" failed!\n", __CuvidLibName));
        return CUDA_ERROR_UNKNOWN;
    }
    return CUDA_SUCCESS;
}

#define GET_PROC_EX(name, alias, required)                              \
    alias = (t##name *)dlsym(curr_lib, #name);                        \
    if (alias == NULL && required) {                                    \
        GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec] Failed to find required function \"%s\"\n",       \
               #name));                                  \
        }

#define GET_PROC_EX_V2(name, alias, required)                           \
    alias = (t##name *)dlsym(curr_lib, STRINGIFY(name##_v2));         \
    if (alias == NULL && required) {                                    \
        GF_LOG(GF_LOG_WARNING, GF_LOG_CODEC, ("[NVDec]Failed to find required function \"%s\"\n",       \
               STRINGIFY(name##_v2)));                    \
        }

#else
#error unsupported platform
#endif

#define CHECKED_CALL(call)              \
    do {                                \
        CUresult result = (call);       \
        if (CUDA_SUCCESS != result) {   \
            return result;              \
        }                               \
    } while(0)

#define GET_PROC_REQUIRED(name) GET_PROC_EX(name,name,1)
#define GET_PROC_OPTIONAL(name) GET_PROC_EX(name,name,0)
#define GET_PROC(name)          GET_PROC_REQUIRED(name)
#define GET_PROC_V2(name)       GET_PROC_EX_V2(name,name,1)

CUresult CUDAAPI cuInitGL(unsigned int Flags, int cudaVersion, CUDADRIVER curr_lib)
{

    return CUDA_SUCCESS;
}

CUDADRIVER CudaDrvLib=0;
CUVIDDRIVER CuvidDrvLib = 0;

void CUDAAPI cuUninit()
{
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
	if (CudaDrvLib) FreeLibrary((HMODULE)CudaDrvLib);
	if (CuvidDrvLib) FreeLibrary((HMODULE)CuvidDrvLib);
#else
	if (CudaDrvLib) dlclose(CudaDrvLib);
	if (CuvidDrvLib) dlclose(CuvidDrvLib);
#endif
	CudaDrvLib = 0;
	CuvidDrvLib = 0;
}

CUresult CUDAAPI cuInit(unsigned int Flags, int cudaVersion)
{
    int driverVer = 1000;
	CUDADRIVER curr_lib ;

	if (CudaDrvLib) return CUDA_SUCCESS;

	CHECKED_CALL(LOAD_LIBRARY_CUDA(&CudaDrvLib));
	curr_lib = CudaDrvLib;

	// cuInit is required; alias it to _cuInit
    GET_PROC_EX(cuInit, _cuInit, 1);
    CHECKED_CALL(_cuInit(Flags));

    // available since 2.2. if not present, version 1.0 is assumed
    GET_PROC_OPTIONAL(cuDriverGetVersion);

    if (cuDriverGetVersion)
    {
        CHECKED_CALL(cuDriverGetVersion(&driverVer));
    }

    // fetch all function pointers
    GET_PROC(cuDeviceGet);
    GET_PROC(cuDeviceGetCount);
    GET_PROC(cuDeviceGetName);
    GET_PROC(cuDeviceComputeCapability);
    GET_PROC(cuDeviceGetProperties);
    GET_PROC(cuDeviceGetAttribute);
    GET_PROC(cuCtxDestroy);
    GET_PROC(cuCtxAttach);
    GET_PROC(cuCtxDetach);
    GET_PROC(cuCtxPushCurrent);
    GET_PROC(cuCtxPopCurrent);
    GET_PROC(cuCtxGetDevice);
    GET_PROC(cuCtxSynchronize);
    GET_PROC(cuModuleLoad);
    GET_PROC(cuModuleLoadData);
    GET_PROC(cuModuleUnload);
    GET_PROC(cuModuleGetFunction);
    GET_PROC(cuModuleGetTexRef);
    GET_PROC(cuMemFreeHost);
    GET_PROC(cuMemHostAlloc);
    GET_PROC(cuFuncSetBlockShape);
    GET_PROC(cuFuncSetSharedSize);
    GET_PROC(cuFuncGetAttribute);
    GET_PROC(cuArrayDestroy);
    GET_PROC(cuTexRefCreate);
    GET_PROC(cuTexRefDestroy);
    GET_PROC(cuTexRefSetArray);
    GET_PROC(cuTexRefSetFormat);
    GET_PROC(cuTexRefSetAddressMode);
    GET_PROC(cuTexRefSetFilterMode);
    GET_PROC(cuTexRefSetFlags);
    GET_PROC(cuTexRefGetArray);
    GET_PROC(cuTexRefGetAddressMode);
    GET_PROC(cuTexRefGetFilterMode);
    GET_PROC(cuTexRefGetFormat);
    GET_PROC(cuTexRefGetFlags);
    GET_PROC(cuParamSetSize);
    GET_PROC(cuParamSeti);
    GET_PROC(cuParamSetf);
    GET_PROC(cuParamSetv);
    GET_PROC(cuParamSetTexRef);
    GET_PROC(cuLaunch);
    GET_PROC(cuLaunchGrid);
    GET_PROC(cuLaunchGridAsync);
    GET_PROC(cuEventCreate);
    GET_PROC(cuEventRecord);
    GET_PROC(cuEventQuery);
    GET_PROC(cuEventSynchronize);
    GET_PROC(cuEventDestroy);
    GET_PROC(cuEventElapsedTime);
    GET_PROC(cuStreamCreate);
    GET_PROC(cuStreamQuery);
    GET_PROC(cuStreamSynchronize);
    GET_PROC(cuStreamDestroy);

    // These could be _v2 interfaces
    if (cudaVersion >= 4000)
    {
        GET_PROC_V2(cuCtxDestroy);
        GET_PROC_V2(cuCtxPopCurrent);
        GET_PROC_V2(cuCtxPushCurrent);
        GET_PROC_V2(cuStreamDestroy);
        GET_PROC_V2(cuEventDestroy);
    }

    if (cudaVersion >= 3020)
    {
        GET_PROC_V2(cuDeviceTotalMem);
        GET_PROC_V2(cuCtxCreate);
        GET_PROC_V2(cuModuleGetGlobal);
        GET_PROC_V2(cuMemGetInfo);
        GET_PROC_V2(cuMemAlloc);
        GET_PROC_V2(cuMemAllocPitch);
        GET_PROC_V2(cuMemFree);
        GET_PROC_V2(cuMemGetAddressRange);
        GET_PROC_V2(cuMemAllocHost);
        GET_PROC_V2(cuMemHostGetDevicePointer);
        GET_PROC_V2(cuMemcpyHtoD);
        GET_PROC_V2(cuMemcpyDtoH);
        GET_PROC_V2(cuMemcpyDtoD);
        GET_PROC_V2(cuMemcpyDtoA);
        GET_PROC_V2(cuMemcpyAtoD);
        GET_PROC_V2(cuMemcpyHtoA);
        GET_PROC_V2(cuMemcpyAtoH);
        GET_PROC_V2(cuMemcpyAtoA);
        GET_PROC_V2(cuMemcpy2D);
        GET_PROC_V2(cuMemcpy2DUnaligned);
        GET_PROC_V2(cuMemcpy3D);
        GET_PROC_V2(cuMemcpyHtoDAsync);
        GET_PROC_V2(cuMemcpyDtoHAsync);
        GET_PROC_V2(cuMemcpyHtoAAsync);
        GET_PROC_V2(cuMemcpyAtoHAsync);
        GET_PROC_V2(cuMemcpy2DAsync);
        GET_PROC_V2(cuMemcpy3DAsync);
        GET_PROC_V2(cuMemsetD8);
        GET_PROC_V2(cuMemsetD16);
        GET_PROC_V2(cuMemsetD32);
        GET_PROC_V2(cuMemsetD2D8);
        GET_PROC_V2(cuMemsetD2D16);
        GET_PROC_V2(cuMemsetD2D32);
        GET_PROC_V2(cuArrayCreate);
        GET_PROC_V2(cuArrayGetDescriptor);
        GET_PROC_V2(cuArray3DCreate);
        GET_PROC_V2(cuArray3DGetDescriptor);
        GET_PROC_V2(cuTexRefSetAddress);
        GET_PROC_V2(cuTexRefSetAddress2D);
        GET_PROC_V2(cuTexRefGetAddress);
    }
    else
    {
        GET_PROC(cuDeviceTotalMem);
        GET_PROC(cuCtxCreate);
        GET_PROC(cuModuleGetGlobal);
        GET_PROC(cuMemGetInfo);
        GET_PROC(cuMemAlloc);
        GET_PROC(cuMemAllocPitch);
        GET_PROC(cuMemFree);
        GET_PROC(cuMemGetAddressRange);
        GET_PROC(cuMemAllocHost);
        GET_PROC(cuMemHostGetDevicePointer);
        GET_PROC(cuMemcpyHtoD);
        GET_PROC(cuMemcpyDtoH);
        GET_PROC(cuMemcpyDtoD);
        GET_PROC(cuMemcpyDtoA);
        GET_PROC(cuMemcpyAtoD);
        GET_PROC(cuMemcpyHtoA);
        GET_PROC(cuMemcpyAtoH);
        GET_PROC(cuMemcpyAtoA);
        GET_PROC(cuMemcpy2D);
        GET_PROC(cuMemcpy2DUnaligned);
        GET_PROC(cuMemcpy3D);
        GET_PROC(cuMemcpyHtoDAsync);
        GET_PROC(cuMemcpyDtoHAsync);
        GET_PROC(cuMemcpyHtoAAsync);
        GET_PROC(cuMemcpyAtoHAsync);
        GET_PROC(cuMemcpy2DAsync);
        GET_PROC(cuMemcpy3DAsync);
        GET_PROC(cuMemsetD8);
        GET_PROC(cuMemsetD16);
        GET_PROC(cuMemsetD32);
        GET_PROC(cuMemsetD2D8);
        GET_PROC(cuMemsetD2D16);
        GET_PROC(cuMemsetD2D32);
        GET_PROC(cuArrayCreate);
        GET_PROC(cuArrayGetDescriptor);
        GET_PROC(cuArray3DCreate);
        GET_PROC(cuArray3DGetDescriptor);
        GET_PROC(cuTexRefSetAddress);
        GET_PROC(cuTexRefSetAddress2D);
        GET_PROC(cuTexRefGetAddress);
    }

    // The following functions are specific to CUDA versions
    if (driverVer >= 2010)
    {
        GET_PROC(cuModuleLoadDataEx);
        GET_PROC(cuModuleLoadFatBinary);
    }

    if (driverVer >= 2030)
    {
        GET_PROC(cuMemHostGetFlags);
    }

    if (driverVer >= 3000)
    {
        GET_PROC(cuMemcpyDtoDAsync);
        GET_PROC(cuFuncSetCacheConfig);

        GET_PROC(cuGraphicsUnregisterResource);
        GET_PROC(cuGraphicsSubResourceGetMappedArray);

#if (__CUDA_API_VERSION >= 3020)
        if (cudaVersion >= 3020)
        {
            GET_PROC_V2(cuGraphicsResourceGetMappedPointer);
        }
        else
        {
            GET_PROC(cuGraphicsResourceGetMappedPointer);
        }
#endif
        GET_PROC(cuGraphicsResourceSetMapFlags);
        GET_PROC(cuGraphicsMapResources);
        GET_PROC(cuGraphicsUnmapResources);
        GET_PROC(cuGetExportTable);
    }

    if (driverVer >= 3010)
    {
        GET_PROC(cuModuleGetSurfRef);
        GET_PROC(cuSurfRefSetArray);
        GET_PROC(cuSurfRefGetArray);
        GET_PROC(cuCtxSetLimit);
        GET_PROC(cuCtxGetLimit);
    }

    if (driverVer >= 4000)
    {
        GET_PROC(cuCtxSetCurrent);
        GET_PROC(cuCtxGetCurrent);
        GET_PROC(cuMemHostRegister);
        GET_PROC(cuMemHostUnregister);
        GET_PROC(cuMemcpy);
        GET_PROC(cuMemcpyPeer);
        GET_PROC(cuLaunchKernel);
    }

	/*load OpenGL*/
    if (cudaVersion >= 2010)
    {
        GET_PROC(cuGLCtxCreate);
        GET_PROC(cuGraphicsGLRegisterBuffer);
        GET_PROC(cuGraphicsGLRegisterImage);
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
        GET_PROC(cuWGLGetDevice);
#endif
    }
    if (cudaVersion >= 2030)
    {
        GET_PROC(cuGraphicsGLRegisterBuffer);
        GET_PROC(cuGraphicsGLRegisterImage);
    }
    if (cudaVersion >= 3000)
    {
        GET_PROC(cuGLGetDevices);
#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
        GET_PROC(cuWGLGetDevice);
#endif
        GET_PROC_V2(cuGLCtxCreate);

        GET_PROC_V2(cuGLMapBufferObject);
        GET_PROC(cuGLUnmapBufferObject);
        GET_PROC(cuGLMapBufferObjectAsync);
        GET_PROC(cuGLUnmapBufferObjectAsync);
        GET_PROC(cuGLRegisterBufferObject);
        GET_PROC(cuGLUnregisterBufferObject);
        GET_PROC(cuGLSetBufferObjectMapFlags);
    }

	//init cuvid
    CHECKED_CALL(LOAD_LIBRARY_CUVID(&CuvidDrvLib));

	curr_lib = CuvidDrvLib;
#if !defined(__APPLE__)
    // fetch all function pointers
    GET_PROC(cuvidCreateVideoSource);
    GET_PROC(cuvidCreateVideoSourceW);
    GET_PROC(cuvidDestroyVideoSource);
    GET_PROC(cuvidSetVideoSourceState);
    GET_PROC(cuvidGetVideoSourceState);
    GET_PROC(cuvidGetSourceVideoFormat);
    GET_PROC(cuvidGetSourceAudioFormat);
#endif

    GET_PROC(cuvidCreateVideoParser);
    GET_PROC(cuvidParseVideoData);
    GET_PROC(cuvidDestroyVideoParser);

    GET_PROC(cuvidCreateDecoder);
    GET_PROC(cuvidDestroyDecoder);
    GET_PROC(cuvidDecodePicture);

#if defined(WIN64) || defined(_WIN64) || defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
    GET_PROC(cuvidMapVideoFrame64);
    GET_PROC(cuvidUnmapVideoFrame64);
    cuvidMapVideoFrame   = cuvidMapVideoFrame64;
    cuvidUnmapVideoFrame = cuvidUnmapVideoFrame64;
#else
    GET_PROC(cuvidMapVideoFrame);
    GET_PROC(cuvidUnmapVideoFrame);
#endif

//    GET_PROC(cuvidGetVideoFrameSurface);
    GET_PROC(cuvidCtxLockCreate);
    GET_PROC(cuvidCtxLockDestroy);
    GET_PROC(cuvidCtxLock);
    GET_PROC(cuvidCtxUnlock);


	return CUDA_SUCCESS;
}


// CUDA Driver API errors
const char *cudaGetErrorEnum(CUresult error)
{
	switch (error) {
	case CUDA_SUCCESS: return "CUDA_SUCCESS";
	case CUDA_ERROR_INVALID_VALUE: return "CUDA_ERROR_INVALID_VALUE";
	case CUDA_ERROR_OUT_OF_MEMORY:	return "CUDA_ERROR_OUT_OF_MEMORY";
	case CUDA_ERROR_NOT_INITIALIZED: return "CUDA_ERROR_NOT_INITIALIZED";
	case CUDA_ERROR_DEINITIALIZED:	return "CUDA_ERROR_DEINITIALIZED";
	case CUDA_ERROR_PROFILER_DISABLED:	return "CUDA_ERROR_PROFILER_DISABLED";
	case CUDA_ERROR_PROFILER_NOT_INITIALIZED:	return "CUDA_ERROR_PROFILER_NOT_INITIALIZED";
	case CUDA_ERROR_PROFILER_ALREADY_STARTED:	return "CUDA_ERROR_PROFILER_ALREADY_STARTED";
	case CUDA_ERROR_PROFILER_ALREADY_STOPPED:	return "CUDA_ERROR_PROFILER_ALREADY_STOPPED";
	case CUDA_ERROR_NO_DEVICE:	return "CUDA_ERROR_NO_DEVICE";
	case CUDA_ERROR_INVALID_DEVICE:	return "CUDA_ERROR_INVALID_DEVICE";
	case CUDA_ERROR_INVALID_IMAGE:	return "CUDA_ERROR_INVALID_IMAGE";
	case CUDA_ERROR_INVALID_CONTEXT:	return "CUDA_ERROR_INVALID_CONTEXT";
	case CUDA_ERROR_CONTEXT_ALREADY_CURRENT:	return "CUDA_ERROR_CONTEXT_ALREADY_CURRENT";
	case CUDA_ERROR_MAP_FAILED:	return "CUDA_ERROR_MAP_FAILED";
	case CUDA_ERROR_UNMAP_FAILED:	return "CUDA_ERROR_UNMAP_FAILED";
	case CUDA_ERROR_ARRAY_IS_MAPPED:	return "CUDA_ERROR_ARRAY_IS_MAPPED";
	case CUDA_ERROR_ALREADY_MAPPED:	return "CUDA_ERROR_ALREADY_MAPPED";
	case CUDA_ERROR_NO_BINARY_FOR_GPU:	return "CUDA_ERROR_NO_BINARY_FOR_GPU";
	case CUDA_ERROR_ALREADY_ACQUIRED:	return "CUDA_ERROR_ALREADY_ACQUIRED";
	case CUDA_ERROR_NOT_MAPPED:	return "CUDA_ERROR_NOT_MAPPED";
	case CUDA_ERROR_NOT_MAPPED_AS_ARRAY:	return "CUDA_ERROR_NOT_MAPPED_AS_ARRAY";
	case CUDA_ERROR_NOT_MAPPED_AS_POINTER:	return "CUDA_ERROR_NOT_MAPPED_AS_POINTER";
	case CUDA_ERROR_ECC_UNCORRECTABLE:	return "CUDA_ERROR_ECC_UNCORRECTABLE";
	case CUDA_ERROR_UNSUPPORTED_LIMIT:	return "CUDA_ERROR_UNSUPPORTED_LIMIT";
	case CUDA_ERROR_CONTEXT_ALREADY_IN_USE:	return "CUDA_ERROR_CONTEXT_ALREADY_IN_USE";
	case CUDA_ERROR_INVALID_SOURCE:	return "CUDA_ERROR_INVALID_SOURCE";
	case CUDA_ERROR_FILE_NOT_FOUND:	return "CUDA_ERROR_FILE_NOT_FOUND";
	case CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND:	return "CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND";
	case CUDA_ERROR_SHARED_OBJECT_INIT_FAILED:	return "CUDA_ERROR_SHARED_OBJECT_INIT_FAILED";
	case CUDA_ERROR_OPERATING_SYSTEM:	return "CUDA_ERROR_OPERATING_SYSTEM";
	case CUDA_ERROR_INVALID_HANDLE:	return "CUDA_ERROR_INVALID_HANDLE";
	case CUDA_ERROR_NOT_FOUND:	return "CUDA_ERROR_NOT_FOUND";
	case CUDA_ERROR_NOT_READY:	return "CUDA_ERROR_NOT_READY";
	case CUDA_ERROR_LAUNCH_FAILED:	return "CUDA_ERROR_LAUNCH_FAILED";
	case CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES:	return "CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES";
	case CUDA_ERROR_LAUNCH_TIMEOUT:	return "CUDA_ERROR_LAUNCH_TIMEOUT";
	case CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING:	return "CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING";
	case CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED:	return "CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED";
	case CUDA_ERROR_PEER_ACCESS_NOT_ENABLED:	return "CUDA_ERROR_PEER_ACCESS_NOT_ENABLED";
	case CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE:	return "CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE";
	case CUDA_ERROR_CONTEXT_IS_DESTROYED:	return "CUDA_ERROR_CONTEXT_IS_DESTROYED";
	case CUDA_ERROR_PEER_MEMORY_ALREADY_REGISTERED:	return "CUDA_ERROR_PEER_MEMORY_ALREADY_REGISTERED";
	case CUDA_ERROR_PEER_MEMORY_NOT_REGISTERED:	return "CUDA_ERROR_PEER_MEMORY_NOT_REGISTERED";
	case CUDA_ERROR_UNKNOWN: return "CUDA_ERROR_UNKNOWN";
	}
	return "<unknown>";
}

#endif // ((defined(WIN32) || defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_DARWIN)) && !defined(GPAC_DISABLE_NVDEC))
