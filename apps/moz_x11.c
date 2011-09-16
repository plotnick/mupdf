#include "fitz.h"
#include "mupdf.h"
#include "muxps.h"
#include "pdfapp.h"

#include <glib.h>
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
	char *src;
	char *filename;
	NPWindow *nav_window;
	GtkWidget *canvas;
	GdkCursor *arrow, *hand, *wait;
	char copyutf8[1024*48];
	Time copytime;
	int justcopied;
} pdfmoz_t;

/* pdfapp callbacks */

void winwarn(pdfapp_t *app, char* msg)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	npn.status(moz->instance, msg);
}

void winerror(pdfapp_t *app, fz_error error)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	fz_catch(error, "unhandled error");
	npn.status(moz->instance, "mupdf error");
}

char *winpassword(pdfapp_t *app, char *filename)
{
	return "";
}

void wintitle(pdfapp_t *app, char *title)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
	NPError err;
	NPObject *window = NULL, *document = NULL;
	NPVariant documentv, titlev;
	NPIdentifier document_id = npn.getstringidentifier("document"),
		title_id = npn.getstringidentifier("title");

	if (!title || !document_id || !title_id)
		return;

	err = npn.getvalue(moz->instance, NPNVWindowNPObject, &window);
	if (err != NPERR_NO_ERROR || !window)
		goto cleanup;
	if (!npn.getproperty(moz->instance, window, document_id, &documentv) ||
		!(document = NPVARIANT_TO_OBJECT(documentv)))
		goto cleanup;
	STRINGZ_TO_NPVARIANT(title, titlev);
	npn.setproperty(moz->instance, document, title_id, &titlev);

cleanup:
	if (document)
		npn.releaseobject(document);
	if (window)
		npn.releaseobject(window);
}

void winhelp(pdfapp_t *app)
{
	winopenuri(app, "http://mupdf.com/manual");
}

void winclose(pdfapp_t *app)
{
	/* No-op; we're not allowed to close the browser window. */
}

void wincursor(pdfapp_t *app, int cursor)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
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

void winrepaint(pdfapp_t *app)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
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

	winrepaintsearch(app);
}

void winrepaintsearch(pdfapp_t *app)
{
	if (app->isediting)
	{
		pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
		char buf[sizeof(app->search) + 50];

		sprintf(buf, "Search: %s", app->search);
		gdk_draw_rectangle(moz->canvas->window,
			moz->canvas->style->white_gc,
			TRUE, 0, 0, app->winw, 30);
		windrawstring(app, 10, 20, buf);
	}
}

void windrawstring(pdfapp_t *app, int x, int y, char *s)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	gdk_draw_string(moz->canvas->window,
		gtk_style_get_font(moz->canvas->style),
		moz->canvas->style->fg_gc[GTK_STATE_NORMAL],
		x, y, s);
}

void windocopy(pdfapp_t *app)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
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
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	if (moz->src)
		npn.geturl(moz->instance, moz->src, NULL);
	else
		winwarn(app, "cannot reload file");
}

void winopenuri(pdfapp_t *app, char *buf)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	npn.geturl(moz->instance, buf, "_blank");
}

/* GTK callbacks */

static void
onkey(pdfapp_t *app, int c)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	if (moz->justcopied)
	{
		moz->justcopied = 0;
		winrepaint(app);
	}

	pdfapp_onkey(app, c);
}

static void
onmouse(pdfapp_t *app, int x, int y, int btn, int modifiers, int state)
{
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	if (state != 0 && moz->justcopied)
	{
		moz->justcopied = 0;
		winrepaint(app);
	}

	pdfapp_onmouse(app, x, y, btn, modifiers, state);
}

static gboolean
handle_event(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	pdfapp_t *app = (pdfapp_t *)user_data;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	if (!app->image)
		return FALSE;

	switch (event->type)
	{
	case GDK_EXPOSE:
		winrepaint(app);
		return TRUE;

	case GDK_KEY_PRESS:
		if (app->isediting)
		{
			switch (event->key.keyval)
			{
			case GDK_BackSpace:
				onkey(app, '\b');
				break;
			default:
				if (event->key.length > 0)
					onkey(app, event->key.string[0]);
				break;
			}
		}
		else
		{
			switch (event->key.keyval)
			{
			case GDK_Escape:
				onkey(app, '\033');
				break;
			case GDK_Up:
				onkey(app, 'k');
				break;
			case GDK_Down:
				onkey(app, 'j');
				break;
			case GDK_Left:
				onkey(app, 'b');
				break;
			case GDK_Right:
				onkey(app, ' ');
				break;
			case GDK_Page_Up:
				onkey(app, ',');
				break;
			case GDK_Page_Down:
				onkey(app, '.');
				break;
			default:
				if (event->key.length > 0)
					onkey(app, event->key.string[0]);
				break;
			}
		}
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (event->button.button == 1)
			gtk_widget_grab_focus(widget);
		onmouse(app, (int) event->button.x, (int) event->button.y,
			event->button.button, event->button.state, 1);
		return TRUE;

	case GDK_BUTTON_RELEASE:
		moz->copytime = event->button.time;
		onmouse(app, (int) event->button.x, (int) event->button.y,
			event->button.button, event->button.state, -1);
		return TRUE;

	case GDK_SCROLL:
		/* GDK helpfully translates button presses for buttons 4-7 into scroll
		 * events, but pdfapp_onmouse expects raw button press data. So we'll
		 * translate such events back into a form that it undertands. */
		{
			int button;

			switch (event->scroll.direction)
			{
			case GDK_SCROLL_LEFT:
				event->scroll.state |= GDK_SHIFT_MASK;
				/* fall through */
			case GDK_SCROLL_UP:
				button = 4;
				break;

			case GDK_SCROLL_RIGHT:
				event->scroll.state |= GDK_SHIFT_MASK;
				/* fall through */
			case GDK_SCROLL_DOWN:
				button = 5;
				break;
			}
			onmouse(app, (int) event->scroll.x, (int) event->scroll.y,
				button, event->scroll.state, 1);
		}
		return TRUE;

	case GDK_MOTION_NOTIFY:
		onmouse(app, (int) event->motion.x, (int) event->motion.y,
			0, event->motion.state, 0);
		gdk_event_request_motions((GdkEventMotion *)event);
		return TRUE;

	default:
		return FALSE;
	}
}

