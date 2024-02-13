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

#ifndef __cuda_tools_h__
#define __cuda_tools_h__

#include <stdlib.h>

#if defined(WIN32) || defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_DARWIN)

#ifdef __cplusplus
extern "C" {
#endif

//needed for dec_nvdec_sdk.h which uses GL prototypes
#ifndef GPAC_DISABLE_3D
#include "../compositor/gl_inc.h"
#else
typedef u32 GLuint;
typedef u32 GLenum;
#endif

#ifndef __CUDA_API_VERSION
#define __CUDA_API_VERSION 4000
#endif

/**
 * \defgroup CUDA_DRIVER CUDA Driver API
 *
 * This section describes the low-level CUDA driver application programming
 * interface.
 *
 * @{
 */

/**
 * \defgroup CUDA_TYPES Data types used by CUDA driver
 * @{
 */

/**
 * CUDA API version number
 */
#define CUDA_VERSION 4000 /* 4.0 */

/**
 * CUDA device pointer
 */
#if __CUDA_API_VERSION >= 3020

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64) || defined(__aarch64__)
    typedef unsigned long long CUdeviceptr;
#else
    typedef unsigned int CUdeviceptr;
#endif

#endif /* __CUDA_API_VERSION >= 3020 */

typedef int CUdevice;                                     /**< CUDA device */
typedef struct CUctx_st *CUcontext;                       /**< CUDA context */
typedef struct CUmod_st *CUmodule;                        /**< CUDA module */
typedef struct CUfunc_st *CUfunction;                     /**< CUDA function */
typedef struct CUarray_st *CUarray;                       /**< CUDA array */
typedef struct CUtexref_st *CUtexref;                     /**< CUDA texture reference */
typedef struct CUsurfref_st *CUsurfref;                   /**< CUDA surface reference */
typedef struct CUevent_st *CUevent;                       /**< CUDA event */
typedef struct CUstream_st *CUstream;                     /**< CUDA stream */
typedef struct CUgraphicsResource_st *CUgraphicsResource; /**< CUDA graphics interop resource */

typedef struct CUuuid_st                                  /**< CUDA definition of UUID */
{
    char bytes[16];
} CUuuid;

/**
 * Context creation flags
 */
typedef enum CUctx_flags_enum
{
    CU_CTX_SCHED_AUTO          = 0x00, /**< Automatic scheduling */
    CU_CTX_SCHED_SPIN          = 0x01, /**< Set spin as default scheduling */
    CU_CTX_SCHED_YIELD         = 0x02, /**< Set yield as default scheduling */
    CU_CTX_SCHED_BLOCKING_SYNC = 0x04, /**< Set blocking synchronization as default scheduling */
    CU_CTX_BLOCKING_SYNC       = 0x04, /**< Set blocking synchronization as default scheduling \deprecated */
    CU_CTX_MAP_HOST            = 0x08, /**< Support mapped pinned allocations */
    CU_CTX_LMEM_RESIZE_TO_MAX  = 0x10, /**< Keep local memory allocation after launch */
#if __CUDA_API_VERSION < 4000
    CU_CTX_SCHED_MASK          = 0x03,
    CU_CTX_FLAGS_MASK          = 0x1f
#else
    CU_CTX_SCHED_MASK          = 0x07,
    CU_CTX_PRIMARY             = 0x20, /**< Initialize and return the primary context */
    CU_CTX_FLAGS_MASK          = 0x3f
#endif
} CUctx_flags;

/**
 * Event creation flags
 */
typedef enum CUevent_flags_enum
{
    CU_EVENT_DEFAULT        = 0, /**< Default event flag */
    CU_EVENT_BLOCKING_SYNC  = 1, /**< Event uses blocking synchronization */
    CU_EVENT_DISABLE_TIMING = 2  /**< Event will not record timing data */
} CUevent_flags;

/**
 * Array formats
 */
typedef enum CUarray_format_enum
{
    CU_AD_FORMAT_UNSIGNED_INT8  = 0x01, /**< Unsigned 8-bit integers */
    CU_AD_FORMAT_UNSIGNED_INT16 = 0x02, /**< Unsigned 16-bit integers */
    CU_AD_FORMAT_UNSIGNED_INT32 = 0x03, /**< Unsigned 32-bit integers */
    CU_AD_FORMAT_SIGNED_INT8    = 0x08, /**< Signed 8-bit integers */
    CU_AD_FORMAT_SIGNED_INT16   = 0x09, /**< Signed 16-bit integers */
    CU_AD_FORMAT_SIGNED_INT32   = 0x0a, /**< Signed 32-bit integers */
    CU_AD_FORMAT_HALF           = 0x10, /**< 16-bit floating point */
    CU_AD_FORMAT_FLOAT          = 0x20  /**< 32-bit floating point */
} CUarray_format;

/**
 * Texture reference addressing modes
 */
typedef enum CUaddress_mode_enum
{
    CU_TR_ADDRESS_MODE_WRAP   = 0, /**< Wrapping address mode */
    CU_TR_ADDRESS_MODE_CLAMP  = 1, /**< Clamp to edge address mode */
    CU_TR_ADDRESS_MODE_MIRROR = 2, /**< Mirror address mode */
    CU_TR_ADDRESS_MODE_BORDER = 3  /**< Border address mode */
} CUaddress_mode;

/**
 * Texture reference filtering modes
 */
typedef enum CUfilter_mode_enum
{
    CU_TR_FILTER_MODE_POINT  = 0, /**< Point filter mode */
    CU_TR_FILTER_MODE_LINEAR = 1  /**< Linear filter mode */
} CUfilter_mode;

/**
 * Device properties
 */
typedef enum CUdevice_attribute_enum
{
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 1,              /**< Maximum number of threads per block */
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_X = 2,                    /**< Maximum block dimension X */
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Y = 3,                    /**< Maximum block dimension Y */
    CU_DEVICE_ATTRIBUTE_MAX_BLOCK_DIM_Z = 4,                    /**< Maximum block dimension Z */
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_X = 5,                     /**< Maximum grid dimension X */
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Y = 6,                     /**< Maximum grid dimension Y */
    CU_DEVICE_ATTRIBUTE_MAX_GRID_DIM_Z = 7,                     /**< Maximum grid dimension Z */
    CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK = 8,        /**< Maximum shared memory available per block in bytes */
    CU_DEVICE_ATTRIBUTE_SHARED_MEMORY_PER_BLOCK = 8,            /**< Deprecated, use CU_DEVICE_ATTRIBUTE_MAX_SHARED_MEMORY_PER_BLOCK */
    CU_DEVICE_ATTRIBUTE_TOTAL_CONSTANT_MEMORY = 9,              /**< Memory available on device for __constant__ variables in a CUDA C kernel in bytes */
    CU_DEVICE_ATTRIBUTE_WARP_SIZE = 10,                         /**< Warp size in threads */
    CU_DEVICE_ATTRIBUTE_MAX_PITCH = 11,                         /**< Maximum pitch in bytes allowed by memory copies */
    CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK = 12,           /**< Maximum number of 32-bit registers available per block */
    CU_DEVICE_ATTRIBUTE_REGISTERS_PER_BLOCK = 12,               /**< Deprecated, use CU_DEVICE_ATTRIBUTE_MAX_REGISTERS_PER_BLOCK */
    CU_DEVICE_ATTRIBUTE_CLOCK_RATE = 13,                        /**< Peak clock frequency in kilohertz */
    CU_DEVICE_ATTRIBUTE_TEXTURE_ALIGNMENT = 14,                 /**< Alignment requirement for textures */
    CU_DEVICE_ATTRIBUTE_GPU_OVERLAP = 15,                       /**< Device can possibly copy memory and execute a kernel concurrently */
    CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT = 16,              /**< Number of multiprocessors on device */
    CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT = 17,               /**< Specifies whether there is a run time limit on kernels */
    CU_DEVICE_ATTRIBUTE_INTEGRATED = 18,                        /**< Device is integrated with host memory */
    CU_DEVICE_ATTRIBUTE_CAN_MAP_HOST_MEMORY = 19,               /**< Device can map host memory into CUDA address space */
    CU_DEVICE_ATTRIBUTE_COMPUTE_MODE = 20,                      /**< Compute mode (See ::CUcomputemode for details) */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_WIDTH = 21,           /**< Maximum 1D texture width */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_WIDTH = 22,           /**< Maximum 2D texture width */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_HEIGHT = 23,          /**< Maximum 2D texture height */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_WIDTH = 24,           /**< Maximum 3D texture width */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_HEIGHT = 25,          /**< Maximum 3D texture height */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE3D_DEPTH = 26,           /**< Maximum 3D texture depth */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_WIDTH = 27,     /**< Maximum texture array width */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_HEIGHT = 28,    /**< Maximum texture array height */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE2D_ARRAY_NUMSLICES = 29, /**< Maximum slices in a texture array */
    CU_DEVICE_ATTRIBUTE_SURFACE_ALIGNMENT = 30,                 /**< Alignment requirement for surfaces */
    CU_DEVICE_ATTRIBUTE_CONCURRENT_KERNELS = 31,                /**< Device can possibly execute multiple kernels concurrently */
    CU_DEVICE_ATTRIBUTE_ECC_ENABLED = 32,                       /**< Device has ECC support enabled */
    CU_DEVICE_ATTRIBUTE_PCI_BUS_ID = 33,                        /**< PCI bus ID of the device */
    CU_DEVICE_ATTRIBUTE_PCI_DEVICE_ID = 34,                     /**< PCI device ID of the device */
    CU_DEVICE_ATTRIBUTE_TCC_DRIVER = 35                         /**< Device is using TCC driver model */
#if __CUDA_API_VERSION >= 4000
  , CU_DEVICE_ATTRIBUTE_MEMORY_CLOCK_RATE = 36,                 /**< Peak memory clock frequency in kilohertz */
    CU_DEVICE_ATTRIBUTE_GLOBAL_MEMORY_BUS_WIDTH = 37,           /**< Global memory bus width in bits */
    CU_DEVICE_ATTRIBUTE_L2_CACHE_SIZE = 38,                     /**< Size of L2 cache in bytes */
    CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR = 39,    /**< Maximum resident threads per multiprocessor */
    CU_DEVICE_ATTRIBUTE_ASYNC_ENGINE_COUNT = 40,                /**< Number of asynchronous engines */
    CU_DEVICE_ATTRIBUTE_UNIFIED_ADDRESSING = 41,                /**< Device uses shares a unified address space with the host */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_WIDTH = 42,   /**< Maximum 1D layered texture width */
    CU_DEVICE_ATTRIBUTE_MAXIMUM_TEXTURE1D_LAYERED_LAYERS = 43   /**< Maximum layers in a 1D layered texture */
#endif
} CUdevice_attribute;

/**
 * Legacy device properties
 */
typedef struct CUdevprop_st
{
    int maxThreadsPerBlock;     /**< Maximum number of threads per block */
    int maxThreadsDim[3];       /**< Maximum size of each dimension of a block */
    int maxGridSize[3];         /**< Maximum size of each dimension of a grid */
    int sharedMemPerBlock;      /**< Shared memory available per block in bytes */
    int totalConstantMemory;    /**< Constant memory available on device in bytes */
    int SIMDWidth;              /**< Warp size in threads */
    int memPitch;               /**< Maximum pitch in bytes allowed by memory copies */
    int regsPerBlock;           /**< 32-bit registers available per block */
    int clockRate;              /**< Clock frequency in kilohertz */
    int textureAlign;           /**< Alignment requirement for textures */
} CUdevprop;

/**
 * Function properties
 */
typedef enum CUfunction_attribute_enum
{
    /**
     * The maximum number of threads per block, beyond which a launch of the
     * function would fail. This number depends on both the function and the
     * device on which the function is currently loaded.
     */
    CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK = 0,

    /**
     * The size in bytes of statically-allocated shared memory required by
     * this function. This does not include dynamically-allocated shared
     * memory requested by the user at runtime.
     */
    CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES = 1,

    /**
     * The size in bytes of user-allocated constant memory required by this
     * function.
     */
    CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES = 2,

    /**
     * The size in bytes of local memory used by each thread of this function.
     */
    CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES = 3,

    /**
     * The number of registers used by each thread of this function.
     */
    CU_FUNC_ATTRIBUTE_NUM_REGS = 4,

    /**
     * The PTX virtual architecture version for which the function was
     * compiled. This value is the major PTX version * 10 + the minor PTX
     * version, so a PTX version 1.3 function would return the value 13.
     * Note that this may return the undefined value of 0 for cubins
     * compiled prior to CUDA 3.0.
     */
    CU_FUNC_ATTRIBUTE_PTX_VERSION = 5,

    /**
     * The binary architecture version for which the function was compiled.
     * This value is the major binary version * 10 + the minor binary version,
     * so a binary version 1.3 function would return the value 13. Note that
     * this will return a value of 10 for legacy cubins that do not have a
     * properly-encoded binary architecture version.
     */
    CU_FUNC_ATTRIBUTE_BINARY_VERSION = 6,

    CU_FUNC_ATTRIBUTE_MAX
} CUfunction_attribute;

/**
 * Function cache configurations
 */
typedef enum CUfunc_cache_enum
{
    CU_FUNC_CACHE_PREFER_NONE    = 0x00, /**< no preference for shared memory or L1 (default) */
    CU_FUNC_CACHE_PREFER_SHARED  = 0x01, /**< prefer larger shared memory and smaller L1 cache */
    CU_FUNC_CACHE_PREFER_L1      = 0x02  /**< prefer larger L1 cache and smaller shared memory */
} CUfunc_cache;

/**
 * Memory types
 */
typedef enum CUmemorytype_enum
{
    CU_MEMORYTYPE_HOST    = 0x01,    /**< Host memory */
    CU_MEMORYTYPE_DEVICE  = 0x02,    /**< Device memory */
    CU_MEMORYTYPE_ARRAY   = 0x03     /**< Array memory */
#if __CUDA_API_VERSION >= 4000
  , CU_MEMORYTYPE_UNIFIED = 0x04     /**< Unified device or host memory */
#endif
} CUmemorytype;

/**
 * Compute Modes
 */
typedef enum CUcomputemode_enum
{
    CU_COMPUTEMODE_DEFAULT           = 0,  /**< Default compute mode (Multiple contexts allowed per device) */
    CU_COMPUTEMODE_EXCLUSIVE         = 1, /**< Compute-exclusive-thread mode (Only one context used by a single thread can be present on this device at a time) */
    CU_COMPUTEMODE_PROHIBITED        = 2  /**< Compute-prohibited mode (No contexts can be created on this device at this time) */
#if __CUDA_API_VERSION >= 4000
  , CU_COMPUTEMODE_EXCLUSIVE_PROCESS = 3  /**< Compute-exclusive-process mode (Only one context used by a single process can be present on this device at a time) */
#endif
} CUcomputemode;

/**
 * Online compiler options
 */
