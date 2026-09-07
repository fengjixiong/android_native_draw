#include "PureColorDisplay.h"
#include <thread>
#include <chrono>

using namespace std::chrono_literals;
#define LOGTAG "PureColorDisplay "
PureColorDisplay::PureColorDisplay()
    : mComposer(nullptr)
    , mClient(nullptr)
    , mDisplayId(-1)
    , mLayerId(-1)
    , mInitialized(false) {
}

PureColorDisplay::~PureColorDisplay() {
    cleanup();
}

bool PureColorDisplay::initialize() {
    // 获取 HWC 服务
    auto composerName = std::string(IComposer::descriptor) + "/default";
    ndk::SpAIBinder binder(AServiceManager_checkService(composerName.c_str()));
    mComposer = IComposer::fromBinder(binder);

    if (!mComposer) {
        ALOGE(LOGTAG "Failed to get HWC service");
        return false;
    }

    // 创建 ComposerClient
    std::shared_ptr<IComposerClient> client;
    auto status = mComposer->createClient(&client);
    if (!status.isOk() || !client) {
        ALOGE(LOGTAG "Failed to create composer client");
        return false;
    }
    mClient = client;

    // 设置显示和图层
    if (!setupDisplay()) {
        ALOGE(LOGTAG "Failed to setup display");
        return false;
    }

    if (!createLayer()) {
        ALOGE(LOGTAG "Failed to create layer");
        return false;
    }

    // 检查支持的合成类型
    std::vector<DisplayCapability> caps;
    if (mClient->getDisplayCapabilities(mDisplayId, &caps).isOk()) {
        for (const auto& cap : caps) {
            ALOGI(LOGTAG "Capability: %d", static_cast<int>(cap));
        }
    }

    // 检查颜色模式
    std::vector<ColorMode> colorModes;
    if (mClient->getColorModes(mDisplayId, &colorModes).isOk()) {
        ALOGI(LOGTAG "Color modes: %zu", colorModes.size());
    }

    mInitialized = true;
    ALOGI(LOGTAG "initialized successfully");
    return true;
}

void PureColorDisplay::cleanup() {
    if (mLayerId != -1 && mDisplayId != -1 && mClient) {
        mClient->destroyLayer(mDisplayId, mLayerId);
        mLayerId = -1;
    }

    mClient = nullptr;
    mComposer = nullptr;
    mInitialized = false;
}

bool PureColorDisplay::setupDisplay() {
    // 获取主显示（通常 ID 为 0）
    mDisplayId = 0;

    // 设置电源模式为打开
    auto status = mClient->setPowerMode(mDisplayId,
        aidl::android::hardware::graphics::composer3::PowerMode::ON);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to set power mode");
        return false;
    }

    // 启用 VSYNC
    status = mClient->setVsyncEnabled(mDisplayId, true);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to enable VSYNC");
    }

    ALOGD(LOGTAG "setup display OK");
    return true;
}

bool PureColorDisplay::createLayer() {
    if (!mClient || mDisplayId == -1) {
        return false;
    }

    // 创建图层，设置缓冲区槽位数
    const int32_t bufferSlotCount = 1;
    auto status = mClient->createLayer(mDisplayId, bufferSlotCount, &mLayerId);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to create layer");
        return false;
    }

    ALOGI(LOGTAG "Created layer: %lld", (long long)mLayerId);
    return true;
}

void PureColorDisplay::generateDisplayCommand(DisplayCommand& displayCommand, float r, float g, float b, float a) {

    // 创建图层命令（不包含present）
    LayerCommand layerCommand;
    layerCommand.layer = mLayerId;

    // 设置图层颜色
    Color color;
    color.r = r;
    color.g = g;
    color.b = b;
    color.a = a;
    layerCommand.color = color;

    // 设置几何属性
    int32_t width = 480;
    int32_t height = 800;
    layerCommand.sourceCrop = {0, 0, static_cast<float>(width), static_cast<float>(height)};
    layerCommand.displayFrame = {0, 0, width, height};
    aidl::android::hardware::graphics::composer3::ZOrder zOrder;
    zOrder.z = 0x7FFFFFFF;
    layerCommand.z = zOrder;
    aidl::android::hardware::graphics::composer3::PlaneAlpha planeAlpha;
    planeAlpha.alpha = 1.0f;
    layerCommand.planeAlpha = planeAlpha;
    aidl::android::hardware::graphics::composer3::ParcelableBlendMode blendMode;
    blendMode.blendMode = BlendMode::NONE;
    layerCommand.blendMode = blendMode;
    ParcelableComposition composition;
    composition.composition = Composition::SOLID_COLOR;
    layerCommand.composition = composition;

    displayCommand.display = mDisplayId;
    displayCommand.layers.push_back(std::move(layerCommand));

    ALOGD(LOGTAG "generateDisplayCommand OK");
}

