LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

MY_ROOT := ../..

LOCAL_C_INCLUDES := \
	../thirdparty/jbig2dec \
	../thirdparty/openjpeg-1.4/libopenjpeg \
	../thirdparty/jpeg-8d \
	../thirdparty/zlib-1.2.5 \
	../thirdparty/freetype-2.4.8/include \
	../scripts

LOCAL_CFLAGS := \
	-DFT2_BUILD_LIBRARY -DDARWIN_NO_CARBON -DHAVE_STDINT_H

LOCAL_MODULE    := mupdfthirdparty
LOCAL_SRC_FILES := \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith_int.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_arith_iaid.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_halftone.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_huffman.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_segment.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_page.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_symbol_dict.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_text.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_generic.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_refinement.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_mmr.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_image.c \
	$(MY_ROOT)/thirdparty/jbig2dec/jbig2_metadata.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/bio.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/cio.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/dwt.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/event.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/image.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/j2k.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/j2k_lib.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/jp2.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/jpt.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/mct.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/mqc.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/openjpeg.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/pi.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/raw.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/t1.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/t2.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/tcd.c \
	$(MY_ROOT)/thirdparty/openjpeg-1.4/libopenjpeg/tgt.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jaricom.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcapimin.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcapistd.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcarith.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jccoefct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jccolor.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcdctmgr.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jchuff.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcinit.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcmainct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcmarker.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcmaster.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcomapi.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcparam.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcprepct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jcsample.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jctrans.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdapimin.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdapistd.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdarith.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdatadst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdatasrc.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdcoefct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdcolor.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jddctmgr.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdhuff.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdinput.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmainct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmarker.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmaster.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdmerge.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdpostct.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdsample.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jdtrans.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jerror.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctflt.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctfst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jfdctint.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctflt.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctfst.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jidctint.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jquant1.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jquant2.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jutils.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jmemmgr.c \
	$(MY_ROOT)/thirdparty/jpeg-8d/jmemnobs.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/adler32.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/compress.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/crc32.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/deflate.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/gzclose.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/gzlib.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/gzread.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/gzwrite.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/infback.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inffast.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inflate.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/inftrees.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/trees.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/uncompr.c \
	$(MY_ROOT)/thirdparty/zlib-1.2.5/zutil.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/autofit/autofit.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftbase.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftbbox.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftbdf.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftbitmap.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftdebug.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftgasp.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftglyph.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftgxval.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftinit.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftlcdfil.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftmm.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftotval.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftpfr.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftstroke.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftsynth.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftsystem.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/fttype1.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftwinfnt.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftxf86.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/base/ftpatent.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/bdf/bdf.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/cache/ftcache.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/cff/cff.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/cid/type1cid.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/gzip/ftgzip.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/lzw/ftlzw.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/pcf/pcf.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/pfr/pfr.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/psaux/psaux.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/pshinter/pshinter.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/psnames/psnames.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/raster/raster.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/smooth/smooth.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/sfnt/sfnt.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/truetype/truetype.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/type1/type1.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/type42/type42.c \
	$(MY_ROOT)/thirdparty/freetype-2.4.8/src/winfonts/winfnt.c

include $(BUILD_STATIC_LIBRARY)
