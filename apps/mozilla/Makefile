X11_INCLUDES=-I/usr/local/include
NPAPI_FLAGS=-DXP_UNIX -DMOZ_X11 -I/usr/local/include/firefox3 $(X11_INCLUDES)
MUPDF_FLAGS=-I.. -I../../fitz -I../../mupdf
CFLAGS=-Wall -g -fPIC $(NPAPI_FLAGS) $(MUPDF_FLAGS)
LDFLAGS=-fPIC -L/usr/local/lib
MUPDF_OBJDIR=../../build/debug
MUPDF_OBJS=$(MUPDF_OBJDIR)/pdfapp.o \
    $(MUPDF_OBJDIR)/x11_image.o \
    $(MUPDF_OBJDIR)/libmupdf.a
LIBS=-lfreetype -ljbig2dec -lopenjpeg -ljpeg -lz -lm

libmupdfplugin.so: moz_x11.o
	$(CC) $(LDFLAGS) -shared moz_x11.o $(MUPDF_OBJS) -o libmupdfplugin.so \
		$(LIBS)

clean:
	rm -f *.so *.o

.PHONY: clean