static void
get_selection(GtkWidget *widget, GtkSelectionData *selection_data,
			  guint info, guint timestamp, gpointer user_data)
{
	pdfapp_t *app = (pdfapp_t *)user_data;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	gtk_selection_data_set_text(selection_data, moz->copyutf8, -1);
}

static gboolean
clear_selection(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	pdfapp_t *app = (pdfapp_t *)user_data;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	moz->justcopied = 0;
	winrepaint(app);

	return gtk_selection_owner_set(NULL, GDK_SELECTION_PRIMARY, moz->copytime);
}

/* NPAPI plugin functions */

NPError
NPP_New(NPMIMEType mime, NPP instance, uint16_t mode,
	int16_t argc, char *argn[], char *argv[], NPSavedData *saved)
{
	pdfapp_t *app;
	pdfmoz_t *moz;
	int i;

	app = fz_malloc(sizeof(pdfapp_t));
	if (!app)
		return NPERR_OUT_OF_MEMORY_ERROR;
	memset(app, 0, sizeof(pdfapp_t));
	pdfapp_init(app);

	moz = fz_malloc(sizeof(pdfmoz_t));
	if (!moz)
		return NPERR_OUT_OF_MEMORY_ERROR;
	memset(moz, 0, sizeof(pdfmoz_t));

	for (i = 0; i < argc; i++)
		if (strcasecmp(argn[i], "src") == 0)
			moz->src = strdup(argv[i]);

	moz->instance = instance; /* the nav-bone's connected to the moz-bone... */
	app->userdata = moz; /* the moz-bone's connected to the app-bone... */
	instance->pdata = app; /* the app-bone's connected to the nav-bone... */
	return NPERR_NO_ERROR;
}

NPError
NPP_Destroy(NPP instance, NPSavedData **saved)
{
	pdfapp_t *app = instance->pdata;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;

	if (moz->src)
	{
		free(moz->src);
		moz->src = NULL;
	}
	if (moz->filename)
	{
		free(moz->filename);
		moz->filename = NULL;
	}

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
	pdfapp_t *app = (pdfapp_t *)instance->pdata;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
	NPSetWindowCallbackStruct *ws_info =
		(NPSetWindowCallbackStruct *)nav_window->ws_info;

	if (moz->nav_window == nav_window)
	{
		pdfapp_onresize(app, nav_window->width, nav_window->height);
		return NPERR_NO_ERROR;
	}
	else
	{
		GdkDisplay *display = gdk_x11_lookup_xdisplay(ws_info->display);
		GtkWidget *plug = gtk_plug_new((GdkNativeWindow) nav_window->window);
		GdkScreen *screen = gtk_window_get_screen(GTK_WINDOW(plug));

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
			GDK_SCROLL_MASK |
			GDK_EXPOSURE_MASK);
		gtk_signal_connect(GTK_OBJECT(moz->canvas), "event",
			GTK_SIGNAL_FUNC(handle_event), app);
		gtk_signal_connect(GTK_OBJECT(moz->canvas), "selection-get",
			GTK_SIGNAL_FUNC(get_selection), app);
		gtk_signal_connect(GTK_OBJECT(moz->canvas), "selection-clear-event",
			GTK_SIGNAL_FUNC(clear_selection), app);
		gtk_container_add(GTK_CONTAINER(plug), moz->canvas);
		gtk_widget_show_all(plug);

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
	pdfapp_t *app = (pdfapp_t *)instance->pdata;
	pdfmoz_t *moz = (pdfmoz_t *)app->userdata;
	int fd;

	fd = open(filename, O_BINARY | O_RDONLY, 0666);
	if (fd < 0)
		winwarn(app, "cannot open file");
	app->pageno = 1;

	/* The filename we're given will usually be the name of a temporary
	 * file, and so will probably not be particularly meaningful. If have
	 * a suitable source URL, we'll use that instead. */
	if (!(moz->src && (moz->filename = g_uri_unescape_segment(moz->src,
											strrchr(moz->src, '?'), "/\\"))))
		moz->filename = strdup(filename);
	pdfapp_open(app, moz->filename, fd, 0);
}

void
NPP_Print(NPP instance, NPPrint* platformPrint)
{
	/* No-op; not yet supported. */
}

int16_t
NPP_HandleEvent(NPP instance, void* event)
{
	/* Only used on Mac OS for windowed plugins. */
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
		*((char **)value) = PLUGIN_NAME;
		return NPERR_NO_ERROR;

	case NPPVpluginDescriptionString:
		*((char **)value) = PLUGIN_DESCRIPTION;
		return NPERR_NO_ERROR;

	case NPPVpluginNeedsXEmbed:
		*((bool *)value) = true;
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

	/* The navigator functions table may have a size different from what we
	 * were compiled with. That's fine, as long as it contains (at least)
	 * the last function we need. */
	size = MIN(sizeof(npn), npn_funcs->size);
	if (size < offsetof(NPNetscapeFuncs, setproperty) + sizeof(void*))
		return NPERR_INVALID_FUNCTABLE_ERROR;
	memcpy(&npn, npn_funcs, size);
	npn.size = size;

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

NP_EXPORT(const char *)
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
