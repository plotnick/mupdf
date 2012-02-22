#ifndef _FITZ_H_
#define _FITZ_H_

/*
 * Include the standard libc headers.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <assert.h>
#include <errno.h>
#include <limits.h>	/* INT_MAX & co */
#include <float.h> /* FLT_EPSILON */
#include <fcntl.h> /* O_RDONLY & co */

#include <setjmp.h>

#include "memento.h"

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "libmupdf"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#else
#define LOGI(...) do {} while(0)
#define LOGE(...) do {} while(0)
#endif

#define nelem(x) (sizeof(x)/sizeof((x)[0]))

#define ABS(x) ( (x) < 0 ? -(x) : (x) )
#define MIN(a,b) ( (a) < (b) ? (a) : (b) )
#define MAX(a,b) ( (a) > (b) ? (a) : (b) )
#define CLAMP(x,a,b) ( (x) > (b) ? (b) : ( (x) < (a) ? (a) : (x) ) )

/*
 * Some differences in libc can be smoothed over
 */

#ifdef _MSC_VER /* Microsoft Visual C */

#pragma warning( disable: 4244 ) /* conversion from X to Y, possible loss of data */
#pragma warning( disable: 4996 ) /* The POSIX name for this item is deprecated */
#pragma warning( disable: 4996 ) /* This function or variable may be unsafe */

#include <io.h>

int gettimeofday(struct timeval *tv, struct timezone *tz);

#define snprintf _snprintf
#define strtoll _strtoi64

#else /* Unix or close enough */

#include <unistd.h>

#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef M_SQRT2
#define M_SQRT2 1.41421356237309504880
#endif

/*
 * Variadic macros, inline and restrict keywords
 */

#if __STDC_VERSION__ == 199901L /* C99 */
#elif _MSC_VER >= 1500 /* MSVC 9 or newer */
#define inline __inline
#define restrict __restrict
#elif __GNUC__ >= 3 /* GCC 3 or newer */
#define inline __inline
#define restrict __restrict
#else /* Unknown or ancient */
#define inline
#define restrict
#endif

/*
 * GCC can do type checking of printf strings
 */

#ifndef __printflike
#if __GNUC__ > 2 || __GNUC__ == 2 && __GNUC_MINOR__ >= 7
#define __printflike(fmtarg, firstvararg) \
	__attribute__((__format__ (__printf__, fmtarg, firstvararg)))
#else
#define __printflike(fmtarg, firstvararg)
#endif
#endif

/* Contexts */

typedef struct fz_alloc_context_s fz_alloc_context;
typedef struct fz_error_context_s fz_error_context;
typedef struct fz_warn_context_s fz_warn_context;
typedef struct fz_font_context_s fz_font_context;
typedef struct fz_aa_context_s fz_aa_context;
typedef struct fz_locks_context_s fz_locks_context;
typedef struct fz_store_s fz_store;
typedef struct fz_glyph_cache_s fz_glyph_cache;
typedef struct fz_context_s fz_context;

struct fz_alloc_context_s
{
	void *user;
	void *(*malloc)(void *, unsigned int);
	void *(*realloc)(void *, void *, unsigned int);
	void (*free)(void *, void *);
};

/* Default allocator */
extern fz_alloc_context fz_alloc_default;

struct fz_error_context_s
{
	int top;
	struct {
		int code;
		jmp_buf buffer;
	} stack[256];
	char message[256];
};

void fz_var_imp(void *);
#define fz_var(var) fz_var_imp((void *)&(var))

/*

MuPDF uses a set of exception handling macros to simplify error return
and cleanup. Conceptually, they work a lot like C++'s try/catch system,
but do not require any special compiler support.

The basic formulation is as follows:

	fz_try(ctx)
	{
		// Try to perform a task. Never 'return', 'goto' or 'longjmp' out
		// of here. 'break' may be used to safely exit (just) the try block
		// scope.
	}
	fz_always(ctx)
	{
		// Any code here is always executed, regardless of whether an
		// exception was thrown within the try or not. Never 'return', 'goto'
		// or longjmp out from here. 'break' may be used to safely exit (just)
		// the always block scope.
	}
	fz_catch(ctx)
	{
		// This code is called (after any always block) only if something
		// within the fz_try block (including any functions it called) threw
		// an exception. The code here is expected to handle the exception
		// (maybe record/report the error, cleanup any stray state etc) and
		// can then either exit the block, or pass on the exception to a
		// higher level (enclosing) fz_try block (using fz_throw, or
		// fz_rethrow).
	}

The fz_always block is optional, and can safely be omitted.

The macro based nature of this system has 3 main limitations:

1)	Never return from within try (or 'goto' or longjmp out of it).
	This upsets the internal housekeeping of the macros and will cause
	problems later on. The code will detect such things happening, but
	by then it is too late to give a helpful error report as to where the
	original infraction occurred.

2)	The fz_try(ctx) { ... } fz_always(ctx) { ... } fz_catch(ctx) { ... }
	is not one atomic C statement. That is to say, if you do:

		if (condition)
			fz_try(ctx) { ... }
			fz_catch(ctx) { ... }

	then you will not get what you want. Use the following instead:

		if (condition) {
			fz_try(ctx) { ... }
			fz_catch(ctx) { ... }
		}

3)	The macros are implemented using setjmp and longjmp, and so the standard
	C restrictions on the use of those functions apply to fz_try/fz_catch
	too. In particular, any "truly local" variable that is set between the
	start of fz_try and something in fz_try throwing an exception may become
	undefined as part of the process of throwing that exception.

	As a way of mitigating this problem, we provide an fz_var() macro that
	tells the compiler to ensure that that variable is not unset by the
	act of throwing the exception.

A model piece of code using these macros then might be:

	house build_house(plans *p)
	{
		material m = NULL;
		walls w = NULL;
		roof r = NULL;
		house h = NULL;
		tiles t = make_tiles();

		fz_var(w);
		fz_var(r);
		fz_var(h);

		fz_try(ctx)
		{
			fz_try(ctx)
			{
				m = make_bricks();
			}
			fz_catch(ctx)
			{
				// No bricks available, make do with straw?
				m = make_straw();
			}
			w = make_walls(m, p);
			r = make_roof(m, t);
			h = combine(w, r);	// Note, NOT: return combine(w,r);
		}
		fz_always(ctx)
		{
			drop_walls(w);
			drop_roof(r);
			drop_material(m);
			drop_tiles(t);
		}
		fz_catch(ctx)
		{
			fz_throw(ctx, "build_house failed");
		}
		return h;
	}

Things to note about this:

a)	If make_tiles throws an exception, this will immediately be handled
	by some higher level exception handler. If it succeeds, t will be
	set before fz_try starts, so there is no need to fz_var(t);

b)	We try first off to make some bricks as our building material. If
	this fails, we fall back to straw. If this fails, we'll end up in
	the fz_catch, and the process will fail neatly.

c)	We assume in this code that combine takes new reference to both the
	walls and the roof it uses, and therefore that w and r need to be
	cleaned up in all cases.

d)	We assume the standard C convention that it is safe to destroy
	NULL things.

*/

/* Exception macro definitions. Just treat these as a black box - pay no
 * attention to the man behind the curtain. */

#define fz_try(ctx) \
	if (fz_push_try(ctx->error), \
		(ctx->error->stack[ctx->error->top].code = setjmp(ctx->error->stack[ctx->error->top].buffer)) == 0) \
	{ do {

#define fz_always(ctx) \
		} while (0); \
	} \
	{ do { \

#define fz_catch(ctx) \
		} while(0); \
	} \
	if (ctx->error->stack[ctx->error->top--].code)