typedef enum CUjit_option_enum
{
    /**
     * Max number of registers that a thread may use.\n
     * Option type: unsigned int
     */
    CU_JIT_MAX_REGISTERS = 0,

    /**
     * IN: Specifies minimum number of threads per block to target compilation
     * for\n
     * OUT: Returns the number of threads the compiler actually targeted.
     * This restricts the resource utilization fo the compiler (e.g. max
     * registers) such that a block with the given number of threads should be
     * able to launch based on register limitations. Note, this option does not
     * currently take into account any other resource limitations, such as
     * shared memory utilization.\n
     * Option type: unsigned int
     */
    CU_JIT_THREADS_PER_BLOCK,

    /**
     * Returns a float value in the option of the wall clock time, in
     * milliseconds, spent creating the cubin\n
     * Option type: float
     */
    CU_JIT_WALL_TIME,

    /**
     * Pointer to a buffer in which to print any log messsages from PTXAS
     * that are informational in nature (the buffer size is specified via
     * option ::CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES) \n
     * Option type: char*
     */
    CU_JIT_INFO_LOG_BUFFER,

    /**
     * IN: Log buffer size in bytes.  Log messages will be capped at this size
     * (including null terminator)\n
     * OUT: Amount of log buffer filled with messages\n
     * Option type: unsigned int
     */
    CU_JIT_INFO_LOG_BUFFER_SIZE_BYTES,

    /**
     * Pointer to a buffer in which to print any log messages from PTXAS that
     * reflect errors (the buffer size is specified via option
     * ::CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES)\n
     * Option type: char*
     */
    CU_JIT_ERROR_LOG_BUFFER,

    /**
     * IN: Log buffer size in bytes.  Log messages will be capped at this size
     * (including null terminator)\n
     * OUT: Amount of log buffer filled with messages\n
     * Option type: unsigned int
     */
    CU_JIT_ERROR_LOG_BUFFER_SIZE_BYTES,

    /**
     * Level of optimizations to apply to generated code (0 - 4), with 4
     * being the default and highest level of optimizations.\n
     * Option type: unsigned int
     */
    CU_JIT_OPTIMIZATION_LEVEL,

    /**
     * No option value required. Determines the target based on the current
     * attached context (default)\n
     * Option type: No option value needed
     */
    CU_JIT_TARGET_FROM_CUCONTEXT,

    /**
     * Target is chosen based on supplied ::CUjit_target_enum.\n
     * Option type: unsigned int for enumerated type ::CUjit_target_enum
     */
    CU_JIT_TARGET,

    /**
     * Specifies choice of fallback strategy if matching cubin is not found.
     * Choice is based on supplied ::CUjit_fallback_enum.\n
     * Option type: unsigned int for enumerated type ::CUjit_fallback_enum
     */
    CU_JIT_FALLBACK_STRATEGY

} CUjit_option;

/**
 * Online compilation targets
 */
typedef enum CUjit_target_enum
{
    CU_TARGET_COMPUTE_10 = 0,   /**< Compute device class 1.0 */
    CU_TARGET_COMPUTE_11,       /**< Compute device class 1.1 */
    CU_TARGET_COMPUTE_12,       /**< Compute device class 1.2 */
    CU_TARGET_COMPUTE_13,       /**< Compute device class 1.3 */
    CU_TARGET_COMPUTE_20,       /**< Compute device class 2.0 */
    CU_TARGET_COMPUTE_21        /**< Compute device class 2.1 */
} CUjit_target;

/**
 * Cubin matching fallback strategies
 */
typedef enum CUjit_fallback_enum
{
    CU_PREFER_PTX = 0,  /**< Prefer to compile ptx */

    CU_PREFER_BINARY    /**< Prefer to fall back to compatible binary code */

} CUjit_fallback;

/**
 * Flags to register a graphics resource
 */
typedef enum CUgraphicsRegisterFlags_enum
{
    CU_GRAPHICS_REGISTER_FLAGS_NONE          = 0x00,
    CU_GRAPHICS_REGISTER_FLAGS_READ_ONLY     = 0x01,
    CU_GRAPHICS_REGISTER_FLAGS_WRITE_DISCARD = 0x02,
    CU_GRAPHICS_REGISTER_FLAGS_SURFACE_LDST  = 0x04
} CUgraphicsRegisterFlags;

/**
 * Flags for mapping and unmapping interop resources
 */
typedef enum CUgraphicsMapResourceFlags_enum
{
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_NONE          = 0x00,
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_READ_ONLY     = 0x01,
    CU_GRAPHICS_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02
} CUgraphicsMapResourceFlags;

/**
 * Array indices for cube faces
 */
typedef enum CUarray_cubemap_face_enum
{
    CU_CUBEMAP_FACE_POSITIVE_X  = 0x00, /**< Positive X face of cubemap */
    CU_CUBEMAP_FACE_NEGATIVE_X  = 0x01, /**< Negative X face of cubemap */
    CU_CUBEMAP_FACE_POSITIVE_Y  = 0x02, /**< Positive Y face of cubemap */
    CU_CUBEMAP_FACE_NEGATIVE_Y  = 0x03, /**< Negative Y face of cubemap */
    CU_CUBEMAP_FACE_POSITIVE_Z  = 0x04, /**< Positive Z face of cubemap */
    CU_CUBEMAP_FACE_NEGATIVE_Z  = 0x05  /**< Negative Z face of cubemap */
} CUarray_cubemap_face;

/**
 * Limits
 */
typedef enum CUlimit_enum
{
    CU_LIMIT_STACK_SIZE        = 0x00, /**< GPU thread stack size */
    CU_LIMIT_PRINTF_FIFO_SIZE  = 0x01, /**< GPU printf FIFO size */
    CU_LIMIT_MALLOC_HEAP_SIZE  = 0x02  /**< GPU malloc heap size */
} CUlimit;

/**
 * Error codes
 */
typedef enum cudaError_enum
{
    /**
     * The API call returned with no errors. In the case of query calls, this
     * can also mean that the operation being queried is complete (see
     * ::cuEventQuery() and ::cuStreamQuery()).
     */
    CUDA_SUCCESS                              = 0,

    /**
     * This indicates that one or more of the parameters passed to the API call
     * is not within an acceptable range of values.
     */
    CUDA_ERROR_INVALID_VALUE                  = 1,

    /**
     * The API call failed because it was unable to allocate enough memory to
     * perform the requested operation.
     */
    CUDA_ERROR_OUT_OF_MEMORY                  = 2,

    /**
     * This indicates that the CUDA driver has not been initialized with
     * ::cuInit() or that initialization has failed.
     */
    CUDA_ERROR_NOT_INITIALIZED                = 3,

    /**
     * This indicates that the CUDA driver is in the process of shutting down.
     */
    CUDA_ERROR_DEINITIALIZED                  = 4,

    /**
     * This indicates profiling APIs are called while application is running
     * in visual profiler mode.
    */
    CUDA_ERROR_PROFILER_DISABLED           = 5,
    /**
     * This indicates profiling has not been initialized for this context.
     * Call cuProfilerInitialize() to resolve this.
    */
    CUDA_ERROR_PROFILER_NOT_INITIALIZED       = 6,
    /**
     * This indicates profiler has already been started and probably
     * cuProfilerStart() is incorrectly called.
    */
    CUDA_ERROR_PROFILER_ALREADY_STARTED       = 7,
    /**
     * This indicates profiler has already been stopped and probably
     * cuProfilerStop() is incorrectly called.
    */
    CUDA_ERROR_PROFILER_ALREADY_STOPPED       = 8,
    /**
     * This indicates that no CUDA-capable devices were detected by the installed
     * CUDA driver.
     */
    CUDA_ERROR_NO_DEVICE                      = 100,

    /**
     * This indicates that the device ordinal supplied by the user does not
     * correspond to a valid CUDA device.
     */
    CUDA_ERROR_INVALID_DEVICE                 = 101,


    /**
     * This indicates that the device kernel image is invalid. This can also
     * indicate an invalid CUDA module.
     */
    CUDA_ERROR_INVALID_IMAGE                  = 200,

    /**
     * This most frequently indicates that there is no context bound to the
     * current thread. This can also be returned if the context passed to an
     * API call is not a valid handle (such as a context that has had
     * ::cuCtxDestroy() invoked on it). This can also be returned if a user
     * mixes different API versions (i.e. 3010 context with 3020 API calls).
     * See ::cuCtxGetApiVersion() for more details.
     */
    CUDA_ERROR_INVALID_CONTEXT                = 201,

    /**
     * This indicated that the context being supplied as a parameter to the
     * API call was already the active context.
     * \deprecated
     * This error return is deprecated as of CUDA 3.2. It is no longer an
     * error to attempt to push the active context via ::cuCtxPushCurrent().
     */
    CUDA_ERROR_CONTEXT_ALREADY_CURRENT        = 202,

    /**
     * This indicates that a map or register operation has failed.
     */
    CUDA_ERROR_MAP_FAILED                     = 205,

    /**
     * This indicates that an unmap or unregister operation has failed.
     */
    CUDA_ERROR_UNMAP_FAILED                   = 206,

    /**
     * This indicates that the specified array is currently mapped and thus
     * cannot be destroyed.
     */
    CUDA_ERROR_ARRAY_IS_MAPPED                = 207,

    /**
     * This indicates that the resource is already mapped.
     */
    CUDA_ERROR_ALREADY_MAPPED                 = 208,

    /**
     * This indicates that there is no kernel image available that is suitable
     * for the device. This can occur when a user specifies code generation
     * options for a particular CUDA source file that do not include the
     * corresponding device configuration.
     */
    CUDA_ERROR_NO_BINARY_FOR_GPU              = 209,

    /**
     * This indicates that a resource has already been acquired.
     */
    CUDA_ERROR_ALREADY_ACQUIRED               = 210,

    /**
     * This indicates that a resource is not mapped.
     */
    CUDA_ERROR_NOT_MAPPED                     = 211,

    /**
     * This indicates that a mapped resource is not available for access as an
     * array.
     */
    CUDA_ERROR_NOT_MAPPED_AS_ARRAY            = 212,

    /**
     * This indicates that a mapped resource is not available for access as a
     * pointer.
     */
    CUDA_ERROR_NOT_MAPPED_AS_POINTER          = 213,

    /**
     * This indicates that an uncorrectable ECC error was detected during
     * execution.
     */
    CUDA_ERROR_ECC_UNCORRECTABLE              = 214,

    /**
     * This indicates that the ::CUlimit passed to the API call is not
     * supported by the active device.
     */
    CUDA_ERROR_UNSUPPORTED_LIMIT              = 215,

    /**
     * This indicates that the ::CUcontext passed to the API call can
     * only be bound to a single CPU thread at a time but is already
     * bound to a CPU thread.
     */
    CUDA_ERROR_CONTEXT_ALREADY_IN_USE         = 216,

    /**
     * This indicates that the device kernel source is invalid.
     */
    CUDA_ERROR_INVALID_SOURCE                 = 300,

    /**
     * This indicates that the file specified was not found.
     */
    CUDA_ERROR_FILE_NOT_FOUND                 = 301,

    /**
     * This indicates that a link to a shared object failed to resolve.
     */
    CUDA_ERROR_SHARED_OBJECT_SYMBOL_NOT_FOUND = 302,

    /**
     * This indicates that initialization of a shared object failed.
     */
    CUDA_ERROR_SHARED_OBJECT_INIT_FAILED      = 303,

    /**
     * This indicates that an OS call failed.
     */
    CUDA_ERROR_OPERATING_SYSTEM               = 304,


    /**
     * This indicates that a resource handle passed to the API call was not
     * valid. Resource handles are opaque types like ::CUstream and ::CUevent.
     */
    CUDA_ERROR_INVALID_HANDLE                 = 400,


    /**
     * This indicates that a named symbol was not found. Examples of symbols
     * are global/constant variable names, texture names, and surface names.
     */
    CUDA_ERROR_NOT_FOUND                      = 500,


    /**
     * This indicates that asynchronous operations issued previously have not
     * completed yet. This result is not actually an error, but must be indicated
     * differently than ::CUDA_SUCCESS (which indicates completion). Calls that
     * may return this value include ::cuEventQuery() and ::cuStreamQuery().
     */
    CUDA_ERROR_NOT_READY                      = 600,


    /**
     * An exception occurred on the device while executing a kernel. Common
     * causes include dereferencing an invalid device pointer and accessing
     * out of bounds shared memory. The context cannot be used, so it must
     * be destroyed (and a new one should be created). All existing device
     * memory allocations from this context are invalid and must be
     * reconstructed if the program is to continue using CUDA.
     */
    CUDA_ERROR_LAUNCH_FAILED                  = 700,

    /**
     * This indicates that a launch did not occur because it did not have
     * appropriate resources. This error usually indicates that the user has
     * attempted to pass too many arguments to the device kernel, or the
     * kernel launch specifies too many threads for the kernel's register
     * count. Passing arguments of the wrong size (i.e. a 64-bit pointer
     * when a 32-bit int is expected) is equivalent to passing too many
     * arguments and can also result in this error.
     */
    CUDA_ERROR_LAUNCH_OUT_OF_RESOURCES        = 701,

    /**
     * This indicates that the device kernel took too long to execute. This can
     * only occur if timeouts are enabled - see the device attribute
     * ::CU_DEVICE_ATTRIBUTE_KERNEL_EXEC_TIMEOUT for more information. The
     * context cannot be used (and must be destroyed similar to
     * ::CUDA_ERROR_LAUNCH_FAILED). All existing device memory allocations from
     * this context are invalid and must be reconstructed if the program is to
     * continue using CUDA.
     */
    CUDA_ERROR_LAUNCH_TIMEOUT                 = 702,

    /**
     * This error indicates a kernel launch that uses an incompatible texturing
     * mode.
     */
    CUDA_ERROR_LAUNCH_INCOMPATIBLE_TEXTURING  = 703,

    /**
     * This error indicates that a call to ::cuCtxEnablePeerAccess() is
     * trying to re-enable peer access to a context which has already
     * had peer access to it enabled.
     */
    CUDA_ERROR_PEER_ACCESS_ALREADY_ENABLED = 704,

    /**
     * This error indicates that a call to ::cuMemPeerRegister is trying to
     * register memory from a context which has not had peer access
     * enabled yet via ::cuCtxEnablePeerAccess(), or that
     * ::cuCtxDisablePeerAccess() is trying to disable peer access
     * which has not been enabled yet.
     */
    CUDA_ERROR_PEER_ACCESS_NOT_ENABLED    = 705,

    /**
     * This error indicates that a call to ::cuMemPeerRegister is trying to
     * register already-registered memory.
     */
    CUDA_ERROR_PEER_MEMORY_ALREADY_REGISTERED = 706,

    /**
     * This error indicates that a call to ::cuMemPeerUnregister is trying to
     * unregister memory that has not been registered.
     */
    CUDA_ERROR_PEER_MEMORY_NOT_REGISTERED     = 707,

    /**
     * This error indicates that ::cuCtxCreate was called with the flag
     * ::CU_CTX_PRIMARY on a device which already has initialized its
     * primary context.
     */
    CUDA_ERROR_PRIMARY_CONTEXT_ACTIVE         = 708,

    /**
     * This error indicates that the context current to the calling thread
     * has been destroyed using ::cuCtxDestroy, or is a primary context which
     * has not yet been initialized.
     */
    CUDA_ERROR_CONTEXT_IS_DESTROYED           = 709,

    /**
     * This indicates that an unknown internal error has occurred.
     */
    CUDA_ERROR_UNKNOWN                        = 999
} CUresult;

#if __CUDA_API_VERSION >= 4000
/**
 * If set, host memory is portable between CUDA contexts.
 * Flag for ::cuMemHostAlloc()
 */
#define CU_MEMHOSTALLOC_PORTABLE        0x01

/**
 * If set, host memory is mapped into CUDA address space and
 * ::cuMemHostGetDevicePointer() may be called on the host pointer.
 * Flag for ::cuMemHostAlloc()
 */
#define CU_MEMHOSTALLOC_DEVICEMAP       0x02

/**
 * If set, host memory is allocated as write-combined - fast to write,
 * faster to DMA, slow to read except via SSE4 streaming load instruction
 * (MOVNTDQA).
 * Flag for ::cuMemHostAlloc()
 */
#define CU_MEMHOSTALLOC_WRITECOMBINED   0x04

/**
 * If set, host memory is portable between CUDA contexts.
 * Flag for ::cuMemHostRegister()
 */
#define CU_MEMHOSTREGISTER_PORTABLE     0x01

/**
 * If set, host memory is mapped into CUDA address space and
 * ::cuMemHostGetDevicePointer() may be called on the host pointer.
 * Flag for ::cuMemHostRegister()
 */
#define CU_MEMHOSTREGISTER_DEVICEMAP    0x02

