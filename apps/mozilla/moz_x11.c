#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/Intrinsic.h>

#include "fitz.h"
#include "mupdf.h"
#include "pdfapp.h"

#include "npapi.h"
#include "npfunctions.h"

extern int ximage_init(Display *display, int screen, Visual *visual);
extern void ximage_blit(Drawable d, GC gc, int dstx, int dsty,
	unsigned char *srcdata,
	int srcx, int srcy, int srcw, int srch, int srcstride);

#define PLUGIN_NAME "MuPDF Plug-in"
#define PLUGIN_VERSION "0.0.1"

#define DEBUG(FORMAT, ...) fprintf(stderr, FORMAT "\n", ##__VA_ARGS__)

static NPNetscapeFuncs *npn;
static Atom XA_TARGETS;
static Atom XA_TIMESTAMP;
static Atom XA_UTF8_STRING;

typedef struct
{
    NPP instance;
    Window window;
    Display *display;
    Cursor xcarrow, xchand, xcwait;
    unsigned long bgcolor, fgcolor;
    GC gc;
    char copylatin1[1024*16];
    char copyutf8[1024*48];
    Time copytime;
    int justcopied;
} pdfmoz_t;

/* pdfapp callbacks */

void winwarn(pdfapp_t *app, char* msg)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

    npn->status(moz->instance, msg);
}

void winerror(pdfapp_t *app, fz_error error)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

    fz_catch(error, "unhandled error");
    npn->status(moz->instance, "mupdf error");
}

char *winpassword(pdfapp_t *app, char *filename)
{
    return "";
}

void wintitle(pdfapp_t *app, char *s)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

    npn->status(moz->instance, s);
}

void winhelp(pdfapp_t *app)
{
    winopenuri(app, "http://mupdf.com/");
}

void winclose(pdfapp_t *app)
{
    /* No-op; we're not allowed to close the browser window. */
}

void wincursor(pdfapp_t *app, int cursor)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    Display *dpy = moz->display;
    Window window = moz->window;

    switch (cursor)
    {
    case ARROW:
        XDefineCursor(dpy, window, moz->xcarrow);
        break;
    case HAND:
        XDefineCursor(dpy, window, moz->xchand);
        break;
    case WAIT:
        XDefineCursor(dpy, window, moz->xcwait);
        break;
    }
    XFlush(dpy);
}

void winresize(pdfapp_t *app, int w, int h)
{
    /* No-op; we're not allowed to resize the browser window. */
}

static void search_status(pdfapp_t *app)
{
    if (app->isediting)
    {
        pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
        const char *label = "Search: ";
        char buf[sizeof(label) + strlen(app->search)];

        sprintf(buf, "%s%s", label, app->search);
        npn->status(moz->instance, buf);
    }
}

void winrepaint(pdfapp_t *app)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    Display *dpy = moz->display;
    Window window = moz->window;
    GC gc = moz->gc;

    if (!app->image)
        return;

    int x0 = app->panx;
    int y0 = app->pany;
    int x1 = app->panx + app->image->w;
    int y1 = app->pany + app->image->h;

#define fillrect(x, y, w, h) \
    if (w > 0 && h > 0) \
        XFillRectangle(dpy, window, gc, x, y, w, h)

    XSetForeground(dpy, gc, moz->bgcolor);
    fillrect(0, 0, x0, app->winh);
    fillrect(x1, 0, app->winw - x1, app->winh);
    fillrect(0, 0, app->winw, y0);
    fillrect(0, y1, app->winw, app->winh - y1);

    XSetForeground(dpy, gc, moz->fgcolor);
    fillrect(x0+2, y1, app->image->w, 2);
    fillrect(x1, y0+2, 2, app->image->h);

    if (app->iscopying || moz->justcopied)
    {
        pdfapp_invert(app, app->selr);
        moz->justcopied = 1;
    }

    pdfapp_inverthit(app);

    if (app->image->n == 4)
        ximage_blit(window, gc,
                    x0, y0,
                    app->image->samples,
                    0, 0,
                    app->image->w,
                    app->image->h,
                    app->image->w * app->image->n);
    else if (app->image->n == 2)
    {
        int i = app->image->w*app->image->h;
        unsigned char *color = malloc(i*4);
        if (color != NULL)
        {
            unsigned char *s = app->image->samples;
            unsigned char *d = color;
            for (; i > 0 ; i--)
            {
                d[2] = d[1] = d[0] = *s++;
                d[3] = *s++;
                d += 4;
            }
            ximage_blit(window, gc,
                        x0, y0,
                        color,
                        0, 0,
                        app->image->w,
                        app->image->h,
                        app->image->w * 4);
            free(color);
        }
    }

    pdfapp_inverthit(app);

    if (app->iscopying || moz->justcopied)
    {
        pdfapp_invert(app, app->selr);
        moz->justcopied = 1;
    }

    search_status(app);
}

