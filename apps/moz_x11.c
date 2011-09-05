#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"
#include "pdfapp.h"

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <gdk/gdkkeysyms.h>

#include <npapi.h>
#include <npfunctions.h>

#define PLUGIN_NAME "MuPDF Plugin"
#define PLUGIN_VERSION "0.0.2"
#define PLUGIN_DESCRIPTION "A plugin based on " \
	"<a href=\"http://mupdf.com/\">MuPDF</a>, a lightweight PDF toolkit from " \
	"<a href=\"http://www.artifex.com/\">Artifex Software, Inc.</a>"

static NPNetscapeFuncs npn;

typedef struct
{
	NPP instance;
	NPWindow *nav_window;
	GtkWidget *canvas;
	GdkDisplay *display;
	GdkCursor *arrow, *hand, *wait;
	char copyutf8[1024*48];
	Time copytime;
	int justcopied;
} pdfmoz_t;

/* pdfapp callbacks */

void winwarn(pdfapp_t *app, char* msg)
{
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	npn.status(moz->instance, msg);
}

void winerror(pdfapp_t *app, fz_error error)
{
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	fz_catch(error, "unhandled error");
	npn.status(moz->instance, "mupdf error");
}

char *winpassword(pdfapp_t *app, char *filename)
{
	return "";
}

void wintitle(pdfapp_t *app, char *s)
{
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	npn.status(moz->instance, s);
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
	GdkWindow *window = moz->canvas->window;

	switch (cursor)
	{
	case ARROW:
		gdk_window_set_cursor(window, moz->arrow);
		break;
	case HAND:
		gdk_window_set_cursor(window, moz->hand);
		break;
	case WAIT:
		gdk_window_set_cursor(window, moz->wait);
		break;
	}
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
		npn.status(moz->instance, buf);
	}
}

