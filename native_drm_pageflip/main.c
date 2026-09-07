#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xf86drm.h"
#include "xf86drmMode.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

struct buffer_object {
    uint32_t width;
    uint32_t height;
    uint32_t pitch; //表示Framebuffer的一行占有的字节数量
    uint32_t handle;//可以绘制到的缓冲区对象的 DRM 句柄
    uint32_t size;  //Framebuffer在内存中占有的总字节数量
    uint32_t *vaddr;//指向内存映射缓冲区的指针
    uint32_t fb_id; //缓冲区对象作为扫描输出缓冲区的freambuffer句柄
};

static struct buffer_object buf[2];

static int modeset_create_fb(int fd, struct buffer_object *bo, uint32_t value)
{
    struct drm_mode_create_dumb create = {};
    struct drm_mode_map_dumb map = {};

    /* create a dumb-buffer, the pixel format is XRG8888 */
    create.width = bo->width;
    create.height = bo->height;
    create.bpp = 32;    //Bits Per Pixel，像素深度
    /* drmIOCTL：发出DRM输入/输出控制（IOCTL） */
    drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);

    /* bind the dumb-buffer to an FB object */
    bo->pitch = create.pitch;
    bo->size = create.size;
    bo->handle = create.handle;
    /* drmModeAddFB：创建一个framebuffer，该函数使用指定的缓冲区对象作为内存后备存储，以指定的大小和格式创建一个framebuffer */
    drmModeAddFB(fd, bo->width, bo->height, 24, 32, bo->pitch, bo->handle, &bo->fb_id);

    /* map the dumb-buffer to userspace */
    map.handle = create.handle;
    drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map);

    /* 把文件映射到进程的虚拟内存空间。通过对这段内存的读取和修改，可以实现对文件的读取和修改 */
    bo->vaddr = mmap(0, create.size, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, map.offset);

	printf("create buffer: %dx%d, value=0x%x\n", bo->width, bo->height, value);
    /* initialize the dumb-buffer with blue-color */
    /* memset(bo->vaddr, 0xff, bo->size); */
    for(int i = 0; i < (bo->size / 4); i++)
            bo->vaddr[i] = value;

    return 0;
}

static void modeset_destroy_fb(int fd, struct buffer_object *bo)
{
    struct drm_mode_destroy_dumb destroy = {};

    /* 破坏一个framebuffer，销毁(释放)由drmModeAddFB或drmModeAddFB2分配的帧缓冲区。*/
    drmModeRmFB(fd, bo->fb_id);

    /* 解除映射函数 */
    munmap(bo->vaddr, bo->size);

    destroy.handle = bo->handle;
    drmIoctl(fd,DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

static void modeset_page_flip_handler(int fd, uint32_t frame,
                    uint32_t sec, uint32_t usec,
                    void *data)
{
    static int i = 0;
    static int count = 0;
	uint32_t crtc_id = *(uint32_t *)data;

    i ^= 1;
	count++;

	if (count < 100)
		return;

	count = 0;
	printf("flip enter, i=%d\n", i);
    /* 在指定的CRTC上安排页面交换。默认情况下，CRTC将被重新编程，以在下一次垂直刷新后显示指定的framebuffer。*/
    drmModePageFlip(fd, crtc_id, buf[i].fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, data);

    //usleep(1000000);
}

static int terminate = 0;
static void sigint_handler(int arg)
{
    terminate = 1;
}

static int parse_args(int argc, char **argv, 
	int* crtc_index, int* conn_index, int *mode_index, uint32_t* color)
{
	if (argc != 5)
	{
		printf("usages:\n  %s [crtc_index] [conn_index] [mode_index] [0x-color]\n", argv[0]);
		return -1;
	}
	
	*crtc_index = atoi(argv[1]);
	*conn_index = atoi(argv[2]);
	*mode_index = atoi(argv[3]);
	sscanf(argv[4], "%x", color);
	
	return 0;
}

int main(int argc, char **argv)
{
    int fd;
    drmEventContext ev = {};
    drmModeConnector *conn;
    drmModeRes *res;
    uint32_t conn_id;
    uint32_t crtc_id;
	int crtc_index = 0;
	int conn_index = 0;
	int mode_index = 0;
	uint32_t color_value = 0xff;

	/* 获取命令行参数 */
	int ret = parse_args(argc, argv, &crtc_index, &conn_index, &mode_index, &color_value);
	if (ret != 0) {
		return -1;
	}

    /* register CTRL+C terminate interrupt */
    /* 信号处理函数 */
    signal(SIGINT, sigint_handler);
 
    ev.version = DRM_EVENT_CONTEXT_VERSION;
    ev.page_flip_handler = modeset_page_flip_handler;
 
    //fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
 	fd = drmOpen("msm_drm", NULL);

    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[crtc_index];
    conn_id = res->connectors[conn_index];
 
    conn = drmModeGetConnector(fd, conn_id);
    buf[0].width = conn->modes[mode_index].hdisplay;
    buf[0].height = conn->modes[mode_index].vdisplay;
    buf[1].width = conn->modes[mode_index].hdisplay;
    buf[1].height = conn->modes[mode_index].vdisplay;
 
    modeset_create_fb(fd, &buf[0], color_value);
    modeset_create_fb(fd, &buf[1], 0xffffff ^ color_value);
 
    drmModeSetCrtc(fd, crtc_id, buf[0].fb_id,
            0, 0, &conn_id, 1, &conn->modes[mode_index]);

	// 必须加这个，不然会卡住
	usleep(20000);

    drmModePageFlip(fd, crtc_id, buf[0].fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, &crtc_id);
 
    while (!terminate) {
        /* 如果发生了指定类型的事件，则调用回调函数 */
        drmHandleEvent(fd, &ev);
    }
 
    modeset_destroy_fb(fd, &buf[1]);
    modeset_destroy_fb(fd, &buf[0]);
 
    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
 
    drmClose(fd);
 
    return 0;
}

