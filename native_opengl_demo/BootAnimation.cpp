/*
 * Copyright (C) 2007 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_NDEBUG 0
#define LOG_TAG "native_opengl_demo"

#include <vector>

#include <stdint.h>
#include <inttypes.h>
#include <sys/inotify.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <math.h>
#include <fcntl.h>
#include <utils/misc.h>
#include <signal.h>
#include <time.h>

#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <android/imagedecoder.h>
#include <androidfw/AssetManager.h>
#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/Log.h>
#include <utils/SystemClock.h>

#include <android-base/properties.h>

#include <ui/DisplayMode.h>
#include <ui/PixelFormat.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#include <gui/ISurfaceComposer.h>
#include <gui/DisplayEventReceiver.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/eglext.h>

#include "BootAnimation.h"

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

#define ANIM_PATH_MAX 255
#define STR(x)   #x
#define STRTO(x) STR(x)

namespace android {

using ui::DisplayMode;

static const char DISPLAYS_PROP_NAME[] = "persist.service.bootanim.displays";
static const char U_TEXTURE[] = "uTexture";
static const char U_FADE[] = "uFade";
// static const char U_CROP_AREA[] = "uCropArea";
static const char A_UV[] = "aUv";
static const char A_POSITION[] = "aPosition";
static const char VERTEX_SHADER_SOURCE[] = R"(
    precision mediump float;
    attribute vec4 aPosition;
    attribute highp vec2 aUv;
    varying highp vec2 vUv;
    void main() {
        gl_Position = aPosition;
        vUv = aUv;
    })";

static const char IMAGE_FRAG_SHADER_SOURCE[] = R"(
    precision mediump float;
    uniform sampler2D uTexture;
    uniform float uFade;
    varying highp vec2 vUv;
    void main() {
        vec4 color = texture2D(uTexture, vUv);
        gl_FragColor = vec4(color.x, color.y, color.z, (1.0 - uFade)) * color.a;
    })";

static GLfloat quadPositions[] = {
    -0.5f, -0.5f,
    +0.5f, -0.5f,
    +0.5f, +0.5f,
    +0.5f, +0.5f,
    -0.5f, +0.5f,
    -0.5f, -0.5f
};
static GLfloat quadUVs[] = {
    0.0f, 1.0f,
    1.0f, 1.0f,
    1.0f, 0.0f,
    1.0f, 0.0f,
    0.0f, 0.0f,
    0.0f, 1.0f
};

///////////////// 创建相关  ///////////////////////////

BootAnimation::BootAnimation()
        : Thread(false), mLooper(new Looper(false)) {

    ALOGD("%s enter, version=%s", __func__, MY_VERSION);
    mSession = new SurfaceComposerClient();
}

BootAnimation::~BootAnimation() {
    ALOGD("%s enter", __func__);
}

void BootAnimation::onFirstRef() {
    ALOGD("%s enter", __func__);
}

void BootAnimation::binderDied(const wp<IBinder>&) {
    ALOGD("%s enter", __func__);
}

sp<SurfaceComposerClient> BootAnimation::session() const {
    return mSession;
}

///////////////// 主线程初始化相关 //////////////////
class BootAnimation::DisplayEventCallback : public LooperCallback {
    BootAnimation* mBootAnimation;

public:
    DisplayEventCallback(BootAnimation* bootAnimation) {
        mBootAnimation = bootAnimation;
    }

    int handleEvent(int /* fd */, int /*events*/, void* /* data */) {
        return 0; // remove the callback
    }
};

EGLConfig BootAnimation::getEglConfig(const EGLDisplay& display) {
    const EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_DEPTH_SIZE, 0,
        EGL_NONE
    };
    EGLint numConfigs;
    EGLConfig config;
    eglChooseConfig(display, attribs, &config, 1, &numConfigs);
    return config;
}

ui::Size BootAnimation::limitSurfaceSize(int width, int height) const {
    ui::Size limited(width, height);
    // bool wasLimited = false;
    const float aspectRatio = float(width) / float(height);
    if (mMaxWidth != 0 && width > mMaxWidth) {
        limited.height = mMaxWidth / aspectRatio;
        limited.width = mMaxWidth;
        // wasLimited = true;
    }
    if (mMaxHeight != 0 && limited.height > mMaxHeight) {
        limited.height = mMaxHeight;
        limited.width = mMaxHeight * aspectRatio;
        // wasLimited = true;
    }
    ALOGD("Surface size has been limited to [%dx%d] from [%dx%d]",
             limited.width, limited.height, width, height);
    return limited;
}