void winrepaint(pdfapp_t *app)
{
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
	GdkGC *bg_gc = moz->canvas->style->dark_gc[GTK_STATE_NORMAL],
		*shadow_gc = moz->canvas->style->fg_gc[GTK_STATE_NORMAL];

	if (!app->image)
		return;

	int x0 = app->panx;
	int y0 = app->pany;
	int x1 = app->panx + app->image->w;
	int y1 = app->pany + app->image->h;

#define fillrect(gc, x, y, w, h) \
	if ((w) > 0 && (h) > 0) \
		gdk_draw_rectangle(moz->canvas->window, (gc), TRUE, (x), (y), (w), (h))

	/* Fill the background. */
	fillrect(bg_gc, 0, 0, x0, app->winh);
	fillrect(bg_gc, x1, 0, app->winw - x1, app->winh);
	fillrect(bg_gc, 0, 0, app->winw, y0);
	fillrect(bg_gc, 0, y1, app->winw, app->winh - y1);

	/* Draw a half-border "shadow". */
	fillrect(shadow_gc, x0+2, y1, app->image->w, 2);
	fillrect(shadow_gc, x1, y0+2, 2, app->image->h);

	if (app->iscopying || moz->justcopied)
	{
		pdfapp_invert(app, app->selr);
		moz->justcopied = 1;
	}

	pdfapp_inverthit(app);

	if (app->image->n == 4)
		gdk_draw_rgb_32_image(moz->canvas->window,
							  moz->canvas->style->fg_gc[GTK_STATE_NORMAL],
							  x0, y0,
							  app->image->w, app->image->h,
							  GDK_RGB_DITHER_MAX,
							  app->image->samples,
							  app->image->w * app->image->n);
	else if (app->image->n == 2)
	{
		int i = app->image->w*app->image->h;
		unsigned char *gray = malloc(i);
		if (gray)
		{
			unsigned char *s = app->image->samples;
			unsigned char *d = gray;
			for (; i > 0; i--)
			{
				*d++ = *s++;
				s++;
			}
			gdk_draw_gray_image(moz->canvas->window,
								moz->canvas->style->fg_gc[GTK_STATE_NORMAL],
								x0, y0,
								app->image->w, app->image->h,
								GDK_RGB_DITHER_MAX,
								gray,
								app->image->w);
			free(gray);
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
	char *utf8 = moz->copyutf8;
	unsigned short *ucs2;
	int ucs;

	pdfapp_oncopy(app, copyucs2, 16 * 1024);

	for (ucs2 = copyucs2; ucs2[0] != 0; ucs2++)
	{
		ucs = ucs2[0];
		utf8 += runetochar(utf8, &ucs);
	}
	*utf8 = 0;

	gtk_selection_owner_set(moz->canvas, GDK_SELECTION_PRIMARY, moz->copytime);

	moz->justcopied = 1;
}

void winreloadfile(pdfapp_t *app)
{
	/* No-op; reloading should be done through the browser. */
}

void winopenuri(pdfapp_t *app, char *buf)
{
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	npn.geturl(moz->instance, buf, "_blank");
}


/* GTK callbacks */

static gboolean
handle_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	pdfapp_t *app = (pdfapp_t *) user_data;
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	if (event->type == GDK_MAP) {
		gtk_widget_grab_focus(widget);
		return TRUE;
	}

	if (!app->image)
		return FALSE;

	switch (event->type)
	{
	case GDK_EXPOSE:
		winrepaint(app);
		return TRUE;

	case GDK_KEY_PRESS:
		moz->justcopied = false;
		switch (event->key.keyval)
		{
		case GDK_Escape:
			pdfapp_onkey(app, '\033');
			break;
		case GDK_Up:
			pdfapp_onkey(app, 'k');
			break;
		case GDK_Down:
			pdfapp_onkey(app, 'j');
			break;
		case GDK_Left:
			pdfapp_onkey(app, 'b');
			break;
		case GDK_Right:
			pdfapp_onkey(app, ' ');
			break;
		case GDK_Page_Up:
			pdfapp_onkey(app, ',');
			break;
		case GDK_Page_Down:
			pdfapp_onkey(app, '.');
			break;
		default:
			pdfapp_onkey(app, (int) event->key.keyval);
			break;
		}
		search_status(app);
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (event->button.button == 1)
			gtk_widget_grab_focus(widget);
		moz->justcopied = false;
		pdfapp_onmouse(app, (int) event->button.x, (int) event->button.y,
					   event->button.button, event->button.state, 1);
		return TRUE;

	case GDK_BUTTON_RELEASE:
		moz->copytime = event->button.time;
		pdfapp_onmouse(app, (int) event->button.x, (int) event->button.y,
					   event->button.button, event->button.state, -1);
		return TRUE;

	case GDK_MOTION_NOTIFY:
		pdfapp_onmouse(app, (int) event->motion.x, (int) event->motion.y,
					   0, event->motion.state, 0);
		gdk_event_request_motions((GdkEventMotion *) event);
		return TRUE;

	default:
		return FALSE;
	}
}

static gboolean
handle_selection(GtkWidget *widget, GtkSelectionData *selection_data,
				 guint info, guint timestamp, gpointer user_data)
{
	pdfapp_t *app = (pdfapp_t *) user_data;
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;

	gtk_selection_data_set_text(selection_data, moz->copyutf8, -1);
	return TRUE;
}

/* NPAPI plugin functions */

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

	gdk_cursor_unref(moz->arrow);
	gdk_cursor_unref(moz->hand);
	gdk_cursor_unref(moz->wait);

	fz_free(moz);
	app->userdata = NULL;

	pdfapp_close(app);
	fz_free(app);
	instance->pdata = NULL;

	return NPERR_NO_ERROR;
}