void windocopy(pdfapp_t *app)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    unsigned short copyucs2[16 * 1024];
    char *latin1 = moz->copylatin1;
    char *utf8 = moz->copyutf8;
    unsigned short *ucs2;
    int ucs;

    pdfapp_oncopy(app, copyucs2, 16 * 1024);

    for (ucs2 = copyucs2; ucs2[0] != 0; ucs2++)
    {
        ucs = ucs2[0];

        utf8 += runetochar(utf8, &ucs);

        if (ucs < 256)
            *latin1++ = ucs;
        else
            *latin1++ = '?';
    }

    *utf8 = 0;
    *latin1 = 0;

    XSetSelectionOwner(moz->display, XA_PRIMARY, moz->window, moz->copytime);

    moz->justcopied = 1;
}

static void onselreq(pdfapp_t *app, Window requestor, Atom selection,
                     Atom target, Atom property, Time time)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    Display *dpy = moz->display;
    XEvent nevt;

    if (property == None)
        property = target;

    nevt.xselection.type = SelectionNotify;
    nevt.xselection.send_event = True;
    nevt.xselection.display = dpy;
    nevt.xselection.requestor = requestor;
    nevt.xselection.selection = selection;
    nevt.xselection.target = target;
    nevt.xselection.property = property;
    nevt.xselection.time = time;

    if (target == XA_TARGETS)
    {
        Atom atomlist[4];
        atomlist[0] = XA_TARGETS;
        atomlist[1] = XA_TIMESTAMP;
        atomlist[2] = XA_STRING;
        atomlist[3] = XA_UTF8_STRING;
        XChangeProperty(dpy, requestor, property, target,
			32, PropModeReplace,
			(unsigned char *) atomlist,
                        sizeof(atomlist)/sizeof(Atom));
    }
    else if (target == XA_STRING)
    {
        XChangeProperty(dpy, requestor, property, target,
			8, PropModeReplace,
			(unsigned char *) moz->copylatin1,
                        strlen(moz->copylatin1));
    }
    else if (target == XA_UTF8_STRING)
    {
        XChangeProperty(dpy, requestor, property, target,
			8, PropModeReplace,
			(unsigned char *) moz->copyutf8,
                        strlen(moz->copyutf8));
    }
    else
    {
        nevt.xselection.property = None;
    }

    XSendEvent(dpy, requestor, False, SelectionNotify, &nevt);
}

void winreloadfile(pdfapp_t *app)
{
    /* No-op; reloading should be done through the browser. */
}

void winopenuri(pdfapp_t *app, char *buf)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

    npn->geturl(moz->instance, buf, "_blank");
}

