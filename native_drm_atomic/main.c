#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "xf86drm.h"
#include "xf86drmMode.h"
#include <sys/mman.h>    // for mmap, munmap
#include <fcntl.h>       // for open, O_RDWR, O_CREAT, etc.
#include <unistd.h>      // for close, ftruncate
#include <sys/stat.h>    // for file stat information
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

static uint32_t get_property_id(int fd, drmModeObjectProperties *props,
                const char *name)
{
    drmModePropertyPtr property;
    uint32_t i, id = 0;
 
    for (i = 0; i < props->count_props; i++) {
        property = drmModeGetProperty(fd, props->props[i]);
        if (!strcmp(property->name, name))
            id = property->prop_id;
        drmModeFreeProperty(property);
 
        if (id)
            break;
    }
 
    return id;
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
    drmModeConnector *conn;
    drmModeRes *res;
    drmModePlaneRes *plane_res;
    drmModeObjectProperties *props;
    drmModeAtomicReq *req;
    uint32_t conn_id;
    uint32_t crtc_id;
    uint32_t blob_id;
    uint32_t property_crtc_id;
    uint32_t property_mode_id;
    uint32_t property_active;
    uint32_t property_fb_id[2];
    uint32_t property_crtc_x[2];
    uint32_t property_crtc_y[2];
    uint32_t property_crtc_w[2];
    uint32_t property_crtc_h[2];
    uint32_t property_src_x[2];
    uint32_t property_src_y[2];
    uint32_t property_src_w[2];
    uint32_t property_src_h[2];
    uint32_t property_zposition[2];
	struct buffer_object buf[2];
	int crtc_index = 0;
	int conn_index = 0;
	int mode_index = 0;
	uint32_t color_value = 0xff;

	/* 获取命令行参数 */
	int ret = parse_args(argc, argv, &crtc_index, &conn_index, &mode_index, &color_value);
	if (ret != 0) {
		return -1;
	}

    //fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC);
	fd = drmOpen("msm_drm", NULL);

    res = drmModeGetResources(fd);
    crtc_id = res->crtcs[crtc_index];
    conn_id = res->connectors[conn_index];

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    plane_res = drmModeGetPlaneResources(fd);

    conn = drmModeGetConnector(fd, conn_id);
    buf[0].width = conn->modes[mode_index].hdisplay;
    buf[0].height = conn->modes[mode_index].vdisplay;
    buf[1].width = conn->modes[mode_index].hdisplay;
    buf[1].height = conn->modes[mode_index].vdisplay;

    modeset_create_fb(fd, &buf[0], color_value);
    modeset_create_fb(fd, &buf[1], 0xffffff ^ color_value);

    drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);

    props = drmModeObjectGetProperties(fd, conn_id, DRM_MODE_OBJECT_CONNECTOR);
    property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    drmModeFreeObjectProperties(props);

    props = drmModeObjectGetProperties(fd, crtc_id, DRM_MODE_OBJECT_CRTC);
    property_active = get_property_id(fd, props, "ACTIVE");
    property_mode_id = get_property_id(fd, props, "MODE_ID");
    drmModeFreeObjectProperties(props);

    drmModeCreatePropertyBlob(fd, &conn->modes[mode_index],
                sizeof(conn->modes[mode_index]), &blob_id);

    req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, crtc_id, property_active, 1);
    drmModeAtomicAddProperty(req, crtc_id, property_mode_id, blob_id);
    drmModeAtomicAddProperty(req, conn_id, property_crtc_id, crtc_id);
    drmModeAtomicCommit(fd, req, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
    drmModeAtomicFree(req);

    printf("drmModeAtomicCommit SetCrtc\n");
    getchar();

    /* get plane0 properties */
    props = drmModeObjectGetProperties(fd, plane_res->planes[0], DRM_MODE_OBJECT_PLANE);
    property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    property_fb_id[0] = get_property_id(fd, props, "FB_ID");
    property_crtc_x[0] = get_property_id(fd, props, "CRTC_X");
    property_crtc_y[0] = get_property_id(fd, props, "CRTC_Y");
    property_crtc_w[0] = get_property_id(fd, props, "CRTC_W");
    property_crtc_h[0] = get_property_id(fd, props, "CRTC_H");
    property_src_x[0] = get_property_id(fd, props, "SRC_X");
    property_src_y[0] = get_property_id(fd, props, "SRC_Y");
    property_src_w[0] = get_property_id(fd, props, "SRC_W");
    property_src_h[0] = get_property_id(fd, props, "SRC_H");
    property_zposition[0] = get_property_id(fd, props, "zposition");

    /* atomic plane0 update */
    req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_crtc_id, crtc_id);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_fb_id[0], buf[0].fb_id);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_crtc_x[0], 50);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_crtc_y[0], 50);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_crtc_w[0], 320);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_crtc_h[0], 320);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_src_x[0], 0);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_src_y[0], 0);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_src_w[0], 320 << 16);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_src_h[0], 320 << 16);
    drmModeAtomicAddProperty(req, plane_res->planes[0], property_zposition[0], 1);
    drmModeAtomicCommit(fd, req, 0, NULL);

    printf("drmModeAtomicCommit SetPlane0\n");
    getchar();

    /* get plane1 properties */
    props = drmModeObjectGetProperties(fd, plane_res->planes[1], DRM_MODE_OBJECT_PLANE);
    property_crtc_id = get_property_id(fd, props, "CRTC_ID");
    property_fb_id[1] = get_property_id(fd, props, "FB_ID");
    property_crtc_x[1] = get_property_id(fd, props, "CRTC_X");
    property_crtc_y[1] = get_property_id(fd, props, "CRTC_Y");
    property_crtc_w[1] = get_property_id(fd, props, "CRTC_W");
    property_crtc_h[1] = get_property_id(fd, props, "CRTC_H");
    property_src_x[1] = get_property_id(fd, props, "SRC_X");
    property_src_y[1] = get_property_id(fd, props, "SRC_Y");
    property_src_w[1] = get_property_id(fd, props, "SRC_W");
    property_src_h[1] = get_property_id(fd, props, "SRC_H");
    property_zposition[1] = get_property_id(fd, props, "zposition");
    drmModeFreeObjectProperties(props);

    /* atomic plane1 update */
    req = drmModeAtomicAlloc();
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_crtc_id, crtc_id);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_fb_id[1], buf[1].fb_id);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_crtc_x[1], 150);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_crtc_y[1], 150);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_crtc_w[1], 500);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_crtc_h[1], 500);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_src_x[1], 0);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_src_y[1], 0);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_src_w[1], 320 << 16);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_src_h[1], 320 << 16);
    drmModeAtomicAddProperty(req, plane_res->planes[1], property_zposition[1], 0);
    drmModeAtomicCommit(fd, req, 0, NULL);

    drmModeAtomicFree(req);

    printf("drmModeAtomicCommit SetPlane1\n");
    getchar();

    modeset_destroy_fb(fd, &buf[0]);
    modeset_destroy_fb(fd, &buf[1]);

    drmModeFreeConnector(conn);
    drmModeFreePlaneResources(plane_res);
    drmModeFreeResources(res);

    drmClose(fd);

    return 0;
}