bool PureColorDisplay::setLayerColor(float r, float g, float b, float a) {
    if (!mInitialized || mDisplayId == -1 || mLayerId == -1) {
        return false;
    }

    // 准备显示命令
    std::vector<DisplayCommand> commands;

    // 创建显示命令
    DisplayCommand displayCommand;
    generateDisplayCommand(displayCommand, r, g, b, a);

    // 添加present命令
    displayCommand.presentOrValidateDisplay = true; // true 表示present

    commands.push_back(std::move(displayCommand));

    // 执行命令
    std::vector<aidl::android::hardware::graphics::composer3::CommandResultPayload> results;
    auto status = mClient->executeCommands(commands, &results);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to execute commands");
        return false;
    }

    // // 检查present结果
    // if (!results.empty()) {
    //     for (const auto& result : results) {
    //         if (result.getTag() == aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult) {
    //             auto presentResult = result.get<aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult>();
    //             if (presentResult.result != aidl::android::hardware::graphics::composer3::PresentOrValidate::Result::Presented) {
    //                 ALOGE(LOGTAG "Present failed with result: %d", static_cast<int>(presentResult.result));
    //                 return false;
    //             }
    //         }
    //     }
    // }
    ALOGD(LOGTAG "setLayerColor OK");
    return true;
}

bool PureColorDisplay::setLayerColorWithoutPresent(float r, float g, float b, float a) {
    if (!mInitialized || mDisplayId == -1 || mLayerId == -1) {
        return false;
    }
    std::vector<DisplayCommand> commands;
    DisplayCommand displayCommand;
    generateDisplayCommand(displayCommand, r, g, b, a);
    commands.push_back(std::move(displayCommand));
    std::vector<aidl::android::hardware::graphics::composer3::CommandResultPayload> results;
    auto status = mClient->executeCommands(commands, &results);

    ALOGD(LOGTAG "setLayerColorWithoutPresent OK");
    return status.isOk();
}

bool PureColorDisplay::validateDisplay() {
    if (!mInitialized || mDisplayId == -1) {
        return false;
    }

    std::vector<DisplayCommand> commands;

    // 创建validate命令
    DisplayCommand validateCommand;
    validateCommand.display = mDisplayId;
    validateCommand.validateDisplay = true; // false 表示validate

    commands.push_back(std::move(validateCommand));

    std::vector<aidl::android::hardware::graphics::composer3::CommandResultPayload> results;
    auto status = mClient->executeCommands(commands, &results);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to validate display");
        return false;
    }

    // 检查验证结果
    if (!results.empty()) {
        for (const auto& result : results) {
            if (result.getTag() == aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult) {
                auto validateResult = result.get<aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult>();
                if (validateResult.result != aidl::android::hardware::graphics::composer3::PresentOrValidate::Result::Validated) {
                    ALOGE(LOGTAG "Validate failed with result: %d", static_cast<int>(validateResult.result));
                    return false;
                }
            }
        }
    }

    ALOGD(LOGTAG "validateDisplay OK");
    return true;
}

bool PureColorDisplay::presentDisplay() {
    if (!mInitialized || mDisplayId == -1) {
        ALOGE(LOGTAG "presentDisplay null");
        return false;
    }

    std::vector<DisplayCommand> commands;

    // 创建present命令
    DisplayCommand presentCommand;
    presentCommand.display = mDisplayId;
    presentCommand.presentDisplay = true; // true 表示present

    commands.push_back(std::move(presentCommand));

    std::vector<aidl::android::hardware::graphics::composer3::CommandResultPayload> results;
    auto status = mClient->executeCommands(commands, &results);
    if (!status.isOk()) {
        ALOGE(LOGTAG "Failed to present display");
        return false;
    }

    // 检查present结果
    if (!results.empty()) {
        for (const auto& result : results) {
            if (result.getTag() == aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult) {
                auto presentResult = result.get<aidl::android::hardware::graphics::composer3::CommandResultPayload::presentOrValidateResult>();
                if (presentResult.result != aidl::android::hardware::graphics::composer3::PresentOrValidate::Result::Presented) {
                    ALOGE(LOGTAG "Present failed with result: %d", static_cast<int>(presentResult.result));
                    return false;
                }
            }
        }
    }
    ALOGD(LOGTAG "presentDisplay OK");
    return true;
}


bool PureColorDisplay::displaySolidColor(float r, float g, float b, float a) {
    if (!mInitialized) {
        ALOGE("Not initialized");
        return false;
    }

    ALOGI(LOGTAG "Displaying color: R=%.2f, G=%.2f, B=%.2f, A=%.2f", r, g, b, a);

    if (false) {
        if (!setLayerColor(r, g, b, a)) {
            ALOGE(LOGTAG "Display validation failed");
            return false;
        }
    } else {
        if (!setLayerColorWithoutPresent(r, g, b, a)) {
            ALOGE(LOGTAG "Failed to set layer color");
            return false;
        }

        // 验证显示配置
        if (!validateDisplay()) {
            ALOGE(LOGTAG "Display validation failed");
            return false;
        }

        // 提交显示
        if (!presentDisplay()) {
            ALOGE(LOGTAG "Display presentation failed");
            return false;
        }

    }


    ALOGI("Color displayed successfully");
    return true;
}
