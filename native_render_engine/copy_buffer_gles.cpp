#include <android/log.h>
#include <ui/GraphicBuffer.h>
#include <ui/PixelFormat.h>
#include <ui/DebugUtils.h>
#include <utils/RefBase.h>
#include <cutils/properties.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <memory>
#include <unistd.h>
#include <sys/stat.h>

// #undef  LOG_TAG
// #define LOG_TAG "BufferDumper"

using namespace android;


// // ============= dump thread pool =================================================

// #include <mutex>
// #include <condition_variable>
// #include <queue>
// #include <functional>
// #include <atomic>
// #include <future>

// class DumpThreadPool {
// public:
//     struct DumpTask {
//         sp<GraphicBuffer> buffer;
//         std::string dumpPath;
//         bool isCompressed;
//     };

//     static DumpThreadPool& getInstance() {
//         static DumpThreadPool instance;
//         return instance;
//     }

//     template <class F>
//     auto runSync(F&& func) -> decltype(func()) {
//         std::packaged_task<decltype(func())()> task{std::forward<F>(func)};
//         enqueue([&task]() { std::invoke(task); });
//         return task.get_future().get();
//     }

//     void shutdown() {
//         mRunning.store(false);
//         mCv.notify_one();
//         if (mThread.joinable()) {
//             mThread.join();
//         }
//     }

// private:
//     DumpThreadPool() : mRunning(true) {
//         mThread = std::thread([this]() { workerLoop(); });
//     }

//     ~DumpThreadPool() {
//         shutdown();
//     }

//     DumpThreadPool(const DumpThreadPool&) = delete;
//     DumpThreadPool& operator=(const DumpThreadPool&) = delete;

//     void enqueue(std::function<void()>&& task) {
//         {
//             std::lock_guard<std::mutex> lock(mMutex);
//             mQueue.emplace(std::move(task));
//         }
//         mCv.notify_one();
//     }

//     void workerLoop() {
//         ALOGD("DumpThreadPool worker started!");
//         while (true) {
//             std::function<void()> task;
//             {
//                 std::unique_lock<std::mutex> lock(mMutex);
//                 mCv.wait(lock, [this] { return !mQueue.empty() || !mRunning.load(); });
//                 if (!mRunning.load() && mQueue.empty()) {
//                     break;
//                 }
//                 task = std::move(mQueue.front());
//                 mQueue.pop();
//             }
//             task();
//         }
//         ALOGD("DumpThreadPool worker exited!");
//     }

//     std::thread mThread;
//     std::mutex mMutex;
//     std::condition_variable mCv;
//     std::queue<std::function<void()>> mQueue;
//     std::atomic<bool> mRunning;
// };

// // ============= dump thread pool end =============================================


#ifndef EGL_NATIVE_BUFFER_ANDROID
#define EGL_NATIVE_BUFFER_ANDROID 0x3140
#endif

enum FormatType {
    UNKNOWN,
    RAW,
    UBWC,
    AFBC,
};

// static bool dumpToFile(const std::string& path, const void* data, size_t size) {
//     FILE* file = fopen(path.c_str(), "wb");
//     if (!file) {
//         ALOGE("Failed to open file:%s(%s)", path.c_str(), strerror(errno));
//         return false;
//     }
//     fwrite(data, 1, size, file);
//     fclose(file);
//     ALOGD("dump frame:%s", path.c_str());
//     return true;
// }

