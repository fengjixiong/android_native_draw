#define LOG_TAG "CompanyFramework"
#include <utils/Log.h>
#include <utils/RefBase.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <mutex>
#include <map>
#include <android/native_window.h>
#include <binder/ProcessState.h>
#include <binder/IPCThreadState.h>

#include "company_framework_helper.h"
#include "company_gpu_helper.h"

using namespace android;
namespace company {

// ===================== 真正的内部实现：全部放进 Impl =====================
class CompanyFrameworkHelper::Impl {
public:
    sp<SurfaceComposerClient> composerClient;
    std::mutex mutex;
    std::map<surface_handle, sp<SurfaceControl>> surfaceMap;
    std::map<buffer_handle,  sp<GraphicBuffer>>  bufferMap;
    std::map<buffer_handle,  int>                bufferLockMap;
    std::map<buffer_handle,  int>                bufferAvailableMap;
    std::map<uint64_t, buffer_handle>            bufferIdMap;
    std::map<buffer_handle, uint64_t>            bufferIdMapInvert;
    std::map<buffer_handle,  int>                bufferIsGPU;
    std::map<buffer_handle,  sp<CompanyGPUHelper>>  bufferGPUHelper;
    int nextHandle = 1;
    sp<ProcessState> binderProc = nullptr;

    Impl() {
        ALOGD("Impl create");
        // Binder 系统初始化
        binderProc = ProcessState::self();
        binderProc->startThreadPool();  // 启动 Binder 接收线程池

        composerClient = new SurfaceComposerClient();
        status_t err = composerClient->initCheck();
        if (err != NO_ERROR) {
            ALOGE("SurfaceComposerClient init failed: %d", err);
        }
    }

    ~Impl() {
        ALOGD("Impl dispose");
        std::lock_guard<std::mutex> lock(mutex);
        surfaceMap.clear();
        bufferMap.clear();
        bufferIsGPU.clear();
        bufferGPUHelper.clear();
        // composerClient->dispose();
        // IPCThreadState::self()->joinThreadPool(false);
    }

    int allocHandle() {
        std::lock_guard<std::mutex> lock(mutex);
        return nextHandle++;
    }
};

static CompanyFrameworkHelper instance;
CompanyFrameworkHelper* CompanyFrameworkHelper::GetInstance() {
    return &instance;
}

// ===================== 构造 / 析构 =====================
CompanyFrameworkHelper::CompanyFrameworkHelper()
{
    ALOGD("CompanyFrameworkHelper create: %s", COMPANY_FRAMEWORK_VERSION);
    mImpl = new Impl();
}

CompanyFrameworkHelper::~CompanyFrameworkHelper() {
    if (mImpl)
        delete mImpl;
    ALOGD("CompanyFrameworkHelper dispose");
}

// ===================== Surface =====================
surface_handle CompanyFrameworkHelper::SurfaceCreate(
    const char* name, int width, int height,
    company_pixel_format_t pixel_format, int flags, int x, int y, int z_order, const char* matrix) {
    ALOGD("%s enter", __func__);
    if (!name || width <=0 || height <=0 || !mImpl->composerClient) {
        ALOGE("%s parameter error", __func__);
        return INVALID_HANDLE;
    }

    sp<SurfaceControl> sc = mImpl->composerClient->createSurface(String8(name), width, height, pixel_format, flags);
    if (!sc || !sc->isValid())  {
        ALOGE("%s createSurface error", __func__);
        return INVALID_HANDLE;
    }

    SurfaceComposerClient::Transaction t;
    t.setLayer(sc, z_order);
    // t.show(sc); // 刚开始不显示
    t.setPosition(sc, x, y); // 设置在屏幕上的位置
    // 设置matrix
    if (matrix != NULL) {
        // 旋转还无法使用
        ALOGD("%s matrix: %s", __func__, matrix);
        // 变换类型    矩阵值 (dsdx, dtdx, dsdy, dtdy)    X轴新方向   Y轴新方向   效果
        // 恒等（无变换） (1, 0, 0, 1)    → 右 ↓ 下 不变
        // 顺时针90°  (0, 1, -1, 0)   ↓ 下 ← 左 向右倒
        // 逆时针90°  (0, -1, 1, 0)   ↑ 上 → 右 向左倒
        // 180°旋转   (-1, 0, 0, -1)  ← 左 ↑ 上 上下颠倒
        // 水平翻转   (-1, 0, 0, 1)   ← 左 ↓ 下 镜像
        // 垂直翻转   (1, 0, 0, -1)   → 右 ↑ 上 倒影
        // 缩放2倍    (2, 0, 0, 2)    → 右(2倍长)    ↓ 下(2倍长)    放大
        float dsdx = 1.0f;
        float dtdx = 0.0f;
        float dsdy = 0.0f;
        float dtdy = 1.0f;
        if (sscanf(matrix, "%f,%f,%f,%f", &dsdx, &dtdx, &dsdy, &dtdy) == 4) {
            ALOGD("%s matrix: (%.1f, %.1f, %.1f, %1.f)", __func__, dsdx, dtdx, dsdy, dtdy);
            t.setMatrix(sc, dsdx, dtdx, dsdy, dtdy);
        }
    }
    t.apply();

    int handle = mImpl->allocHandle();
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    mImpl->surfaceMap[handle] = sc;
    return handle;
}

int CompanyFrameworkHelper::SurfaceDelete(surface_handle surface) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->surfaceMap.find(surface);
    if (it == mImpl->surfaceMap.end()) return -1;

