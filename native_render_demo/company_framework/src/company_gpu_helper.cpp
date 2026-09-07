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
#define LOG_TAG "CompanyFramework"

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

#include <binder/IPCThreadState.h>
#include <utils/Errors.h>
#include <utils/Log.h>

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

#include "company_gpu_helper.h"

#define ANIM_PATH_MAX 255
#define STR(x)   #x
#define STRTO(x) STR(x)

using namespace android;

namespace company {

using ui::DisplayMode;

static const char U_TEXTURE[] = "uTexture";
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
    varying highp vec2 vUv;
    void main() {
        vec4 color = texture2D(uTexture, vUv);
        gl_FragColor = vec4(color.x, color.y, color.z, 1.0) * color.a;
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

CompanyGPUHelper::CompanyGPUHelper() {
    ALOGD("%s enter", __func__);
}

CompanyGPUHelper::~CompanyGPUHelper() {
    ALOGD("%s enter", __func__);
    deleteGPUBuffer();
    if (m_backup_pixels) {
        free(m_backup_pixels);
        m_backup_pixels = nullptr;
    }
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

void CompanyGPUHelper::initShaders() {
    ALOGD("%s enter", __func__);
    GLuint vertexShader        = compileShader(GL_VERTEX_SHADER, (const GLchar *)VERTEX_SHADER_SOURCE);
    GLuint imageFragmentShader = compileShader(GL_FRAGMENT_SHADER, (const GLchar *)IMAGE_FRAG_SHADER_SOURCE);

    // Initialize image shader.
    mImageShader = linkShader(vertexShader, imageFragmentShader);
    GLint positionLocation = glGetAttribLocation(mImageShader, A_POSITION);
    GLint uvLocation = glGetAttribLocation(mImageShader, A_UV);
    mImageTextureLocation = glGetUniformLocation(mImageShader, U_TEXTURE);
    glEnableVertexAttribArray(positionLocation);
    glVertexAttribPointer(positionLocation, 2,  GL_FLOAT, GL_FALSE, 0, quadPositions);
    glVertexAttribPointer(uvLocation, 2, GL_FLOAT, GL_FALSE, 0, quadUVs);
    glEnableVertexAttribArray(uvLocation);
}

///////////////// OpenGL draw 相关 ///////////////
float mapLinear(float x, float a1, float a2, float b1, float b2) {
    return b1 + ( x - a1 ) * ( b2 - b1 ) / ( a2 - a1 );
}

void CompanyGPUHelper::drawTexturedQuad(float xStart, float yStart, float width, float height) {
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

status_t CompanyGPUHelper::initTexture(Texture* texture, void* pixels, int w, int h, int pixel_format) {
    ALOGD("%s enter", __func__);
    if (!pixels) { // 如果传入的pixel是空的，先用一个临时的buffer作为texture
        ALOGE("%s pixels NULL", __func__);
        m_backup_pixels = (void *)malloc(w * h * 4);
        memset(m_backup_pixels, 0, w * h * 4);
        pixels = m_backup_pixels;
        pixel_format = HAL_PIXEL_FORMAT_RGBA_8888;
    }

    texture->w = w;
    texture->h = h;

    glGenTextures(1, &texture->name);
    glBindTexture(GL_TEXTURE_2D, texture->name);

    switch (pixel_format) {
        case HAL_PIXEL_FORMAT_RGBA_8888:
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA,
                    GL_UNSIGNED_BYTE, pixels);
            break;
        case HAL_PIXEL_FORMAT_RGB_565:
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

EGLConfig CompanyGPUHelper::getEglConfig(const EGLDisplay& display) {
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

status_t CompanyGPUHelper::initOpenGL(int width, int height, int pixel_format) {
    ALOGD("%s enter", __func__);
    // 1. 创建 BufferQueue 对
    sp<IGraphicBufferProducer> producer;
    sp<IGraphicBufferConsumer> consumer;
    BufferQueue::createBufferQueue(&producer, &consumer);
    ALOGD("%s display width=%d, height=%d", __func__, width, height);
    // 2. 配置 Consumer 端 BufferQueue
    consumer->setDefaultBufferSize(width, height);
    consumer->setDefaultBufferFormat(pixel_format);
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
    native_window_set_buffers_dimensions(window, width, height);
    native_window_set_buffers_format(window, pixel_format);
    native_window_set_buffer_count(window, 3);

    // initialize opengl and egl
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);
    EGLConfig config = getEglConfig(display);
    EGLSurface surface = eglCreateWindowSurface(display, config, window, nullptr);
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

    glViewport(0, 0, mWidth, mHeight);
    glScissor(0, 0, mWidth, mHeight);

    ALOGD("%s init done, w=%d, h=%d", __func__, w, h);
    return NO_ERROR;
}

int CompanyGPUHelper::drawOnece(void* pixels, int width, int height, int pixel_format) {
    ALOGD("%s enter", __func__);
    glActiveTexture(GL_TEXTURE0);
    initTexture(&mTexture, pixels, width, height, pixel_format);

    // clear screen
    glDisable(GL_DITHER);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glUseProgram(mImageShader);

    glClearColor(0,0,0,1);
    glClear(GL_COLOR_BUFFER_BIT);
    EGLBoolean res = eglSwapBuffers(mDisplay, mSurface);
    if (res == EGL_FALSE) {
        ALOGE("%s eglSwapBuffers1 fail", __func__);
        return -1;
    }
    status_t err = 0;
    if (mGLConsumer != nullptr) {
        err = mGLConsumer->updateTexImage();// 每次swapbuffers后都要updatetextImage
        if (err < 0) {
            ALOGE("GLConsumer::updateTexImage error: %d\n", err);
            return -1;
        }
    }
    ALOGD("%s frame ready", __func__);

    ALOGD("%s frame begin", __func__);
    const GLint xc = (mWidth  - mTexture.w) / 2;
    const GLint yc = (mHeight - mTexture.h) / 2;
    glBindTexture(GL_TEXTURE_2D, mTexture.name);
    drawTexturedQuad(xc, yc, mTexture.w, mTexture.h);

    ALOGD("frame end");
    res = eglSwapBuffers(mDisplay, mSurface);
    if (res == EGL_FALSE) {
        ALOGE("%s eglSwapBuffers2 fail", __func__);
        return -1;
    }

    ALOGD("swap buffer done");
    if (mGLConsumer != nullptr) {
        err = mGLConsumer->updateTexImage();
        if (err < 0) {
            ALOGE("GLConsumer::updateTexImage error: %d\n", err);
            return -1;
        }
    }
    return 0;
}

sp<GraphicBuffer> CompanyGPUHelper::createGPUBuffer(void* pixels, int width, int height, int pixel_format) {
    ALOGD("%s enter", __func__);

    initOpenGL(width, height, pixel_format);
    initShaders();
    drawOnece(pixels, width, height, pixel_format);

    sp<GraphicBuffer> buffer = mGLConsumer->getCurrentBuffer();
    if (buffer == nullptr) {
        ALOGE("acquireBuffer failed");
        return nullptr;
    }
    if (true) {
        int width  = buffer->getWidth();
        int height = buffer->getHeight();
        int stride = buffer->getStride();
        int format = buffer->getPixelFormat();
        uint64_t usage  = buffer->getUsage();
        uint64_t totalSize = 0;
        GraphicBufferMapper::get().getAllocationSize(buffer->handle, &totalSize);
        ALOGD("buffer width %d, height %d, size: %u", width, height, (uint32_t)totalSize);
        ALOGD("buffer stride %d, format %d, usage: 0x%" PRIx64, stride, format, usage);
    }

    return buffer;
}

void CompanyGPUHelper::deleteGPUBuffer() {
    ALOGD("%s enter", __func__);
    glDeleteTextures(1, &mTexture.name);
    eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(mDisplay, mContext);
    eglDestroySurface(mDisplay, mSurface);
    eglTerminate(mDisplay);
    eglReleaseThread();
}

} // namespace company