/*

We also include a couple of other formulations of the macros, with
different strengths and weaknesses. These will be removed shortly, but
I want them in git for at least 1 revision so I have a record of them.

A formulation of try/always/catch that lifts limitation 2 above, but
has problems when try/catch are nested in the same function; the inner
nestings need to use fz_always_(ctx, label) and fz_catch_(ctx, label)
instead. This was held as too high a price to pay to drop limitation 2.

#define fz_try(ctx) \
	if (fz_push_try(ctx->error), \
		(ctx->error->stack[ctx->error->top].code = setjmp(ctx->error->stack[ctx->error->top].buffer)) == 0) \
	{ do {

#define fz_always_(ctx, label) \
		} while (0); \
		goto ALWAYS_LABEL_ ## label ; \
	} \
	else if (ctx->error->stack[ctx->error->top].code) \
	{ ALWAYS_LABEL_ ## label : \
		do {

#define fz_catch_(ctx, label) \
		} while(0); \
		if (ctx->error->stack[ctx->error->top--].code) \
			goto CATCH_LABEL_ ## label; \
	} \
	else if (ctx->error->top--, 1) \
	CATCH_LABEL ## label:

#define fz_always(ctx) fz_always_(ctx, TOP)
#define fz_catch(ctx) fz_catch_(ctx, TOP)

Another alternative formulation, that again removes limitation 2, but at
the cost of an always block always costing us 1 extra longjmp per
execution. Again this was felt to be too high a cost to use.

#define fz_try(ctx) \
	if (fz_push_try(ctx->error), \
		(ctx->error->stack[ctx->error->top].code = setjmp(ctx->error->stack[ctx->error->top].buffer)) == 0) \
	{ do {

#define fz_always(ctx) \
		} while (0); \
		longjmp(ctx->error->stack[ctx->error->top].buffer, 3); \
	} \
	else if (ctx->error->stack[ctx->error->top].code & 1) \
	{ do {

#define fz_catch(ctx) \
		} while(0); \
		if (ctx->error->stack[ctx->error->top].code == 1) \
			longjmp(ctx->error->stack[ctx->error->top].buffer, 2); \
		ctx->error->top--;\
	} \
	else if (ctx->error->top--, 1)

*/

void fz_push_try(fz_error_context *ex);
void fz_throw(fz_context *, char *, ...) __printflike(2, 3);
void fz_rethrow(fz_context *);

struct fz_warn_context_s
{
	char message[256];
	int count;
};

void fz_warn(fz_context *ctx, char *fmt, ...) __printflike(2, 3);
void fz_flush_warnings(fz_context *ctx);

struct fz_context_s
{
	fz_alloc_context *alloc;
	fz_locks_context *locks;
	fz_error_context *error;
	fz_warn_context *warn;
	fz_font_context *font;
	fz_aa_context *aa;
	fz_store *store;
	fz_glyph_cache *glyph_cache;
};

fz_context *fz_new_context(fz_alloc_context *alloc, fz_locks_context *locks, unsigned int max_store);
fz_context *fz_clone_context(fz_context *ctx);
fz_context *fz_clone_context_internal(fz_context *ctx);
void fz_free_context(fz_context *ctx);

void fz_new_aa_context(fz_context *ctx);
void fz_free_aa_context(fz_context *ctx);

/* Locking functions
 *
 * MuPDF is kept deliberately free of any knowledge of particular threading
 * systems. As such, in order for safe multi-threaded operation, we rely on
 * callbacks to client provided functions.
 *
 * A client is expected to provide FZ_LOCK_MAX mutexes, and a function to
 * lock/unlock each of them. These may be recursive mutexes, but do not have
 * to be.
 *
 * If a client does not intend to use multiple threads, then it may pass
 * NULL instead of the address of a lock structure.
 *
 * In order to avoid deadlocks, we have 1 simple rules internally as to how
 * we use locks: We can never take lock n when we already hold any lock i,
 * where 0 <= i <= n. In order to verify this, we have some debugging code
 * built in, that is enabled by defining FITZ_DEBUG_LOCKING.
 */

#if defined(MEMENTO) || defined(DEBUG)
#define FITZ_DEBUG_LOCKING
#endif

struct fz_locks_context_s
{
	void *user;
	void (*lock)(void *, int);
	void (*unlock)(void *, int);
};

enum {
	FZ_LOCK_ALLOC = 0,
	FZ_LOCK_FILE,
	FZ_LOCK_FREETYPE,
	FZ_LOCK_GLYPHCACHE,
	FZ_LOCK_MAX
};

/* Default locks */
extern fz_locks_context fz_locks_default;

#ifdef FITZ_DEBUG_LOCKING

void fz_assert_lock_held(fz_context *ctx, int lock);
void fz_assert_lock_not_held(fz_context *ctx, int lock);
void fz_lock_debug_lock(fz_context *ctx, int lock);
void fz_lock_debug_unlock(fz_context *ctx, int lock);

#else

#define fz_assert_lock_held(A,B) do { } while (0)
#define fz_assert_lock_not_held(A,B) do { } while (0)
#define fz_lock_debug_lock(A,B) do { } while (0)
#define fz_lock_debug_unlock(A,B) do { } while (0)

#endif /* !FITZ_DEBUG_LOCKING */

static inline void
fz_lock(fz_context *ctx, int lock)
{
	fz_lock_debug_lock(ctx, lock);
	ctx->locks->lock(ctx->locks->user, lock);
}

static inline void
fz_unlock(fz_context *ctx, int lock)
{
	fz_lock_debug_unlock(ctx, lock);
	ctx->locks->unlock(ctx->locks->user, lock);
}


/*
 * Basic runtime and utility functions
 */

/* memory allocation */

/* The following throw exceptions on failure to allocate */
void *fz_malloc(fz_context *ctx, unsigned int size);
void *fz_calloc(fz_context *ctx, unsigned int count, unsigned int size);
void *fz_malloc_array(fz_context *ctx, unsigned int count, unsigned int size);
void *fz_resize_array(fz_context *ctx, void *p, unsigned int count, unsigned int size);
char *fz_strdup(fz_context *ctx, char *s);

void fz_free(fz_context *ctx, void *p);

/* The following returns NULL on failure to allocate */
void *fz_malloc_no_throw(fz_context *ctx, unsigned int size);
void *fz_malloc_array_no_throw(fz_context *ctx, unsigned int count, unsigned int size);
void *fz_calloc_no_throw(fz_context *ctx, unsigned int count, unsigned int size);
void *fz_resize_array_no_throw(fz_context *ctx, void *p, unsigned int count, unsigned int size);
char *fz_strdup_no_throw(fz_context *ctx, char *s);