    SurfaceComposerClient::Transaction t;
    t.hide(it->second);
    t.apply();
    mImpl->surfaceMap.erase(it);
    return 0;
}

void releaseBufferCallback(const ReleaseCallbackId& id, const sp<Fence>& releaseFence, std::optional<uint32_t> count) {
    ALOGD("%s enter", __func__);
    // 这个回调在 buffer 可重用时被调用
    // releaseFence 需要等待，确保硬件已完成访问
    if (releaseFence && releaseFence->isValid()) {
        releaseFence->waitForever("BufferRelease");
    }
    (void)count;
    CompanyFrameworkHelper* framework = CompanyFrameworkHelper::GetInstance();
    framework->BufferSetAvailable(id.bufferId);

}

int CompanyFrameworkHelper::SurfaceSetBuffer(surface_handle surface, buffer_handle buffer) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto s = mImpl->surfaceMap.find(surface);
    auto b = mImpl->bufferMap.find(buffer);
    if (s == mImpl->surfaceMap.end() || b == mImpl->bufferMap.end())
        return -1;

    mImpl->bufferAvailableMap[buffer] = 0;
    SurfaceComposerClient::Transaction t;
    //t.setBuffer(s->second, b->second, Fence::NO_FENCE);
    t.setBuffer(s->second, b->second, Fence::NO_FENCE,
                std::nullopt, 0,  // optFrameNumber, producerId
                releaseBufferCallback);

    t.show(s->second);
    t.apply();

    return 0;
}

// ===================== Buffer =====================
buffer_handle CompanyFrameworkHelper::BufferCreate(
    const char* name, int width, int height,
    company_pixel_format_t pixel_format, uint64_t usage,
    void* pixels, int gpu_buffer) {

    if (gpu_buffer != 0)
        return BufferCreateGPU(name, width, height, pixel_format, usage, pixels);

    ALOGD("%s enter", __func__);
    sp<GraphicBuffer> gb = new GraphicBuffer(width, height, pixel_format, usage);
    if (gb->initCheck() != NO_ERROR) {
        ALOGE("%s GraphicBuffer initCheck error", __func__);
        return INVALID_HANDLE;
    }

    uint64_t buffer_id = gb->getId();
    int handle = mImpl->allocHandle();
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    mImpl->bufferMap[handle]          = gb;
    mImpl->bufferLockMap[handle]      = 0;
    mImpl->bufferAvailableMap[handle] = 1;
    mImpl->bufferIdMap[buffer_id]     = handle;
    mImpl->bufferIdMapInvert[handle]  = buffer_id;
    mImpl->bufferIsGPU[handle]        = 0;
    ALOGD("%s buffer handle=%d, id=0x%" PRIx64, __func__, handle, buffer_id);
    return handle;
}

int CompanyFrameworkHelper::BufferDelete(buffer_handle buffer) {
    ALOGD("%s enter", __func__);
    if (BufferDeleteGPU(buffer) == 0)
        return 0;

    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end()) {
        ALOGE("%s No such buffer [%d]", __func__, buffer);
        return -1;
    }
    if (mImpl->bufferLockMap[buffer] == 1) {
        ALOGI("%s warning: buffer[%d] locked!", __func__, buffer);
        mImpl->bufferMap[buffer]->unlock();
        mImpl->bufferLockMap[buffer] = 0;
    }
    uint64_t buffer_id = mImpl->bufferIdMapInvert[buffer];
    mImpl->bufferMap.erase(buffer);
    mImpl->bufferLockMap.erase(buffer);
    mImpl->bufferAvailableMap.erase(buffer);
    mImpl->bufferIdMap.erase(buffer_id);
    mImpl->bufferIdMapInvert.erase(buffer);
    mImpl->bufferIsGPU.erase(buffer);
    return 0;
}