static void
handle_event(Widget widget, pdfapp_t *app, XEvent *event, Boolean *b)
{
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    Display *dpy = moz->display;
    Window window = moz->window;
    int len;
    char buf[128];
    KeySym keysym;

    if (!app->image)
        return;

    switch (event->type)
    {
    case Expose:
        /* Ignore duplicate expose events. */
        while (XCheckTypedWindowEvent(dpy, window, Expose, event));
        winrepaint(app);
        break;

    case KeyPress:
        moz->justcopied = false;
        len = XLookupString(&event->xkey, buf, sizeof buf, &keysym, NULL);

        switch (keysym)
        {
        case XK_Escape:
            len = 1; buf[0] = '\033';
            break;
        case XK_Up:
            len = 1; buf[0] = 'k';
            break;
        case XK_Down:
            len = 1; buf[0] = 'j';
            break;
        case XK_Left:
            len = 1; buf[0] = 'b';
            break;
        case XK_Right:
            len = 1; buf[0] = ' ';
            break;
        case XK_Page_Up:
            len = 1; buf[0] = ',';
            break;
        case XK_Page_Down:
            len = 1; buf[0] = '.';
            break;
        }
        if (len)
            pdfapp_onkey(app, buf[0]);
        search_status(app);
        break;

    case MotionNotify:
        /* Coalesce motion notifications. */
        while (XCheckTypedWindowEvent(dpy, window, MotionNotify, event));
        pdfapp_onmouse(app, event->xbutton.x, event->xbutton.y,
                       event->xbutton.button, event->xbutton.state, 0);
        break;

    case ButtonPress:
        moz->justcopied = false;
        pdfapp_onmouse(app, event->xbutton.x, event->xbutton.y,
                       event->xbutton.button, event->xbutton.state, 1);
        break;

    case ButtonRelease:
        moz->copytime = event->xbutton.time;
        pdfapp_onmouse(app, event->xbutton.x, event->xbutton.y,
                       event->xbutton.button, event->xbutton.state, -1);
        break;

    case SelectionRequest:
        onselreq(app,
                 event->xselectionrequest.requestor,
                 event->xselectionrequest.selection,
                 event->xselectionrequest.target,
                 event->xselectionrequest.property,
                 event->xselectionrequest.time);
        break;

    default:
        break;
    }
}

/* NPAPI Interface */

NPError
NPP_New(NPMIMEType mime, NPP instance, uint16_t mode,
	int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
{
    pdfapp_t *app;
    pdfmoz_t *moz;

    app = fz_malloc(sizeof(pdfapp_t));
    if (!app)
	return NPERR_OUT_OF_MEMORY_ERROR;
    memset(app, 0, sizeof(pdfapp_t));
    pdfapp_init(app);

    moz = fz_malloc(sizeof(pdfmoz_t));
    if (!moz)
        return NPERR_OUT_OF_MEMORY_ERROR;
    memset(moz, 0, sizeof(pdfmoz_t));

    moz->instance = instance; /* the nav-bone's connected to the moz-bone... */
    app->userdata = moz; /* the moz-bone's connected to the app-bone... */
    instance->pdata = app; /* the app-bone's connected to the nav-bone... */
    return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData **saved)
{
    pdfapp_t *app = instance->pdata;
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    Display *dpy = moz->display;

    XFreeCursor(dpy, moz->xcarrow);
    XFreeCursor(dpy, moz->xchand);
    XFreeCursor(dpy, moz->xcwait);
    XFreeGC(dpy, moz->gc);
    fz_free(moz);
    app->userdata = NULL;

    pdfapp_close(app);
    fz_free(app);
    instance->pdata = NULL;

    return NPERR_NO_ERROR;
}

#define XWindow(npwindow) ((Window) ((npwindow)->window))

NPError
NPP_SetWindow(NPP instance, NPWindow *window)
{
    pdfapp_t *app = (pdfapp_t *) instance->pdata;
    pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
    NPSetWindowCallbackStruct *ws_info = (NPSetWindowCallbackStruct *)(window->ws_info);

    if (moz->window == XWindow(window)) {
        pdfapp_onresize(app, window->width, window->height);
        return NPERR_NO_ERROR;
    } else {
        Display *dpy = ws_info->display;
        int screen = DefaultScreen(dpy);
        Widget widget = XtWindowToWidget(dpy, XWindow(window));

        XA_TARGETS = XInternAtom(dpy, "TARGETS", False);
        XA_TIMESTAMP = XInternAtom(dpy, "TIMESTAMP", False);
        XA_UTF8_STRING = XInternAtom(dpy, "UTF8_STRING", False);

        moz->display = dpy;
        moz->window = XWindow(window);
        moz->gc = XCreateGC(dpy, moz->window, 0, nil);

        ximage_init(dpy, screen, ws_info->visual);

        if (moz->xcarrow != None) XFreeCursor(dpy, moz->xcarrow);
        if (moz->xchand != None) XFreeCursor(dpy, moz->xchand);
        if (moz->xcwait != None) XFreeCursor(dpy, moz->xcwait);
        moz->xcarrow = XCreateFontCursor(dpy, XC_left_ptr);
        moz->xchand = XCreateFontCursor(dpy, XC_hand2);
        moz->xcwait = XCreateFontCursor(dpy, XC_watch);

        {
            Colormap colormap = ws_info->colormap;
            XColor bgcolor, fgcolor;

            bgcolor.red = 0x7000;
            bgcolor.green = 0x7000;
            bgcolor.blue = 0x7000;

            fgcolor.red = 0x4000;
            fgcolor.green = 0x4000;
            fgcolor.blue = 0x4000;

            XAllocColor(dpy, colormap, &bgcolor);
            XAllocColor(dpy, colormap, &fgcolor);

            moz->bgcolor = bgcolor.pixel;
            moz->fgcolor = fgcolor.pixel;
        }

        app->winh = window->height;
        app->winw = window->width;
        app->resolution = (((double) DisplayWidth(dpy, screen) * 25.4) /
                           ((double) DisplayWidthMM(dpy, screen)));

        if (widget)
        {
            long event_mask = ExposureMask | KeyPressMask | PointerMotionMask |
                              ButtonPressMask | ButtonReleaseMask;
            XSelectInput(dpy, moz->window, event_mask);
            XtAddEventHandler(widget, event_mask, False,
                              (XtEventHandler) handle_event, app);
        }
    }
    return NPERR_NO_ERROR;
}

NPError
NPP_NewStream(NPP instance, NPMIMEType type,
	NPStream* stream, NPBool seekable,
	uint16_t* stype)
{
    *stype = NP_ASFILEONLY;
    return NPERR_NO_ERROR;
}

NPError
NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return NPERR_NO_ERROR;
}

