#ifndef _DOCUMENT_H_
#define _DOCUMENT_H_

#ifndef _FITZ_H_
#error "fitz.h must be included before document.h"
#endif

#ifndef _MUPDF_H_
#error "mupdf.h must be included before document.h"
#endif

#ifndef _MUXPS_H_
#error "muxps.h must be included before document.h"
#endif

#ifndef _MUCBZ_H_
#error "mucbz.h must be included before document.h"
#endif

struct document
{
	fz_context *ctx;
	pdf_document *pdf;
	xps_document *xps;
	cbz_document *cbz;
	int number;
	pdf_page *pdf_page;
	xps_page *xps_page;
	cbz_page *cbz_page;
	fz_bbox hit_bbox[500];
	int hit_count;
};

struct document *open_document(fz_context *ctx, char *filename);
int needs_password(struct document *doc);
int authenticate_password(struct document *doc, char *password);
fz_outline *load_outline(struct document *doc);
int count_pages(struct document *doc);
void measure_page(struct document *doc, int number, float *w, float *h);
void draw_page(struct document *doc, int number, fz_device *dev, fz_matrix ctm, fz_cookie *cookie);
int search_page(struct document *doc, int number, char *needle, fz_cookie *cookie);
fz_bbox search_result_bbox(struct document *doc, int i);
void close_document(struct document *doc);

#endif