void* CompanyFrameworkHelper::BufferLock(buffer_handle buffer) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end()) {
        ALOGE("%s No such buffer [%d]", __func__, buffer);
        return NULL;
    }
    if (mImpl->bufferLockMap[buffer] == 1) {
        ALOGE("%s buffer already locked!: %d", __func__, buffer);
        return NULL;
    }
    sp<GraphicBuffer> gb = it->second;
    void* vaddr;
    int ret = gb->lock(GraphicBuffer::USAGE_SW_WRITE_OFTEN, &vaddr);
    if (ret != NO_ERROR) {
        ALOGE("%s GraphicBuffer lock error: %d", __func__, ret);
        return NULL;
    }
    mImpl->bufferLockMap[buffer] = 1;
    return vaddr;
}

int CompanyFrameworkHelper::BufferLockYCbCr(buffer_handle buffer, uint64_t usage,
        CompanyRect_t jrect, CompanyYCbCr_t* result) {
    ALOGD("%s enter", __func__);
    (void)usage;
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end()) {
        ALOGE("%s No such buffer [%d]", __func__, buffer);
        return -1;
    }
    if (mImpl->bufferLockMap[buffer] == 1) {
        ALOGE("%s buffer already locked!: %d", __func__, buffer);
        return -1;
    }
    sp<GraphicBuffer> gb = it->second;
    android_ycbcr ycbcr {};
    Rect rect(jrect.width, jrect.height);
    status_t err = gb->lockYCbCr(GRALLOC_USAGE_SW_WRITE_OFTEN, rect, &ycbcr);
    if (err != NO_ERROR) {
        ALOGE("%s GraphicBuffer lockycbcr error: %d", __func__, err);
        return -1;
    }
    mImpl->bufferLockMap[buffer] = 1;

    // 更新输出
    if (result != NULL) {
        result->y           = static_cast<uint8_t*>(ycbcr.y);
        result->cb          = static_cast<uint8_t*>(ycbcr.cb);
        result->cr          = static_cast<uint8_t*>(ycbcr.cr);
        result->ystride     = ycbcr.ystride;
        result->cstride     = ycbcr.cstride;
        result->chroma_step = ycbcr.chroma_step;
    }

    return 0;
}

int CompanyFrameworkHelper::BufferUnlock(buffer_handle buffer) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end()) {
        ALOGE("%s No such buffer [%d]", __func__, buffer);
        return -1;
    }
    if (mImpl->bufferLockMap[buffer] == 1) {
        it->second->unlock();
        mImpl->bufferLockMap[buffer] = 0;
    } else {
        ALOGI("%s unlock a non-lock buffer!", __func__);
    }
    return 0;
}

int CompanyFrameworkHelper::BufferSetAvailable(uint64_t buffer_id) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferIdMap.find(buffer_id);
    if (it == mImpl->bufferIdMap.end()) {
        ALOGE("%s No such buffer_id [0x%" PRIx64 "]", __func__, buffer_id);
        return -1;
    }
    buffer_handle buffer = mImpl->bufferIdMap[buffer_id];
    mImpl->bufferAvailableMap[buffer] = 1;
    return 0;
}

// ===================== 属性获取 =====================
int CompanyFrameworkHelper::BufferAvailable(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferAvailableMap.find(buffer);
    return (it != mImpl->bufferAvailableMap.end()) ? it->second : 0;
}

int CompanyFrameworkHelper::BufferGetFd(buffer_handle buffer, int platform) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return -1;

    struct ANativeWindowBuffer* native_buffer = it->second.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;
    int version = buffer_handle->version;
    int numFds  = buffer_handle->numFds;
    int numInts = buffer_handle->numInts;
    if (platform >= numFds + numInts) {
        ALOGE("%s platform(%d) exceeds handle->data[%d+%d]!", __func__, platform, numFds, numInts);
        return -1;
    } else {
        ALOGD("%s platform (%d), version (%d), numFds (%d), numInts (%d)",
            __func__, platform, version, numFds, numInts);
    }
    int fd = buffer_handle->data[platform];
    // 不同平台对 buffer_handle 的实现不一样
    // 0 - 高通, gralloc_priv.h
    // 1 - MTK, gralloc_extra.h
    // 3 - goldfish, gralloc_cb_bp.h
    return fd;
}

