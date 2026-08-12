// SHIM: NVIDIA NVAPI
//
// Replaces:  nvapi.h from the NVIDIA NVAPI SDK.
// Why:       NVAPI is free to download but ships under NVIDIA's SDK licence agreement,
//            not a permissive one, and no redistributable source exists. It is
//            therefore out of scope for this port on licensing grounds alone --
//            see ../../../DEPENDENCIES.md.
// Status:    NO-OP. There is no SLI detection and no 3D Vision stereo.
//
// What is actually lost: NVAPI is used in exactly one file, r3dRender.CPP, for two
// NVIDIA-only features -- querying SLI state (to report GPU count) and driving 3D
// Vision stereo rendering. Both are optional paths the renderer already handles being
// without: NvAPI_Initialize() failing sets NVApiActive = 0 and every stereo call site
// is guarded by that or by a null NVApiStereoHandle. 3D Vision itself was discontinued
// by NVIDIA in 2019, so this is dead capability regardless of the port.
//
// Contract (see ../README.md): declare what is referenced, fail honestly, never fake
// success. NvAPI_Initialize returns NVAPI_ERROR, so the renderer takes its "NVApi not
// available" path on the first call and nothing further engages.
//
// Clean-room declarations derived from the call sites in
// src/Eternity/Source/r3dRender.CPP. No code originates from the NVAPI SDK.

#ifndef __WARZ_COMPAT_NVAPI_H
#define __WARZ_COMPAT_NVAPI_H

// ---------------------------------------------------------------------------
// Scalar types and handles
// ---------------------------------------------------------------------------

typedef unsigned char  NvU8;
typedef unsigned int   NvU32;

// The real SDK uses opaque struct pointers. void* keeps the null checks in
// r3dRender.CPP working unchanged (r3dRender.h stores the stereo handle as void*).
typedef void*          NvDisplayHandle;
typedef void*          NvPhysicalGpuHandle;
typedef void*          StereoHandle;

// NvAPI_GetErrorMessage writes into one of these.
typedef char           NvAPI_ShortString[64];

#define NVAPI_MAX_PHYSICAL_GPUS 64

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------

typedef enum
{
    NVAPI_OK    =  0,
    NVAPI_ERROR = -1,
} NvAPI_Status;

// ---------------------------------------------------------------------------
// Stereo (3D Vision)
// ---------------------------------------------------------------------------

typedef enum
{
    NVAPI_STEREO_EYE_RIGHT = 0,
    NVAPI_STEREO_EYE_LEFT  = 1,
    NVAPI_STEREO_EYE_MONO  = 2,
} NV_STEREO_ACTIVE_EYE;

typedef enum
{
    NVAPI_STEREO_DRIVER_MODE_AUTOMATIC = 0,
    NVAPI_STEREO_DRIVER_MODE_DIRECT    = 2,
} NV_STEREO_DRIVER_MODE;

typedef enum
{
    NVAPI_STEREO_SURFACECREATEMODE_AUTO      = 0,
    NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO= 1,
    NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO = 2,
} NVAPI_STEREO_SURFACECREATEMODE;

// ---------------------------------------------------------------------------
// SLI
// ---------------------------------------------------------------------------

#define NV_GET_CURRENT_SLI_STATE_VER 1

typedef struct
{
    NvU32 version;
    NvU32 maxNumAFRGroups;
    NvU32 numAFRGroups;
    NvU32 currentAFRIndex;
    NvU32 nextFrameAFRIndex;
    NvU32 previousFrameAFRIndex;
    NvU32 bIsCurAFRGroupNew;
} NV_GET_CURRENT_SLI_STATE;

// ---------------------------------------------------------------------------
// Display enumeration
// ---------------------------------------------------------------------------

#define NV_GPU_DISPLAYIDS_VER 1

typedef enum
{
    NV_MONITOR_CONN_TYPE_UNINITIALIZED = 0,
    NV_MONITOR_CONN_TYPE_VGA,
    NV_MONITOR_CONN_TYPE_COMPONENT,
    NV_MONITOR_CONN_TYPE_SVIDEO,
    NV_MONITOR_CONN_TYPE_HDMI,
    NV_MONITOR_CONN_TYPE_DVI,
    NV_MONITOR_CONN_TYPE_LVDS,
    NV_MONITOR_CONN_TYPE_DP,
    NV_MONITOR_CONN_TYPE_COMPOSITE,
    NV_MONITOR_CONN_TYPE_UNKNOWN = -1,
} NV_MONITOR_CONN_TYPE;

