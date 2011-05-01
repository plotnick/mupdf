#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>

#define POOLSIZE 4
#define WIDTH 256
#define HEIGHT 256

typedef void (*ximage_convert_func_t)
(
	const unsigned char *src,
	int srcstride,
	unsigned char *dst,
	int dststride,
	int w,
	int h
	);

typedef struct
{
	Display *display;
	int screen;
	XVisualInfo visual;
	Colormap colormap;

	int bitsperpixel;
	int mode;

	XColor rgbcube[256];

	ximage_convert_func_t convert_func;

	int useshm;
	int shmcode;
	XImage *pool[POOLSIZE];
	/* MUST exist during the lifetime of the shared ximage according to the
	xc/doc/hardcopy/Xext/mit-shm.PS.gz */
	XShmSegmentInfo shminfo[POOLSIZE];
	int lastused;
} ximage_info;

ximage_info *ximage_init(Display *display, int screen, Visual *visual);
void ximage_free_info(ximage_info *info);
int ximage_get_depth(ximage_info *info);
Visual *ximage_get_visual(ximage_info *info);
Colormap ximage_get_colormap(ximage_info *info);
void ximage_blit(ximage_info *info,
	Drawable d, GC gc, int dstx, int dsty,
	unsigned char *srcdata,
	int srcx, int srcy, int srcw, int srch, int srcstride);
