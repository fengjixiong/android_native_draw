/*
 * 2025.10.30 created by jxfeng
 * 2025.10.31 add multi-layer -- multilayer 有问题，还是单layer
 */

#include "advanced_surface_manager.h"
#define __CLASS__ "(main)"

// 全局变量
using namespace android;
int mLayerStack = 0;

void fillRGBA8Buffer(uint8_t* img, int width, int height, int stride, int r, int g, int b) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            uint8_t* pixel = img + (4 * (y*stride + x));
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
            pixel[3] = 0;
        }
    }
}

bool mQuit = false;
sp<AdvancedSurfaceManager> gSurfaceManager;

void sighandler(int num) {
    if (num == SIGINT) {
        JLOGI("SIGINT received, stopping...");
        mQuit = true;
        if (gSurfaceManager != nullptr) {
            gSurfaceManager->stopRendering();
        }
    }
}

std::vector<sp<IGraphicBufferProducer>> igbProducers;
std::vector<sp<BLASTBufferQueue>> mBlastBufferQueues;
std::vector<sp<SurfaceControl>> mSurfaceControls;

ui::Size resolution;
int init_color() {

    // 建立 App 到 SurfaceFlinger 的 Binder 通信通道
    sp<SurfaceComposerClient> surfaceComposerClient = new SurfaceComposerClient;
    status_t err = surfaceComposerClient->initCheck();
    if (err != OK) {
        ALOGD("SurfaceComposerClient::initCheck error: %#x\n", err);
        return -1;
    }


    // 获取到显示设备的 ID
    // 返回的是一个 vector，因为存在多屏或者投屏等情况
    const std::vector<PhysicalDisplayId> ids = SurfaceComposerClient::getPhysicalDisplayIds();
    if (ids.empty()) {
        ALOGE("Failed to get ID for any displays\n");
        return -1;
    }

    //displayToken 是屏幕的索引
    sp<IBinder> displayToken = nullptr;

    // 示例仅考虑只有一个屏幕的情况
    displayToken = SurfaceComposerClient::getPhysicalDisplayToken(ids.front());

    // 获取屏幕相关参数
    ui::DisplayMode displayMode;
    err = SurfaceComposerClient::getActiveDisplayMode(displayToken, &displayMode);
    if (err != OK)
        return -1;


    resolution = displayMode.resolution;
    //resolution = limitSurfaceSize(resolution.width, resolution.height);
    const char* names[3] = {"d1", "d2", "d3"};
    const char* names_bbq[3] = {"b1", "b2", "b3"};
    int x[3] = {0, 0, 0};
    int y[3] = {0, 300, 600};
    SurfaceComposerClient::Transaction t;
    for (int i = 0; i < 1; i++) {
        // 创建 SurfaceControl 对象
        // 会远程调用到 SurfaceFlinger 进程中，Surfaceflinger 中会创建一个 Layer 对象
        String8 name(names[i]);
        sp<SurfaceControl> surfaceControl = surfaceComposerClient->createSurface(name, resolution.getWidth(),
                                                                                 resolution.getHeight(), PIXEL_FORMAT_RGBA_8888,
                                                                                 ISurfaceComposerClient::eFXSurfaceBufferState,
                                                                                 /*parent*/ nullptr);

        // 配置 Layer 对象
        //SurfaceComposerClient::Transaction{}
        t.setLayer(surfaceControl, std::numeric_limits<int32_t>::max())
                .show(surfaceControl)
                .setPosition(surfaceControl, x[i], y[i])
                .setBackgroundColor(surfaceControl, half3{0, 0, 0}, 1.0f, ui::Dataspace::UNKNOWN) // black background
                .setAlpha(surfaceControl, 1.0f)
                .setLayerStack(surfaceControl, ui::LayerStack::fromValue(mLayerStack))
                .apply();


        // 初始化一个 BLASTBufferQueue 对象，传入了前面获取到的 surfaceControl
        // BLASTBufferQueue 是帧缓存的大管家
        sp<BLASTBufferQueue> mBlastBufferQueue = new BLASTBufferQueue(names_bbq[i], surfaceControl,
                                                 resolution.getWidth(), resolution.getHeight(),
                                                 PIXEL_FORMAT_RGBA_8888);
        // 获取到 GraphicBuffer 的生产者并完成初始化。
        //sp<IGraphicBufferProducer> igbProducer;
        sp<IGraphicBufferProducer> igbProducer = mBlastBufferQueue->getIGraphicBufferProducer();
        igbProducer->setMaxDequeuedBufferCount(2);
        IGraphicBufferProducer::QueueBufferOutput qbOutput;
        igbProducer->connect(new StubProducerListener, NATIVE_WINDOW_API_CPU, false, &qbOutput);

        igbProducers.push_back(igbProducer);
        mBlastBufferQueues.push_back(mBlastBufferQueue);
        mSurfaceControls.push_back(surfaceControl);
    }

    return 0;
}

int show_color(int index) {
    JLOGD("index=%d", index);
    status_t err = NO_ERROR;

    sp<IGraphicBufferProducer> igbProducer = igbProducers[0];

    int slot;
    sp<Fence> fence;
    sp<GraphicBuffer> buf;

    // 1. dequeue buffer
    igbProducer->dequeueBuffer(&slot, &fence, resolution.getWidth(), resolution.getHeight(),
                                          PIXEL_FORMAT_RGBA_8888, GRALLOC_USAGE_SW_WRITE_OFTEN,
                                          nullptr, nullptr);
    igbProducer->requestBuffer(slot, &buf);

    int waitResult = fence->waitForever("dequeueBuffer_EmptyNative");
    if (waitResult != OK) {
        ALOGE("dequeueBuffer_EmptyNative: Fence::wait returned an error: %d", waitResult);
        return -1;
    }

    // 2. fill the buffer with color
    uint8_t* img = nullptr;
    err = buf->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&img));
    if (err != NO_ERROR) {
        ALOGE("error: lock failed: %s (%d)", strerror(-err), -err);
        return -1;
    }
    fillRGBA8Buffer(img, resolution.getWidth(), resolution.getHeight(), buf->getStride(),
                    index == 0 ? 255 : 0,
                    index == 1 ? 255 : 0,
                    index == 2 ? 255 : 0);

    err = buf->unlock();
    if (err != NO_ERROR) {
        ALOGE("error: unlock failed: %s (%d)", strerror(-err), -err);
        return -1;
    }

    // 3. queue the buffer to display
    IGraphicBufferProducer::QueueBufferOutput qbOutput;
    IGraphicBufferProducer::QueueBufferInput input(systemTime(), true /* autotimestamp */,
                                                   HAL_DATASPACE_UNKNOWN, {},
                                                   NATIVE_WINDOW_SCALING_MODE_FREEZE, 0,
                                                   Fence::NO_FENCE);
    igbProducer->queueBuffer(slot, input, &qbOutput);


    return 0;
}

int main(int argc, char** argv) {
    JLOGD("enter");
    signal(SIGINT, sighandler);

    JLOGI("Starting Surface Type and Alpha Demo 1.7");
    init_color();
    while(!mQuit) {
        show_color(0);
		sleep(1);
        show_color(1);
		sleep(1);
        show_color(2);
        sleep(1);
    }
    return 0;
}