ui::Rotation BootAnimation::parseOrientationProperty() {
    const auto displayIds = SurfaceComposerClient::getPhysicalDisplayIds();
    if (displayIds.size() == 0) {
        return ui::ROTATION_0;
    }
    const auto displayId = displayIds[0];
    const auto syspropName = [displayId] {
        std::stringstream ss;
        ss << "ro.bootanim.set_orientation_" << displayId.value;
        return ss.str();
    }();
    const auto syspropValue = android::base::GetProperty(syspropName, "ORIENTATION_0");
    if (syspropValue == "ORIENTATION_90") {
        return ui::ROTATION_90;
    } else if (syspropValue == "ORIENTATION_180") {
        return ui::ROTATION_180;
    } else if (syspropValue == "ORIENTATION_270") {
        return ui::ROTATION_270;
    }
    return ui::ROTATION_0;
}

void BootAnimation::rotateAwayFromNaturalOrientationIfNeeded() {
    const auto orientation = parseOrientationProperty();

    if (orientation == ui::ROTATION_0) {
        // Do nothing if the sysprop isn't set or is set to ROTATION_0.
        return;
    }

    if (orientation == ui::ROTATION_90 || orientation == ui::ROTATION_270) {
        std::swap(mWidth, mHeight);
        std::swap(mInitWidth, mInitHeight);
        mFlingerSurfaceControl->updateDefaultBufferSize(mWidth, mHeight);
    }

    Rect displayRect(0, 0, mWidth, mHeight);
    Rect layerStackRect(0, 0, mWidth, mHeight);

    SurfaceComposerClient::Transaction t;
    t.setDisplayProjection(mDisplayToken, orientation, layerStackRect, displayRect);
    t.apply();
}

void BootAnimation::projectSceneToWindow() {
    glViewport(0, 0, mWidth, mHeight);
    glScissor(0, 0, mWidth, mHeight);
}

