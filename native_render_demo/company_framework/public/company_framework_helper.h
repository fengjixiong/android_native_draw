#ifndef COMPANY_FRAMEWORK_HELPER_H
#define COMPANY_FRAMEWORK_HELPER_H

#define COMPANY_FRAMEWORK_VERSION "JFV1.3"
namespace android {
    class SurfaceControl;
    class GraphicBuffer;
    class SurfaceComposerClient;
}

// 高通平台定义 android_vendor_qcom_opensource_display-intf/common/PixelFormat.h
// MTK平台定义  android_vendor_mediatek_proprietary_hardware_libhwcomposer/hwc_ui/include/hwc_ui/PixelFormat.h
enum company_pixel_format_t : int32_t {
    COMPANY_PIXEL_FORMAT_UNKNOWN    =   0,
    COMPANY_PIXEL_FORMAT_NONE       =   0,
    COMPANY_PIXEL_FORMAT_CUSTOM         = -4,
    COMPANY_PIXEL_FORMAT_TRANSLUCENT    = -3,
    COMPANY_PIXEL_FORMAT_TRANSPARENT    = -2,
    COMPANY_PIXEL_FORMAT_OPAQUE         = -1,

    COMPANY_HAL_PIXEL_FORMAT_RGBA_8888 = 1,
    COMPANY_HAL_PIXEL_FORMAT_RGBX_8888 = 2,
    COMPANY_HAL_PIXEL_FORMAT_RGB_888 = 3,
    COMPANY_HAL_PIXEL_FORMAT_RGB_565 = 4,
    COMPANY_HAL_PIXEL_FORMAT_BGRA_8888 = 5,

    COMPANY_HAL_PIXEL_FORMAT_MTK_RGBA_5551 = 6, // MTK自定义
    COMPANY_HAL_PIXEL_FORMAT_MTK_RGBA_4444 = 7, // MTK自定义

    COMPANY_HAL_PIXEL_FORMAT_YCBCR_422_SP = 16,
    COMPANY_HAL_PIXEL_FORMAT_YCRCB_420_SP = 17,
    COMPANY_HAL_PIXEL_FORMAT_YCBCR_422_I = 20,
    COMPANY_HAL_PIXEL_FORMAT_RGBA_FP16 = 22,
    COMPANY_HAL_PIXEL_FORMAT_RAW16 = 32,
    COMPANY_HAL_PIXEL_FORMAT_BLOB = 33,
    COMPANY_HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED = 34,
    COMPANY_HAL_PIXEL_FORMAT_YCBCR_420_888 = 35,
    COMPANY_HAL_PIXEL_FORMAT_RAW_OPAQUE = 36,
    COMPANY_HAL_PIXEL_FORMAT_RAW10 = 37,
    COMPANY_HAL_PIXEL_FORMAT_RAW12 = 38,
    COMPANY_HAL_PIXEL_FORMAT_RGBA_1010102 = 43, // Qcom定义, MTK一样

    COMPANY_HAL_PIXEL_FORMAT_YCBCR_P010 = 54, // Qcom定义, MTK一样

    COMPANY_HAL_PIXEL_FORMAT_MTK_R_8           = 56, // MTK自定义
    COMPANY_HAL_PIXEL_FORMAT_MTK_R_16_UINT     = 57, // MTK自定义
    COMPANY_HAL_PIXEL_FORMAT_MTK_RG_1616_UINT  = 58, // MTK自定义
    COMPANY_HAL_PIXEL_FORMAT_MTK_RGBA_10101010 = 59, // MTK自定义

    COMPANY_HAL_PIXEL_FORMAT_YCbCr_420_SP = 0x109,   // Qcom定义NV12, 265
    COMPANY_HAL_PIXEL_FORMAT_NV12         = 0x1000,  // MTK定义NV12, 4096
    COMPANY_HAL_PIXEL_FORMAT_Y8 = 538982489,
    COMPANY_HAL_PIXEL_FORMAT_Y16 = 540422489,
    COMPANY_HAL_PIXEL_FORMAT_YV12 = 842094169,
};

