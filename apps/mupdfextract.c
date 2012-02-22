/*
 * pdfextract -- the ultimate way to extract images and fonts from pdfs
 */

#include "fitz.h"
#include "mupdf.h"

static pdf_document *doc = NULL;
static fz_context *ctx = NULL;
static int dorgb = 0;

static void usage(void)
{
	fprintf(stderr, "usage: pdfextract [options] file.pdf [object numbers]\n");
	fprintf(stderr, "\t-p\tpassword\n");
	fprintf(stderr, "\t-r\tconvert images to rgb\n");
	exit(1);
}

static int isimage(fz_obj *obj)
{
	fz_obj *type = fz_dict_gets(obj, "Subtype");
	return fz_is_name(type) && !strcmp(fz_to_name(type), "Image");
}

static int isfontdesc(fz_obj *obj)
{
	fz_obj *type = fz_dict_gets(obj, "Type");
	return fz_is_name(type) && !strcmp(fz_to_name(type), "FontDescriptor");
}

static void saveimage(int num)
{
	fz_pixmap *img;
	fz_obj *ref;
	char name[1024];

	ref = fz_new_indirect(ctx, num, 0, doc);

	/* TODO: detect DCTD and save as jpeg */

	img = pdf_load_image(doc, ref);

	if (dorgb && img->colorspace && img->colorspace != fz_device_rgb)
	{
		fz_pixmap *temp;
		temp = fz_new_pixmap_with_rect(ctx, fz_device_rgb, fz_bound_pixmap(img));
		fz_convert_pixmap(ctx, img, temp);
		fz_drop_pixmap(ctx, img);
		img = temp;
	}

	if (img->n <= 4)
	{
		sprintf(name, "img-%04d.png", num);
		printf("extracting image %s\n", name);
		fz_write_png(ctx, img, name, 0);
	}
	else
	{
		sprintf(name, "img-%04d.pam", num);
		printf("extracting image %s\n", name);
		fz_write_pam(ctx, img, name, 0);
	}

	fz_drop_pixmap(ctx, img);
	fz_drop_obj(ref);
}

static void savefont(fz_obj *dict, int num)
{
	char name[1024];
	char *subtype;
	fz_buffer *buf;
	fz_obj *stream = NULL;
	fz_obj *obj;
	char *ext = "";
	FILE *f;
	char *fontname = "font";
	int n;

	obj = fz_dict_gets(dict, "FontName");
	if (obj)
		fontname = fz_to_name(obj);

	obj = fz_dict_gets(dict, "FontFile");
	if (obj)
	{
		stream = obj;
		ext = "pfa";
	}

	obj = fz_dict_gets(dict, "FontFile2");
	if (obj)
	{
		stream = obj;
		ext = "ttf";
	}

	obj = fz_dict_gets(dict, "FontFile3");
	if (obj)
	{
		stream = obj;

		obj = fz_dict_gets(obj, "Subtype");
		if (obj && !fz_is_name(obj))
			fz_throw(ctx, "Invalid font descriptor subtype");

		subtype = fz_to_name(obj);
		if (!strcmp(subtype, "Type1C"))
			ext = "cff";
		else if (!strcmp(subtype, "CIDFontType0C"))
			ext = "cid";
		else
			fz_throw(ctx, "Unhandled font type '%s'", subtype);
	}

	if (!stream)
	{
		fz_warn(ctx, "Unhandled font type");
		return;
	}

	buf = pdf_load_stream(doc, fz_to_num(stream), fz_to_gen(stream));

	sprintf(name, "%s-%04d.%s", fontname, num, ext);
	printf("extracting font %s\n", name);

	f = fopen(name, "wb");
	if (!f)
		fz_throw(ctx, "Error creating font file");

	n = fwrite(buf->data, 1, buf->len, f);
	if (n < buf->len)
		fz_throw(ctx, "Error writing font file");

	if (fclose(f) < 0)
		fz_throw(ctx, "Error closing font file");

	fz_drop_buffer(ctx, buf);
}

static void showobject(int num)
{
	fz_obj *obj;

	if (!doc)
		fz_throw(ctx, "no file specified");

	obj = pdf_load_object(doc, num, 0);

	if (isimage(obj))
		saveimage(num);
	else if (isfontdesc(obj))
		savefont(obj, num);

	fz_drop_obj(obj);
}

#ifdef MUPDF_COMBINED_EXE
int pdfextract_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
	char *infile;
	char *password = "";
	int c, o;

	while ((c = fz_getopt(argc, argv, "p:r")) != -1)
	{
		switch (c)
		{
		case 'p': password = fz_optarg; break;
		case 'r': dorgb++; break;
		default: usage(); break;
		}
	}

	if (fz_optind == argc)
		usage();

	infile = argv[fz_optind++];

	ctx = fz_new_context(NULL, NULL, FZ_STORE_UNLIMITED);
	if (!ctx)
	{
		fprintf(stderr, "cannot initialise context\n");
		exit(1);
	}

	doc = pdf_open_document(ctx, infile);
	if (pdf_needs_password(doc))
		if (!pdf_authenticate_password(doc, password))
			fz_throw(ctx, "cannot authenticate password: %s\n", infile);

	if (fz_optind == argc)
	{
		for (o = 0; o < doc->len; o++)
			showobject(o);
	}
	else
	{
		while (fz_optind < argc)
		{
			showobject(atoi(argv[fz_optind]));
			fz_optind++;
		}
	}

	pdf_close_document(doc);
	fz_flush_warnings(ctx);
	fz_free_context(ctx);
	return 0;
}