/* alloc and zero a struct, and tag it for memento */
#define fz_malloc_struct(CTX, STRUCT) \
	Memento_label(fz_calloc(CTX,1,sizeof(STRUCT)), #STRUCT)

/* runtime (hah!) test for endian-ness */
int fz_is_big_endian(void);

/* safe string functions */
char *fz_strsep(char **stringp, const char *delim);
int fz_strlcpy(char *dst, const char *src, int n);
int fz_strlcat(char *dst, const char *src, int n);

/* Range checking atof */
float fz_atof(const char *s);

/* utf-8 encoding and decoding */
int chartorune(int *rune, char *str);
int runetochar(char *str, int *rune);
int runelen(int c);

/* getopt */
extern int fz_getopt(int nargc, char * const *nargv, const char *ostr);
extern int fz_optind;
extern char *fz_optarg;

/*
 * Generic hash-table with fixed-length keys.
 */

typedef struct fz_hash_table_s fz_hash_table;

fz_hash_table *fz_new_hash_table(fz_context *ctx, int initialsize, int keylen);
void fz_debug_hash(fz_context *ctx, fz_hash_table *table);
void fz_empty_hash(fz_context *ctx, fz_hash_table *table);
void fz_free_hash(fz_context *ctx, fz_hash_table *table);

void *fz_hash_find(fz_context *ctx, fz_hash_table *table, void *key);
void *fz_hash_insert(fz_context *ctx, fz_hash_table *table, void *key, void *val);
void fz_hash_remove(fz_context *ctx, fz_hash_table *table, void *key);

int fz_hash_len(fz_context *ctx, fz_hash_table *table);
void *fz_hash_get_key(fz_context *ctx, fz_hash_table *table, int idx);
void *fz_hash_get_val(fz_context *ctx, fz_hash_table *table, int idx);

/*
 * Math and geometry
 */

/* Multiply scaled two integers in the 0..255 range */
static inline int fz_mul255(int a, int b)
{
	/* see Jim Blinn's book "Dirty Pixels" for how this works */
	int x = a * b + 128;
	x += x >> 8;
	return x >> 8;
}

/* Expand a value A from the 0...255 range to the 0..256 range */
#define FZ_EXPAND(A) ((A)+((A)>>7))

/* Combine values A (in any range) and B (in the 0..256 range),
 * to give a single value in the same range as A was. */
#define FZ_COMBINE(A,B) (((A)*(B))>>8)

/* Combine values A and C (in the same (any) range) and B and D (in the
 * 0..256 range), to give a single value in the same range as A and C were. */
#define FZ_COMBINE2(A,B,C,D) (FZ_COMBINE((A), (B)) + FZ_COMBINE((C), (D)))

/* Blend SRC and DST (in the same range) together according to
 * AMOUNT (in the 0...256 range). */
#define FZ_BLEND(SRC, DST, AMOUNT) ((((SRC)-(DST))*(AMOUNT) + ((DST)<<8))>>8)

typedef struct fz_matrix_s fz_matrix;
typedef struct fz_point_s fz_point;
typedef struct fz_rect_s fz_rect;
typedef struct fz_bbox_s fz_bbox;

extern const fz_rect fz_unit_rect;
extern const fz_rect fz_empty_rect;
extern const fz_rect fz_infinite_rect;

extern const fz_bbox fz_unit_bbox;
extern const fz_bbox fz_empty_bbox;
extern const fz_bbox fz_infinite_bbox;

#define fz_is_empty_rect(r) ((r).x0 == (r).x1)
#define fz_is_infinite_rect(r) ((r).x0 > (r).x1)
#define fz_is_empty_bbox(b) ((b).x0 == (b).x1)
#define fz_is_infinite_bbox(b) ((b).x0 > (b).x1)

struct fz_matrix_s
{
	float a, b, c, d, e, f;
};

struct fz_point_s
{
	float x, y;
};

struct fz_rect_s
{
	float x0, y0;
	float x1, y1;
};

struct fz_bbox_s
{
	int x0, y0;
	int x1, y1;
};

extern const fz_matrix fz_identity;

fz_matrix fz_concat(fz_matrix one, fz_matrix two);
fz_matrix fz_scale(float sx, float sy);
fz_matrix fz_shear(float sx, float sy);
fz_matrix fz_rotate(float theta);
fz_matrix fz_translate(float tx, float ty);
fz_matrix fz_invert_matrix(fz_matrix m);
int fz_is_rectilinear(fz_matrix m);
float fz_matrix_expansion(fz_matrix m);
float fz_matrix_max_expansion(fz_matrix m);

fz_bbox fz_round_rect(fz_rect r);
fz_bbox fz_intersect_bbox(fz_bbox a, fz_bbox b);
fz_rect fz_intersect_rect(fz_rect a, fz_rect b);
fz_bbox fz_union_bbox(fz_bbox a, fz_bbox b);
fz_rect fz_union_rect(fz_rect a, fz_rect b);

fz_point fz_transform_point(fz_matrix m, fz_point p);
fz_point fz_transform_vector(fz_matrix m, fz_point p);
fz_rect fz_transform_rect(fz_matrix m, fz_rect r);
fz_bbox fz_transform_bbox(fz_matrix m, fz_bbox b);

void fz_gridfit_matrix(fz_matrix *m);

/*
 * Basic crypto functions.
 * Independent of the rest of fitz.
 * For further encapsulation in filters, or not.
 */

/* md5 digests */

typedef struct fz_md5_s fz_md5;

struct fz_md5_s
{
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
};

void fz_md5_init(fz_md5 *state);
void fz_md5_update(fz_md5 *state, const unsigned char *input, unsigned inlen);
void fz_md5_final(fz_md5 *state, unsigned char digest[16]);

/* sha-256 digests */

typedef struct fz_sha256_s fz_sha256;

struct fz_sha256_s
{
	unsigned int state[8];
	unsigned int count[2];
	union {
		unsigned char u8[64];
		unsigned int u32[16];
	} buffer;
};

void fz_sha256_init(fz_sha256 *state);
void fz_sha256_update(fz_sha256 *state, const unsigned char *input, unsigned int inlen);
void fz_sha256_final(fz_sha256 *state, unsigned char digest[32]);

/* arc4 crypto */

typedef struct fz_arc4_s fz_arc4;

struct fz_arc4_s
{
	unsigned x;
	unsigned y;
	unsigned char state[256];
};

void fz_arc4_init(fz_arc4 *state, const unsigned char *key, unsigned len);
void fz_arc4_encrypt(fz_arc4 *state, unsigned char *dest, const unsigned char *src, unsigned len);

/* AES block cipher implementation from XYSSL */

typedef struct fz_aes_s fz_aes;

#define AES_DECRYPT 0
#define AES_ENCRYPT 1

struct fz_aes_s
{
	int nr; /* number of rounds */
	unsigned long *rk; /* AES round keys */
	unsigned long buf[68]; /* unaligned data */
};

void aes_setkey_enc( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_setkey_dec( fz_aes *ctx, const unsigned char *key, int keysize );
void aes_crypt_cbc( fz_aes *ctx, int mode, int length,
	unsigned char iv[16],
	const unsigned char *input,
	unsigned char *output );

/*
 * Dynamic objects.
 * The same type of objects as found in PDF and PostScript.
 * Used by the filters and the mupdf parser.
 */

typedef struct fz_obj_s fz_obj;

extern fz_obj *(*fz_resolve_indirect)(fz_obj *obj);

fz_obj *fz_new_null(fz_context *ctx);
fz_obj *fz_new_bool(fz_context *ctx, int b);
fz_obj *fz_new_int(fz_context *ctx, int i);
fz_obj *fz_new_real(fz_context *ctx, float f);
fz_obj *fz_new_name(fz_context *ctx, char *str);
fz_obj *fz_new_string(fz_context *ctx, char *str, int len);
fz_obj *fz_new_indirect(fz_context *ctx, int num, int gen, void *doc);

fz_obj *fz_new_array(fz_context *ctx, int initialcap);
fz_obj *fz_new_dict(fz_context *ctx, int initialcap);
fz_obj *fz_copy_array(fz_context *ctx, fz_obj *array);
fz_obj *fz_copy_dict(fz_context *ctx, fz_obj *dict);

fz_obj *fz_keep_obj(fz_obj *obj);
void fz_drop_obj(fz_obj *obj);

/* type queries */
int fz_is_null(fz_obj *obj);
int fz_is_bool(fz_obj *obj);
int fz_is_int(fz_obj *obj);
int fz_is_real(fz_obj *obj);
int fz_is_name(fz_obj *obj);
int fz_is_string(fz_obj *obj);
int fz_is_array(fz_obj *obj);
int fz_is_dict(fz_obj *obj);
int fz_is_indirect(fz_obj *obj);

int fz_objcmp(fz_obj *a, fz_obj *b);

/* dict marking and unmarking functions - to avoid infinite recursions */
int fz_dict_marked(fz_obj *obj);
int fz_dict_mark(fz_obj *obj);
void fz_dict_unmark(fz_obj *obj);

/* safe, silent failure, no error reporting on type mismatches */
int fz_to_bool(fz_obj *obj);
int fz_to_int(fz_obj *obj);
float fz_to_real(fz_obj *obj);
char *fz_to_name(fz_obj *obj);
char *fz_to_str_buf(fz_obj *obj);
fz_obj *fz_to_dict(fz_obj *obj);
int fz_to_str_len(fz_obj *obj);
int fz_to_num(fz_obj *obj);
int fz_to_gen(fz_obj *obj);

int fz_array_len(fz_obj *array);
fz_obj *fz_array_get(fz_obj *array, int i);
void fz_array_put(fz_obj *array, int i, fz_obj *obj);
void fz_array_push(fz_obj *array, fz_obj *obj);
void fz_array_insert(fz_obj *array, fz_obj *obj);
int fz_array_contains(fz_obj *array, fz_obj *obj);

int fz_dict_len(fz_obj *dict);
fz_obj *fz_dict_get_key(fz_obj *dict, int idx);
fz_obj *fz_dict_get_val(fz_obj *dict, int idx);
fz_obj *fz_dict_get(fz_obj *dict, fz_obj *key);
fz_obj *fz_dict_gets(fz_obj *dict, char *key);
fz_obj *fz_dict_getsa(fz_obj *dict, char *key, char *abbrev);
void fz_dict_put(fz_obj *dict, fz_obj *key, fz_obj *val);
void fz_dict_puts(fz_obj *dict, char *key, fz_obj *val);
void fz_dict_del(fz_obj *dict, fz_obj *key);
void fz_dict_dels(fz_obj *dict, char *key);
void fz_sort_dict(fz_obj *dict);

int fz_fprint_obj(FILE *fp, fz_obj *obj, int tight);
void fz_debug_obj(fz_obj *obj);
void fz_debug_ref(fz_obj *obj);

void fz_set_str_len(fz_obj *obj, int newlen); /* private */
void *fz_get_indirect_document(fz_obj *obj); /* private */

/*
 * Data buffers.
 */

typedef struct fz_buffer_s fz_buffer;

struct fz_buffer_s
{
	int refs;
	unsigned char *data;
	int cap, len;
};

fz_buffer *fz_new_buffer(fz_context *ctx, int size);
fz_buffer *fz_keep_buffer(fz_context *ctx, fz_buffer *buf);
void fz_drop_buffer(fz_context *ctx, fz_buffer *buf);

void fz_resize_buffer(fz_context *ctx, fz_buffer *buf, int size);
void fz_grow_buffer(fz_context *ctx, fz_buffer *buf);

/*
 * Resource store
 */

typedef struct fz_storable_s fz_storable;

typedef struct fz_item_s fz_item;

typedef void (fz_store_free_fn)(fz_context *, fz_storable *);

struct fz_storable_s {
	int refs;
	fz_store_free_fn *free;
};

#define FZ_INIT_STORABLE(S_,RC,FREE) \
	do { fz_storable *S = &(S_)->storable; S->refs = (RC); \
	S->free = (FREE); \
	} while (0)

enum {
	FZ_STORE_UNLIMITED = 0,
	FZ_STORE_DEFAULT = 256 << 20,
};

void fz_new_store_context(fz_context *ctx, unsigned int max);
void fz_drop_store_context(fz_context *ctx);
fz_store *fz_store_keep(fz_context *ctx);
void fz_debug_store(fz_context *ctx);

void *fz_keep_storable(fz_context *, fz_storable *);
void fz_drop_storable(fz_context *, fz_storable *);

void fz_store_item(fz_context *ctx, fz_obj *key, void *val, unsigned int itemsize);
void *fz_find_item(fz_context *ctx, fz_store_free_fn *freefn, fz_obj *key);
void fz_remove_item(fz_context *ctx, fz_store_free_fn *freefn, fz_obj *key);
void fz_empty_store(fz_context *ctx);
int fz_store_scavenge(fz_context *ctx, unsigned int size, int *phase);

/*
 * Buffered reader.
 * Only the data between rp and wp is valid data.
 */

typedef struct fz_stream_s fz_stream;

struct fz_stream_s
{
	fz_context *ctx;
	int refs;
	int error;
	int eof;
	int pos;
	int avail;
	int bits;
	int locked;
	unsigned char *bp, *rp, *wp, *ep;
	void *state;
	int (*read)(fz_stream *stm, unsigned char *buf, int len);
	void (*close)(fz_context *ctx, void *state);
	void (*seek)(fz_stream *stm, int offset, int whence);
	unsigned char buf[4096];
};

fz_stream *fz_open_fd(fz_context *ctx, int file);
fz_stream *fz_open_file(fz_context *ctx, const char *filename);
fz_stream *fz_open_file_w(fz_context *ctx, const wchar_t *filename); /* only on win32 */
fz_stream *fz_open_buffer(fz_context *ctx, fz_buffer *buf);
fz_stream *fz_open_memory(fz_context *ctx, unsigned char *data, int len);
void fz_close(fz_stream *stm);
void fz_lock_stream(fz_stream *stm);

fz_stream *fz_new_stream(fz_context *ctx, void*, int(*)(fz_stream*, unsigned char*, int), void(*)(fz_context *, void *));
fz_stream *fz_keep_stream(fz_stream *stm);
void fz_fill_buffer(fz_stream *stm);

int fz_tell(fz_stream *stm);
void fz_seek(fz_stream *stm, int offset, int whence);

int fz_read(fz_stream *stm, unsigned char *buf, int len);
void fz_read_line(fz_stream *stm, char *buf, int max);
fz_buffer *fz_read_all(fz_stream *stm, int initial);

static inline int fz_read_byte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fill_buffer(stm);
		return stm->rp < stm->wp ? *stm->rp++ : EOF;
	}
	return *stm->rp++;
}