void processBuffer(const sp<GraphicBuffer>& buffer, std::function<void(void *vaddr, uint64_t bufferSize, FormatType type)> &&processor) {
    struct stat st[2] {{}, {}};
    int fd = -1;
    uint64_t bufferSize = 0;
    if (fstat(buffer->handle->data[0], &st[0]) < 0) {
        ALOGW("Failed to fstat.(%s)", strerror(errno));
    }
    if (fstat(buffer->handle->data[1], &st[1]) < 0) {
        ALOGW("Failed to fstat.(%s)", strerror(errno));
    }

    FormatType type = UNKNOWN;
    if (st[0].st_size > st[1].st_size) {
        fd = buffer->handle->data[0]; // data[0]
        bufferSize = st[0].st_size;
        type = UBWC;
    } else {
        fd = buffer->handle->data[1]; // data[1]
        bufferSize = st[1].st_size;
        type = AFBC;
    }
    // TODO: 可以使用lock，不用每次都mmap
    const auto mem = static_cast<char*>(mmap(nullptr, bufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
    if (mem == MAP_FAILED) {
        ALOGE("Failed to mmap(%s)", strerror(errno));
        return;
    }
    processor(mem, bufferSize, type);
    munmap(mem, bufferSize);
}

// void dumpGraphicBuffer(const sp<GraphicBuffer>& buffer, const std::string &dumpPath, const std::string& suffix = "") {
//     processBuffer(buffer, [&](void *vaddr, uint64_t bufferSize, FormatType type){
//         auto filename = dumpPath;
//         if (!suffix.empty()) {
//             filename += suffix;
//         } else {
//             filename += type == UBWC ? ".ubwc" : ".afbc";
//         }
//         dumpToFile(filename, vaddr, bufferSize);
//     });
// }

void clearBuffer(const sp<GraphicBuffer>& buffer) {
    processBuffer(buffer, [](void *vaddr, uint64_t bufferSize, FormatType /*type*/){
        memset(vaddr, 0, bufferSize);
    });
}

static PFNEGLCREATEIMAGEKHRPROC s_eglCreateImageKHR = nullptr;
static PFNEGLDESTROYIMAGEKHRPROC s_eglDestroyImageKHR = nullptr;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC s_glEGLImageTargetTexture2DOES = nullptr;

struct EglContextHolder {
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLContext context = EGL_NO_CONTEXT;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLConfig config = EGL_NO_CONTEXT;
};

struct ImageTarget {
    EGLImageKHR image = EGL_NO_IMAGE_KHR;
    GLuint tex = 0;
    GLuint fbo = 0;
};

static bool loadEglGlExtensions() {
    s_eglCreateImageKHR =
        reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(eglGetProcAddress("eglCreateImageKHR"));
    s_eglDestroyImageKHR =
        reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(eglGetProcAddress("eglDestroyImageKHR"));
    s_glEGLImageTargetTexture2DOES =
        reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
            eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    if (!s_eglCreateImageKHR || !s_eglDestroyImageKHR || !s_glEGLImageTargetTexture2DOES) {
        ALOGE("failed to load EGL/GL extension functions");
        return false;
    }
    return true;
}

static bool checkGlError(const char* step) {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        ALOGE("%s: gl error=0x%x", step, err);
        return false;
    }
    return true;
}

static bool initEgl(EglContextHolder* env, bool is1010102) {
    ALOGD("%s enter, is1010102=%d", __func__, is1010102);
    env->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (env->display == EGL_NO_DISPLAY) {
        ALOGE("eglGetDisplay failed");
        return false;
    }

    if (!eglInitialize(env->display, nullptr, nullptr)) {
        ALOGE("eglInitialize failed");
        return false;
    }

    const EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RED_SIZE,
        is1010102? 10 : 8,
        EGL_GREEN_SIZE,
        is1010102? 10 : 8,
        EGL_BLUE_SIZE,
        is1010102? 10 : 8,
        EGL_ALPHA_SIZE,
        is1010102?  2 : 8,
        EGL_NONE
    };

    EGLint numConfigs = 0;
    if (!eglChooseConfig(env->display, configAttribs, &env->config, 1, &numConfigs) || numConfigs < 1) {
        ALOGE("eglChooseConfig failed");
        return false;
    }

    const EGLint pbufferAttribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };

    env->surface = eglCreatePbufferSurface(env->display, env->config, pbufferAttribs);
    if (env->surface == EGL_NO_SURFACE) {
        ALOGE("eglCreatePbufferSurface failed");
        return false;
    }

    const EGLint ctxAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    env->context = eglCreateContext(env->display, env->config, EGL_NO_CONTEXT, ctxAttribs);
    if (env->context == EGL_NO_CONTEXT) {
        ALOGE("eglCreateContext failed");
        return false;
    }

    if (!eglMakeCurrent(env->display, env->surface, env->surface, env->context)) {
        ALOGE("eglMakeCurrent failed");
        return false;
    }

    if (!loadEglGlExtensions()) {
        return false;
    }

    return true;
}