/**
 * If set, peer memory is mapped into CUDA address space and
 * ::cuMemPeerGetDevicePointer() may be called on the host pointer.
 * Flag for ::cuMemPeerRegister()
 */
#define CU_MEMPEERREGISTER_DEVICEMAP    0x02
#endif

#if __CUDA_API_VERSION >= 3020
/**
 * 2D memory copy parameters
 */
typedef struct CUDA_MEMCPY2D_st
{
    size_t srcXInBytes;         /**< Source X in bytes */
    size_t srcY;                /**< Source Y */

    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void *srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    size_t srcPitch;            /**< Source pitch (ignored when src is array) */

    size_t dstXInBytes;         /**< Destination X in bytes */
    size_t dstY;                /**< Destination Y */

    CUmemorytype dstMemoryType; /**< Destination memory type (host, device, array) */
    void *dstHost;              /**< Destination host pointer */
    CUdeviceptr dstDevice;      /**< Destination device pointer */
    CUarray dstArray;           /**< Destination array reference */
    size_t dstPitch;            /**< Destination pitch (ignored when dst is array) */

    size_t WidthInBytes;        /**< Width of 2D memory copy in bytes */
    size_t Height;              /**< Height of 2D memory copy */
} CUDA_MEMCPY2D;

/**
 * 3D memory copy parameters
 */
typedef struct CUDA_MEMCPY3D_st
{
    size_t srcXInBytes;         /**< Source X in bytes */
    size_t srcY;                /**< Source Y */
    size_t srcZ;                /**< Source Z */
    size_t srcLOD;              /**< Source LOD */
    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void *srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    void *reserved0;            /**< Must be NULL */
    size_t srcPitch;            /**< Source pitch (ignored when src is array) */
    size_t srcHeight;           /**< Source height (ignored when src is array; may be 0 if Depth==1) */

    size_t dstXInBytes;         /**< Destination X in bytes */
    size_t dstY;                /**< Destination Y */
    size_t dstZ;                /**< Destination Z */
    size_t dstLOD;              /**< Destination LOD */
    CUmemorytype dstMemoryType; /**< Destination memory type (host, device, array) */
    void *dstHost;              /**< Destination host pointer */
    CUdeviceptr dstDevice;      /**< Destination device pointer */
    CUarray dstArray;           /**< Destination array reference */
    void *reserved1;            /**< Must be NULL */
    size_t dstPitch;            /**< Destination pitch (ignored when dst is array) */
    size_t dstHeight;           /**< Destination height (ignored when dst is array; may be 0 if Depth==1) */

    size_t WidthInBytes;        /**< Width of 3D memory copy in bytes */
    size_t Height;              /**< Height of 3D memory copy */
    size_t Depth;               /**< Depth of 3D memory copy */
} CUDA_MEMCPY3D;

/**
 * 3D memory cross-context copy parameters
 */
typedef struct CUDA_MEMCPY3D_PEER_st
{
    size_t srcXInBytes;         /**< Source X in bytes */
    size_t srcY;                /**< Source Y */
    size_t srcZ;                /**< Source Z */
    size_t srcLOD;              /**< Source LOD */
    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void *srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    CUcontext srcContext;       /**< Source context (ignored with srcMemoryType is ::CU_MEMORYTYPE_ARRAY) */
    size_t srcPitch;            /**< Source pitch (ignored when src is array) */
    size_t srcHeight;           /**< Source height (ignored when src is array; may be 0 if Depth==1) */

    size_t dstXInBytes;         /**< Destination X in bytes */
    size_t dstY;                /**< Destination Y */
    size_t dstZ;                /**< Destination Z */
    size_t dstLOD;              /**< Destination LOD */
    CUmemorytype dstMemoryType; /**< Destination memory type (host, device, array) */
    void *dstHost;              /**< Destination host pointer */
    CUdeviceptr dstDevice;      /**< Destination device pointer */
    CUarray dstArray;           /**< Destination array reference */
    CUcontext dstContext;       /**< Destination context (ignored with dstMemoryType is ::CU_MEMORYTYPE_ARRAY) */
    size_t dstPitch;            /**< Destination pitch (ignored when dst is array) */
    size_t dstHeight;           /**< Destination height (ignored when dst is array; may be 0 if Depth==1) */

    size_t WidthInBytes;        /**< Width of 3D memory copy in bytes */
    size_t Height;              /**< Height of 3D memory copy */
    size_t Depth;               /**< Depth of 3D memory copy */
} CUDA_MEMCPY3D_PEER;

/**
 * Array descriptor
 */
typedef struct CUDA_ARRAY_DESCRIPTOR_st
{
    size_t Width;             /**< Width of array */
    size_t Height;            /**< Height of array */

    CUarray_format Format;    /**< Array format */
    unsigned int NumChannels; /**< Channels per array element */
} CUDA_ARRAY_DESCRIPTOR;

/**
 * 3D array descriptor
 */
typedef struct CUDA_ARRAY3D_DESCRIPTOR_st
{
    size_t Width;             /**< Width of 3D array */
    size_t Height;            /**< Height of 3D array */
    size_t Depth;             /**< Depth of 3D array */

    CUarray_format Format;    /**< Array format */
    unsigned int NumChannels; /**< Channels per array element */
    unsigned int Flags;       /**< Flags */
} CUDA_ARRAY3D_DESCRIPTOR;

#endif /* __CUDA_API_VERSION >= 3020 */

/**
 * If set, the CUDA array is a collection of layers, where each layer is either a 1D
 * or a 2D array and the Depth member of CUDA_ARRAY3D_DESCRIPTOR specifies the number
 * of layers, not the depth of a 3D array.
 */
#define CUDA_ARRAY3D_LAYERED        0x01

/**
 * Deprecated, use CUDA_ARRAY3D_LAYERED
 */
#define CUDA_ARRAY3D_2DARRAY        0x01

/**
 * This flag must be set in order to bind a surface reference
 * to the CUDA array
 */
#define CUDA_ARRAY3D_SURFACE_LDST   0x02

/**
 * Override the texref format with a format inferred from the array.
 * Flag for ::cuTexRefSetArray()
 */
#define CU_TRSA_OVERRIDE_FORMAT 0x01

/**
 * Read the texture as integers rather than promoting the values to floats
 * in the range [0,1].
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_READ_AS_INTEGER         0x01

/**
 * Use normalized texture coordinates in the range [0,1) instead of [0,dim).
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_NORMALIZED_COORDINATES  0x02

/**
 * Perform sRGB->linear conversion during texture read.
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_SRGB  0x10

/**
 * End of array terminator for the \p extra parameter to
 * ::cuLaunchKernel
 */
#define CU_LAUNCH_PARAM_END            ((void*)0x00)

/**
 * Indicator that the next value in the \p extra parameter to
 * ::cuLaunchKernel will be a pointer to a buffer containing all kernel
 * parameters used for launching kernel \p f.  This buffer needs to
 * honor all alignment/padding requirements of the individual parameters.
 * If ::CU_LAUNCH_PARAM_BUFFER_SIZE is not also specified in the
 * \p extra array, then ::CU_LAUNCH_PARAM_BUFFER_POINTER will have no
 * effect.
 */
#define CU_LAUNCH_PARAM_BUFFER_POINTER ((void*)0x01)

/**
 * Indicator that the next value in the \p extra parameter to
 * ::cuLaunchKernel will be a pointer to a size_t which contains the
 * size of the buffer specified with ::CU_LAUNCH_PARAM_BUFFER_POINTER.
 * It is required that ::CU_LAUNCH_PARAM_BUFFER_POINTER also be specified
 * in the \p extra array if the value associated with
 * ::CU_LAUNCH_PARAM_BUFFER_SIZE is not zero.
 */
#define CU_LAUNCH_PARAM_BUFFER_SIZE    ((void*)0x02)

/**
 * For texture references loaded into the module, use default texunit from
 * texture reference.
 */
#define CU_PARAM_TR_DEFAULT -1

/**
 * CUDA API made obselete at API version 3020
 */
#if defined(__CUDA_API_VERSION_INTERNAL)
    #define CUdeviceptr                  CUdeviceptr_v1
    #define CUDA_MEMCPY2D_st             CUDA_MEMCPY2D_v1_st
    #define CUDA_MEMCPY2D                CUDA_MEMCPY2D_v1
    #define CUDA_MEMCPY3D_st             CUDA_MEMCPY3D_v1_st
    #define CUDA_MEMCPY3D                CUDA_MEMCPY3D_v1
    #define CUDA_ARRAY_DESCRIPTOR_st     CUDA_ARRAY_DESCRIPTOR_v1_st
    #define CUDA_ARRAY_DESCRIPTOR        CUDA_ARRAY_DESCRIPTOR_v1
    #define CUDA_ARRAY3D_DESCRIPTOR_st   CUDA_ARRAY3D_DESCRIPTOR_v1_st
    #define CUDA_ARRAY3D_DESCRIPTOR      CUDA_ARRAY3D_DESCRIPTOR_v1
#endif /* CUDA_FORCE_LEGACY32_INTERNAL */

#if defined(__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION < 3020
typedef unsigned int CUdeviceptr;

typedef struct CUDA_MEMCPY2D_st
{
    unsigned int srcXInBytes;   /**< Source X in bytes */
    unsigned int srcY;          /**< Source Y */
    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void *srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    unsigned int srcPitch;      /**< Source pitch (ignored when src is array) */

    unsigned int dstXInBytes;   /**< Destination X in bytes */
    unsigned int dstY;          /**< Destination Y */
    CUmemorytype dstMemoryType; /**< Destination memory type (host, device, array) */
    void *dstHost;              /**< Destination host pointer */
    CUdeviceptr dstDevice;      /**< Destination device pointer */
    CUarray dstArray;           /**< Destination array reference */
    unsigned int dstPitch;      /**< Destination pitch (ignored when dst is array) */

    unsigned int WidthInBytes;  /**< Width of 2D memory copy in bytes */
    unsigned int Height;        /**< Height of 2D memory copy */
} CUDA_MEMCPY2D;

typedef struct CUDA_MEMCPY3D_st
{
    unsigned int srcXInBytes;   /**< Source X in bytes */
    unsigned int srcY;          /**< Source Y */
    unsigned int srcZ;          /**< Source Z */
    unsigned int srcLOD;        /**< Source LOD */
    CUmemorytype srcMemoryType; /**< Source memory type (host, device, array) */
    const void *srcHost;        /**< Source host pointer */
    CUdeviceptr srcDevice;      /**< Source device pointer */
    CUarray srcArray;           /**< Source array reference */
    void *reserved0;            /**< Must be NULL */
    unsigned int srcPitch;      /**< Source pitch (ignored when src is array) */
    unsigned int srcHeight;     /**< Source height (ignored when src is array; may be 0 if Depth==1) */

    unsigned int dstXInBytes;   /**< Destination X in bytes */
    unsigned int dstY;          /**< Destination Y */
    unsigned int dstZ;          /**< Destination Z */
    unsigned int dstLOD;        /**< Destination LOD */
    CUmemorytype dstMemoryType; /**< Destination memory type (host, device, array) */
    void *dstHost;              /**< Destination host pointer */
    CUdeviceptr dstDevice;      /**< Destination device pointer */
    CUarray dstArray;           /**< Destination array reference */
    void *reserved1;            /**< Must be NULL */
    unsigned int dstPitch;      /**< Destination pitch (ignored when dst is array) */
    unsigned int dstHeight;     /**< Destination height (ignored when dst is array; may be 0 if Depth==1) */

    unsigned int WidthInBytes;  /**< Width of 3D memory copy in bytes */
    unsigned int Height;        /**< Height of 3D memory copy */
    unsigned int Depth;         /**< Depth of 3D memory copy */
} CUDA_MEMCPY3D;

typedef struct CUDA_ARRAY_DESCRIPTOR_st
{
    unsigned int Width;         /**< Width of array */
    unsigned int Height;        /**< Height of array */

    CUarray_format Format;      /**< Array format */
    unsigned int NumChannels;   /**< Channels per array element */
} CUDA_ARRAY_DESCRIPTOR;

typedef struct CUDA_ARRAY3D_DESCRIPTOR_st
{
    unsigned int Width;         /**< Width of 3D array */
    unsigned int Height;        /**< Height of 3D array */
    unsigned int Depth;         /**< Depth of 3D array */

    CUarray_format Format;      /**< Array format */
    unsigned int NumChannels;   /**< Channels per array element */
    unsigned int Flags;         /**< Flags */
} CUDA_ARRAY3D_DESCRIPTOR;

#endif /* (__CUDA_API_VERSION_INTERNAL) || __CUDA_API_VERSION < 3020 */

/*
 * If set, the CUDA array contains an array of 2D slices
 * and the Depth member of CUDA_ARRAY3D_DESCRIPTOR specifies
 * the number of slices, not the depth of a 3D array.
 */
#define CUDA_ARRAY3D_2DARRAY        0x01

/**
 * This flag must be set in order to bind a surface reference
 * to the CUDA array
 */
#define CUDA_ARRAY3D_SURFACE_LDST   0x02

/**
 * Override the texref format with a format inferred from the array.
 * Flag for ::cuTexRefSetArray()
 */
#define CU_TRSA_OVERRIDE_FORMAT 0x01

/**
 * Read the texture as integers rather than promoting the values to floats
 * in the range [0,1].
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_READ_AS_INTEGER         0x01

/**
 * Use normalized texture coordinates in the range [0,1) instead of [0,dim).
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_NORMALIZED_COORDINATES  0x02

/**
 * Perform sRGB->linear conversion during texture read.
 * Flag for ::cuTexRefSetFlags()
 */
#define CU_TRSF_SRGB  0x10

/**
 * For texture references loaded into the module, use default texunit from
 * texture reference.
 */
#define CU_PARAM_TR_DEFAULT -1

/** @} */ /* END CUDA_TYPES */

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
    #define CUDAAPI __stdcall
#else
    #define CUDAAPI
#endif

/**
 * \defgroup CUDA_INITIALIZE Initialization
 *
 * This section describes the initialization functions of the low-level CUDA
 * driver application programming interface.
 *
 * @{
 */

/*********************************
 ** Initialization
 *********************************/
typedef CUresult  CUDAAPI tcuInit(unsigned int Flags);

/*********************************
 ** Driver Version Query
 *********************************/
typedef CUresult  CUDAAPI tcuDriverGetVersion(int *driverVersion);

/************************************
 **
 **    Device management
 **
 ***********************************/

typedef CUresult  CUDAAPI tcuDeviceGet(CUdevice *device, int ordinal);
typedef CUresult  CUDAAPI tcuDeviceGetCount(int *count);
typedef CUresult  CUDAAPI tcuDeviceGetName(char *name, int len, CUdevice dev);
typedef CUresult  CUDAAPI tcuDeviceComputeCapability(int *major, int *minor, CUdevice dev);
#if __CUDA_API_VERSION >= 3020
    typedef CUresult  CUDAAPI tcuDeviceTotalMem(size_t *bytes, CUdevice dev);
#else
    typedef CUresult  CUDAAPI tcuDeviceTotalMem(unsigned int *bytes, CUdevice dev);
#endif

typedef CUresult  CUDAAPI tcuDeviceGetProperties(CUdevprop *prop, CUdevice dev);
typedef CUresult  CUDAAPI tcuDeviceGetAttribute(int *pi, CUdevice_attribute attrib, CUdevice dev);

/************************************
 **
 **    Context management
 **
 ***********************************/
typedef CUresult  CUDAAPI tcuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev);
typedef CUresult  CUDAAPI tcuCtxDestroy(CUcontext ctx);
typedef CUresult  CUDAAPI tcuCtxAttach(CUcontext *pctx, unsigned int flags);
typedef CUresult  CUDAAPI tcuCtxDetach(CUcontext ctx);
typedef CUresult  CUDAAPI tcuCtxPushCurrent(CUcontext ctx);
typedef CUresult  CUDAAPI tcuCtxPopCurrent(CUcontext *pctx);