static inline int fz_peek_byte(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		fz_fill_buffer(stm);
		return stm->rp < stm->wp ? *stm->rp : EOF;
	}
	return *stm->rp;
}

static inline void fz_unread_byte(fz_stream *stm)
{
	if (stm->rp > stm->bp)
		stm->rp--;
}

static inline int fz_is_eof(fz_stream *stm)
{
	if (stm->rp == stm->wp)
	{
		if (stm->eof)
			return 1;
		return fz_peek_byte(stm) == EOF;
	}
	return 0;
}

static inline unsigned int fz_read_bits(fz_stream *stm, int n)
{
	unsigned int x;

	if (n <= stm->avail)
	{
		stm->avail -= n;
		x = (stm->bits >> stm->avail) & ((1 << n) - 1);
	}
	else
	{
		x = stm->bits & ((1 << stm->avail) - 1);
		n -= stm->avail;
		stm->avail = 0;

		while (n > 8)
		{
			x = (x << 8) | fz_read_byte(stm);
			n -= 8;
		}

		if (n > 0)
		{
			stm->bits = fz_read_byte(stm);
			stm->avail = 8 - n;
			x = (x << n) | (stm->bits >> stm->avail);
		}
	}

	return x;
}

static inline void fz_sync_bits(fz_stream *stm)
{
	stm->avail = 0;
}

