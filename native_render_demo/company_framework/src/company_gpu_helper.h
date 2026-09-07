/*
 * 2026.3.30 fengjx 改编自BootAnimation.h
 */

#ifndef COMPANY_GPU_HELPER_H
#define COMPANY_GPU_HELPER_H

#include <vector>
#include <queue>
#include <climits>

#include <stdint.h>
#include <sys/types.h>
#include <ui/Fence.h>
#include <gui/DisplayEventReceiver.h>
#include <utils/Looper.h>
#include <utils/Thread.h>
#include <binder/IBinder.h>
#include <utils/RefBase.h>
#include <ui/Rotation.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/SurfaceControl.h>
#include <gui/GLConsumer.h>
#include <ui/GraphicBuffer.h>
// #include <sync/sync.h>  // 这是 sync_wait 的头文件
#include <unistd.h>     // 对于 close 函数

using namespace android;

namespace company {

// ---------------------------------------------------------------------------

class CompanyGPUHelper : public RefBase
{
public:
    struct Texture {
        GLint   w;
        GLint   h;
        GLuint  name;
    };

    explicit CompanyGPUHelper();
    virtual ~CompanyGPUHelper();
    sp<GraphicBuffer> createGPUBuffer(void* pixels, int width, int height, int pixel_format);

private:
    status_t  initOpenGL(int width, int height, int pixel_format);
    status_t  initTexture(Texture* texture, void* pixels, int width, int height, int pixel_format);
    void      initShaders();
    void      drawTexturedQuad(float xStart, float yStart, float width, float height);
    EGLConfig getEglConfig(const EGLDisplay&);
    int       drawOnece(void* pixels, int width, int height, int pixel_format);
    void      deleteGPUBuffer();


private:
    Texture   mTexture;
    int       mWidth;
    int       mHeight;
    int       mInitWidth;
    int       mInitHeight;

    EGLDisplay  mDisplay;
    EGLDisplay  mContext;
    EGLDisplay  mSurface;

    GLuint mImageShader;
    GLuint mImageTextureLocation;

    sp<GLConsumer> mGLConsumer = nullptr;
    sp<Surface> mProducerSurface = nullptr;
    void* m_backup_pixels = nullptr;
};

// ---------------------------------------------------------------------------

}; // namespace company

#endif // ANDROID_BOOTANIMATION_H