int CompanyFrameworkHelper::BufferGetWidth(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getWidth() : -1;
}

int CompanyFrameworkHelper::BufferGetHeight(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getHeight() : -1;
}

int CompanyFrameworkHelper::BufferGetStride(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getStride() : -1;
}

int CompanyFrameworkHelper::BufferGetFormat(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getPixelFormat() : 0;
}

uint64_t CompanyFrameworkHelper::BufferGetUsage(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getUsage() : 0;
}

int CompanyFrameworkHelper::BufferGetTotalSize(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return -1;

    struct ANativeWindowBuffer* native_buffer = it->second.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;

    uint64_t totalSize = 0;
    GraphicBufferMapper::get().getAllocationSize(buffer_handle, &totalSize);
    return totalSize;
}

uint64_t CompanyFrameworkHelper::BufferGetModifier(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return 0;

    struct ANativeWindowBuffer* native_buffer = it->second.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;

    uint64_t modifiers = 0;
    GraphicBufferMapper::get().getPixelFormatModifier(buffer_handle, &modifiers);
    return modifiers;
}

uint32_t CompanyFrameworkHelper::BufferGetFourCC(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return 0;

    struct ANativeWindowBuffer* native_buffer = it->second.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;

    uint32_t fourCC = 0;
    GraphicBufferMapper::get().getPixelFormatFourCC(buffer_handle, &fourCC);
    return fourCC;
}

int32_t CompanyFrameworkHelper::BufferGetLayouts(buffer_handle buffer, CompanyPlaneLayout** out_layouts, int* num_layout) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return -1;
    if (out_layouts == NULL || num_layout == NULL) {
        ALOGE("%s layouts(0x%p) or num_layout(0x%p) is NULL", __func__, out_layouts, num_layout);
        return -1;
    }
    struct ANativeWindowBuffer* native_buffer = it->second.get();
    //转成Android标准的handle
    const native_handle_t *buffer_handle = native_buffer->handle;

    std::vector<android::ui::PlaneLayout> layouts;
    GraphicBufferMapper::get().getPlaneLayouts(buffer_handle, &layouts);

    *num_layout = layouts.size();
    if (*out_layouts != NULL) {
        free(*out_layouts);
    }
    CompanyPlaneLayout* p_layout = (CompanyPlaneLayout*) malloc(layouts.size() * sizeof(CompanyPlaneLayout));
    *out_layouts = p_layout;
    for (int i = 0; i < layouts.size(); i++) {
        const auto& layout = layouts[i];
        int alignedHeight = (int)layout.totalSizeInBytes / (int)layout.strideInBytes;
        p_layout->offsetInBytes         = (int)layout.offsetInBytes,
        p_layout->totalSizeInBytes      = (int)layout.totalSizeInBytes,
        p_layout->strideInBytes         = (int)layout.strideInBytes,
        p_layout->alignedHeight         = (int)alignedHeight,
        p_layout->sampleIncrementInBits = (int)layout.sampleIncrementInBits;
        p_layout += sizeof(CompanyPlaneLayout);
    }
    return 0;
}

uint64_t CompanyFrameworkHelper::BufferGetId(buffer_handle buffer) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    return (it != mImpl->bufferMap.end()) ? it->second->getId() : 0;
}

int CompanyFrameworkHelper::BufferCompareSame(
    buffer_handle buffer1, buffer_handle buffer2
) {
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto a = mImpl->bufferMap.find(buffer1);
    auto b = mImpl->bufferMap.find(buffer2);
    if (a == mImpl->bufferMap.end() || b == mImpl->bufferMap.end()) return 0;
    return (a->second == b->second) ? 1 : 0;
}