static inline int fz_is_eof_bits(fz_stream *stm)
{
	return fz_is_eof(stm) && (stm->avail == 0 || stm->bits == EOF);
}

/*
 * Data filters.
 */

fz_stream *fz_open_copy(fz_stream *chain);
fz_stream *fz_open_null(fz_stream *chain, int len);
fz_stream *fz_open_arc4(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_aesd(fz_stream *chain, unsigned char *key, unsigned keylen);
fz_stream *fz_open_a85d(fz_stream *chain);
fz_stream *fz_open_ahxd(fz_stream *chain);
fz_stream *fz_open_rld(fz_stream *chain);
fz_stream *fz_open_dctd(fz_stream *chain, int color_transform);
fz_stream *fz_open_faxd(fz_stream *chain,
	int k, int end_of_line, int encoded_byte_align,
	int columns, int rows, int end_of_block, int black_is_1);
fz_stream *fz_open_flated(fz_stream *chain);
fz_stream *fz_open_lzwd(fz_stream *chain, int early_change);
fz_stream *fz_open_predict(fz_stream *chain, int predictor, int columns, int colors, int bpc);
fz_stream *fz_open_jbig2d(fz_stream *chain, fz_buffer *global);

/*
 * Resources and other graphics related objects.
 */

enum { FZ_MAX_COLORS = 32 };

int fz_find_blendmode(char *name);
char *fz_blendmode_name(int blendmode);

/*
 * Pixmaps have n components per pixel. the last is always alpha.
 * premultiplied alpha when rendering, but non-premultiplied for colorspace
 * conversions and rescaling.
 */

typedef struct fz_pixmap_s fz_pixmap;
typedef struct fz_colorspace_s fz_colorspace;

struct fz_pixmap_s
{
	fz_storable storable;
	int x, y, w, h, n;
	fz_pixmap *mask; /* explicit soft/image mask */
	int interpolate;
	int xres, yres;
	fz_colorspace *colorspace;
	unsigned char *samples;
	int free_samples;
};

fz_bbox fz_bound_pixmap(fz_pixmap *pix);

fz_pixmap *fz_new_pixmap_with_data(fz_context *ctx, fz_colorspace *colorspace, int w, int h, unsigned char *samples);
fz_pixmap *fz_new_pixmap_with_rect(fz_context *ctx, fz_colorspace *, fz_bbox bbox);
fz_pixmap *fz_new_pixmap_with_rect_and_data(fz_context *ctx, fz_colorspace *, fz_bbox bbox, unsigned char *samples);
fz_pixmap *fz_new_pixmap(fz_context *ctx, fz_colorspace *, int w, int h);
fz_pixmap *fz_keep_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_drop_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_free_pixmap_imp(fz_context *ctx, fz_storable *pix);
void fz_clear_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_clear_pixmap_with_value(fz_context *ctx, fz_pixmap *pix, int value);
void fz_clear_pixmap_rect_with_value(fz_context *ctx, fz_pixmap *pix, int value, fz_bbox r);
void fz_copy_pixmap_rect(fz_context *ctx, fz_pixmap *dest, fz_pixmap *src, fz_bbox r);
void fz_premultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_unmultiply_pixmap(fz_context *ctx, fz_pixmap *pix);
fz_pixmap *fz_alpha_from_gray(fz_context *ctx, fz_pixmap *gray, int luminosity);
void fz_invert_pixmap(fz_context *ctx, fz_pixmap *pix);
void fz_gamma_pixmap(fz_context *ctx, fz_pixmap *pix, float gamma);
unsigned int fz_pixmap_size(fz_context *ctx, fz_pixmap *pix);

fz_pixmap *fz_scale_pixmap(fz_context *ctx, fz_pixmap *src, float x, float y, float w, float h, fz_bbox *clip);

void fz_write_pnm(fz_context *ctx, fz_pixmap *pixmap, char *filename);
void fz_write_pam(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha);
void fz_write_png(fz_context *ctx, fz_pixmap *pixmap, char *filename, int savealpha);

fz_pixmap *fz_load_jpx(fz_context *ctx, unsigned char *data, int size, fz_colorspace *cs);
fz_pixmap *fz_load_jpeg(fz_context *doc, unsigned char *data, int size);
fz_pixmap *fz_load_png(fz_context *doc, unsigned char *data, int size);
fz_pixmap *fz_load_tiff(fz_context *doc, unsigned char *data, int size);

/*
 * Bitmaps have 1 component per bit. Only used for creating halftoned versions
 * of contone buffers, and saving out. Samples are stored msb first, akin to
 * pbms.
 */

typedef struct fz_bitmap_s fz_bitmap;

struct fz_bitmap_s
{
	int refs;
	int w, h, stride, n;
	unsigned char *samples;
};

fz_bitmap *fz_new_bitmap(fz_context *ctx, int w, int h, int n);
fz_bitmap *fz_keep_bitmap(fz_context *ctx, fz_bitmap *bit);
void fz_clear_bitmap(fz_context *ctx, fz_bitmap *bit);
void fz_drop_bitmap(fz_context *ctx, fz_bitmap *bit);

void fz_write_pbm(fz_context *ctx, fz_bitmap *bitmap, char *filename);

/*
 * A halftone is a set of threshold tiles, one per component. Each threshold
 * tile is a pixmap, possibly of varying sizes and phases.
 */

typedef struct fz_halftone_s fz_halftone;

struct fz_halftone_s
{
	int refs;
	int n;
	fz_pixmap *comp[1];
};

fz_halftone *fz_new_halftone(fz_context *ctx, int num_comps);
fz_halftone *fz_get_default_halftone(fz_context *ctx, int num_comps);
fz_halftone *fz_keep_halftone(fz_context *ctx, fz_halftone *half);
void fz_drop_halftone(fz_context *ctx, fz_halftone *half);

fz_bitmap *fz_halftone_pixmap(fz_context *ctx, fz_pixmap *pix, fz_halftone *ht);

/*
 * Colorspace resources.
 */

extern fz_colorspace *fz_device_gray;
extern fz_colorspace *fz_device_rgb;
extern fz_colorspace *fz_device_bgr;
extern fz_colorspace *fz_device_cmyk;

struct fz_colorspace_s
{
	fz_storable storable;
	unsigned int size;
	char name[16];
	int n;
	void (*to_rgb)(fz_context *ctx, fz_colorspace *, float *src, float *rgb);
	void (*from_rgb)(fz_context *ctx, fz_colorspace *, float *rgb, float *dst);
	void (*free_data)(fz_context *Ctx, fz_colorspace *);
	void *data;
};

fz_colorspace *fz_new_colorspace(fz_context *ctx, char *name, int n);
fz_colorspace *fz_keep_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_drop_colorspace(fz_context *ctx, fz_colorspace *colorspace);
void fz_free_colorspace_imp(fz_context *ctx, fz_storable *colorspace);

void fz_convert_color(fz_context *ctx, fz_colorspace *srcs, float *srcv, fz_colorspace *dsts, float *dstv);
void fz_convert_pixmap(fz_context *ctx, fz_pixmap *src, fz_pixmap *dst);

fz_colorspace *fz_find_device_colorspace(char *name);

/*
 * Fonts come in two variants:
 *	Regular fonts are handled by FreeType.
 *	Type 3 fonts have callbacks to the interpreter.
 */

typedef struct fz_device_s fz_device;

typedef struct fz_font_s fz_font;
char *ft_error_string(int err);

struct fz_font_s
{
	int refs;
	char name[32];

	void *ft_face; /* has an FT_Face if used */
	int ft_substitute; /* ... substitute metrics */
	int ft_bold; /* ... synthesize bold */
	int ft_italic; /* ... synthesize italic */
	int ft_hint; /* ... force hinting for DynaLab fonts */

	/* origin of font data */
	char *ft_file;
	unsigned char *ft_data;
	int ft_size;

	fz_matrix t3matrix;
	fz_obj *t3resources;
	fz_buffer **t3procs; /* has 256 entries if used */
	float *t3widths; /* has 256 entries if used */
	char *t3flags; /* has 256 entries if used */
	void *t3doc; /* a pdf_document for the callback */
	void (*t3run)(void *doc, fz_obj *resources, fz_buffer *contents, fz_device *dev, fz_matrix ctm, void *gstate);

	fz_rect bbox;	/* font bbox is used only for t3 fonts */

	/* per glyph bounding box cache */
	int use_glyph_bbox;
	int bbox_count;
	fz_rect *bbox_table;

	/* substitute metrics */
	int width_count;
	int *width_table; /* in 1000 units */
};

void fz_new_font_context(fz_context *ctx);
fz_font_context *fz_keep_font_context(fz_context *ctx);
void fz_drop_font_context(fz_context *ctx);

fz_font *fz_new_type3_font(fz_context *ctx, char *name, fz_matrix matrix);

fz_font *fz_new_font_from_memory(fz_context *ctx, unsigned char *data, int len, int index, int use_glyph_bbox);
fz_font *fz_new_font_from_file(fz_context *ctx, char *path, int index, int use_glyph_bbox);

fz_font *fz_keep_font(fz_context *ctx, fz_font *font);
void fz_drop_font(fz_context *ctx, fz_font *font);

void fz_debug_font(fz_context *ctx, fz_font *font);

void fz_set_font_bbox(fz_context *ctx, fz_font *font, float xmin, float ymin, float xmax, float ymax);
fz_rect fz_bound_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm);
int fz_glyph_cacheable(fz_context *ctx, fz_font *font, int gid);