typedef CUresult  CUDAAPI tcuCtxSetCurrent(CUcontext ctx);
typedef CUresult  CUDAAPI tcuCtxGetCurrent(CUcontext *pctx);

typedef CUresult  CUDAAPI tcuCtxGetDevice(CUdevice *device);
typedef CUresult  CUDAAPI tcuCtxSynchronize(void);


/************************************
 **
 **    Module management
 **
 ***********************************/
typedef CUresult  CUDAAPI tcuModuleLoad(CUmodule *module, const char *fname);
typedef CUresult  CUDAAPI tcuModuleLoadData(CUmodule *module, const void *image);
typedef CUresult  CUDAAPI tcuModuleLoadDataEx(CUmodule *module, const void *image, unsigned int numOptions, CUjit_option *options, void **optionValues);
typedef CUresult  CUDAAPI tcuModuleLoadFatBinary(CUmodule *module, const void *fatCubin);
typedef CUresult  CUDAAPI tcuModuleUnload(CUmodule hmod);
typedef CUresult  CUDAAPI tcuModuleGetFunction(CUfunction *hfunc, CUmodule hmod, const char *name);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult  CUDAAPI tcuModuleGetGlobal(CUdeviceptr *dptr, size_t *bytes, CUmodule hmod, const char *name);
#else
    typedef CUresult  CUDAAPI tcuModuleGetGlobal(CUdeviceptr *dptr, unsigned int *bytes, CUmodule hmod, const char *name);
#endif

typedef CUresult  CUDAAPI tcuModuleGetTexRef(CUtexref *pTexRef, CUmodule hmod, const char *name);
typedef CUresult  CUDAAPI tcuModuleGetSurfRef(CUsurfref *pSurfRef, CUmodule hmod, const char *name);

/************************************
 **
 **    Memory management
 **
 ***********************************/
#if __CUDA_API_VERSION >= 3020
    typedef CUresult CUDAAPI tcuMemGetInfo(size_t *free, size_t *total);
    typedef CUresult CUDAAPI tcuMemAlloc(CUdeviceptr *dptr, size_t bytesize);
    typedef CUresult CUDAAPI tcuMemGetAddressRange(CUdeviceptr *pbase, size_t *psize, CUdeviceptr dptr);
    typedef CUresult CUDAAPI tcuMemAllocPitch(CUdeviceptr *dptr,
                                              size_t *pPitch,
                                              size_t WidthInBytes,
                                              size_t Height,
                                              // size of biggest r/w to be performed by kernels on this memory
                                              // 4, 8 or 16 bytes
                                              unsigned int ElementSizeBytes
                                             );
#else
    typedef CUresult CUDAAPI tcuMemGetInfo(unsigned int *free, unsigned int *total);
    typedef CUresult CUDAAPI tcuMemAlloc(CUdeviceptr *dptr, unsigned int bytesize);
    typedef CUresult CUDAAPI tcuMemGetAddressRange(CUdeviceptr *pbase, unsigned int *psize, CUdeviceptr dptr);
    typedef CUresult CUDAAPI tcuMemAllocPitch(CUdeviceptr *dptr,
                                              unsigned int *pPitch,
                                              unsigned int WidthInBytes,
                                              unsigned int Height,
                                              // size of biggest r/w to be performed by kernels on this memory
                                              // 4, 8 or 16 bytes
                                              unsigned int ElementSizeBytes
                                             );
#endif

typedef CUresult CUDAAPI tcuMemFree(CUdeviceptr dptr);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult CUDAAPI tcuMemAllocHost(void **pp, size_t bytesize);
#else
    typedef CUresult CUDAAPI tcuMemAllocHost(void **pp, unsigned int bytesize);
#endif

typedef CUresult CUDAAPI tcuMemFreeHost(void *p);
typedef CUresult CUDAAPI tcuMemHostAlloc(void **pp, size_t bytesize, unsigned int Flags);

typedef CUresult CUDAAPI tcuMemHostGetDevicePointer(CUdeviceptr *pdptr, void *p, unsigned int Flags);
typedef CUresult CUDAAPI tcuMemHostGetFlags(unsigned int *pFlags, void *p);

typedef CUresult CUDAAPI tcuMemHostRegister(void *p, size_t bytesize, unsigned int Flags);
typedef CUresult CUDAAPI tcuMemHostUnregister(void *p);
typedef CUresult CUDAAPI tcuMemcpy(CUdeviceptr dst, CUdeviceptr src, size_t ByteCount);
typedef CUresult CUDAAPI tcuMemcpyPeer(CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount);

/************************************
 **
 **    Synchronous Memcpy
 **
 ** Intra-device memcpy's done with these functions may execute in parallel with the CPU,
 ** but if host memory is involved, they wait until the copy is done before returning.
 **
 ***********************************/
// 1D functions
#if __CUDA_API_VERSION >= 3020
    // system <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);

    // device <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);

    // device <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoA(CUarray dstArray, size_t dstOffset, CUdeviceptr srcDevice, size_t ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyAtoD(CUdeviceptr dstDevice, CUarray srcArray, size_t srcOffset, size_t ByteCount);

    // system <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoA(CUarray dstArray, size_t dstOffset, const void *srcHost, size_t ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyAtoH(void *dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount);

    // array <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyAtoA(CUarray dstArray, size_t dstOffset, CUarray srcArray, size_t srcOffset, size_t ByteCount);
#else
    // system <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, unsigned int ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, unsigned int ByteCount);

    // device <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, unsigned int ByteCount);

    // device <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoA(CUarray dstArray, unsigned int dstOffset, CUdeviceptr srcDevice, unsigned int ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyAtoD(CUdeviceptr dstDevice, CUarray srcArray, unsigned int srcOffset, unsigned int ByteCount);

    // system <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoA(CUarray dstArray, unsigned int dstOffset, const void *srcHost, unsigned int ByteCount);
    typedef CUresult  CUDAAPI tcuMemcpyAtoH(void *dstHost, CUarray srcArray, unsigned int srcOffset, unsigned int ByteCount);

    // array <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyAtoA(CUarray dstArray, unsigned int dstOffset, CUarray srcArray, unsigned int srcOffset, unsigned int ByteCount);
#endif

// 2D memcpy
typedef CUresult  CUDAAPI tcuMemcpy2D(const CUDA_MEMCPY2D *pCopy);
typedef CUresult  CUDAAPI tcuMemcpy2DUnaligned(const CUDA_MEMCPY2D *pCopy);

// 3D memcpy
typedef CUresult  CUDAAPI tcuMemcpy3D(const CUDA_MEMCPY3D *pCopy);

/************************************
 **
 **    Asynchronous Memcpy
 **
 ** Any host memory involved must be DMA'able (e.g., allocated with cuMemAllocHost).
 ** memcpy's done with these functions execute in parallel with the CPU and, if
 ** the hardware is available, may execute in parallel with the GPU.
 ** Asynchronous memcpy must be accompanied by appropriate stream synchronization.
 **
 ***********************************/

// 1D functions
#if __CUDA_API_VERSION >= 3020
    // system <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoDAsync(CUdeviceptr dstDevice,
                                                 const void *srcHost, size_t ByteCount, CUstream hStream);
    typedef CUresult  CUDAAPI tcuMemcpyDtoHAsync(void *dstHost,
                                                 CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);

    // device <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoDAsync(CUdeviceptr dstDevice,
                                                 CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);

    // system <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoAAsync(CUarray dstArray, size_t dstOffset,
                                                 const void *srcHost, size_t ByteCount, CUstream hStream);
    typedef CUresult  CUDAAPI tcuMemcpyAtoHAsync(void *dstHost, CUarray srcArray, size_t srcOffset,
                                                 size_t ByteCount, CUstream hStream);
#else
    // system <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoDAsync(CUdeviceptr dstDevice,
                                                 const void *srcHost, unsigned int ByteCount, CUstream hStream);
    typedef CUresult  CUDAAPI tcuMemcpyDtoHAsync(void *dstHost,
                                                 CUdeviceptr srcDevice, unsigned int ByteCount, CUstream hStream);

    // device <-> device memory
    typedef CUresult  CUDAAPI tcuMemcpyDtoDAsync(CUdeviceptr dstDevice,
                                                 CUdeviceptr srcDevice, unsigned int ByteCount, CUstream hStream);

    // system <-> array memory
    typedef CUresult  CUDAAPI tcuMemcpyHtoAAsync(CUarray dstArray, unsigned int dstOffset,
                                                 const void *srcHost, unsigned int ByteCount, CUstream hStream);
    typedef CUresult  CUDAAPI tcuMemcpyAtoHAsync(void *dstHost, CUarray srcArray, unsigned int srcOffset,
                                                 unsigned int ByteCount, CUstream hStream);
#endif

// 2D memcpy
typedef CUresult  CUDAAPI tcuMemcpy2DAsync(const CUDA_MEMCPY2D *pCopy, CUstream hStream);

// 3D memcpy
typedef CUresult  CUDAAPI tcuMemcpy3DAsync(const CUDA_MEMCPY3D *pCopy, CUstream hStream);

/************************************
 **
 **    Memset
 **
 ***********************************/
typedef CUresult  CUDAAPI tcuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, unsigned int N);
typedef CUresult  CUDAAPI tcuMemsetD16(CUdeviceptr dstDevice, unsigned short us, unsigned int N);
typedef CUresult  CUDAAPI tcuMemsetD32(CUdeviceptr dstDevice, unsigned int ui, unsigned int N);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult  CUDAAPI tcuMemsetD2D8(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned char uc, size_t Width, size_t Height);
    typedef CUresult  CUDAAPI tcuMemsetD2D16(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned short us, size_t Width, size_t Height);
    typedef CUresult  CUDAAPI tcuMemsetD2D32(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned int ui, size_t Width, size_t Height);
#else
    typedef CUresult  CUDAAPI tcuMemsetD2D8(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned char uc, unsigned int Width, unsigned int Height);
    typedef CUresult  CUDAAPI tcuMemsetD2D16(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned short us, unsigned int Width, unsigned int Height);
    typedef CUresult  CUDAAPI tcuMemsetD2D32(CUdeviceptr dstDevice, unsigned int dstPitch, unsigned int ui, unsigned int Width, unsigned int Height);
#endif

/************************************
 **
 **    Function management
 **
 ***********************************/


typedef CUresult CUDAAPI tcuFuncSetBlockShape(CUfunction hfunc, int x, int y, int z);
typedef CUresult CUDAAPI tcuFuncSetSharedSize(CUfunction hfunc, unsigned int bytes);
typedef CUresult CUDAAPI tcuFuncGetAttribute(int *pi, CUfunction_attribute attrib, CUfunction hfunc);
typedef CUresult CUDAAPI tcuFuncSetCacheConfig(CUfunction hfunc, CUfunc_cache config);
typedef CUresult CUDAAPI tcuLaunchKernel(CUfunction f,
                                         unsigned int gridDimX,  unsigned int gridDimY,  unsigned int gridDimZ,
                                         unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                                         unsigned int sharedMemBytes,
                                         CUstream hStream, void **kernelParams, void **extra);

/************************************
 **
 **    Array management
 **
 ***********************************/

typedef CUresult  CUDAAPI tcuArrayCreate(CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR *pAllocateArray);
typedef CUresult  CUDAAPI tcuArrayGetDescriptor(CUDA_ARRAY_DESCRIPTOR *pArrayDescriptor, CUarray hArray);
typedef CUresult  CUDAAPI tcuArrayDestroy(CUarray hArray);

typedef CUresult  CUDAAPI tcuArray3DCreate(CUarray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pAllocateArray);
typedef CUresult  CUDAAPI tcuArray3DGetDescriptor(CUDA_ARRAY3D_DESCRIPTOR *pArrayDescriptor, CUarray hArray);


/************************************
 **
 **    Texture reference management
 **
 ***********************************/
typedef CUresult  CUDAAPI tcuTexRefCreate(CUtexref *pTexRef);
typedef CUresult  CUDAAPI tcuTexRefDestroy(CUtexref hTexRef);

typedef CUresult  CUDAAPI tcuTexRefSetArray(CUtexref hTexRef, CUarray hArray, unsigned int Flags);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult  CUDAAPI tcuTexRefSetAddress(size_t *ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, size_t bytes);
    typedef CUresult  CUDAAPI tcuTexRefSetAddress2D(CUtexref hTexRef, const CUDA_ARRAY_DESCRIPTOR *desc, CUdeviceptr dptr, size_t Pitch);
#else
    typedef CUresult  CUDAAPI tcuTexRefSetAddress(unsigned int *ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, unsigned int bytes);
    typedef CUresult  CUDAAPI tcuTexRefSetAddress2D(CUtexref hTexRef, const CUDA_ARRAY_DESCRIPTOR *desc, CUdeviceptr dptr, unsigned int Pitch);
#endif

