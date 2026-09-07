#include <log/log.h>
#include <utils/RefBase.h>
#include <renderengine/ExternalTexture.h>
#include <renderengine/LayerSettings.h>
#include <renderengine/RenderEngine.h>
#include <renderengine/impl/ExternalTexture.h>
#include <ui/GraphicBuffer.h>
#include <ui/GraphicBufferMapper.h>
#include <mutex>
#include <inttypes.h>   // PRIx64
#include <android/native_window.h>
#include <getopt.h>

using namespace android;
using namespace android::renderengine;

static std::unique_ptr<RenderEngine> createRenderEngine(int use_vulkan, int pixel_format) {
    ALOGD("%s enter, use_vulkan=%d, pixel format=%d", __func__, use_vulkan, pixel_format);

    RenderEngine::GraphicsApi graphicsApi = RenderEngine::GraphicsApi::GL;
    if (use_vulkan)
        graphicsApi = RenderEngine::GraphicsApi::VK;

    auto args = RenderEngineCreationArgs::Builder()
                        //.setPixelFormat(static_cast<int>(ui::PixelFormat::RGBA_8888))
                        .setPixelFormat(pixel_format)
                        .setImageCacheSize(1)
                        .setEnableProtectedContext(true)
                        .setPrecacheToneMapperShaderOnly(false)
                        .setBlurAlgorithm(RenderEngine::BlurAlgorithm::NONE)
                        .setContextPriority(RenderEngine::ContextPriority::REALTIME)
                        .setThreaded(RenderEngine::Threaded::YES)
                        .setGraphicsApi(graphicsApi)
                        .build();

    return RenderEngine::create(args);
}

int copyBuffer_engine(sp<GraphicBuffer> in_buffer,
                      sp<GraphicBuffer> out_buffer,
                      int renderengine_VulKan) {

    ALOGD("%s enter, vulkan=%d", __func__, renderengine_VulKan);

    int pixel_format = (int)in_buffer->getPixelFormat();
    std::unique_ptr<RenderEngine> engine = createRenderEngine(renderengine_VulKan, pixel_format);

    std::shared_ptr<ExternalTexture> original =
        std::make_shared<impl::ExternalTexture>(in_buffer, *engine,
                   impl::ExternalTexture::Usage::READABLE |
                   impl::ExternalTexture::Usage::WRITEABLE);

    std::shared_ptr<ExternalTexture> texture =
        std::make_shared<impl::ExternalTexture>(out_buffer, *engine,
                   impl::ExternalTexture::Usage::READABLE |
                   impl::ExternalTexture::Usage::WRITEABLE);

    const uint32_t width = original->getBuffer()->getWidth();
    const uint32_t height = original->getBuffer()->getHeight();
    const Rect displayRect(0, 0, static_cast<int32_t>(width), static_cast<int32_t>(height));
    DisplaySettings display{
            .physicalDisplay = displayRect,
            .clip = displayRect,
            .maxLuminance = 500,
    };

    const FloatRect layerRect(0, 0, width, height);
    LayerSettings layer{
        .geometry =
            Geometry{
                .boundaries = layerRect,
            },
        .source =
            PixelSource{
                .buffer =
                    Buffer{
                            .buffer = original,
                    },
            },
        .alpha = half(1.0f),
    };
    auto layers = std::vector<LayerSettings>{layer};

    sp<Fence> waitFence =
            engine->drawLayers(display, layers, texture, base::unique_fd())
                    .get()
                    .value();
    waitFence->waitForever(LOG_TAG);
    return 0;
}