/*
 * Vector path buffer.
 * It can be stroked and dashed, or be filled.
 * It has a fill rule (nonzero or even_odd).
 *
 * When rendering, they are flattened, stroked and dashed straight
 * into the Global Edge List.
 */

typedef struct fz_path_s fz_path;
typedef struct fz_stroke_state_s fz_stroke_state;

typedef union fz_path_item_s fz_path_item;

typedef enum fz_path_item_kind_e
{
	FZ_MOVETO,
	FZ_LINETO,
	FZ_CURVETO,
	FZ_CLOSE_PATH
} fz_path_item_kind;

typedef enum fz_linecap_e
{
	FZ_LINECAP_BUTT = 0,
	FZ_LINECAP_ROUND = 1,
	FZ_LINECAP_SQUARE = 2,
	FZ_LINECAP_TRIANGLE = 3
} fz_linecap;

typedef enum fz_linejoin_e
{
	FZ_LINEJOIN_MITER = 0,
	FZ_LINEJOIN_ROUND = 1,
	FZ_LINEJOIN_BEVEL = 2,
	FZ_LINEJOIN_MITER_XPS = 3
} fz_linejoin;

union fz_path_item_s
{
	fz_path_item_kind k;
	float v;
};

struct fz_path_s
{
	int len, cap;
	fz_path_item *items;
	int last;
};

struct fz_stroke_state_s
{
	fz_linecap start_cap, dash_cap, end_cap;
	fz_linejoin linejoin;
	float linewidth;
	float miterlimit;
	float dash_phase;
	int dash_len;
	float dash_list[32];
};

fz_path *fz_new_path(fz_context *ctx);
void fz_moveto(fz_context*, fz_path*, float x, float y);
void fz_lineto(fz_context*, fz_path*, float x, float y);
void fz_curveto(fz_context*,fz_path*, float, float, float, float, float, float);
void fz_curvetov(fz_context*,fz_path*, float, float, float, float);
void fz_curvetoy(fz_context*,fz_path*, float, float, float, float);
void fz_closepath(fz_context*,fz_path*);
void fz_free_path(fz_context *ctx, fz_path *path);

void fz_transform_path(fz_context *ctx, fz_path *path, fz_matrix transform);

fz_path *fz_clone_path(fz_context *ctx, fz_path *old);

fz_rect fz_bound_path(fz_context *ctx, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm);
void fz_debug_path(fz_context *ctx, fz_path *, int indent);

/*
 * Glyph cache
 */

void fz_new_glyph_cache_context(fz_context *ctx);
fz_glyph_cache *fz_keep_glyph_cache(fz_context *ctx);
void fz_drop_glyph_cache_context(fz_context *ctx);
void fz_purge_glyph_cache(fz_context *ctx);

fz_pixmap *fz_render_ft_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm);
fz_pixmap *fz_render_t3_glyph(fz_context *ctx, fz_font *font, int cid, fz_matrix trm, fz_colorspace *model);
fz_pixmap *fz_render_ft_stroked_glyph(fz_context *ctx, fz_font *font, int gid, fz_matrix trm, fz_matrix ctm, fz_stroke_state *state);
fz_pixmap *fz_render_glyph(fz_context *ctx, fz_font*, int, fz_matrix, fz_colorspace *model);
fz_pixmap *fz_render_stroked_glyph(fz_context *ctx, fz_font*, int, fz_matrix, fz_matrix, fz_stroke_state *stroke);
void fz_render_t3_glyph_direct(fz_context *ctx, fz_device *dev, fz_font *font, int gid, fz_matrix trm, void *gstate);

/*
 * Text buffer.
 *
 * The trm field contains the a, b, c and d coefficients.
 * The e and f coefficients come from the individual elements,
 * together they form the transform matrix for the glyph.
 *
 * Glyphs are referenced by glyph ID.
 * The Unicode text equivalent is kept in a separate array
 * with indexes into the glyph array.
 */

typedef struct fz_text_s fz_text;
typedef struct fz_text_item_s fz_text_item;

struct fz_text_item_s
{
	float x, y;
	int gid; /* -1 for one gid to many ucs mappings */
	int ucs; /* -1 for one ucs to many gid mappings */
};

struct fz_text_s
{
	fz_font *font;
	fz_matrix trm;
	int wmode;
	int len, cap;
	fz_text_item *items;
};

fz_text *fz_new_text(fz_context *ctx, fz_font *face, fz_matrix trm, int wmode);
void fz_add_text(fz_context *ctx, fz_text *text, int gid, int ucs, float x, float y);
void fz_free_text(fz_context *ctx, fz_text *text);
fz_rect fz_bound_text(fz_context *ctx, fz_text *text, fz_matrix ctm);
fz_text *fz_clone_text(fz_context *ctx, fz_text *old);
void fz_debug_text(fz_context *ctx, fz_text*, int indent);

/*
 * The shading code uses gouraud shaded triangle meshes.
 */

enum
{
	FZ_LINEAR,
	FZ_RADIAL,
	FZ_MESH,
};

typedef struct fz_shade_s fz_shade;

struct fz_shade_s
{
	fz_storable storable;

	fz_rect bbox;		/* can be fz_infinite_rect */
	fz_colorspace *colorspace;