status_t BootAnimation::readyToRun_bak() {
    ALOGD("%s enter", __func__);
    mAssets.addDefaultAssets();

    const std::vector<PhysicalDisplayId> ids = SurfaceComposerClient::getPhysicalDisplayIds();
    if (ids.empty()) {
        SLOGE("Failed to get ID for any displays\n");
        return NAME_NOT_FOUND;
    }

    // this system property specifies multi-display IDs to show the boot animation
    // multiple ids can be set with comma (,) as separator, for example:
    // setprop persist.boot.animation.displays 19260422155234049,19261083906282754
    Vector<PhysicalDisplayId> physicalDisplayIds;
    char displayValue[PROPERTY_VALUE_MAX] = "";
    property_get(DISPLAYS_PROP_NAME, displayValue, "");
    bool isValid = displayValue[0] != '\0';
    if (isValid) {
        char *p = displayValue;
        while (*p) {
            if (!isdigit(*p) && *p != ',') {
                isValid = false;
                break;
            }
            p ++;
        }
        if (!isValid)
            SLOGE("Invalid syntax for the value of system prop: %s", DISPLAYS_PROP_NAME);
    }
    if (isValid) {
        std::istringstream stream(displayValue);
        for (PhysicalDisplayId id; stream >> id.value; ) {
            physicalDisplayIds.add(id);
            if (stream.peek() == ',')
                stream.ignore();
        }

        // the first specified display id is used to retrieve mDisplayToken
        for (const auto id : physicalDisplayIds) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
                if (const auto token = SurfaceComposerClient::getPhysicalDisplayToken(id)) {
                    mDisplayToken = token;
                    break;
                }
            }
        }
    }

    // If the system property is not present or invalid, display 0 is used
    if (mDisplayToken == nullptr) {
        mDisplayToken = SurfaceComposerClient::getPhysicalDisplayToken(ids.front());
        if (mDisplayToken == nullptr) {
            return NAME_NOT_FOUND;
        }
    }

    DisplayMode displayMode;
    const status_t error =
            SurfaceComposerClient::getActiveDisplayMode(mDisplayToken, &displayMode);
    if (error != NO_ERROR) {
        return error;
    }

    mMaxWidth = android::base::GetIntProperty("ro.surface_flinger.max_graphics_width", 0);
    mMaxHeight = android::base::GetIntProperty("ro.surface_flinger.max_graphics_height", 0);
    ui::Size resolution = displayMode.resolution;
    resolution = limitSurfaceSize(resolution.width, resolution.height);
    // create the native surface
    sp<SurfaceControl> control = session()->createSurface(String8("BootAnimation"),
            resolution.getWidth(), resolution.getHeight(), PIXEL_FORMAT_RGB_565,
            ISurfaceComposerClient::eOpaque);

    SurfaceComposerClient::Transaction t;
    if (isValid) {
        // In the case of multi-display, boot animation shows on the specified displays
        for (const auto id : physicalDisplayIds) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
                if (const auto token = SurfaceComposerClient::getPhysicalDisplayToken(id)) {
                    t.setDisplayLayerStack(token, ui::DEFAULT_LAYER_STACK);
                }
            }
        }
        t.setLayerStack(control, ui::DEFAULT_LAYER_STACK);
    }

    t.setLayer(control, 0x40000000)
        .apply();

    sp<Surface> s = control->getSurface();

    // initialize opengl and egl
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    EGLConfig config = getEglConfig(display);
    EGLSurface surface = eglCreateWindowSurface(display, config, s.get(), nullptr);
    // Initialize egl context with client version number 2.0.
    EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttributes);
    EGLint w, h;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        return NO_INIT;
    }

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mInitWidth = mWidth = w;
    mInitHeight = mHeight = h;
    mFlingerSurfaceControl = control;
    mFlingerSurface = s;
    // mTargetInset = -1;

    // Rotate the boot animation according to the value specified in the sysprop
    // ro.bootanim.set_orientation_<display_id>. Four values are supported: ORIENTATION_0,
    // ORIENTATION_90, ORIENTATION_180 and ORIENTATION_270.
    // If the value isn't specified or is ORIENTATION_0, nothing will be changed.
    // This is needed to support having boot animation in orientations different from the natural
    // device orientation. For example, on tablets that may want to keep natural orientation
    // portrait for applications compatibility and to have the boot animation in landscape.
    rotateAwayFromNaturalOrientationIfNeeded();

    projectSceneToWindow();

    // Register a display event receiver
    mDisplayEventReceiver = std::make_unique<DisplayEventReceiver>();
    status_t status = mDisplayEventReceiver->initCheck();
    SLOGE_IF(status != NO_ERROR, "Initialization of DisplayEventReceiver failed with status: %d",
            status);
    mLooper->addFd(mDisplayEventReceiver->getFd(), 0, Looper::EVENT_INPUT,
            new DisplayEventCallback(this), nullptr);

    ALOGD("%s init done, w=%d, h=%d", __func__, w, h);
    return NO_ERROR;
}

/////////////////  shader 相关 ///////////////
GLuint compileShader(GLenum shaderType, const GLchar *source) {
    ALOGD("%s enter", __func__);
    GLuint shader = glCreateShader(shaderType);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);
    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE) {
        SLOGE("Compile shader failed. Shader type: %d", shaderType);
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
        std::vector<GLchar> errorLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);
        SLOGE("Shader compilation error: %s", &errorLog[0]);
        return 0;
    }
    return shader;
}

GLuint linkShader(GLuint vertexShader, GLuint fragmentShader) {
    ALOGD("%s enter", __func__);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE) {
        SLOGE("Linking shader failed. Shader handles: vert %d, frag %d",
            vertexShader, fragmentShader);
        return 0;
    }
    return program;
}