typedef CUresult  CUDAAPI tcuTexRefSetFormat(CUtexref hTexRef, CUarray_format fmt, int NumPackedComponents);
typedef CUresult  CUDAAPI tcuTexRefSetAddressMode(CUtexref hTexRef, int dim, CUaddress_mode am);
typedef CUresult  CUDAAPI tcuTexRefSetFilterMode(CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult  CUDAAPI tcuTexRefSetFlags(CUtexref hTexRef, unsigned int Flags);

typedef CUresult  CUDAAPI tcuTexRefGetAddress(CUdeviceptr *pdptr, CUtexref hTexRef);
typedef CUresult  CUDAAPI tcuTexRefGetArray(CUarray *phArray, CUtexref hTexRef);
typedef CUresult  CUDAAPI tcuTexRefGetAddressMode(CUaddress_mode *pam, CUtexref hTexRef, int dim);
typedef CUresult  CUDAAPI tcuTexRefGetFilterMode(CUfilter_mode *pfm, CUtexref hTexRef);
typedef CUresult  CUDAAPI tcuTexRefGetFormat(CUarray_format *pFormat, int *pNumChannels, CUtexref hTexRef);
typedef CUresult  CUDAAPI tcuTexRefGetFlags(unsigned int *pFlags, CUtexref hTexRef);

/************************************
 **
 **    Surface reference management
 **
 ***********************************/
typedef CUresult  CUDAAPI tcuSurfRefSetArray(CUsurfref hSurfRef, CUarray hArray, unsigned int Flags);
typedef CUresult  CUDAAPI tcuSurfRefGetArray(CUarray *phArray, CUsurfref hSurfRef);

/************************************
 **
 **    Parameter management
 **
 ***********************************/

typedef CUresult  CUDAAPI tcuParamSetSize(CUfunction hfunc, unsigned int numbytes);
typedef CUresult  CUDAAPI tcuParamSeti(CUfunction hfunc, int offset, unsigned int value);
typedef CUresult  CUDAAPI tcuParamSetf(CUfunction hfunc, int offset, float value);
typedef CUresult  CUDAAPI tcuParamSetv(CUfunction hfunc, int offset, void *ptr, unsigned int numbytes);
typedef CUresult  CUDAAPI tcuParamSetTexRef(CUfunction hfunc, int texunit, CUtexref hTexRef);


/************************************
 **
 **    Launch functions
 **
 ***********************************/

typedef CUresult CUDAAPI tcuLaunch(CUfunction f);
typedef CUresult CUDAAPI tcuLaunchGrid(CUfunction f, int grid_width, int grid_height);
typedef CUresult CUDAAPI tcuLaunchGridAsync(CUfunction f, int grid_width, int grid_height, CUstream hStream);

/************************************
 **
 **    Events
 **
 ***********************************/
typedef CUresult CUDAAPI tcuEventCreate(CUevent *phEvent, unsigned int Flags);
typedef CUresult CUDAAPI tcuEventRecord(CUevent hEvent, CUstream hStream);
typedef CUresult CUDAAPI tcuEventQuery(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventSynchronize(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventDestroy(CUevent hEvent);
typedef CUresult CUDAAPI tcuEventElapsedTime(float *pMilliseconds, CUevent hStart, CUevent hEnd);

/************************************
 **
 **    Streams
 **
 ***********************************/
typedef CUresult CUDAAPI  tcuStreamCreate(CUstream *phStream, unsigned int Flags);
typedef CUresult CUDAAPI  tcuStreamQuery(CUstream hStream);
typedef CUresult CUDAAPI  tcuStreamSynchronize(CUstream hStream);
typedef CUresult CUDAAPI  tcuStreamDestroy(CUstream hStream);

/************************************
 **
 **    Graphics interop
 **
 ***********************************/
typedef CUresult CUDAAPI tcuGraphicsUnregisterResource(CUgraphicsResource resource);
typedef CUresult CUDAAPI tcuGraphicsSubResourceGetMappedArray(CUarray *pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);

#if __CUDA_API_VERSION >= 3020
    typedef CUresult CUDAAPI tcuGraphicsResourceGetMappedPointer(CUdeviceptr *pDevPtr, size_t *pSize, CUgraphicsResource resource);
#else
    typedef CUresult CUDAAPI tcuGraphicsResourceGetMappedPointer(CUdeviceptr *pDevPtr, unsigned int *pSize, CUgraphicsResource resource);
#endif

typedef CUresult CUDAAPI tcuGraphicsResourceSetMapFlags(CUgraphicsResource resource, unsigned int flags);
typedef CUresult CUDAAPI tcuGraphicsMapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);
typedef CUresult CUDAAPI tcuGraphicsUnmapResources(unsigned int count, CUgraphicsResource *resources, CUstream hStream);

/************************************
 **
 **    Export tables
 **
 ***********************************/
typedef CUresult CUDAAPI tcuGetExportTable(const void **ppExportTable, const CUuuid *pExportTableId);

/************************************
 **
 **    Limits
 **
 ***********************************/

typedef CUresult CUDAAPI tcuCtxSetLimit(CUlimit limit, size_t value);
typedef CUresult CUDAAPI tcuCtxGetLimit(size_t *pvalue, CUlimit limit);


extern tcuDriverGetVersion             *cuDriverGetVersion;
extern tcuDeviceGet                    *cuDeviceGet;
extern tcuDeviceGetCount               *cuDeviceGetCount;
extern tcuDeviceGetName                *cuDeviceGetName;
extern tcuDeviceComputeCapability      *cuDeviceComputeCapability;
extern tcuDeviceGetProperties          *cuDeviceGetProperties;
extern tcuDeviceGetAttribute           *cuDeviceGetAttribute;
extern tcuCtxDestroy                   *cuCtxDestroy;
extern tcuCtxAttach                    *cuCtxAttach;
extern tcuCtxDetach                    *cuCtxDetach;
extern tcuCtxPushCurrent               *cuCtxPushCurrent;
extern tcuCtxPopCurrent                *cuCtxPopCurrent;

extern tcuCtxSetCurrent                *cuCtxSetCurrent;
extern tcuCtxGetCurrent                *cuCtxGetCurrent;

extern tcuCtxGetDevice                 *cuCtxGetDevice;
extern tcuCtxSynchronize               *cuCtxSynchronize;
extern tcuModuleLoad                   *cuModuleLoad;
extern tcuModuleLoadData               *cuModuleLoadData;
extern tcuModuleLoadDataEx             *cuModuleLoadDataEx;
extern tcuModuleLoadFatBinary          *cuModuleLoadFatBinary;
extern tcuModuleUnload                 *cuModuleUnload;
extern tcuModuleGetFunction            *cuModuleGetFunction;
extern tcuModuleGetTexRef              *cuModuleGetTexRef;
extern tcuModuleGetSurfRef             *cuModuleGetSurfRef;
extern tcuMemFreeHost                  *cuMemFreeHost;
extern tcuMemHostAlloc                 *cuMemHostAlloc;
extern tcuMemHostGetFlags              *cuMemHostGetFlags;

extern tcuMemHostRegister              *cuMemHostRegister;
extern tcuMemHostUnregister            *cuMemHostUnregister;
extern tcuMemcpy                       *cuMemcpy;
extern tcuMemcpyPeer                   *cuMemcpyPeer;

extern tcuDeviceTotalMem               *cuDeviceTotalMem;
extern tcuCtxCreate                    *cuCtxCreate;
extern tcuModuleGetGlobal              *cuModuleGetGlobal;
extern tcuMemGetInfo                   *cuMemGetInfo;
extern tcuMemAlloc                     *cuMemAlloc;
extern tcuMemAllocPitch                *cuMemAllocPitch;
extern tcuMemFree                      *cuMemFree;
extern tcuMemGetAddressRange           *cuMemGetAddressRange;
extern tcuMemAllocHost                 *cuMemAllocHost;
extern tcuMemHostGetDevicePointer      *cuMemHostGetDevicePointer;
extern tcuFuncSetBlockShape            *cuFuncSetBlockShape;
extern tcuFuncSetSharedSize            *cuFuncSetSharedSize;
extern tcuFuncGetAttribute             *cuFuncGetAttribute;
extern tcuFuncSetCacheConfig           *cuFuncSetCacheConfig;
extern tcuLaunchKernel                 *cuLaunchKernel;
extern tcuArrayDestroy                 *cuArrayDestroy;
extern tcuTexRefCreate                 *cuTexRefCreate;
extern tcuTexRefDestroy                *cuTexRefDestroy;
extern tcuTexRefSetArray               *cuTexRefSetArray;
extern tcuTexRefSetFormat              *cuTexRefSetFormat;
extern tcuTexRefSetAddressMode         *cuTexRefSetAddressMode;
extern tcuTexRefSetFilterMode          *cuTexRefSetFilterMode;
extern tcuTexRefSetFlags               *cuTexRefSetFlags;
extern tcuTexRefGetArray               *cuTexRefGetArray;
extern tcuTexRefGetAddressMode         *cuTexRefGetAddressMode;
extern tcuTexRefGetFilterMode          *cuTexRefGetFilterMode;
extern tcuTexRefGetFormat              *cuTexRefGetFormat;
extern tcuTexRefGetFlags               *cuTexRefGetFlags;
extern tcuSurfRefSetArray              *cuSurfRefSetArray;
extern tcuSurfRefGetArray              *cuSurfRefGetArray;
extern tcuParamSetSize                 *cuParamSetSize;
extern tcuParamSeti                    *cuParamSeti;
extern tcuParamSetf                    *cuParamSetf;
extern tcuParamSetv                    *cuParamSetv;
extern tcuParamSetTexRef               *cuParamSetTexRef;
extern tcuLaunch                       *cuLaunch;
extern tcuLaunchGrid                   *cuLaunchGrid;
extern tcuLaunchGridAsync              *cuLaunchGridAsync;
extern tcuEventCreate                  *cuEventCreate;
extern tcuEventRecord                  *cuEventRecord;
extern tcuEventQuery                   *cuEventQuery;
extern tcuEventSynchronize             *cuEventSynchronize;
extern tcuEventDestroy                 *cuEventDestroy;
extern tcuEventElapsedTime             *cuEventElapsedTime;
extern tcuStreamCreate                 *cuStreamCreate;
extern tcuStreamQuery                  *cuStreamQuery;
extern tcuStreamSynchronize            *cuStreamSynchronize;
extern tcuStreamDestroy                *cuStreamDestroy;
extern tcuGraphicsUnregisterResource   *cuGraphicsUnregisterResource;
extern tcuGraphicsSubResourceGetMappedArray  *cuGraphicsSubResourceGetMappedArray;
extern tcuGraphicsResourceSetMapFlags  *cuGraphicsResourceSetMapFlags;
extern tcuGraphicsMapResources         *cuGraphicsMapResources;
extern tcuGraphicsUnmapResources       *cuGraphicsUnmapResources;
extern tcuGetExportTable               *cuGetExportTable;
extern tcuCtxSetLimit                  *cuCtxSetLimit;
extern tcuCtxGetLimit                  *cuCtxGetLimit;

// These functions could be using the CUDA 3.2 interface (_v2)
extern tcuMemcpyHtoD                   *cuMemcpyHtoD;
extern tcuMemcpyDtoH                   *cuMemcpyDtoH;
extern tcuMemcpyDtoD                   *cuMemcpyDtoD;
extern tcuMemcpyDtoA                   *cuMemcpyDtoA;
extern tcuMemcpyAtoD                   *cuMemcpyAtoD;
extern tcuMemcpyHtoA                   *cuMemcpyHtoA;
extern tcuMemcpyAtoH                   *cuMemcpyAtoH;
extern tcuMemcpyAtoA                   *cuMemcpyAtoA;
extern tcuMemcpy2D                     *cuMemcpy2D;
extern tcuMemcpy2DUnaligned            *cuMemcpy2DUnaligned;
extern tcuMemcpy3D                     *cuMemcpy3D;
extern tcuMemcpyHtoDAsync              *cuMemcpyHtoDAsync;
extern tcuMemcpyDtoHAsync              *cuMemcpyDtoHAsync;
extern tcuMemcpyDtoDAsync              *cuMemcpyDtoDAsync;
extern tcuMemcpyHtoAAsync              *cuMemcpyHtoAAsync;
extern tcuMemcpyAtoHAsync              *cuMemcpyAtoHAsync;
extern tcuMemcpy2DAsync                *cuMemcpy2DAsync;
extern tcuMemcpy3DAsync                *cuMemcpy3DAsync;
extern tcuMemsetD8                     *cuMemsetD8;
extern tcuMemsetD16                    *cuMemsetD16;
extern tcuMemsetD32                    *cuMemsetD32;
extern tcuMemsetD2D8                   *cuMemsetD2D8;
extern tcuMemsetD2D16                  *cuMemsetD2D16;
extern tcuMemsetD2D32                  *cuMemsetD2D32;
extern tcuArrayCreate                  *cuArrayCreate;
extern tcuArrayGetDescriptor           *cuArrayGetDescriptor;
extern tcuArray3DCreate                *cuArray3DCreate;
extern tcuArray3DGetDescriptor         *cuArray3DGetDescriptor;
extern tcuTexRefSetAddress             *cuTexRefSetAddress;
extern tcuTexRefSetAddress2D           *cuTexRefSetAddress2D;
extern tcuTexRefGetAddress             *cuTexRefGetAddress;
extern tcuGraphicsResourceGetMappedPointer   *cuGraphicsResourceGetMappedPointer;

/************************************/
CUresult CUDAAPI cuInit   (unsigned int, int cudaVersion, const char *cuda_path, const char *cuvid_path);
void CUDAAPI cuUninit();
/************************************/


#ifndef __CUDA_API_VERSION
#define __CUDA_API_VERSION 4000
#endif


#include <gpac/setup.h>
#include "../../src/compositor/gl_inc.h"

/**
 * \file dynlink_cudaGL.h
 * \brief Header file for the OpenGL interoperability functions of the
 * low-level CUDA driver application programming interface.
 */

/**
 * \defgroup CUDA_GL OpenGL Interoperability
 * \ingroup CUDA_DRIVER
 *
 * ___MANBRIEF___ OpenGL interoperability functions of the low-level CUDA
 * driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes the OpenGL interoperability functions of the
 * low-level CUDA driver application programming interface. Note that mapping
 * of OpenGL resources is performed with the graphics API agnostic, resource
 * mapping interface described in \ref CUDA_GRAPHICS "Graphics Interoperability".
 *
 * @{
 */

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#if !defined(WGL_NV_gpu_affinity)
typedef void* HGPUNV;
#endif
#endif /* _WIN32 */

typedef CUresult CUDAAPI tcuGraphicsGLRegisterBuffer(CUgraphicsResource *pCudaResource, GLuint buffer, unsigned int Flags);
typedef CUresult CUDAAPI tcuGraphicsGLRegisterImage(CUgraphicsResource *pCudaResource, GLuint image, GLenum target, unsigned int Flags);

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
typedef CUresult CUDAAPI tcuWGLGetDevice(CUdevice *pDevice, HGPUNV hGpu);
#endif /* _WIN32 */

/**
 * CUDA devices corresponding to an OpenGL device
 */
typedef enum {
    CU_GL_DEVICE_LIST_ALL            = 0x01, /**< The CUDA devices for all GPUs used by the current OpenGL context */
    CU_GL_DEVICE_LIST_CURRENT_FRAME  = 0x02, /**< The CUDA devices for the GPUs used by the current OpenGL context in its currently rendering frame */
    CU_GL_DEVICE_LIST_NEXT_FRAME     = 0x03, /**< The CUDA devices for the GPUs to be used by the current OpenGL context in the next frame */
} CUGLDeviceList;

#if __CUDA_API_VERSION >= 6050
typedef CUresult CUDAAPI tcuGLGetDevices(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevices, unsigned int cudaDeviceCount, CUGLDeviceList deviceList);
#endif /* __CUDA_API_VERSION >= 6050 */

/**
 * \defgroup CUDA_GL_DEPRECATED OpenGL Interoperability [DEPRECATED]
 *
 * ___MANBRIEF___ deprecated OpenGL interoperability functions of the low-level
 * CUDA driver API (___CURRENT_FILE___) ___ENDMANBRIEF___
 *
 * This section describes deprecated OpenGL interoperability functionality.
 *
 * @{
 */

/** Flags to map or unmap a resource */
typedef enum CUGLmap_flags_enum {
    CU_GL_MAP_RESOURCE_FLAGS_NONE          = 0x00,
    CU_GL_MAP_RESOURCE_FLAGS_READ_ONLY     = 0x01,
    CU_GL_MAP_RESOURCE_FLAGS_WRITE_DISCARD = 0x02,
} CUGLmap_flags;

//#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuGLCtxCreate(CUcontext *pCtx, unsigned int Flags, CUdevice device);
//#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuGLInit(void);
typedef CUresult CUDAAPI tcuGLRegisterBufferObject(GLuint buffer);

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuGLMapBufferObject(CUdeviceptr *dptr, size_t *size, GLuint buffer);
#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuGLUnmapBufferObject(GLuint buffer);
typedef CUresult CUDAAPI tcuGLUnregisterBufferObject(GLuint buffer);
typedef CUresult CUDAAPI tcuGLSetBufferObjectMapFlags(GLuint buffer, unsigned int Flags);

#if __CUDA_API_VERSION >= 3020
typedef CUresult CUDAAPI tcuGLMapBufferObjectAsync(CUdeviceptr *dptr, size_t *size, GLuint buffer, CUstream hStream);
#endif /* __CUDA_API_VERSION >= 3020 */

typedef CUresult CUDAAPI tcuGLUnmapBufferObjectAsync(GLuint buffer, CUstream hStream);
typedef CUresult CUDAAPI tcuGLGetDevices(unsigned int *pCudaDeviceCount, CUdevice *pCudaDevices, unsigned int cudaDeviceCount, CUGLDeviceList deviceList);

#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
extern tcuWGLGetDevice                 *cuWGLGetDevice;
#endif

extern tcuGLCtxCreate                  *cuGLCtxCreate;
extern tcuGLCtxCreate                  *cuGLCtxCreate_v2;
extern tcuGLMapBufferObject            *cuGLMapBufferObject;
extern tcuGLMapBufferObject            *cuGLMapBufferObject_v2;
extern tcuGLMapBufferObjectAsync       *cuGLMapBufferObjectAsync;

#if __CUDA_API_VERSION >= 6050
extern tcuGLGetDevices                 *cuGLGetDevices;
#endif

extern tcuGraphicsGLRegisterBuffer     *cuGraphicsGLRegisterBuffer;
extern tcuGraphicsGLRegisterImage      *cuGraphicsGLRegisterImage;
extern tcuGLSetBufferObjectMapFlags    *cuGLSetBufferObjectMapFlags;
extern tcuGLRegisterBufferObject       *cuGLRegisterBufferObject;

extern tcuGLUnmapBufferObject          *cuGLUnmapBufferObject;
extern tcuGLUnmapBufferObjectAsync     *cuGLUnmapBufferObjectAsync;

extern tcuGLUnregisterBufferObject     *cuGLUnregisterBufferObject;
extern tcuGLGetDevices                 *cuGLGetDevices; // CUDA 6.5 only


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
#include <windows.h>
typedef HMODULE CUDADRIVER;
#else
typedef void *CUDADRIVER;
#endif




#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
#if (CUDA_VERSION >= 3020) && (!defined(CUDA_FORCE_API_VERSION) || (CUDA_FORCE_API_VERSION >= 3020))
#define __CUVID_DEVPTR64
#endif
#endif


typedef void *CUvideodecoder;
typedef struct _CUcontextlock_st *CUvideoctxlock;

/**
 * \addtogroup VIDEO_DECODER Video Decoder
 * @{
 */

/*!
 * \enum cudaVideoCodec
 * Video Codec Enums
 */
typedef enum cudaVideoCodec_enum {
    cudaVideoCodec_MPEG1=0,                 /**<  MPEG1   */
    cudaVideoCodec_MPEG2,                   /**<  MPEG2  */
    cudaVideoCodec_MPEG4,                   /**<  MPEG4   */
    cudaVideoCodec_VC1,                     /**<  VC1   */
    cudaVideoCodec_H264,                    /**<  H264   */
    cudaVideoCodec_JPEG,                    /**<  JPEG   */
    cudaVideoCodec_H264_SVC,                /**<  H264-SVC   */
    cudaVideoCodec_H264_MVC,                /**<  H264-MVC   */
    cudaVideoCodec_HEVC,                    /**<  HEVC   */
    cudaVideoCodec_VP8,                     /**<  VP8   */
    cudaVideoCodec_VP9,                     /**<  VP9   */
    cudaVideoCodec_NumCodecs,               /**<  Max COdecs   */
    // Uncompressed YUV
    cudaVideoCodec_YUV420 = (('I'<<24)|('Y'<<16)|('U'<<8)|('V')),   /**< Y,U,V (4:2:0)  */
    cudaVideoCodec_YV12   = (('Y'<<24)|('V'<<16)|('1'<<8)|('2')),   /**< Y,V,U (4:2:0)  */
    cudaVideoCodec_NV12   = (('N'<<24)|('V'<<16)|('1'<<8)|('2')),   /**< Y,UV  (4:2:0)  */
    cudaVideoCodec_YUYV   = (('Y'<<24)|('U'<<16)|('Y'<<8)|('V')),   /**< YUYV/YUY2 (4:2:2)  */
    cudaVideoCodec_UYVY   = (('U'<<24)|('Y'<<16)|('V'<<8)|('Y'))    /**< UYVY (4:2:2)  */
} cudaVideoCodec;

/*!
 * \enum cudaVideoSurfaceFormat
 * Video Surface Formats Enums
 */
typedef enum cudaVideoSurfaceFormat_enum {
    cudaVideoSurfaceFormat_NV12=0,       /**< NV12 (currently the only supported output format)  */
	cudaVideoSurfaceFormat_P016=1
} cudaVideoSurfaceFormat;

/*!
 * \enum cudaVideoDeinterlaceMode
 * Deinterlacing Modes Enums
 */
typedef enum cudaVideoDeinterlaceMode_enum {
    cudaVideoDeinterlaceMode_Weave=0,   /**< Weave both fields (no deinterlacing) */
    cudaVideoDeinterlaceMode_Bob,       /**< Drop one field  */
    cudaVideoDeinterlaceMode_Adaptive   /**< Adaptive deinterlacing  */
} cudaVideoDeinterlaceMode;

/*!
 * \enum cudaVideoChromaFormat
 * Chroma Formats Enums
 */
typedef enum cudaVideoChromaFormat_enum {
    cudaVideoChromaFormat_Monochrome=0,  /**< MonoChrome */
    cudaVideoChromaFormat_420,           /**< 4:2:0 */
    cudaVideoChromaFormat_422,           /**< 4:2:2 */
    cudaVideoChromaFormat_444            /**< 4:4:4 */
} cudaVideoChromaFormat;

/*!
 * \enum cudaVideoCreateFlags
 * Decoder Flags Enums
 */
typedef enum cudaVideoCreateFlags_enum {
    cudaVideoCreate_Default = 0x00,     /**< Default operation mode: use dedicated video engines */
    cudaVideoCreate_PreferCUDA = 0x01,  /**< Use a CUDA-based decoder if faster than dedicated engines (requires a valid vidLock object for multi-threading) */
    cudaVideoCreate_PreferDXVA = 0x02,  /**< Go through DXVA internally if possible (requires D3D9 interop) */
    cudaVideoCreate_PreferCUVID = 0x04  /**< Use dedicated video engines directly */
} cudaVideoCreateFlags;

/*!
 * \struct CUVIDDECODECREATEINFO
 * Struct used in create decoder
 */
typedef struct _CUVIDDECODECREATEINFO
{
    unsigned long ulWidth;              /**< Coded Sequence Width */
    unsigned long ulHeight;             /**< Coded Sequence Height */
    unsigned long ulNumDecodeSurfaces;  /**< Maximum number of internal decode surfaces */
    cudaVideoCodec CodecType;           /**< cudaVideoCodec_XXX */
    cudaVideoChromaFormat ChromaFormat; /**< cudaVideoChromaFormat_XXX (only 4:2:0 is currently supported) */
    unsigned long ulCreationFlags;      /**< Decoder creation flags (cudaVideoCreateFlags_XXX) */
    unsigned long bitDepthMinus8;
    unsigned long Reserved1[4];         /**< Reserved for future use - set to zero */
    /**
    * area of the frame that should be displayed
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } display_area;

    cudaVideoSurfaceFormat OutputFormat;       /**< cudaVideoSurfaceFormat_XXX */
    cudaVideoDeinterlaceMode DeinterlaceMode;  /**< cudaVideoDeinterlaceMode_XXX */
    unsigned long ulTargetWidth;               /**< Post-processed Output Width (Should be aligned to 2) */
    unsigned long ulTargetHeight;              /**< Post-processed Output Height (Should be aligbed to 2) */
    unsigned long ulNumOutputSurfaces;         /**< Maximum number of output surfaces simultaneously mapped */
    CUvideoctxlock vidLock;                    /**< If non-NULL, context lock used for synchronizing ownership of the cuda context */
    /**
    * target rectangle in the output frame (for aspect ratio conversion)
    * if a null rectangle is specified, {0,0,ulTargetWidth,ulTargetHeight} will be used
    */
    struct {
        short left;
        short top;
        short right;
        short bottom;
    } target_rect;
    unsigned long Reserved2[5];                /**< Reserved for future use - set to zero */
} CUVIDDECODECREATEINFO;

/*!
 * \struct CUVIDH264DPBENTRY
 * H.264 DPB Entry
 */
typedef struct _CUVIDH264DPBENTRY
{
    int PicIdx;                 /**< picture index of reference frame */
    int FrameIdx;               /**< frame_num(short-term) or LongTermFrameIdx(long-term) */
    int is_long_term;           /**< 0=short term reference, 1=long term reference */
    int not_existing;           /**< non-existing reference frame (corresponding PicIdx should be set to -1) */
    int used_for_reference;     /**< 0=unused, 1=top_field, 2=bottom_field, 3=both_fields */
    int FieldOrderCnt[2];       /**< field order count of top and bottom fields */
} CUVIDH264DPBENTRY;

/*!
 * \struct CUVIDH264MVCEXT
 * H.264 MVC Picture Parameters Ext
 */
typedef struct _CUVIDH264MVCEXT
{
    int num_views_minus1;
    int view_id;
    unsigned char inter_view_flag;
    unsigned char num_inter_view_refs_l0;
    unsigned char num_inter_view_refs_l1;
    unsigned char MVCReserved8Bits;
    int InterViewRefsL0[16];
    int InterViewRefsL1[16];
} CUVIDH264MVCEXT;

/*!
 * \struct CUVIDH264SVCEXT
 * H.264 SVC Picture Parameters Ext
 */
typedef struct _CUVIDH264SVCEXT
{
    unsigned char profile_idc;
    unsigned char level_idc;
    unsigned char DQId;
    unsigned char DQIdMax;
    unsigned char disable_inter_layer_deblocking_filter_idc;
    unsigned char ref_layer_chroma_phase_y_plus1;
    signed char   inter_layer_slice_alpha_c0_offset_div2;
    signed char   inter_layer_slice_beta_offset_div2;

    unsigned short DPBEntryValidFlag;
    unsigned char inter_layer_deblocking_filter_control_present_flag;
    unsigned char extended_spatial_scalability_idc;
    unsigned char adaptive_tcoeff_level_prediction_flag;
    unsigned char slice_header_restriction_flag;
    unsigned char chroma_phase_x_plus1_flag;
    unsigned char chroma_phase_y_plus1;

    unsigned char tcoeff_level_prediction_flag;
    unsigned char constrained_intra_resampling_flag;
    unsigned char ref_layer_chroma_phase_x_plus1_flag;
    unsigned char store_ref_base_pic_flag;
    unsigned char Reserved8BitsA;
    unsigned char Reserved8BitsB;
    // For the 4 scaled_ref_layer_XX fields below,
    // if (extended_spatial_scalability_idc == 1), SPS field, G.7.3.2.1.4, add prefix "seq_"
    // if (extended_spatial_scalability_idc == 2), SLH field, G.7.3.3.4,
    short scaled_ref_layer_left_offset;
    short scaled_ref_layer_top_offset;
    short scaled_ref_layer_right_offset;
    short scaled_ref_layer_bottom_offset;
    unsigned short Reserved16Bits;
    struct _CUVIDPICPARAMS *pNextLayer; /**< Points to the picparams for the next layer to be decoded. Linked list ends at the target layer. */
    int bRefBaseLayer;                  /**< whether to store ref base pic */
} CUVIDH264SVCEXT;

/*!
 * \struct CUVIDH264PICPARAMS
 * H.264 Picture Parameters
 */
typedef struct _CUVIDH264PICPARAMS
{
    // SPS
    int log2_max_frame_num_minus4;
    int pic_order_cnt_type;
    int log2_max_pic_order_cnt_lsb_minus4;
    int delta_pic_order_always_zero_flag;
    int frame_mbs_only_flag;
    int direct_8x8_inference_flag;
    int num_ref_frames;             // NOTE: shall meet level 4.1 restrictions
    unsigned char residual_colour_transform_flag;
    unsigned char bit_depth_luma_minus8;    // Must be 0 (only 8-bit supported)
    unsigned char bit_depth_chroma_minus8;  // Must be 0 (only 8-bit supported)
    unsigned char qpprime_y_zero_transform_bypass_flag;
    // PPS
    int entropy_coding_mode_flag;
    int pic_order_present_flag;
    int num_ref_idx_l0_active_minus1;
    int num_ref_idx_l1_active_minus1;
    int weighted_pred_flag;
    int weighted_bipred_idc;
    int pic_init_qp_minus26;
    int deblocking_filter_control_present_flag;
    int redundant_pic_cnt_present_flag;
    int transform_8x8_mode_flag;
    int MbaffFrameFlag;
    int constrained_intra_pred_flag;
    int chroma_qp_index_offset;
    int second_chroma_qp_index_offset;
    int ref_pic_flag;
    int frame_num;
    int CurrFieldOrderCnt[2];
    // DPB
    CUVIDH264DPBENTRY dpb[16];          // List of reference frames within the DPB
    // Quantization Matrices (raster-order)
    unsigned char WeightScale4x4[6][16];
    unsigned char WeightScale8x8[2][64];
    // FMO/ASO
    unsigned char fmo_aso_enable;
    unsigned char num_slice_groups_minus1;
    unsigned char slice_group_map_type;
    signed char pic_init_qs_minus26;
    unsigned int slice_group_change_rate_minus1;
    union
    {
        unsigned long long slice_group_map_addr;
        const unsigned char *pMb2SliceGroupMap;
    } fmo;
    unsigned int  Reserved[12];
    // SVC/MVC
    union
    {
        CUVIDH264MVCEXT mvcext;
        CUVIDH264SVCEXT svcext;
    };
} CUVIDH264PICPARAMS;


/*!
 * \struct CUVIDMPEG2PICPARAMS
 * MPEG-2 Picture Parameters
 */
typedef struct _CUVIDMPEG2PICPARAMS
{
    int ForwardRefIdx;          // Picture index of forward reference (P/B-frames)
    int BackwardRefIdx;         // Picture index of backward reference (B-frames)
    int picture_coding_type;
    int full_pel_forward_vector;
    int full_pel_backward_vector;
    int f_code[2][2];
    int intra_dc_precision;
    int frame_pred_frame_dct;
    int concealment_motion_vectors;
    int q_scale_type;
    int intra_vlc_format;
    int alternate_scan;
    int top_field_first;
    // Quantization matrices (raster order)
    unsigned char QuantMatrixIntra[64];
    unsigned char QuantMatrixInter[64];
} CUVIDMPEG2PICPARAMS;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// MPEG-4 Picture Parameters
//

// MPEG-4 has VOP types instead of Picture types
#define I_VOP 0
#define P_VOP 1
#define B_VOP 2
#define S_VOP 3

/*!
 * \struct CUVIDMPEG4PICPARAMS
 * MPEG-4 Picture Parameters
 */
typedef struct _CUVIDMPEG4PICPARAMS
{
    int ForwardRefIdx;          // Picture index of forward reference (P/B-frames)
    int BackwardRefIdx;         // Picture index of backward reference (B-frames)
    // VOL
    int video_object_layer_width;
    int video_object_layer_height;
    int vop_time_increment_bitcount;
    int top_field_first;
    int resync_marker_disable;
    int quant_type;
    int quarter_sample;
    int short_video_header;
    int divx_flags;
    // VOP
    int vop_coding_type;
    int vop_coded;
    int vop_rounding_type;
    int alternate_vertical_scan_flag;
    int interlaced;
    int vop_fcode_forward;
    int vop_fcode_backward;
    int trd[2];
    int trb[2];
    // Quantization matrices (raster order)
    unsigned char QuantMatrixIntra[64];
    unsigned char QuantMatrixInter[64];
    int gmc_enabled;
} CUVIDMPEG4PICPARAMS;

/*!
 * \struct CUVIDVC1PICPARAMS
 * VC1 Picture Parameters
 */
typedef struct _CUVIDVC1PICPARAMS
{
    int ForwardRefIdx;      /**< Picture index of forward reference (P/B-frames) */
    int BackwardRefIdx;     /**< Picture index of backward reference (B-frames) */
    int FrameWidth;         /**< Actual frame width */
    int FrameHeight;        /**< Actual frame height */
    // PICTURE
    int intra_pic_flag;     /**< Set to 1 for I,BI frames */
    int ref_pic_flag;       /**< Set to 1 for I,P frames */
    int progressive_fcm;    /**< Progressive frame */
    // SEQUENCE
    int profile;
    int postprocflag;
    int pulldown;
    int interlace;
    int tfcntrflag;
    int finterpflag;
    int psf;
    int multires;
    int syncmarker;
    int rangered;
    int maxbframes;
    // ENTRYPOINT
    int panscan_flag;
    int refdist_flag;
    int extended_mv;
    int dquant;
    int vstransform;
    int loopfilter;
    int fastuvmc;
    int overlap;
    int quantizer;
    int extended_dmv;
    int range_mapy_flag;
    int range_mapy;
    int range_mapuv_flag;
    int range_mapuv;
    int rangeredfrm;    // range reduction state
} CUVIDVC1PICPARAMS;

/*!
 * \struct CUVIDJPEGPICPARAMS
 * JPEG Picture Parameters
 */
typedef struct _CUVIDJPEGPICPARAMS
{
    int Reserved;
} CUVIDJPEGPICPARAMS;


 /*!
 * \struct CUVIDHEVCPICPARAMS
 * HEVC Picture Parameters
 */
typedef struct _CUVIDHEVCPICPARAMS
{
    // sps
    int pic_width_in_luma_samples;
    int pic_height_in_luma_samples;
    unsigned char log2_min_luma_coding_block_size_minus3;
    unsigned char log2_diff_max_min_luma_coding_block_size;
    unsigned char log2_min_transform_block_size_minus2;
    unsigned char log2_diff_max_min_transform_block_size;
    unsigned char pcm_enabled_flag;
    unsigned char log2_min_pcm_luma_coding_block_size_minus3;
    unsigned char log2_diff_max_min_pcm_luma_coding_block_size;
    unsigned char pcm_sample_bit_depth_luma_minus1;

    unsigned char pcm_sample_bit_depth_chroma_minus1;
    unsigned char pcm_loop_filter_disabled_flag;
    unsigned char strong_intra_smoothing_enabled_flag;
    unsigned char max_transform_hierarchy_depth_intra;
    unsigned char max_transform_hierarchy_depth_inter;
    unsigned char amp_enabled_flag;
    unsigned char separate_colour_plane_flag;
    unsigned char log2_max_pic_order_cnt_lsb_minus4;

    unsigned char num_short_term_ref_pic_sets;
    unsigned char long_term_ref_pics_present_flag;
    unsigned char num_long_term_ref_pics_sps;
    unsigned char sps_temporal_mvp_enabled_flag;
    unsigned char sample_adaptive_offset_enabled_flag;
    unsigned char scaling_list_enable_flag;
    unsigned char IrapPicFlag;
    unsigned char IdrPicFlag;

    unsigned char bit_depth_luma_minus8;
    unsigned char bit_depth_chroma_minus8;
    unsigned char reserved1[14];

    // pps
    unsigned char dependent_slice_segments_enabled_flag;
    unsigned char slice_segment_header_extension_present_flag;
    unsigned char sign_data_hiding_enabled_flag;
    unsigned char cu_qp_delta_enabled_flag;
    unsigned char diff_cu_qp_delta_depth;
    signed char init_qp_minus26;
    signed char pps_cb_qp_offset;
    signed char pps_cr_qp_offset;

    unsigned char constrained_intra_pred_flag;
    unsigned char weighted_pred_flag;
    unsigned char weighted_bipred_flag;
    unsigned char transform_skip_enabled_flag;
    unsigned char transquant_bypass_enabled_flag;
    unsigned char entropy_coding_sync_enabled_flag;
    unsigned char log2_parallel_merge_level_minus2;
    unsigned char num_extra_slice_header_bits;

    unsigned char loop_filter_across_tiles_enabled_flag;
    unsigned char loop_filter_across_slices_enabled_flag;
    unsigned char output_flag_present_flag;
    unsigned char num_ref_idx_l0_default_active_minus1;
    unsigned char num_ref_idx_l1_default_active_minus1;
    unsigned char lists_modification_present_flag;
    unsigned char cabac_init_present_flag;
    unsigned char pps_slice_chroma_qp_offsets_present_flag;

    unsigned char deblocking_filter_override_enabled_flag;
    unsigned char pps_deblocking_filter_disabled_flag;
    signed char pps_beta_offset_div2;
    signed char pps_tc_offset_div2;
    unsigned char tiles_enabled_flag;
    unsigned char uniform_spacing_flag;
    unsigned char num_tile_columns_minus1;
    unsigned char num_tile_rows_minus1;

    unsigned short column_width_minus1[21];
    unsigned short row_height_minus1[21];
    unsigned int reserved3[15];

    // RefPicSets
    int NumBitsForShortTermRPSInSlice;
    int NumDeltaPocsOfRefRpsIdx;
    int NumPocTotalCurr;
    int NumPocStCurrBefore;
    int NumPocStCurrAfter;
    int NumPocLtCurr;
    int CurrPicOrderCntVal;
    int RefPicIdx[16];                  // [refpic] Indices of valid reference pictures (-1 if unused for reference)
    int PicOrderCntVal[16];             // [refpic]
    unsigned char IsLongTerm[16];       // [refpic] 0=not a long-term reference, 1=long-term reference
    unsigned char RefPicSetStCurrBefore[8]; // [0..NumPocStCurrBefore-1] -> refpic (0..15)
    unsigned char RefPicSetStCurrAfter[8];  // [0..NumPocStCurrAfter-1] -> refpic (0..15)
    unsigned char RefPicSetLtCurr[8];       // [0..NumPocLtCurr-1] -> refpic (0..15)
    unsigned char RefPicSetInterLayer0[8];
    unsigned char RefPicSetInterLayer1[8];
    unsigned int reserved4[12];

    // scaling lists (diag order)
    unsigned char ScalingList4x4[6][16];       // [matrixId][i]
    unsigned char ScalingList8x8[6][64];       // [matrixId][i]
    unsigned char ScalingList16x16[6][64];     // [matrixId][i]
    unsigned char ScalingList32x32[2][64];     // [matrixId][i]
    unsigned char ScalingListDCCoeff16x16[6];  // [matrixId]
    unsigned char ScalingListDCCoeff32x32[2];  // [matrixId]
} CUVIDHEVCPICPARAMS;


/*!
 * \struct CUVIDVP8PICPARAMS
 * VP8 Picture Parameters
 */
typedef struct _CUVIDVP8PICPARAMS
{
    int width;
    int height;
    unsigned int first_partition_size;
    //Frame Indexes
    unsigned char LastRefIdx;
    unsigned char GoldenRefIdx;
    unsigned char AltRefIdx;
    union {
        struct {
            unsigned char frame_type : 1;    /**< 0 = KEYFRAME, 1 = INTERFRAME  */
            unsigned char version : 3;
            unsigned char show_frame : 1;
            unsigned char update_mb_segmentation_data : 1;    /**< Must be 0 if segmentation is not enabled */
            unsigned char Reserved2Bits : 2;
        };
        unsigned char wFrameTagFlags;
    };
    unsigned char Reserved1[4];
    unsigned int  Reserved2[3];
} CUVIDVP8PICPARAMS;

/*!
 * \struct CUVIDVP9PICPARAMS
 * VP9 Picture Parameters
 */
typedef struct _CUVIDVP9PICPARAMS
{
    unsigned int width;
    unsigned int height;

    //Frame Indices
    unsigned char LastRefIdx;
    unsigned char GoldenRefIdx;
    unsigned char AltRefIdx;
    unsigned char colorSpace;

    unsigned short profile : 3;
    unsigned short frameContextIdx : 2;
    unsigned short frameType : 1;
    unsigned short showFrame : 1;
    unsigned short errorResilient : 1;
    unsigned short frameParallelDecoding : 1;
    unsigned short subSamplingX : 1;
    unsigned short subSamplingY : 1;
    unsigned short intraOnly : 1;
    unsigned short allow_high_precision_mv : 1;
    unsigned short refreshEntropyProbs : 1;
    unsigned short reserved2Bits : 2;

    unsigned short reserved16Bits;

    unsigned char  refFrameSignBias[4];

    unsigned char bitDepthMinus8Luma;
    unsigned char bitDepthMinus8Chroma;
    unsigned char loopFilterLevel;
    unsigned char loopFilterSharpness;

    unsigned char modeRefLfEnabled;
    unsigned char log2_tile_columns;
    unsigned char log2_tile_rows;

    unsigned char segmentEnabled : 1;
    unsigned char segmentMapUpdate : 1;
    unsigned char segmentMapTemporalUpdate : 1;
    unsigned char segmentFeatureMode : 1;
    unsigned char reserved4Bits : 4;


    unsigned char segmentFeatureEnable[8][4];
    short segmentFeatureData[8][4];
    unsigned char mb_segment_tree_probs[7];
    unsigned char segment_pred_probs[3];
    unsigned char reservedSegment16Bits[2];

    int qpYAc;
    int qpYDc;
    int qpChDc;
    int qpChAc;

    unsigned int activeRefIdx[3];
    unsigned int resetFrameContext;
    unsigned int mcomp_filter_type;
    unsigned int mbRefLfDelta[4];
    unsigned int mbModeLfDelta[2];
    unsigned int frameTagSize;
    unsigned int offsetToDctParts;
    unsigned int reserved128Bits[4];

} CUVIDVP9PICPARAMS;


/*!
 * \struct CUVIDPICPARAMS
 * Picture Parameters for Decoding
 */
typedef struct _CUVIDPICPARAMS
{
    int PicWidthInMbs;                    /**< Coded Frame Size */
    int FrameHeightInMbs;                 /**< Coded Frame Height */
    int CurrPicIdx;                       /**< Output index of the current picture */
    int field_pic_flag;                   /**< 0=frame picture, 1=field picture */
    int bottom_field_flag;                /**< 0=top field, 1=bottom field (ignored if field_pic_flag=0) */
    int second_field;                     /**< Second field of a complementary field pair */
    // Bitstream data
    unsigned int nBitstreamDataLen;        /**< Number of bytes in bitstream data buffer */
    const unsigned char *pBitstreamData;   /**< Ptr to bitstream data for this picture (slice-layer) */
    unsigned int nNumSlices;               /**< Number of slices in this picture */
    const unsigned int *pSliceDataOffsets; /**< nNumSlices entries, contains offset of each slice within the bitstream data buffer */
    int ref_pic_flag;                      /**< This picture is a reference picture */
    int intra_pic_flag;                    /**< This picture is entirely intra coded */
    unsigned int Reserved[30];             /**< Reserved for future use */
    // Codec-specific data
    union {
        CUVIDMPEG2PICPARAMS mpeg2;         /**< Also used for MPEG-1 */
        CUVIDH264PICPARAMS h264;
        CUVIDVC1PICPARAMS vc1;
        CUVIDMPEG4PICPARAMS mpeg4;
        CUVIDJPEGPICPARAMS jpeg;
        CUVIDHEVCPICPARAMS hevc;
        CUVIDVP8PICPARAMS vp8;
        CUVIDVP9PICPARAMS vp9;
        unsigned int CodecReserved[1024];
    } CodecSpecific;
} CUVIDPICPARAMS;


/*!
 * \struct CUVIDPROCPARAMS
 * Picture Parameters for Postprocessing
 */
typedef struct _CUVIDPROCPARAMS
{
    int progressive_frame;  /**< Input is progressive (deinterlace_mode will be ignored)  */
    int second_field;       /**< Output the second field (ignored if deinterlace mode is Weave) */
    int top_field_first;    /**< Input frame is top field first (1st field is top, 2nd field is bottom) */
    int unpaired_field;     /**< Input only contains one field (2nd field is invalid) */
    // The fields below are used for raw YUV input
    unsigned int reserved_flags;        /**< Reserved for future use (set to zero) */
    unsigned int reserved_zero;         /**< Reserved (set to zero) */
    unsigned long long raw_input_dptr;  /**< Input CUdeviceptr for raw YUV extensions */
    unsigned int raw_input_pitch;       /**< pitch in bytes of raw YUV input (should be aligned appropriately) */
    unsigned int raw_input_format;      /**< Reserved for future use (set to zero) */
    unsigned long long raw_output_dptr; /**< Reserved for future use (set to zero) */
    unsigned int raw_output_pitch;      /**< Reserved for future use (set to zero) */
    unsigned int Reserved[48];
    void *Reserved3[3];
} CUVIDPROCPARAMS;


/**
 *
 * In order to minimize decode latencies, there should be always at least 2 pictures in the decode
 * queue at any time, in order to make sure that all decode engines are always busy.
 *
 * Overall data flow:
 *  - cuvidCreateDecoder(...)
 *  For each picture:
 *  - cuvidDecodePicture(N)
 *  - cuvidMapVideoFrame(N-4)
 *  - do some processing in cuda
 *  - cuvidUnmapVideoFrame(N-4)
 *  - cuvidDecodePicture(N+1)
 *  - cuvidMapVideoFrame(N-3)
 *    ...
 *  - cuvidDestroyDecoder(...)
 *
 * NOTE:
 * - When the cuda context is created from a D3D device, the D3D device must also be created
 *   with the D3DCREATE_MULTITHREADED flag.
 * - There is a limit to how many pictures can be mapped simultaneously (ulNumOutputSurfaces)
 * - cuVidDecodePicture may block the calling thread if there are too many pictures pending
 *   in the decode queue
 */

/**
 * \fn CUresult CUDAAPI cuvidCreateDecoder(CUvideodecoder *phDecoder, CUVIDDECODECREATEINFO *pdci)
 * Create the decoder object
 */
typedef CUresult CUDAAPI tcuvidCreateDecoder(CUvideodecoder *phDecoder, CUVIDDECODECREATEINFO *pdci);

/**
 * \fn CUresult CUDAAPI cuvidDestroyDecoder(CUvideodecoder hDecoder)
 * Destroy the decoder object
 */
typedef CUresult CUDAAPI tcuvidDestroyDecoder(CUvideodecoder hDecoder);

/**
 * \fn CUresult CUDAAPI cuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS *pPicParams)
 * Decode a single picture (field or frame)
 */
typedef CUresult CUDAAPI tcuvidDecodePicture(CUvideodecoder hDecoder, CUVIDPICPARAMS *pPicParams);


#if !defined(__CUVID_DEVPTR64) || defined(__CUVID_INTERNAL)
/**
 * \fn CUresult CUDAAPI cuvidMapVideoFrame(CUvideodecoder hDecoder, int nPicIdx, unsigned int *pDevPtr, unsigned int *pPitch, CUVIDPROCPARAMS *pVPP);
 * Post-process and map a video frame for use in cuda
 */
typedef CUresult CUDAAPI tcuvidMapVideoFrame(CUvideodecoder hDecoder, int nPicIdx,
                                           unsigned int *pDevPtr, unsigned int *pPitch,
                                           CUVIDPROCPARAMS *pVPP);

/**
 * \fn CUresult CUDAAPI cuvidUnmapVideoFrame(CUvideodecoder hDecoder, unsigned int DevPtr)
 * Unmap a previously mapped video frame
 */
typedef CUresult CUDAAPI tcuvidUnmapVideoFrame(CUvideodecoder hDecoder, unsigned int DevPtr);
#endif

#if defined(WIN64) || defined(_WIN64) || defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
/**
 * \fn CUresult CUDAAPI cuvidMapVideoFrame64(CUvideodecoder hDecoder, int nPicIdx, unsigned long long *pDevPtr, unsigned int *pPitch, CUVIDPROCPARAMS *pVPP);
 * map a video frame
 */
typedef CUresult CUDAAPI tcuvidMapVideoFrame64(CUvideodecoder hDecoder, int nPicIdx, unsigned long long *pDevPtr,
                                             unsigned int *pPitch, CUVIDPROCPARAMS *pVPP);

/**
 * \fn CUresult CUDAAPI cuvidUnmapVideoFrame64(CUvideodecoder hDecoder, unsigned long long DevPtr);
 * Unmap a previously mapped video frame
 */
typedef CUresult CUDAAPI tcuvidUnmapVideoFrame64(CUvideodecoder hDecoder, unsigned long long DevPtr);

#if defined(__CUVID_DEVPTR64) && !defined(__CUVID_INTERNAL)
#define tcuvidMapVideoFrame      tcuvidMapVideoFrame64
#define tcuvidUnmapVideoFrame    tcuvidUnmapVideoFrame64
#endif
#endif



/**
 *
 * Context-locking: to facilitate multi-threaded implementations, the following 4 functions
 * provide a simple mutex-style host synchronization. If a non-NULL context is specified
 * in CUVIDDECODECREATEINFO, the codec library will acquire the mutex associated with the given
 * context before making any cuda calls.
 * A multi-threaded application could create a lock associated with a context handle so that
 * multiple threads can safely share the same cuda context:
 *  - use cuCtxPopCurrent immediately after context creation in order to create a 'floating' context
 *    that can be passed to cuvidCtxLockCreate.
 *  - When using a floating context, all cuda calls should only be made within a cuvidCtxLock/cuvidCtxUnlock section.
 *
 * NOTE: This is a safer alternative to cuCtxPushCurrent and cuCtxPopCurrent, and is not related to video
 * decoder in any way (implemented as a critical section associated with cuCtx{Push|Pop}Current calls).
*/

/**
 * \fn CUresult CUDAAPI cuvidCtxLockCreate(CUvideoctxlock *pLock, CUcontext ctx)
 */
typedef CUresult CUDAAPI tcuvidCtxLockCreate(CUvideoctxlock *pLock, CUcontext ctx);

/**
 * \fn CUresult CUDAAPI cuvidCtxLockDestroy(CUvideoctxlock lck)
 */
typedef CUresult CUDAAPI tcuvidCtxLockDestroy(CUvideoctxlock lck);

/**
 * \fn CUresult CUDAAPI cuvidCtxLock(CUvideoctxlock lck, unsigned int reserved_flags)
 */
typedef CUresult CUDAAPI tcuvidCtxLock(CUvideoctxlock lck, unsigned int reserved_flags);

/**
 * \fn CUresult CUDAAPI cuvidCtxUnlock(CUvideoctxlock lck, unsigned int reserved_flags)
 */
typedef CUresult CUDAAPI tcuvidCtxUnlock(CUvideoctxlock lck, unsigned int reserved_flags);

/** @} */  /* End VIDEO_DECODER */
////////////////////////////////////////////////////////////////////////////////////////////////


extern tcuvidCreateDecoder        *cuvidCreateDecoder;
extern tcuvidDestroyDecoder       *cuvidDestroyDecoder;
extern tcuvidDecodePicture        *cuvidDecodePicture;
extern tcuvidMapVideoFrame        *cuvidMapVideoFrame;
extern tcuvidUnmapVideoFrame      *cuvidUnmapVideoFrame;

#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
extern tcuvidMapVideoFrame64      *cuvidMapVideoFrame64;
extern tcuvidUnmapVideoFrame64    *cuvidUnmapVideoFrame64;
#endif

//extern tcuvidGetVideoFrameSurface *cuvidGetVideoFrameSurface;

extern tcuvidCtxLockCreate        *cuvidCtxLockCreate;
extern tcuvidCtxLockDestroy       *cuvidCtxLockDestroy;
extern tcuvidCtxLock              *cuvidCtxLock;
extern tcuvidCtxUnlock            *cuvidCtxUnlock;

////////////////////////////////////////////////////////////////////////////////////////////////
//
// High-level helper APIs for video sources
//

typedef void *CUvideosource;
typedef void *CUvideoparser;
typedef long long CUvideotimestamp;

/**
 * \addtogroup VIDEO_PARSER Video Parser
 * @{
 */

/*!
 * \enum cudaVideoState
 * Video Source State
 */
typedef enum {
    cudaVideoState_Error   = -1,    /**< Error state (invalid source)  */
    cudaVideoState_Stopped = 0,     /**< Source is stopped (or reached end-of-stream)  */
    cudaVideoState_Started = 1      /**< Source is running and delivering data  */
} cudaVideoState;

/*!
 * \enum cudaAudioCodec
 * Audio compression
 */
typedef enum {
    cudaAudioCodec_MPEG1=0,         /**< MPEG-1 Audio  */
    cudaAudioCodec_MPEG2,           /**< MPEG-2 Audio  */
    cudaAudioCodec_MP3,             /**< MPEG-1 Layer III Audio  */
    cudaAudioCodec_AC3,             /**< Dolby Digital (AC3) Audio  */
    cudaAudioCodec_LPCM             /**< PCM Audio  */
} cudaAudioCodec;

/*!
 * \struct CUVIDEOFORMAT
 * Video format
 */
typedef struct
{
    cudaVideoCodec codec;                   /**< Compression format  */
   /**
    * frame rate = numerator / denominator (for example: 30000/1001)
    */
    struct {
        unsigned int numerator;             /**< frame rate numerator   (0 = unspecified or variable frame rate) */
        unsigned int denominator;           /**< frame rate denominator (0 = unspecified or variable frame rate) */
    } frame_rate;
    unsigned char progressive_sequence;     /**< 0=interlaced, 1=progressive */
    unsigned char bit_depth_luma_minus8;    /**< high bit depth Luma */
    unsigned char bit_depth_chroma_minus8;  /**< high bit depth Chroma */
    unsigned char reserved1;                /**< Reserved for future use */
    unsigned int coded_width;               /**< coded frame width */
    unsigned int coded_height;              /**< coded frame height  */
   /**
    *   area of the frame that should be displayed
    * typical example:
    *   coded_width = 1920, coded_height = 1088
    *   display_area = { 0,0,1920,1080 }
    */
    struct {
        int left;                           /**< left position of display rect  */
        int top;                            /**< top position of display rect  */
        int right;                          /**< right position of display rect  */
        int bottom;                         /**< bottom position of display rect  */
    } display_area;
    cudaVideoChromaFormat chroma_format;    /**<  Chroma format */
    unsigned int bitrate;                   /**< video bitrate (bps, 0=unknown) */
   /**
    * Display Aspect Ratio = x:y (4:3, 16:9, etc)
    */
    struct {
        int x;
        int y;
    } display_aspect_ratio;
    /**
    * Video Signal Description
    */
    struct {
        unsigned char video_format          : 3;
        unsigned char video_full_range_flag : 1;
        unsigned char reserved_zero_bits    : 4;
        unsigned char color_primaries;
        unsigned char transfer_characteristics;
        unsigned char matrix_coefficients;
    } video_signal_description;
    unsigned int seqhdr_data_length;          /**< Additional bytes following (CUVIDEOFORMATEX)  */
} CUVIDEOFORMAT;

/*!
 * \struct CUVIDEOFORMATEX
 * Video format including raw sequence header information
 */
typedef struct
{
    CUVIDEOFORMAT format;
    unsigned char raw_seqhdr_data[1024];
} CUVIDEOFORMATEX;

/*!
 * \struct CUAUDIOFORMAT
 * Audio Formats
 */
typedef struct
{
    cudaAudioCodec codec;       /**< Compression format  */
    unsigned int channels;      /**< number of audio channels */
    unsigned int samplespersec; /**< sampling frequency */
    unsigned int bitrate;       /**< For uncompressed, can also be used to determine bits per sample */
    unsigned int reserved1;     /**< Reserved for future use */
    unsigned int reserved2;     /**< Reserved for future use */
} CUAUDIOFORMAT;


/*!
 * \enum CUvideopacketflags
 * Data packet flags
 */
typedef enum {
    CUVID_PKT_ENDOFSTREAM   = 0x01,   /**< Set when this is the last packet for this stream  */
    CUVID_PKT_TIMESTAMP     = 0x02,   /**< Timestamp is valid  */
    CUVID_PKT_DISCONTINUITY = 0x04    /**< Set when a discontinuity has to be signalled  */
} CUvideopacketflags;

/*!
 * \struct CUVIDSOURCEDATAPACKET
 * Data Packet
 */
typedef struct _CUVIDSOURCEDATAPACKET
{
    unsigned long flags;            /**< Combination of CUVID_PKT_XXX flags */
    unsigned long payload_size;     /**< number of bytes in the payload (may be zero if EOS flag is set) */
    const unsigned char *payload;   /**< Pointer to packet payload data (may be NULL if EOS flag is set) */
    CUvideotimestamp timestamp;     /**< Presentation timestamp (10MHz clock), only valid if CUVID_PKT_TIMESTAMP flag is set */
} CUVIDSOURCEDATAPACKET;

// Callback for packet delivery
typedef int (CUDAAPI *PFNVIDSOURCECALLBACK)(void *, CUVIDSOURCEDATAPACKET *);

/*!
 * \struct CUVIDSOURCEPARAMS
 * Source Params
 */
typedef struct _CUVIDSOURCEPARAMS
{
    unsigned int ulClockRate;                   /**< Timestamp units in Hz (0=default=10000000Hz)  */
    unsigned int uReserved1[7];                 /**< Reserved for future use - set to zero  */
    void *pUserData;                            /**< Parameter passed in to the data handlers  */
    PFNVIDSOURCECALLBACK pfnVideoDataHandler;   /**< Called to deliver audio packets  */
    PFNVIDSOURCECALLBACK pfnAudioDataHandler;   /**< Called to deliver video packets  */
    void *pvReserved2[8];                       /**< Reserved for future use - set to NULL */
} CUVIDSOURCEPARAMS;

/*!
 * \enum CUvideosourceformat_flags
 * CUvideosourceformat_flags
 */
typedef enum {
    CUVID_FMT_EXTFORMATINFO = 0x100             /**< Return extended format structure (CUVIDEOFORMATEX) */
} CUvideosourceformat_flags;

#if !defined(__APPLE__)
/**
 * \fn CUresult CUDAAPI cuvidCreateVideoSource(CUvideosource *pObj, const char *pszFileName, CUVIDSOURCEPARAMS *pParams)
 * Create Video Source
 */
typedef CUresult CUDAAPI tcuvidCreateVideoSource(CUvideosource *pObj, const char *pszFileName, CUVIDSOURCEPARAMS *pParams);

/**
 * \fn CUresult CUDAAPI cuvidCreateVideoSourceW(CUvideosource *pObj, const wchar_t *pwszFileName, CUVIDSOURCEPARAMS *pParams)
 * Create Video Source
 */
typedef CUresult CUDAAPI tcuvidCreateVideoSourceW(CUvideosource *pObj, const wchar_t *pwszFileName, CUVIDSOURCEPARAMS *pParams);

/**
 * \fn CUresult CUDAAPI cuvidDestroyVideoSource(CUvideosource obj)
 * Destroy Video Source
 */
typedef CUresult CUDAAPI tcuvidDestroyVideoSource(CUvideosource obj);

/**
 * \fn CUresult CUDAAPI cuvidSetVideoSourceState(CUvideosource obj, cudaVideoState state)
 * Set Video Source state
 */
typedef CUresult CUDAAPI tcuvidSetVideoSourceState(CUvideosource obj, cudaVideoState state);

/**
 * \fn cudaVideoState CUDAAPI cuvidGetVideoSourceState(CUvideosource obj)
 * Get Video Source state
 */
typedef cudaVideoState CUDAAPI tcuvidGetVideoSourceState(CUvideosource obj);

/**
 * \fn CUresult CUDAAPI cuvidGetSourceVideoFormat(CUvideosource obj, CUVIDEOFORMAT *pvidfmt, unsigned int flags)
 * Get Video Source Format
 */
typedef CUresult CUDAAPI tcuvidGetSourceVideoFormat(CUvideosource obj, CUVIDEOFORMAT *pvidfmt, unsigned int flags);

/**
 * \fn CUresult CUDAAPI cuvidGetSourceAudioFormat(CUvideosource obj, CUAUDIOFORMAT *paudfmt, unsigned int flags)
 * Set Video Source state
 */
typedef CUresult CUDAAPI tcuvidGetSourceAudioFormat(CUvideosource obj, CUAUDIOFORMAT *paudfmt, unsigned int flags);

#endif

/**
 * \struct CUVIDPARSERDISPINFO
 */
typedef struct _CUVIDPARSERDISPINFO
{
    int picture_index;         /**<                 */
    int progressive_frame;     /**<                 */
    int top_field_first;       /**<                 */
    int repeat_first_field;    /**< Number of additional fields (1=ivtc, 2=frame doubling, 4=frame tripling, -1=unpaired field)  */
    CUvideotimestamp timestamp; /**<     */
} CUVIDPARSERDISPINFO;

//
// Parser callbacks
// The parser will call these synchronously from within cuvidParseVideoData(), whenever a picture is ready to
// be decoded and/or displayed.
//
typedef int (CUDAAPI *PFNVIDSEQUENCECALLBACK)(void *, CUVIDEOFORMAT *);
typedef int (CUDAAPI *PFNVIDDECODECALLBACK)(void *, CUVIDPICPARAMS *);
typedef int (CUDAAPI *PFNVIDDISPLAYCALLBACK)(void *, CUVIDPARSERDISPINFO *);

/**
 * \struct CUVIDPARSERPARAMS
 */
typedef struct _CUVIDPARSERPARAMS
{
    cudaVideoCodec CodecType;               /**< cudaVideoCodec_XXX  */
    unsigned int ulMaxNumDecodeSurfaces;    /**< Max # of decode surfaces (parser will cycle through these) */
    unsigned int ulClockRate;               /**< Timestamp units in Hz (0=default=10000000Hz) */
    unsigned int ulErrorThreshold;          /**< % Error threshold (0-100) for calling pfnDecodePicture (100=always call pfnDecodePicture even if picture bitstream is fully corrupted) */
    unsigned int ulMaxDisplayDelay;         /**< Max display queue delay (improves pipelining of decode with display) - 0=no delay (recommended values: 2..4) */
    unsigned int uReserved1[5];             /**< Reserved for future use - set to 0 */
    void *pUserData;                        /**< User data for callbacks */
    PFNVIDSEQUENCECALLBACK pfnSequenceCallback; /**< Called before decoding frames and/or whenever there is a format change */
    PFNVIDDECODECALLBACK pfnDecodePicture;      /**< Called when a picture is ready to be decoded (decode order) */
    PFNVIDDISPLAYCALLBACK pfnDisplayPicture;    /**< Called whenever a picture is ready to be displayed (display order)  */
    void *pvReserved2[7];                       /**< Reserved for future use - set to NULL */
    CUVIDEOFORMATEX *pExtVideoInfo;             /**< [Optional] sequence header data from system layer */
} CUVIDPARSERPARAMS;

/**
 * \fn CUresult CUDAAPI cuvidCreateVideoParser(CUvideoparser *pObj, CUVIDPARSERPARAMS *pParams)
 */
typedef CUresult CUDAAPI tcuvidCreateVideoParser(CUvideoparser *pObj, CUVIDPARSERPARAMS *pParams);

/**
 * \fn CUresult CUDAAPI cuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET *pPacket)
 */
typedef CUresult CUDAAPI tcuvidParseVideoData(CUvideoparser obj, CUVIDSOURCEDATAPACKET *pPacket);

/**
 * \fn CUresult CUDAAPI cuvidDestroyVideoParser(CUvideoparser obj)
 */
typedef CUresult CUDAAPI tcuvidDestroyVideoParser(CUvideoparser obj);

#if !defined(__APPLE__)
extern tcuvidCreateVideoSource               *cuvidCreateVideoSource;
extern tcuvidCreateVideoSourceW              *cuvidCreateVideoSourceW;
extern tcuvidDestroyVideoSource              *cuvidDestroyVideoSource;
extern tcuvidSetVideoSourceState             *cuvidSetVideoSourceState;
extern tcuvidGetVideoSourceState             *cuvidGetVideoSourceState;
extern tcuvidGetSourceVideoFormat            *cuvidGetSourceVideoFormat;
extern tcuvidGetSourceAudioFormat            *cuvidGetSourceAudioFormat;
#endif


extern tcuvidCreateVideoParser               *cuvidCreateVideoParser;
extern tcuvidParseVideoData                  *cuvidParseVideoData;
extern tcuvidDestroyVideoParser              *cuvidDestroyVideoParser;

/** @} */  /* END VIDEO_PARSER */
////////////////////////////////////////////////////////////////////////////////////////////////

const char *cudaGetErrorEnum(CUresult error);

#ifdef __cplusplus
}
#endif

#endif // defined(WIN32) || defined(GPAC_CONFIG_LINUX)

#endif //__cuda_tools_h__