	fz_matrix matrix;	/* matrix from pattern dict */
	int use_background;	/* background color for fills but not 'sh' */
	float background[FZ_MAX_COLORS];

	int use_function;
	float function[256][FZ_MAX_COLORS + 1];

	int type; /* linear, radial, mesh */
	int extend[2];

	int mesh_len;
	int mesh_cap;
	float *mesh; /* [x y 0], [x y r], [x y t] or [x y c1 ... cn] */
};

fz_shade *fz_keep_shade(fz_context *ctx, fz_shade *shade);
void fz_drop_shade(fz_context *ctx, fz_shade *shade);
void fz_free_shade_imp(fz_context *ctx, fz_storable *shade);
void fz_debug_shade(fz_context *ctx, fz_shade *shade);

fz_rect fz_bound_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm);
void fz_paint_shade(fz_context *ctx, fz_shade *shade, fz_matrix ctm, fz_pixmap *dest, fz_bbox bbox);

/*
 * Scan converter
 */

int fz_get_aa_level(fz_context *ctx);
void fz_set_aa_level(fz_context *ctx, int bits);

typedef struct fz_gel_s fz_gel;

fz_gel *fz_new_gel(fz_context *ctx);
void fz_insert_gel(fz_gel *gel, float x0, float y0, float x1, float y1);
void fz_reset_gel(fz_gel *gel, fz_bbox clip);
void fz_sort_gel(fz_gel *gel);
fz_bbox fz_bound_gel(fz_gel *gel);
void fz_free_gel(fz_gel *gel);
int fz_is_rect_gel(fz_gel *gel);

void fz_scan_convert(fz_gel *gel, int eofill, fz_bbox clip, fz_pixmap *pix, unsigned char *colorbv);

void fz_flatten_fill_path(fz_gel *gel, fz_path *path, fz_matrix ctm, float flatness);
void fz_flatten_stroke_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth);
void fz_flatten_dash_path(fz_gel *gel, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, float flatness, float linewidth);

/*
 * The device interface.
 */

enum
{
	/* Hints */
	FZ_IGNORE_IMAGE = 1,
	FZ_IGNORE_SHADE = 2,

	/* Flags */
	FZ_DEVFLAG_MASK = 1,
	FZ_DEVFLAG_COLOR = 2,
	FZ_DEVFLAG_UNCACHEABLE = 4,
	FZ_DEVFLAG_FILLCOLOR_UNDEFINED = 8,
	FZ_DEVFLAG_STROKECOLOR_UNDEFINED = 16,
	FZ_DEVFLAG_STARTCAP_UNDEFINED = 32,
	FZ_DEVFLAG_DASHCAP_UNDEFINED = 64,
	FZ_DEVFLAG_ENDCAP_UNDEFINED = 128,
	FZ_DEVFLAG_LINEJOIN_UNDEFINED = 256,
	FZ_DEVFLAG_MITERLIMIT_UNDEFINED = 512,
	FZ_DEVFLAG_LINEWIDTH_UNDEFINED = 1024,
	/* Arguably we should have a bit for the dash pattern itself being
	 * undefined, but that causes problems; do we assume that it should
	 * always be set to non-dashing at the start of every glyph? */
};

struct fz_device_s
{
	int hints;
	int flags;

	void *user;
	void (*free_user)(fz_device *);
	fz_context *ctx;

	void (*fill_path)(fz_device *, fz_path *, int even_odd, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_path)(fz_device *, fz_path *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_path)(fz_device *, fz_path *, fz_rect *rect, int even_odd, fz_matrix);
	void (*clip_stroke_path)(fz_device *, fz_path *, fz_rect *rect, fz_stroke_state *, fz_matrix);

	void (*fill_text)(fz_device *, fz_text *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*stroke_text)(fz_device *, fz_text *, fz_stroke_state *, fz_matrix, fz_colorspace *, float *color, float alpha);
	void (*clip_text)(fz_device *, fz_text *, fz_matrix, int accumulate);
	void (*clip_stroke_text)(fz_device *, fz_text *, fz_stroke_state *, fz_matrix);
	void (*ignore_text)(fz_device *, fz_text *, fz_matrix);

	void (*fill_shade)(fz_device *, fz_shade *shd, fz_matrix ctm, float alpha);
	void (*fill_image)(fz_device *, fz_pixmap *img, fz_matrix ctm, float alpha);
	void (*fill_image_mask)(fz_device *, fz_pixmap *img, fz_matrix ctm, fz_colorspace *, float *color, float alpha);
	void (*clip_image_mask)(fz_device *, fz_pixmap *img, fz_rect *rect, fz_matrix ctm);

	void (*pop_clip)(fz_device *);

	void (*begin_mask)(fz_device *, fz_rect, int luminosity, fz_colorspace *, float *bc);
	void (*end_mask)(fz_device *);
	void (*begin_group)(fz_device *, fz_rect, int isolated, int knockout, int blendmode, float alpha);
	void (*end_group)(fz_device *);