void BootAnimation::initShaders() {
    ALOGD("%s enter", __func__);
    GLuint vertexShader        = compileShader(GL_VERTEX_SHADER, (const GLchar *)VERTEX_SHADER_SOURCE);
    GLuint imageFragmentShader = compileShader(GL_FRAGMENT_SHADER, (const GLchar *)IMAGE_FRAG_SHADER_SOURCE);

    // Initialize image shader.
    mImageShader = linkShader(vertexShader, imageFragmentShader);
    GLint positionLocation = glGetAttribLocation(mImageShader, A_POSITION);
    GLint uvLocation = glGetAttribLocation(mImageShader, A_UV);
    mImageTextureLocation = glGetUniformLocation(mImageShader, U_TEXTURE);
    mImageFadeLocation = glGetUniformLocation(mImageShader, U_FADE);
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2,  GL_FLOAT, GL_FALSE, 0, quadPositions);
    glVertexAttribPointer(uvLocation, 2, GL_FLOAT, GL_FALSE, 0, quadUVs);
    glEnableVertexAttribArray(uvLocation);
}

///////////////// OpenGL draw 相关 ///////////////
float mapLinear(float x, float a1, float a2, float b1, float b2) {
    return b1 + ( x - a1 ) * ( b2 - b1 ) / ( a2 - a1 );
}

void BootAnimation::drawTexturedQuad(float xStart, float yStart, float width, float height) {
    // Map coordinates from screen space to world space.
    float x0 = mapLinear(xStart, 0, mWidth, -1, 1);
    float y0 = mapLinear(yStart, 0, mHeight, -1, 1);
    float x1 = mapLinear(xStart + width, 0, mWidth, -1, 1);
    float y1 = mapLinear(yStart + height, 0, mHeight, -1, 1);
    // Update quad vertex positions.
    quadPositions[0] = x0;
    quadPositions[1] = y0;
    quadPositions[2] = x1;
    quadPositions[3] = y0;
    quadPositions[4] = x1;
    quadPositions[5] = y1;
    quadPositions[6] = x1;
    quadPositions[7] = y1;
    quadPositions[8] = x0;
    quadPositions[9] = y1;
    quadPositions[10] = x0;
    quadPositions[11] = y0;
    glDrawArrays(GL_TRIANGLES, 0,
        sizeof(quadPositions) / sizeof(quadPositions[0]) / 2);
}

///////////////// 线程主循环相关 /////////////////
static void* decodeImage(const void* encodedData, size_t dataLength, AndroidBitmapInfo* outInfo,
    bool premultiplyAlpha) {
    AImageDecoder* decoder = nullptr;
    AImageDecoder_createFromBuffer(encodedData, dataLength, &decoder);
    if (!decoder) {
        return nullptr;
    }

    const AImageDecoderHeaderInfo* info = AImageDecoder_getHeaderInfo(decoder);
    outInfo->width = AImageDecoderHeaderInfo_getWidth(info);
    outInfo->height = AImageDecoderHeaderInfo_getHeight(info);
    outInfo->format = AImageDecoderHeaderInfo_getAndroidBitmapFormat(info);
    outInfo->stride = AImageDecoder_getMinimumStride(decoder);
    outInfo->flags = 0;

    if (!premultiplyAlpha) {
        AImageDecoder_setUnpremultipliedRequired(decoder, true);
    }

    const size_t size = outInfo->stride * outInfo->height;
    void* pixels = malloc(size);
    int result = AImageDecoder_decodeImage(decoder, pixels, outInfo->stride, size);
    AImageDecoder_delete(decoder);

    if (result != ANDROID_IMAGE_DECODER_SUCCESS) {
        free(pixels);
        return nullptr;
    }
    return pixels;
}

status_t BootAnimation::initTexture(Texture* texture, AssetManager& assets, const char* name, bool premultiplyAlpha) {
    Asset* asset = assets.open(name, Asset::ACCESS_BUFFER);
    if (asset == nullptr)
        return NO_INIT;

    AndroidBitmapInfo bitmapInfo;
    void* pixels = decodeImage(asset->getBuffer(false), asset->getLength(), &bitmapInfo,
        premultiplyAlpha);
    auto pixelDeleter = std::unique_ptr<void, decltype(free)*>{ pixels, free };

    asset->close();
    delete asset;

    if (!pixels) {
        return NO_INIT;
    }

    const int w = bitmapInfo.width;
    const int h = bitmapInfo.height;

    texture->w = w;
    texture->h = h;

    glGenTextures(1, &texture->name);
    glBindTexture(GL_TEXTURE_2D, texture->name);

    switch (bitmapInfo.format) {
        case ANDROID_BITMAP_FORMAT_A_8:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA,
                    GL_UNSIGNED_BYTE, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGBA_4444:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_SHORT_4_4_4_4, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGBA_8888:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
            break;
        case ANDROID_BITMAP_FORMAT_RGB_565:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,
                    GL_UNSIGNED_SHORT_5_6_5, pixels);
            break;
        default:
            break;
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return NO_ERROR;
}