buffer_handle CompanyFrameworkHelper::BufferCreateGPU(
    const char* name, int width, int height,
    company_pixel_format_t pixel_format, uint64_t usage, void* pixels) {

    (void)name;
    (void)usage;
    sp<CompanyGPUHelper> bootAnimation = new CompanyGPUHelper();
    sp<GraphicBuffer> gb = bootAnimation->createGPUBuffer(pixels, width, height, pixel_format);

    uint64_t buffer_id = gb->getId();
    int handle = mImpl->allocHandle();
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    mImpl->bufferMap[handle]     = gb;
    mImpl->bufferLockMap[handle] = 0;
    mImpl->bufferAvailableMap[handle] = 1;
    mImpl->bufferIdMap[buffer_id] = handle;
    mImpl->bufferIsGPU[handle] = 1;
    mImpl->bufferGPUHelper[handle] = bootAnimation;
    ALOGD("%s buffer handle=%d, id=0x%" PRIx64, __func__, handle, buffer_id);
    return handle;
}

int CompanyFrameworkHelper::BufferDeleteGPU(buffer_handle buffer) {
    ALOGD("%s enter", __func__);

    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it_factory = mImpl->bufferGPUHelper.find(buffer);
    if (it_factory == mImpl->bufferGPUHelper.end()) {
        return -1; // 不是GPU-buffer
    }

    if (mImpl->bufferLockMap[buffer] == 1) {
        ALOGI("%s warning: buffer[%d] locked!", __func__, buffer);
        mImpl->bufferMap[buffer]->unlock();
        mImpl->bufferLockMap[buffer] = 0;
    }
    uint64_t buffer_id = mImpl->bufferIdMapInvert[buffer];
    mImpl->bufferMap.erase(buffer);
    mImpl->bufferLockMap.erase(buffer);
    mImpl->bufferAvailableMap.erase(buffer);
    mImpl->bufferIdMap.erase(buffer_id);
    mImpl->bufferIdMapInvert.erase(buffer);
    mImpl->bufferIsGPU.erase(buffer);
    mImpl->bufferGPUHelper.erase(buffer);

    return 0;
}