NPError
NPP_SetWindow(NPP instance, NPWindow *nav_window)
{
	pdfapp_t *app = (pdfapp_t *) instance->pdata;
	pdfmoz_t *moz = (pdfmoz_t *) app->userdata;
	NPSetWindowCallbackStruct *ws_info =
		(NPSetWindowCallbackStruct *) nav_window->ws_info;

	if (moz->nav_window == nav_window) {
		pdfapp_onresize(app, nav_window->width, nav_window->height);
		return NPERR_NO_ERROR;
	} else {
		GdkDisplay *display = gdk_x11_lookup_xdisplay(ws_info->display);
		GtkWidget *plug = gtk_plug_new((GdkNativeWindow) nav_window->window);
		GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(plug));

		moz->display = display;
		moz->nav_window = nav_window;

		moz->canvas = gtk_drawing_area_new();
		gtk_widget_set_can_focus(moz->canvas, TRUE);
		gtk_selection_add_target(moz->canvas, GDK_SELECTION_PRIMARY,
								 GDK_SELECTION_TYPE_STRING, 0);
		gtk_widget_add_events(moz->canvas,
							  GDK_BUTTON_PRESS_MASK |
							  GDK_BUTTON_RELEASE_MASK |
							  GDK_KEY_PRESS_MASK |
							  GDK_POINTER_MOTION_MASK |
							  GDK_POINTER_MOTION_HINT_MASK |
							  GDK_EXPOSURE_MASK);
		gtk_signal_connect(GTK_OBJECT(moz->canvas), "event",
						   GTK_SIGNAL_FUNC(handle_event), app);
		gtk_signal_connect(GTK_OBJECT(moz->canvas), "selection_get",
						   GTK_SIGNAL_FUNC(handle_selection), app);
		gtk_widget_show(moz->canvas);

		gtk_container_add(GTK_CONTAINER(plug), moz->canvas);
		gtk_widget_show(plug);

#define maybe_unref_cursor(cursor) do { \
			if (cursor) \
				gdk_cursor_unref(cursor); \
		} while (0)

		maybe_unref_cursor(moz->arrow);
		maybe_unref_cursor(moz->hand);
		maybe_unref_cursor(moz->wait);

		moz->arrow = gdk_cursor_new_for_display(display, GDK_LEFT_PTR);
		moz->hand = gdk_cursor_new_for_display(display, GDK_HAND2);
		moz->wait = gdk_cursor_new_for_display(display, GDK_WATCH);

		app->winh = nav_window->height;
		app->winw = nav_window->width;
		app->resolution = (double) gdk_screen_get_width(screen) * 25.4
			/ (double) gdk_screen_get_width_mm(screen);
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
	pdfapp_open(app, (char *) filename, fd, 0);
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
	switch (variable)
	{
	case NPPVpluginNameString:
		*((char **) value) = PLUGIN_NAME;
		return NPERR_NO_ERROR;

	case NPPVpluginDescriptionString:
		*((char **) value) = PLUGIN_DESCRIPTION;
		return NPERR_NO_ERROR;

	case NPPVpluginNeedsXEmbed:
		*((bool *) value) = true;
		return NPERR_NO_ERROR;

	default:
		return NPERR_GENERIC_ERROR;
	}
}

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
	return NPERR_GENERIC_ERROR;
}

/* NPAPI entry points */

NP_EXPORT(NPError)
NP_Initialize(NPNetscapeFuncs *npn_funcs, NPPluginFuncs *npp_funcs)
{
	uint16_t size;
	NPError err;
	bool supports_xembed;
	NPNToolkitType toolkit;

	if (!npn_funcs || !npp_funcs)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	/* The navigator functions table may have a size different from what
	   we were compiled with. That's fine, as long as it contains (at least)
	   the few functions we use. */
	size = MIN(sizeof(npn), npn_funcs->size);
	memcpy(&npn, npn_funcs, size);
	npn.size = size;
	if (!npn.geturl || !npn.status || !npn.getvalue)
		return NPERR_INVALID_FUNCTABLE_ERROR;

	/* Ensure that the browser supports XEmbed and uses Gtk2. */
	err = npn.getvalue(NULL, NPNVSupportsXEmbedBool, &supports_xembed);
	if (err != NPERR_NO_ERROR || !supports_xembed)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;
	err = npn.getvalue(NULL, NPNVToolkit, &toolkit);
	if (err != NPERR_NO_ERROR || toolkit != NPNVGtk2)
		return NPERR_INCOMPATIBLE_VERSION_ERROR;

	/* Now fill in the plugin functions table. */
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

NP_EXPORT(NPError)
NP_GetValue(void *future, NPPVariable variable, void *value)
{
	return NPP_GetValue(future, variable, value);
}

NP_EXPORT(NPError)
NP_Shutdown(void)
{
	return NPERR_NO_ERROR;
}