status_t BootAnimation::initTexture_RGBA8888(Texture* texture, uint8_t *pixels, int w, int h) {
    ALOGD("%s w=%d, h=%d", __func__, w, h);
    if (!pixels) {
        ALOGE("%s buffer is null", __func__);
        return NO_INIT;
    }

    texture->w = w;
    texture->h = h;

    glGenTextures(1, &texture->name);
    glBindTexture(GL_TEXTURE_2D, texture->name);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    return NO_ERROR;
}

status_t BootAnimation::readyToRun() {
    ALOGD("%s enter", __func__);
    mAssets.addDefaultAssets();

    const std::vector<PhysicalDisplayId> ids = SurfaceComposerClient::getPhysicalDisplayIds();
    if (ids.empty()) {
        SLOGE("Failed to get ID for any displays\n");
        return NAME_NOT_FOUND;
    }

    // this system property specifies multi-display IDs to show the boot animation
    // multiple ids can be set with comma (,) as separator, for example:
    // setprop persist.boot.animation.displays 19260422155234049,19261083906282754
    Vector<PhysicalDisplayId> physicalDisplayIds;
    char displayValue[PROPERTY_VALUE_MAX] = "";
    property_get(DISPLAYS_PROP_NAME, displayValue, "");
    bool isValid = displayValue[0] != '\0';
    if (isValid) {
        char *p = displayValue;
        while (*p) {
            if (!isdigit(*p) && *p != ',') {
                isValid = false;
                break;
            }
            p ++;
        }
        if (!isValid)
            SLOGE("Invalid syntax for the value of system prop: %s", DISPLAYS_PROP_NAME);
    }
    if (isValid) {
        std::istringstream stream(displayValue);
        for (PhysicalDisplayId id; stream >> id.value; ) {
            physicalDisplayIds.add(id);
            if (stream.peek() == ',')
                stream.ignore();
        }

        // the first specified display id is used to retrieve mDisplayToken
        for (const auto id : physicalDisplayIds) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
                if (const auto token = SurfaceComposerClient::getPhysicalDisplayToken(id)) {
                    mDisplayToken = token;
                    break;
                }
            }
        }
    }

    // If the system property is not present or invalid, display 0 is used
    if (mDisplayToken == nullptr) {
        mDisplayToken = SurfaceComposerClient::getPhysicalDisplayToken(ids.front());
        if (mDisplayToken == nullptr) {
            return NAME_NOT_FOUND;
        }
    }

    DisplayMode displayMode;
    const status_t error =
            SurfaceComposerClient::getActiveDisplayMode(mDisplayToken, &displayMode);
    if (error != NO_ERROR) {
        return error;
    }

    mMaxWidth = android::base::GetIntProperty("ro.surface_flinger.max_graphics_width", 0);
    mMaxHeight = android::base::GetIntProperty("ro.surface_flinger.max_graphics_height", 0);
    ui::Size resolution = displayMode.resolution;
    resolution = limitSurfaceSize(resolution.width, resolution.height);
    ALOGD("%s display width=%d, height=%d", __func__, resolution.width, resolution.height);
    // create the native surface

    sp<SurfaceControl> control = session()->createSurface(String8("BootAnimationHide"),
            resolution.getWidth(), resolution.getHeight(), PIXEL_FORMAT_RGB_565,
            ISurfaceComposerClient::eOpaque);


    SurfaceComposerClient::Transaction t;
    if (isValid) {
        // In the case of multi-display, boot animation shows on the specified displays
        for (const auto id : physicalDisplayIds) {
            if (std::find(ids.begin(), ids.end(), id) != ids.end()) {
                if (const auto token = SurfaceComposerClient::getPhysicalDisplayToken(id)) {
                    t.setDisplayLayerStack(token, ui::DEFAULT_LAYER_STACK);
                }
            }
        }
        t.setLayerStack(control, ui::DEFAULT_LAYER_STACK);
    }

    t.setLayer(control, 0x40000000)
     .apply();

    sp<Surface> s = nullptr;
    bool use_one_surface = true; // true--OpenGL结果显示到屏幕，false--OpenGL结果显示到GLConsumer
    if (use_one_surface) {
        ALOGD("%s online render", __func__);
        s = control->getSurface();
    } else {
        ALOGD("%s offline render", __func__);
        // 1. 创建 BufferQueue 对
        sp<IGraphicBufferProducer> producer;
        sp<IGraphicBufferConsumer> consumer;
        BufferQueue::createBufferQueue(&producer, &consumer);
        ALOGD("%s display width=%d, height=%d", __func__, resolution.width, resolution.height);
        // 2. 配置 Consumer 端 BufferQueue
        consumer->setDefaultBufferSize(resolution.width, resolution.height);
        consumer->setDefaultBufferFormat(PIXEL_FORMAT_RGB_565);
        consumer->setConsumerUsageBits(
            GRALLOC_USAGE_HW_TEXTURE |
            GRALLOC_USAGE_SW_READ_OFTEN |
            GRALLOC_USAGE_SW_WRITE_NEVER |
            GRALLOC_USAGE_HW_RENDER);

        // 3. 创建 GLConsumer - 这是 Consumer 端
        mGLConsumer = new GLConsumer(consumer,
                                     123,  // GL 纹理ID
                                     GLConsumer::TEXTURE_EXTERNAL,
                                     true,      // 单缓冲模式
                                     false);    // 不 own 纹理


        // 4. 创建 Producer Surface - 传给 EGL
        mProducerSurface = new Surface(producer);
        ANativeWindow* window = mProducerSurface.get();

        // 再次确认 window 配置
        native_window_set_buffers_dimensions(window, resolution.width, resolution.height);
        native_window_set_buffers_format(window, PIXEL_FORMAT_RGB_565);
        native_window_set_buffer_count(window, 3);
        //mProducerSurface->setMaxDequeuedBufferCount(2); // 防止deadlock
        s = mProducerSurface;

        // 显示的surface
        status_t err = native_window_api_connect(control->getSurface().get(), NATIVE_WINDOW_API_CPU);
        if (err != NO_ERROR) {
            ALOGE("%s Failed to connect", __func__);
            return NO_INIT;
        }
    }

    // initialize opengl and egl
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    EGLConfig config = getEglConfig(display);
    EGLSurface surface = eglCreateWindowSurface(display, config, s.get(), nullptr);
    // Initialize egl context with client version number 2.0.
    EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, nullptr, contextAttributes);
    EGLint w, h;
    eglQuerySurface(display, surface, EGL_WIDTH, &w);
    eglQuerySurface(display, surface, EGL_HEIGHT, &h);

    if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
        ALOGE("%s make current error", __func__);
        return NO_INIT;
    }

    mDisplay = display;
    mContext = context;
    mSurface = surface;
    mInitWidth = mWidth = w;
    mInitHeight = mHeight = h;
    mFlingerSurfaceControl = control;
    mFlingerSurface = control->getSurface();
    // mTargetInset = -1;

    // Rotate the boot animation according to the value specified in the sysprop
    // ro.bootanim.set_orientation_<display_id>. Four values are supported: ORIENTATION_0,
    // ORIENTATION_90, ORIENTATION_180 and ORIENTATION_270.
    // If the value isn't specified or is ORIENTATION_0, nothing will be changed.
    // This is needed to support having boot animation in orientations different from the natural
    // device orientation. For example, on tablets that may want to keep natural orientation
    // portrait for applications compatibility and to have the boot animation in landscape.
    rotateAwayFromNaturalOrientationIfNeeded();

    projectSceneToWindow();

    // Register a display event receiver
    mDisplayEventReceiver = std::make_unique<DisplayEventReceiver>();
    status_t status = mDisplayEventReceiver->initCheck();
    SLOGE_IF(status != NO_ERROR, "Initialization of DisplayEventReceiver failed with status: %d",
            status);
    mLooper->addFd(mDisplayEventReceiver->getFd(), 0, Looper::EVENT_INPUT,
            new DisplayEventCallback(this), nullptr);

    ALOGD("%s init done, w=%d, h=%d", __func__, w, h);
    return NO_ERROR;
}