// 高通平台定义 android_vendor_qcom_opensource_display-intf/common/BufferUsage.h
// MTK平台定义
enum company_gralloc_usage_t : uint64_t  {
    COMPANY_GRALLOC_USAGE_SW_READ_NEVER         = 0x00000000U,
    COMPANY_GRALLOC_USAGE_SW_READ_RARELY        = 0x00000002U,
    COMPANY_GRALLOC_USAGE_SW_READ_OFTEN         = 0x00000003U,
    COMPANY_GRALLOC_USAGE_SW_READ_MASK          = 0x0000000FU,
    COMPANY_GRALLOC_USAGE_SW_WRITE_NEVER        = 0x00000000U,
    COMPANY_GRALLOC_USAGE_SW_WRITE_RARELY       = 0x00000020U,
    COMPANY_GRALLOC_USAGE_SW_WRITE_OFTEN        = 0x00000030U,
    COMPANY_GRALLOC_USAGE_SW_WRITE_MASK         = 0x000000F0U,
    COMPANY_GRALLOC_USAGE_HW_TEXTURE            = 0x00000100U,
    COMPANY_GRALLOC_USAGE_HW_RENDER             = 0x00000200U,
    COMPANY_GRALLOC_USAGE_HW_2D                 = 0x00000400U,
    COMPANY_GRALLOC_USAGE_HW_COMPOSER           = 0x00000800U,
    COMPANY_GRALLOC_USAGE_HW_FB                 = 0x00001000U,
    COMPANY_GRALLOC_USAGE_EXTERNAL_DISP         = 0x00002000U,
    COMPANY_GRALLOC_USAGE_PROTECTED             = 0x00004000U,
    COMPANY_GRALLOC_USAGE_CURSOR                = 0x00008000U,
    COMPANY_GRALLOC_USAGE_HW_VIDEO_ENCODER      = 0x00010000U,
    COMPANY_GRALLOC_USAGE_HW_CAMERA_WRITE       = 0x00020000U,
    COMPANY_GRALLOC_USAGE_HW_CAMERA_READ        = 0x00040000U,
    COMPANY_GRALLOC_USAGE_HW_CAMERA_ZSL         = 0x00060000U,
    COMPANY_GRALLOC_USAGE_HW_CAMERA_MASK        = 0x00060000U,
    COMPANY_GRALLOC_USAGE_HW_MASK               = 0x00071F00U,
    COMPANY_GRALLOC_USAGE_RENDERSCRIPT          = 0x00100000U,
    COMPANY_GRALLOC_USAGE_FOREIGN_BUFFERS       = 0x00200000U,
    COMPANY_GRALLOC_USAGE_HW_IMAGE_ENCODER      = 0x08000000U,
    COMPANY_GRALLOC_USAGE_ALLOC_MASK            = ~(COMPANY_GRALLOC_USAGE_FOREIGN_BUFFERS),
    COMPANY_GRALLOC_USAGE_PRIVATE_0             = 0x10000000U,
    COMPANY_GRALLOC_USAGE_QTI_ALLOC_UBWC        = 0x10000000U,// Qcom平台自定义
    COMPANY_GRALLOC_USAGE_PRIVATE_1             = 0x20000000U,
    COMPANY_GRALLOC_USAGE_PRIVATE_2             = 0x40000000U,
    COMPANY_GRALLOC_USAGE_PRIVATE_3             = 0x80000000U,
    COMPANY_GRALLOC_USAGE_PRIVATE_MASK          = 0xF0000000U,
} ;

#define GRALLOC_USAGE_PRIVATE_ALLOC_UBWC (UINT32_C(1) << 28)