int32_t
NPP_WriteReady(NPP instance, NPStream* stream)
{
    return 0x7FFFFFFF;
}

int32_t
NPP_Write(NPP instance, NPStream* stream,
          int32_t offset, int32_t len, void* buffer)
{
    return len;
}

void
NPP_StreamAsFile(NPP instance, NPStream* stream, const char* filename)
{
    pdfapp_t *app = (pdfapp_t *) instance->pdata;
    int fd;

    fd = open(filename, O_BINARY | O_RDONLY, 0666);
    if (fd < 0)
        winwarn(app, "cannot open file");
    app->pageno = 1;
    pdfapp_open(app, (char *) filename, fd);
}

void
NPP_Print(NPP instance, NPPrint* platformPrint)
{
    /* No-op; not yet supported. */
}

int16_t
NPP_HandleEvent(NPP instance, void* event)
{
    /* Only used on Mac. */
    return 0;
}

void
NPP_URLNotify(NPP instance, const char* url,
	NPReason reason, void* notifyData)
{
}

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    return NPERR_NO_ERROR;
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_NO_ERROR;
}

void
NPP_Shutdown(void)
{
}

NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs *npn_funcs, NPPluginFuncs *npp_funcs)
{
    npn = npn_funcs;

    npp_funcs->newp = NPP_New;
    npp_funcs->destroy = NPP_Destroy;
    npp_funcs->setwindow = NPP_SetWindow;
    npp_funcs->newstream = NPP_NewStream;
    npp_funcs->destroystream = NPP_DestroyStream;
    npp_funcs->asfile = NPP_StreamAsFile;
    npp_funcs->writeready = NPP_WriteReady;
    npp_funcs->write = NPP_Write;
    npp_funcs->print = NPP_Print;
    npp_funcs->event = NPP_HandleEvent;
    npp_funcs->urlnotify = NPP_URLNotify;
    npp_funcs->getvalue = NPP_GetValue;
    npp_funcs->setvalue = NPP_SetValue;

    return NPERR_NO_ERROR;
}

NP_EXPORT(char *)
NP_GetMIMEDescription()
{
    return "application/pdf:pdf:Portable Document Format;"
           "application/x-pdf:pdf:Portable Document Format";
}

NP_EXPORT(char *)
NP_GetPluginVersion()
{
    return PLUGIN_VERSION;
}