void ShowBufferInfo(sp<GraphicBuffer> buffer, int platform=0) {
    uint32_t stride = buffer->getStride();
    struct ANativeWindowBuffer* native_buffer = buffer.get();
    //转成Android标准的handle
    const native_handle_t *handle = native_buffer->handle;

    GraphicBufferMapper& mapper = GraphicBufferMapper::get();

    // 获取 PlaneLayout
    std::vector<android::ui::PlaneLayout> layouts;
    mapper.getPlaneLayouts(handle, &layouts);

    ALOGD("%s === Buffer Details ===", __func__);
    ALOGD("%s Plane count: %d", __func__, (int)layouts.size());

    for (int i = 0; i < layouts.size(); i++) {
        const auto& layout = layouts[i];
        int alignedHeight = (int)layout.totalSizeInBytes / (int)layout.strideInBytes;

        ALOGD("%s     Plane[%d] offset=%d, size=%d, h-stride=%d, v-stride=%d, sampleInc=%d", __func__,i,
            (int)layout.offsetInBytes,
            (int)layout.totalSizeInBytes,
            (int)layout.strideInBytes,
            (int)alignedHeight,
            (int)layout.sampleIncrementInBits);

        for (int j = 0; j < layout.components.size(); j++) {
            const auto& component = layout.components[j];
            ALOGD("%s           Component[%d] Type: 0x%x, Offset: 0x%x bits, Size: 0x%x bits",
                __func__, j,
                (int)component.type.value,
                (int)component.offsetInBits,
                (int)component.sizeInBits);
        }
    }

    // For MTK
    {
        int isCompress = 0;
        bool isQcom = (platform == 0);
        uint64_t w, h;
        uint64_t Modifier;
        uint32_t FourCC;

        uint64_t usage;
        uint64_t size;
        ui::PixelFormat f;

        mapper.getWidth(handle, &w);
        mapper.getHeight(handle, &h);
        mapper.getPixelFormatFourCC(handle, &FourCC);

        mapper.getPixelFormatModifier(handle, &Modifier);
        mapper.getUsage(handle, &usage);
        mapper.getAllocationSize(handle, &size);
        mapper.getPixelFormatRequested(handle, &f);

        ALOGD("--------Allocate info----------");
        ALOGD("w = \t\t:%lld",       (long long)w);
        ALOGD("h = \t\t:%lld",       (long long)h);
        ALOGD("stride = \t:%lld",    (long long)stride);
        // 对pixel排列更精细化配置，见drm_fourcc.h
        ALOGD("FourCC     = \t:0x%x (%c%c%c%c)",
            (int)FourCC, FourCC&0xFF, (FourCC>>8)&0xFF, (FourCC>>16)&0xFF, (FourCC>>24)&0xFF);
        ALOGD("usage      = \t:0x%llx", (long long)usage);
        ALOGD("Alloc size = \t:%lld",   (long long)size);
        //ALOGD("isCompress = \t:%d",     (int)isCompress);
        ALOGD("format     = \t:%d",     (int)f);
        ALOGD("Modifier = \t:0x%llx",  (long long)Modifier);
        if (isQcom == 0) {
            if( (Modifier & 0xFF00000000000000) == 0x0800000000000000) {
                ALOGD("ARM Modifier");
                char* algo = "Unknown";
                if((Modifier & 0xF0000000000000) == 0x0) {
                    algo = "ARM AFBC";
                } else if((Modifier & 0xF0000000000000) == 0x10000000000000) {
                    algo = "ARM AFRC";
                }
                ALOGD("\t ----%s", algo);

                // bit3 - bit0
                if((Modifier & 0xF) == AFBC_FORMAT_MOD_BLOCK_SIZE_16x16) {
                    ALOGD("\t\t-----%s 16x16", algo);
                    isCompress = 1;
                } else if((Modifier & 0xF) == AFBC_FORMAT_MOD_BLOCK_SIZE_32x8) {
                    ALOGD("\t\t-----%s 32x8", algo);
                    isCompress = 1;
                } else if((Modifier & 0xF) == AFBC_FORMAT_MOD_BLOCK_SIZE_32x8_64x4) {
                    ALOGD("\t\t-----%s 32x8_64x4", algo);
                    isCompress = 1;
                } else if((Modifier & 0xF) == AFBC_FORMAT_MOD_BLOCK_SIZE_64x4) {
                    ALOGD("\t\t-----%s 64x4", algo);
                    isCompress = 1;
                }

                if((Modifier & AFBC_FORMAT_MOD_YTR)) { // bit4
                    ALOGD("\t\t-----Enable  YUV transform");
                } else {
                    ALOGD("\t\t-----Disable YUV transform");
                }
                if((Modifier & AFBC_FORMAT_MOD_SPLIT)) { // bit5
                    ALOGD("\t\t-----Enable  block-split");
                } else {
                    ALOGD("\t\t-----Disable block-split");
                }
                if((Modifier & AFBC_FORMAT_MOD_SPARSE)) { // bit6
                    ALOGD("\t\t-----Enable  sparse layout");
                } else {
                    ALOGD("\t\t-----Disable sparse layout");
                }
                if((Modifier & AFBC_FORMAT_MOD_CBR)) { // bit7
                    ALOGD("\t\t-----Enable  copy-block restriction");
                } else {
                    ALOGD("\t\t-----Disable copy-block restriction");
                }
                if((Modifier & AFBC_FORMAT_MOD_TILED)) { // bit8
                    ALOGD("\t\t-----Enable  tiled layout");
                } else {
                    ALOGD("\t\t-----Disable tiled layout");
                }
                if((Modifier & AFBC_FORMAT_MOD_SC)) { // bit9
                    ALOGD("\t\t-----Enable  solid color blocks");
                } else {
                    ALOGD("\t\t-----Disable solid color blocks");
                }
                if((Modifier & AFBC_FORMAT_MOD_DB)) { // bit10
                    ALOGD("\t\t-----Enable  double-buffer");
                } else {
                    ALOGD("\t\t-----Disable double-buffer");
                }
                if((Modifier & AFBC_FORMAT_MOD_BCH)) { // bit11
                    ALOGD("\t\t-----Enable  buffer content hints");
                } else {
                    ALOGD("\t\t-----Disable buffer content hints");
                }
                if((Modifier & AFBC_FORMAT_MOD_USM)) { // bit12
                    ALOGD("\t\t-----Enable  uncompressed storage mode");
                } else {
                    ALOGD("\t\t-----Disable uncompressed storage mode");
                }
            }
        }
    }
}

void CompanyFrameworkHelper::BufferInfo(buffer_handle buffer, int platform=0) {
    ALOGD("%s enter", __func__);
    std::lock_guard<std::mutex> lock(mImpl->mutex);
    auto it = mImpl->bufferMap.find(buffer);
    if (it == mImpl->bufferMap.end())
        return;

    sp<GraphicBuffer> buffer_ptr = it->second;
    ShowBufferInfo(buffer_ptr, platform);
}

} // namespace company