bool BootAnimation::forwardBufferToDisplay() {
    ALOGD("%s enter", __func__);
    // 1. 从 GLConsumer 获取 buffer

    sp<GraphicBuffer> buffer1 = mGLConsumer->getCurrentBuffer();
    if (buffer1 == nullptr) {
        ALOGE("acquireBuffer failed");
        return false;
    }
    ALOGD("wait OK");
    int width1  = buffer1->getWidth();
    int height1 = buffer1->getHeight();
    int stride1 = buffer1->getStride(); // 发现stride是0，width不是0
    int format1 = buffer1->getPixelFormat();
    int usage1  = buffer1->getUsage();
    uint64_t totalSize1 = 0;            // 发现totalSize也是0
    GraphicBufferMapper::get().getAllocationSize(buffer1->handle, &totalSize1);
    ALOGD("buffer1 width %d, height %d, size: %u", width1, height1, (uint32_t)totalSize1);
    ALOGD("buffer1 stride %d, format %d, usage: %u", stride1, format1, usage1);

    // 下面的代码是有问题的，GPU buffer无法直接lock
    // // 从显示Surface dequeue buffer
    // ANativeWindow* window = mFlingerSurface.get();
    // // native_window_set_buffers_dimensions(window, native_buffer1->width, native_buffer1->height);
    // // native_window_set_buffers_format(window, native_buffer1->format);
    // // native_window_set_buffers_transform(window, ANATIVEWINDOW_TRANSFORM_IDENTITY);
    // ANativeWindowBuffer* native_buffer2 = nullptr;
    // int displayFenceFd = -1;
    // status_t err = window->dequeueBuffer(window, &native_buffer2, &displayFenceFd);
    // if (err != NO_ERROR) {
    //     ALOGE("Failed to dequeue buffer from display surface: %d", err);
    //     return false;
    // }

    // // CPU拷贝 buffer1 -> buffer2
    // ALOGD("copy buffer");
    // sp<GraphicBuffer> buffer2 = GraphicBuffer::from(native_buffer2);
    // int width2  = buffer2->getStride();
    // int height2 = buffer2->getHeight();
    // uint64_t totalSize2 = 0;
    // GraphicBufferMapper::get().getAllocationSize(buffer2->handle, &totalSize2);
    // ALOGD("buffer2 width %d, height %d, size: %u", width2, height2, (uint32_t)totalSize2);

    // if (width1 != width2) {
    //     ALOGE("error: width not equal %d %d", width1, width2);
    //     return false;
    // }
    // if (height1 != height2) {
    //     ALOGE("error: height not equal %d %d", height1, height2);
    //     return false;
    // }
    // if (totalSize1 != totalSize2) {
    //     ALOGE("error: total size not equal %u %u", (uint32_t)totalSize1, (uint32_t)totalSize2);
    //     return false;
    // }

    // // 比较内容
    // uint8_t* bufferData1 = nullptr;
    // uint8_t* bufferData2 = nullptr;
    // // ------ 这里会报错，因为 eglSwapBuffers 已经 queue/dequeue，导致buffer是空的 2026.2.12
    // err = buffer1->lock(GRALLOC_USAGE_SW_READ_OFTEN, (void**)(&bufferData1));
    // if (err != NO_ERROR) {
    //     ALOGE("error: lock1 failed: %s (%d)", strerror(-err), -err);
    //     return false;
    // }
    // err = buffer2->lock(GRALLOC_USAGE_SW_WRITE_OFTEN, (void**)(&bufferData2));
    // if (err != NO_ERROR) {
    //     ALOGE("error: lock2 failed: %s (%d)", strerror(-err), -err);
    //     return false;
    // }
    // memcpy(bufferData2, bufferData1, totalSize1 * sizeof(uint8_t));


    // err = buffer2->unlock();
    // if (err != NO_ERROR) {
    //     ALOGE("error: buffer 2 unlock failed: %s (%d)", strerror(-err), -err);
    //     return false;
    // }
    // err = buffer1->unlock();
    // if (err != NO_ERROR) {
    //     ALOGE("error: buffer 2 unlock failed: %s (%d)", strerror(-err), -err);
    //     return false;
    // }

    // err = window->queueBuffer(window, native_buffer2, -1);
    // if (err != NO_ERROR) {
    //     ALOGE("queue error 2: %d", err);
    //     return false;
    // }

    ALOGD("process done");
    return true;
}

