#ifndef PURE_COLOR_DISPLAY_H
#define PURE_COLOR_DISPLAY_H

#include <aidl/android/hardware/graphics/composer3/IComposer.h>
#include <aidl/android/hardware/graphics/composer3/IComposerClient.h>
#include <aidl/android/hardware/graphics/common/Rect.h>  // 添加这个头文件
#include <android/binder_manager.h>
#include <android/binder_process.h>
#include <log/log.h>

using aidl::android::hardware::graphics::composer3::IComposer;
using aidl::android::hardware::graphics::composer3::IComposerClient;
using aidl::android::hardware::graphics::composer3::DisplayCommand;
using aidl::android::hardware::graphics::composer3::LayerCommand;
using aidl::android::hardware::graphics::composer3::Color;
using aidl::android::hardware::graphics::composer3::ParcelableComposition;
using aidl::android::hardware::graphics::composer3::Composition;
using aidl::android::hardware::graphics::common::Rect;
using aidl::android::hardware::graphics::composer3::DisplayCapability;
using aidl::android::hardware::graphics::composer3::OverlayProperties;
using aidl::android::hardware::graphics::composer3::ColorMode;
using aidl::android::hardware::graphics::common::BlendMode;

class PureColorDisplay {
public:
    PureColorDisplay();
    ~PureColorDisplay();

    bool initialize();
    void cleanup();
    bool displaySolidColor(float r, float g, float b, float a = 1.0f);

private:
    std::shared_ptr<IComposer> mComposer;
    std::shared_ptr<IComposerClient> mClient;
    int64_t mDisplayId;
    int64_t mLayerId;
    bool mInitialized;

    bool setupDisplay();
    bool createLayer();
    bool setLayerColor(float r, float g, float b, float a);
	bool setLayerColorWithoutPresent(float r, float g, float b, float a);
    bool presentDisplay();
    bool validateDisplay();
    bool validateOrPresentDisplay();
    void generateDisplayCommand(DisplayCommand& displayCommand, float r, float g, float b, float a);

};

#endif // PURE_COLOR_DISPLAY_H