	void (*begin_tile)(fz_device *, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
	void (*end_tile)(fz_device *);
};

void fz_fill_path(fz_device *dev, fz_path *path, int even_odd, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_path(fz_device *dev, fz_path *path, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_path(fz_device *dev, fz_path *path, fz_rect *rect, int even_odd, fz_matrix ctm);
void fz_clip_stroke_path(fz_device *dev, fz_path *path, fz_rect *rect, fz_stroke_state *stroke, fz_matrix ctm);
void fz_fill_text(fz_device *dev, fz_text *text, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_text(fz_device *dev, fz_text *text, fz_matrix ctm, int accumulate);
void fz_clip_stroke_text(fz_device *dev, fz_text *text, fz_stroke_state *stroke, fz_matrix ctm);
void fz_ignore_text(fz_device *dev, fz_text *text, fz_matrix ctm);
void fz_pop_clip(fz_device *dev);
void fz_fill_shade(fz_device *dev, fz_shade *shade, fz_matrix ctm, float alpha);
void fz_fill_image(fz_device *dev, fz_pixmap *image, fz_matrix ctm, float alpha);
void fz_fill_image_mask(fz_device *dev, fz_pixmap *image, fz_matrix ctm, fz_colorspace *colorspace, float *color, float alpha);
void fz_clip_image_mask(fz_device *dev, fz_pixmap *image, fz_rect *rect, fz_matrix ctm);
void fz_begin_mask(fz_device *dev, fz_rect area, int luminosity, fz_colorspace *colorspace, float *bc);
void fz_end_mask(fz_device *dev);
void fz_begin_group(fz_device *dev, fz_rect area, int isolated, int knockout, int blendmode, float alpha);
void fz_end_group(fz_device *dev);
void fz_begin_tile(fz_device *dev, fz_rect area, fz_rect view, float xstep, float ystep, fz_matrix ctm);
void fz_end_tile(fz_device *dev);

fz_device *fz_new_device(fz_context *ctx, void *user);
void fz_free_device(fz_device *dev);

fz_device *fz_new_trace_device(fz_context *ctx);
fz_device *fz_new_bbox_device(fz_context *ctx, fz_bbox *bboxp);
fz_device *fz_new_draw_device(fz_context *ctx, fz_pixmap *dest);
fz_device *fz_new_draw_device_type3(fz_context *ctx, fz_pixmap *dest);

/*
 * Text extraction device
 */

typedef struct fz_text_span_s fz_text_span;
typedef struct fz_text_char_s fz_text_char;

struct fz_text_char_s
{
	int c;
	fz_bbox bbox;
};

struct fz_text_span_s
{
	fz_font *font;
	float size;
	int wmode;
	int len, cap;
	fz_text_char *text;
	fz_text_span *next;
	int eol;
};

fz_text_span *fz_new_text_span(fz_context *ctx);
void fz_free_text_span(fz_context *ctx, fz_text_span *line);
void fz_debug_text_span(fz_text_span *line);
void fz_debug_text_span_xml(fz_text_span *span);

fz_device *fz_new_text_device(fz_context *ctx, fz_text_span *text);

/*
 * Cookie support - simple communication channel between app/library.
 */

typedef struct fz_cookie_s fz_cookie;

struct fz_cookie_s
{
	int abort;
	int progress;
	int progress_max; /* -1 for unknown */
};

/*
 * Display list device -- record and play back device commands.
 */

typedef struct fz_display_list_s fz_display_list;

fz_display_list *fz_new_display_list(fz_context *ctx);
void fz_free_display_list(fz_context *ctx, fz_display_list *list);
fz_device *fz_new_list_device(fz_context *ctx, fz_display_list *list);
void fz_run_display_list(fz_display_list *list, fz_device *dev, fz_matrix ctm, fz_bbox area, fz_cookie *cookie);

/*
 * Plotting functions.
 */

void fz_decode_tile(fz_pixmap *pix, float *decode);
void fz_decode_indexed_tile(fz_pixmap *pix, float *decode, int maxval);
void fz_unpack_tile(fz_pixmap *dst, unsigned char * restrict src, int n, int depth, int stride, int scale);

void fz_paint_solid_alpha(unsigned char * restrict dp, int w, int alpha);
void fz_paint_solid_color(unsigned char * restrict dp, int n, int w, unsigned char *color);

void fz_paint_span(unsigned char * restrict dp, unsigned char * restrict sp, int n, int w, int alpha);
void fz_paint_span_with_color(unsigned char * restrict dp, unsigned char * restrict mp, int n, int w, unsigned char *color);

void fz_paint_image(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *shape, fz_pixmap *img, fz_matrix ctm, int alpha);
void fz_paint_image_with_color(fz_pixmap *dst, fz_bbox scissor, fz_pixmap *shape, fz_pixmap *img, fz_matrix ctm, unsigned char *colorbv);

void fz_paint_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha);
void fz_paint_pixmap_with_mask(fz_pixmap *dst, fz_pixmap *src, fz_pixmap *msk);
void fz_paint_pixmap_with_rect(fz_pixmap *dst, fz_pixmap *src, int alpha, fz_bbox bbox);

void fz_blend_pixmap(fz_pixmap *dst, fz_pixmap *src, int alpha, int blendmode, int isolated, fz_pixmap *shape);
void fz_blend_pixel(unsigned char dp[3], unsigned char bp[3], unsigned char sp[3], int blendmode);

enum
{
	/* PDF 1.4 -- standard separable */
	FZ_BLEND_NORMAL,
	FZ_BLEND_MULTIPLY,
	FZ_BLEND_SCREEN,
	FZ_BLEND_OVERLAY,
	FZ_BLEND_DARKEN,
	FZ_BLEND_LIGHTEN,
	FZ_BLEND_COLOR_DODGE,
	FZ_BLEND_COLOR_BURN,
	FZ_BLEND_HARD_LIGHT,
	FZ_BLEND_SOFT_LIGHT,
	FZ_BLEND_DIFFERENCE,
	FZ_BLEND_EXCLUSION,

	/* PDF 1.4 -- standard non-separable */
	FZ_BLEND_HUE,
	FZ_BLEND_SATURATION,
	FZ_BLEND_COLOR,
	FZ_BLEND_LUMINOSITY,

	/* For packing purposes */
	FZ_BLEND_MODEMASK = 15,
	FZ_BLEND_ISOLATED = 16,
	FZ_BLEND_KNOCKOUT = 32
};

/* Links */

typedef struct fz_link_s fz_link;

typedef struct fz_link_dest_s fz_link_dest;

typedef enum fz_link_kind_e
{
	FZ_LINK_NONE = 0,
	FZ_LINK_GOTO,
	FZ_LINK_URI,
	FZ_LINK_LAUNCH,
	FZ_LINK_NAMED,
	FZ_LINK_GOTOR
} fz_link_kind;

enum {
	fz_link_flag_l_valid = 1, /* lt.x is valid */
	fz_link_flag_t_valid = 2, /* lt.y is valid */
	fz_link_flag_r_valid = 4, /* rb.x is valid */
	fz_link_flag_b_valid = 8, /* rb.y is valid */
	fz_link_flag_fit_h = 16, /* Fit horizontally */
	fz_link_flag_fit_v = 32, /* Fit vertically */
	fz_link_flag_r_is_zoom = 64 /* rb.x is actually a zoom figure */
};

struct fz_link_dest_s
{
	fz_link_kind kind;
	union
	{
		struct
		{
			int page;
			int flags;
			fz_point lt;
			fz_point rb;
			char *file_spec;
			int new_window;
		}
		gotor;
		struct
		{
			char *uri;
			int is_map;
		}
		uri;
		struct
		{
			char *file_spec;
			int new_window;
		}
		launch;
		struct
		{
			char *named;
		}
		named;
	}
	ld;
};

struct fz_link_s
{
	int refs;
	fz_rect rect;
	fz_link_dest dest;
	fz_link *next;
};

fz_link *fz_new_link(fz_context *ctx, fz_rect bbox, fz_link_dest dest);
fz_link *fz_keep_link(fz_context *ctx, fz_link *link);
void fz_drop_link(fz_context *ctx, fz_link *link);
void fz_free_link_dest(fz_context *ctx, fz_link_dest *dest);

/* Outline */

typedef struct fz_outline_s fz_outline;

struct fz_outline_s
{
	char *title;
	fz_link_dest dest;
	fz_outline *next;
	fz_outline *down;
};

void fz_debug_outline_xml(fz_context *ctx, fz_outline *outline, int level);
void fz_debug_outline(fz_context *ctx, fz_outline *outline, int level);
void fz_free_outline(fz_context *ctx, fz_outline *outline);

/* Document interface */

typedef struct fz_document_s fz_document;
typedef struct fz_page_s fz_page; /* doesn't have a definition -- always cast to *_page */

struct fz_document_s
{
	void (*close)(fz_document *);
	int (*needs_password)(fz_document *doc);
	int (*authenticate_password)(fz_document *doc, char *password);
	fz_outline *(*load_outline)(fz_document *doc);
	int (*count_pages)(fz_document *doc);
	fz_page *(*load_page)(fz_document *doc, int number);
	fz_link *(*load_links)(fz_document *doc, fz_page *page);
	fz_rect (*bound_page)(fz_document *doc, fz_page *page);
	void (*run_page)(fz_document *doc, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);
	void (*free_page)(fz_document *doc, fz_page *page);
};

fz_document *fz_open_document(fz_context *ctx, char *filename);

void fz_close_document(fz_document *doc);

int fz_needs_password(fz_document *doc);
int fz_authenticate_password(fz_document *doc, char *password);

fz_outline *fz_load_outline(fz_document *doc);

int fz_count_pages(fz_document *doc);
fz_page *fz_load_page(fz_document *doc, int number);
fz_link *fz_load_links(fz_document *doc, fz_page *page);
fz_rect fz_bound_page(fz_document *doc, fz_page *page);
void fz_run_page(fz_document *doc, fz_page *page, fz_device *dev, fz_matrix transform, fz_cookie *cookie);
void fz_free_page(fz_document *doc, fz_page *page);

#endif