static void destroyEgl(EglContextHolder* env) {
    if (env->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(env->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

        if (env->context != EGL_NO_CONTEXT) {
            eglDestroyContext(env->display, env->context);
        }
        if (env->surface != EGL_NO_SURFACE) {
            eglDestroySurface(env->display, env->surface);
        }
        eglTerminate(env->display);
    }

    env->display = EGL_NO_DISPLAY;
    env->surface = EGL_NO_SURFACE;
    env->context = EGL_NO_CONTEXT;
    env->config = nullptr;
}

static void destroyImageTarget(EGLDisplay display, ImageTarget* t) {
    if (t->fbo) {
        glDeleteFramebuffers(1, &t->fbo);
        t->fbo = 0;
    }
    if (t->tex) {
        glDeleteTextures(1, &t->tex);
        t->tex = 0;
    }
    if (t->image != EGL_NO_IMAGE_KHR) {
        s_eglDestroyImageKHR(display, t->image);
        t->image = EGL_NO_IMAGE_KHR;
    }
}

static bool createImageTargetFromGraphicBuffer(EGLDisplay display,
                                               const sp<GraphicBuffer>& buffer,
                                               ImageTarget* outTarget) {
    if (buffer == nullptr || outTarget == nullptr) {
        ALOGE("createImageTargetFromGraphicBuffer invalid params");
        return false;
    }

    EGLClientBuffer clientBuffer =
        static_cast<EGLClientBuffer>(buffer->getNativeBuffer());

    const EGLint attrs[] = { EGL_NONE };
    outTarget->image = s_eglCreateImageKHR(
        display,
        EGL_NO_CONTEXT,
        EGL_NATIVE_BUFFER_ANDROID,
        clientBuffer,
        attrs);

    if (outTarget->image == EGL_NO_IMAGE_KHR) {
        ALOGE("eglCreateImageKHR failed, err=%d", eglGetError());
        return false;
    }

    glGenTextures(1, &outTarget->tex);
    glBindTexture(GL_TEXTURE_2D, outTarget->tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    s_glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, outTarget->image);
    if (!checkGlError("glEGLImageTargetTexture2DOES")) {
        destroyImageTarget(display, outTarget);
        return false;
    }

    glGenFramebuffers(1, &outTarget->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, outTarget->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER,
                           GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D,
                           outTarget->tex,
                           0);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        ALOGE("FBO incomplete: 0x%x", status);
        destroyImageTarget(display, outTarget);
        return false;
    }

    return true;
}

static bool blitFboToFbo(GLuint srcFbo, GLuint dstFbo, int32_t width, int32_t height, bool flipY = false) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);

    if (flipY) {
        glBlitFramebuffer(
            0, 0, width, height,
            0, height, width, 0,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
    } else {
        glBlitFramebuffer(
            0, 0, width, height,
            0, 0, width, height,
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST);
    }

    return checkGlError("glBlitFramebuffer");
}

int copyBuffer_gles(sp<GraphicBuffer> inputBuffer,
                    sp<GraphicBuffer> outBuffer,
                    int use_egl_10bit) {

    ALOGD("%s enter", __func__);
    const uint32_t width = inputBuffer->getWidth();
    const uint32_t height = inputBuffer->getHeight();
    const PixelFormat format = inputBuffer->getPixelFormat();
    int ret = -1;
    EglContextHolder egl;
    ImageTarget srcTarget;
    ImageTarget dstTarget;
    bool is1010102 = false;
    if (use_egl_10bit == 1)
        is1010102 = (format == PIXEL_FORMAT_RGBA_1010102);

    do {

        if (!initEgl(&egl, is1010102)) {
            ALOGE("%s initEgl failed",  __func__);
            break;
        }

        // const uint32_t usage = inputBuffer->usage | GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN;
        // sp<GraphicBuffer> outBuffer = new GraphicBuffer(width, height, format, inputBuffer->layerCount, usage);
        // ALOGD("%s outBufferInfo w:%d, h:%d, s:%d, fmt:%d(%s)", __func__,
        //     outBuffer->width, outBuffer->height, outBuffer->stride,
        //     outBuffer->format, decodePixelFormat(outBuffer->format).c_str());
        // if (outBuffer->initCheck() != NO_ERROR) {
        //     ALOGE("create outBuffer failed");
        //     break;
        // }

        bool ok = createImageTargetFromGraphicBuffer(egl.display, inputBuffer, &srcTarget);
        if (!ok) {
            ALOGE("%s createImageTargetFromGraphicBuffer(src) failed",  __func__);
            break;
        }

        ok = createImageTargetFromGraphicBuffer(egl.display, outBuffer, &dstTarget);
        if (!ok) {
            ALOGE("%s createImageTargetFromGraphicBuffer(dst) failed",  __func__);
            break;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, srcTarget.fbo);
        GLenum readStatus = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER);
        if (readStatus != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE("%s src read framebuffer incomplete: 0x%x",  __func__,readStatus);
            break;
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstTarget.fbo);
        GLenum drawStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
        if (drawStatus != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE("%s dst draw framebuffer incomplete: 0x%x",  __func__, drawStatus);
            break;
        }

        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, width, height);

        if (!blitFboToFbo(srcTarget.fbo, dstTarget.fbo, width, height)) {
            ALOGE("%s blitFboToFbo failed",  __func__);
            break;
        }

        glFinish();
        // dumpGraphicBuffer(outBuffer, dumpPath, ".raw");
        ret = 0;
    } while (false);

    destroyImageTarget(egl.display, &dstTarget);
    destroyImageTarget(egl.display, &srcTarget);
    destroyEgl(&egl);
    ALOGD("%s leave with ret=%d", __func__, ret);
    return ret;
}
