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

    /* initialize the dumb-buffer with blue-color */
    /* memset(bo->vaddr, 0xff, bo->size); */
    for(int i = 0; i < (bo->size / 4); i++)
            bo->vaddr[i] = value; //0x0000ff;

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
    /* 声明对象 */
    int fd;
    drmModeConnector *conn;
    drmModeRes *res;
    uint32_t conn_id;
    uint32_t crtc_id;
	struct buffer_object buf;
	
	int crtc_index = 0;
	int conn_index = 0;
	int mode_index = 0;
	uint32_t color_value = 0xff;

	/* 获取命令行参数 */
	int ret = parse_args(argc, argv, &crtc_index, &conn_index, &mode_index, &color_value);
	if (ret != 0) {
		return -1;
	}

    /* 开启DRM设备 */
    fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
 
    /* 获取有关DRM设备的crtc、编码器和连接器的信息。但是，该函数不报告平面资源。*/
    res = drmModeGetResources(fd);


    crtc_id = res->crtcs[crtc_index];
    conn_id = res->connectors[conn_index];
 
    /* 获取连接器的信息。*/
    conn = drmModeGetConnector(fd, conn_id);
 
    buf.width = conn->modes[mode_index].hdisplay;
    buf.height = conn->modes[mode_index].vdisplay;
 
    modeset_create_fb(fd, &buf, color_value);
 
    /* 设置CRTC配置，开启显示 */
    drmModeSetCrtc(fd, crtc_id, buf.fb_id, 0, 0, &conn_id, 1, &conn->modes[mode_index]);
 
    /* 等待 */
    getchar();
 
    modeset_destroy_fb(fd, &buf);
 
    /* 释放一个连接器。*/
    drmModeFreeConnector(conn);
 
    /* 释放资源信息结构。*/
    drmModeFreeResources(res);
 
    close(fd);
 
    return 0;
}