bool BootAnimation::android() {
    ALOGD("%s enter", __func__);
    glActiveTexture(GL_TEXTURE0);
    initTexture(&mAndroid[0], mAssets, "images/android-logo-mask.png", true);
    initTexture(&mAndroid[1], mAssets, "images/android-logo-shine.png", true);

    // clear screen
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glUseProgram(mImageShader);

    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    EGLBoolean res = eglSwapBuffers(mDisplay, mSurface);
    if (res == EGL_FALSE) {
        ALOGE("%s eglSwapBuffers1 fail", __func__);
        return false;
    }
    status_t err = 0;
    if (mGLConsumer != nullptr) {
        err = mGLConsumer->updateTexImage();// 每次swapbuffers后都要updatetextImage
        if (err < 0) {
            ALOGE("GLConsumer::updateTexImage error: %d\n", err);
            return false;
        }
    }
    ALOGD("%s frame ready", __func__);

    // Blend state
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const nsecs_t startTime = systemTime();
    for (int i = 0; i < 10; i++) {
        ALOGD("%s frame %d begin", __func__, i);
        const GLint xc = (mWidth  - mAndroid[0].w) / 2;
        const GLint yc = (mHeight - mAndroid[0].h) / 2;
        const Rect updateRect(xc, yc, xc + mAndroid[0].w, yc + mAndroid[0].h);
        glScissor(updateRect.left, mHeight - updateRect.bottom, updateRect.width(), updateRect.height());

        nsecs_t now = systemTime();
        double time = now - startTime;
        float t = 4.0f * float(time / us2ns(16667)) / mAndroid[1].w;
        GLint offset = (1 - (t - floorf(t))) * mAndroid[1].w;
        GLint x = xc - offset;

        glDisable(GL_SCISSOR_TEST);
        glClear(GL_COLOR_BUFFER_BIT);

        glEnable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, mAndroid[1].name);
        drawTexturedQuad(x,                 yc, mAndroid[1].w, mAndroid[1].h);
        drawTexturedQuad(x + mAndroid[1].w, yc, mAndroid[1].w, mAndroid[1].h);

        glEnable(GL_BLEND);
        glBindTexture(GL_TEXTURE_2D, mAndroid[0].name);
        drawTexturedQuad(xc, yc, mAndroid[0].w, mAndroid[0].h);

        ALOGD("draw end");
        EGLBoolean res = eglSwapBuffers(mDisplay, mSurface);
        if (res == EGL_FALSE) {
            ALOGE("%s eglSwapBuffers2 fail", __func__);
            break;
        }

        ALOGD("swap buffer done");
        if (mGLConsumer != nullptr) {
            err = mGLConsumer->updateTexImage();
            if (err < 0) {
                ALOGE("GLConsumer::updateTexImage error: %d\n", err);
                return false;
            }
        }

        // 1.2fps: don't animate too fast to preserve CPU
        const nsecs_t sleepTime = 833333 - ns2us(systemTime() - now);
        if (sleepTime > 0)
            usleep(sleepTime);

        ALOGD("%s frame %d finish", __func__, i);

        if (mGLConsumer != nullptr) {
            if (false == forwardBufferToDisplay())
                break;
        }
    }

    glDeleteTextures(1, &mAndroid[0].name);
    glDeleteTextures(1, &mAndroid[1].name);
    return false;
}

bool BootAnimation::threadLoop() {
    ALOGD("%s enter", __func__);
    bool result;
    initShaders();

    // We have no bootanimation file, so we use the stock android logo
    // animation.
    ALOGD("render begin");
    result = android();
    ALOGD("render end");
    //mCallbacks->shutdown();
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    mFlingerSurface.clear();
    mFlingerSurfaceControl.clear();
    eglTerminate(mDisplay);
    eglReleaseThread();
    IPCThreadState::self()->stopProcess();
    ALOGD("leave");
    return result;
}

// ---------------------------------------------------------------------------

} // namespace android