typedef struct
{
    NvU32                version;
    NV_MONITOR_CONN_TYPE connectorType;
    NvU32                displayId;
    NvU32                isDynamic;
} NV_GPU_DISPLAYIDS;

// ---------------------------------------------------------------------------
// Entry points -- all inert. Definitions are inline so the shim needs no
// translation unit of its own and leaves no unresolved symbols at link time.
// ---------------------------------------------------------------------------

inline NvAPI_Status NvAPI_Initialize()                                    { return NVAPI_ERROR; }

inline void NvAPI_GetErrorMessage(NvAPI_Status /*status*/, NvAPI_ShortString msg)
{
    if (msg)
    {
        // Keep it self-describing: this string reaches the log verbatim.
        const char* s = "NVAPI is not available in this build";
        int i = 0;
        for (; s[i] && i < 63; ++i) msg[i] = s[i];
        msg[i] = '\0';
    }
}

// Display / GPU enumeration
inline NvAPI_Status NvAPI_GetAssociatedNvidiaDisplayHandle(const char* /*szDisplayName*/,
                                                           NvDisplayHandle* pNvDispHandle)
{ if (pNvDispHandle) *pNvDispHandle = 0; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_GetPhysicalGPUsFromDisplay(NvDisplayHandle /*hNvDisp*/,
                                                     NvPhysicalGpuHandle* /*handles*/,
                                                     NvU32* pGpuCount)
{ if (pGpuCount) *pGpuCount = 0; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_GPU_GetConnectedDisplayIds(NvPhysicalGpuHandle /*hPhysicalGpu*/,
                                                     NV_GPU_DISPLAYIDS* /*pDisplayIds*/,
                                                     NvU32* pDisplayIdCount,
                                                     NvU32 /*flags*/)
{ if (pDisplayIdCount) *pDisplayIdCount = 0; return NVAPI_ERROR; }

// SLI
inline NvAPI_Status NvAPI_D3D_GetCurrentSLIState(void* /*pDevice*/,
                                                 NV_GET_CURRENT_SLI_STATE* /*pSliState*/)
{ return NVAPI_ERROR; }

// Stereo
inline NvAPI_Status NvAPI_Stereo_IsEnabled(NvU8* pIsStereoEnabled)
{ if (pIsStereoEnabled) *pIsStereoEnabled = 0; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_IsWindowedModeSupported(NvU8* pSupported)
{ if (pSupported) *pSupported = 0; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_SetDriverMode(NV_STEREO_DRIVER_MODE /*mode*/)
{ return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_CreateHandleFromIUnknown(void* /*pDevice*/,
                                                          StereoHandle* pStereoHandle)
{ if (pStereoHandle) *pStereoHandle = 0; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_DestroyHandle(StereoHandle /*stereoHandle*/)
{ return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_Activate(StereoHandle /*stereoHandle*/)
{ return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_Deactivate(StereoHandle /*stereoHandle*/)
{ return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_SetActiveEye(StereoHandle /*stereoHandle*/,
                                              NV_STEREO_ACTIVE_EYE /*eye*/)
{ return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_SetSurfaceCreationMode(StereoHandle /*stereoHandle*/,
                                                        NVAPI_STEREO_SURFACECREATEMODE /*mode*/)
{ return NVAPI_ERROR; }

// The renderer feeds these straight into console vars, so leave the outputs at a
// defined value rather than untouched.
inline NvAPI_Status NvAPI_Stereo_GetSeparation(StereoHandle /*stereoHandle*/,
                                               float* pSeparationPercentage)
{ if (pSeparationPercentage) *pSeparationPercentage = 0.0f; return NVAPI_ERROR; }

inline NvAPI_Status NvAPI_Stereo_GetConvergence(StereoHandle /*stereoHandle*/,
                                                float* pConvergence)
{ if (pConvergence) *pConvergence = 0.0f; return NVAPI_ERROR; }

#endif // __WARZ_COMPAT_NVAPI_H
