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

#ifndef ANDROID_BOOTANIMATION_H
#define ANDROID_BOOTANIMATION_H

#include <vector>
#include <queue>
#include <climits>

#include <stdint.h>
#include <sys/types.h>
#include <ui/Fence.h>
#include <androidfw/AssetManager.h>
#include <gui/DisplayEventReceiver.h>
#include <utils/Looper.h>
#include <utils/Thread.h>
#include <binder/IBinder.h>

#include <ui/Rotation.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <gui/GLConsumer.h>
#include <ui/GraphicBuffer.h>
#include <sync/sync.h>  // 这是 sync_wait 的头文件
#include <unistd.h>     // 对于 close 函数

#define MY_VERSION "v1.1"

namespace android {

class Surface;
class SurfaceComposerClient;
class SurfaceControl;

// ---------------------------------------------------------------------------

class BootAnimation : public Thread, public IBinder::DeathRecipient
{
public:
    static constexpr int MAX_FADED_FRAMES_COUNT = std::numeric_limits<int>::max();

    struct Texture {
        GLint   w;
        GLint   h;
        GLuint  name;
    };

    explicit BootAnimation();
    virtual ~BootAnimation();

    sp<SurfaceComposerClient> session() const;

private:
    virtual bool        threadLoop();
    virtual status_t    readyToRun();
    virtual void        onFirstRef();
    virtual void binderDied(const wp<IBinder>& who);
    status_t readyToRun_bak();

    // Display event handling
    class DisplayEventCallback;
    std::unique_ptr<DisplayEventReceiver> mDisplayEventReceiver;
    sp<Looper> mLooper;

    status_t initTexture(Texture* texture, AssetManager& asset, const char* name,
        bool premultiplyAlpha = true);
    status_t initTexture_RGBA8888(Texture* texture, uint8_t *pixels, int w, int h);

    void initShaders();
    bool android();
    void drawTexturedQuad(float xStart, float yStart, float width, float height);

    EGLConfig getEglConfig(const EGLDisplay&);
    ui::Size limitSurfaceSize(int width, int height) const;

    void projectSceneToWindow();
    void rotateAwayFromNaturalOrientationIfNeeded();
    ui::Rotation parseOrientationProperty();

    sp<SurfaceComposerClient>       mSession;
    AssetManager mAssets;
    Texture     mAndroid[2];
    int         mWidth;
    int         mHeight;
    int         mInitWidth;
    int         mInitHeight;
    int         mMaxWidth = 0;
    int         mMaxHeight = 0;
    // int         mCurrentInset;

    EGLDisplay  mDisplay;
    EGLDisplay  mContext;
    EGLDisplay  mSurface;
    sp<IBinder> mDisplayToken;
    sp<SurfaceControl> mFlingerSurfaceControl;
    sp<Surface> mFlingerSurface;

    GLuint mImageShader;
    GLuint mImageFadeLocation;
    GLuint mImageTextureLocation;

private:
    // Buffer转发相关，mGLConsumer是一个不去屏幕显示的消费者
    bool forwardBufferToDisplay();
    sp<GLConsumer> mGLConsumer = nullptr;
    sp<Surface> mProducerSurface = nullptr;
};

// ---------------------------------------------------------------------------

}; // namespace android

#endif // ANDROID_BOOTANIMATION_H