//#include "drm_fourcc.h"
// defined in android/external/libdrm/include/drm/drm_fourcc.h
#define AFBC_FORMAT_MOD_BLOCK_SIZE_16x16     ((__u64)0x1)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8      ((__u64)0x2)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_64x4      ((__u64)0x3)
#define AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4 ((__u64)0x4)
#define AFBC_FORMAT_MOD_YTR                  (((__u64)1) <<  4)
#define AFBC_FORMAT_MOD_SPLIT                (((__u64)1) <<  5)
#define AFBC_FORMAT_MOD_SPARSE               (((__u64)1) <<  6)
#define AFBC_FORMAT_MOD_CBR                  (((__u64)1) <<  7)
#define AFBC_FORMAT_MOD_TILED                (((__u64)1) <<  8)
#define AFBC_FORMAT_MOD_SC                   (((__u64)1) <<  9)
#define AFBC_FORMAT_MOD_DB                   (((__u64)1) << 10)
#define AFBC_FORMAT_MOD_BCH                  (((__u64)1) << 11)
#define AFBC_FORMAT_MOD_USM                  (((__u64)1) << 12)

namespace company {

using surface_handle = int;
using buffer_handle  = int;

enum : int {
    INVALID_HANDLE = -1,
};

typedef struct CompanyYCbCr {
    uint8_t* y;
    uint8_t* cb;
    uint8_t* cr;
    size_t ystride;
    size_t cstride;
    size_t chroma_step;

} CompanyYCbCr_t;

typedef struct CompanyRect {
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
} CompanyRect_t;

typedef struct CompanyPlaneLayout {
    int32_t offsetInBytes;
    int32_t totalSizeInBytes;
    int32_t strideInBytes;
    int32_t alignedHeight;
    int32_t sampleIncrementInBits;
} CompanyPlaneLayout_t;

class CompanyFrameworkHelper final {
public:
    CompanyFrameworkHelper(const CompanyFrameworkHelper&) = delete;
    CompanyFrameworkHelper& operator=(const CompanyFrameworkHelper&) = delete;

    static CompanyFrameworkHelper *GetInstance();
    CompanyFrameworkHelper();
    ~CompanyFrameworkHelper();

    surface_handle SurfaceCreate(const char* name, int width, int height,
        company_pixel_format_t pixel_format, int flags, int x, int y, int z_order, const char* matrix=NULL);
    int SurfaceDelete(surface_handle surface);
    int SurfaceSetBuffer(surface_handle surface, buffer_handle buffer);

    buffer_handle BufferCreate(const char* name, int width, int height,
        company_pixel_format_t pixel_format, uint64_t usage,
        void* pixels=NULL, int gpu_buffer=0); // GPU buffer最好创建时就把pixel放进去
    int BufferDelete(buffer_handle buffer);
    void* BufferLock(buffer_handle buffer);
    int BufferLockYCbCr(buffer_handle buffer, uint64_t usage, CompanyRect_t rect, CompanyYCbCr_t* ycbcr);
    int BufferUnlock(buffer_handle buffer);
    int BufferSetAvailable(uint64_t buffer_id);

    int BufferAvailable(buffer_handle buffer);
    int BufferGetFd(buffer_handle buffer, int platform=0);
    int BufferGetWidth(buffer_handle buffer);
    int BufferGetHeight(buffer_handle buffer);
    int BufferGetStride(buffer_handle buffer);
    int BufferGetFormat(buffer_handle buffer);
    uint64_t BufferGetUsage(buffer_handle buffer);
    int BufferGetTotalSize(buffer_handle buffer);
    uint64_t BufferGetId(buffer_handle buffer);
    uint64_t BufferGetModifier(buffer_handle buffer);
    uint32_t BufferGetFourCC(buffer_handle buffer);
    int32_t BufferGetLayouts(buffer_handle buffer, CompanyPlaneLayout** layouts, int* num_layout);
    int BufferCompareSame(buffer_handle buffer1, buffer_handle buffer2);
    void BufferInfo(buffer_handle buffer, int platform);

private:
    buffer_handle BufferCreateGPU(const char* name, int width, int height,
        company_pixel_format_t pixel_format, uint64_t usage, void* pixels);
    int BufferDeleteGPU(buffer_handle buffer);
private:
    class Impl;         // 内部实现
    Impl* mImpl;        // 指向真正的实现
};

} // namespace company

#endif
