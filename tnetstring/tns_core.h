//
//  tns_core.h:  core code for a tnetstring parser in C
//
//  This is code for parsing and rendering data in the provisional
//  typed-netstring format proposed for inclusion in Mongrel2.  You can
//  think of it like a JSON library that uses a simpler wire format.
//
//  This code is *not* designed to be compiled as a standalone library.
//  Instead, you provide a suite of low-level data manipulation functions
//  and then #include "tns_core.c" to stitch them into a tnetstring parser.
//

#ifndef _tns_core_h
#define _tns_core_h

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

#include "dbg.h"

typedef struct tns_outbuf_s {
  char *buffer;
  size_t used_size;
  size_t alloc_size;
} tns_outbuf;

typedef enum tns_type_tag_e {
    tns_tag_string = ',',
    tns_tag_number = '#',
    tns_tag_bool = '!',
    tns_tag_null = '~',
    tns_tag_dict = '}',
    tns_tag_list = ']',
} tns_type_tag;

//  You must provide implementations for the following functions.
//  They provide the low-level data manipulation routines from which
//  we can build a parser and renderer.

//  Functions to introspect the type of a data object.
static tns_type_tag tns_get_type(void *val);

//  Functions for parsing and rendering primitive datatypes.
static void *tns_parse_string(const char *data, size_t len);
static int tns_render_string(void *val, tns_outbuf *outbuf);
static void *tns_parse_number(const char *data, size_t len);
static int tns_render_number(void *val, tns_outbuf *outbuf);
static int tns_render_bool(void *val, tns_outbuf *outbuf);

//  Constructors to get constant primitive datatypes.
static void *tns_get_null(void);
static void *tns_get_true(void);
static void *tns_get_false(void);

//  Functions for manipulating compound datatypes.
static void *tns_new_dict(void);
static int tns_add_to_dict(void *dict, void *key, void *item);
static int tns_render_dict(void *dict, tns_outbuf *outbuf);
static void *tns_new_list(void);
static int tns_add_to_list(void *list, void *item);
static int tns_render_list(void *dict, tns_outbuf *outbuf);

//  Functions for manaing value lifecycle
static void tns_free_value(void *value);


//  In return, you get the following functions.

//  Parse an object off the front of a tnetstring.
//  Returns a pointer to the parsed object, or NULL if an error occurs.
//  The third argument is an output parameter; if non-NULL it will
//  receive the unparsed remainder of the string.
static void* tns_parse(const char *data, size_t len, char** remain);

//  Render an object into a string.
//  On success this function returns a malloced string containing
//  the serialization of the given object.  The second argument
//  'len' is an output parameter that will receive the number of bytes in
//  the string; if NULL then the string will be null-terminated.
//  The caller is responsible for freeing the returned string.
//  On failure this function returns NULL and 'len' is unmodified.
static char* tns_render(void *val, size_t *len);


//  If you need to copy the final result off somewhere else, you 
//  might like to build your own rendering function from the following.
//  It will avoid some double-copying that tns_render does in order
//  to return a proper C string.
//  Basic plan: Initialize an outbuf, pass it to tns_render_value, then
//  copy the bytes away using tns_outbuf_memmove.
static inline int tns_outbuf_init(tns_outbuf *outbuf);
static int tns_render_value(void *val, tns_outbuf *outbuf);
static void tns_outbuf_memmove(tns_outbuf *outbuf, char *dest);



//  The rest of these are for internal use only.

static inline int tns_parse_dict(void *dict, const char *data, size_t len);
static inline int tns_parse_list(void *list, const char *data, size_t len);
static inline int tns_outbuf_itoa(tns_outbuf *outbuf, size_t n);
static inline int tns_outbuf_extend(tns_outbuf *outbuf);
static inline int tns_outbuf_putc(tns_outbuf *outbuf, char c);
static int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len);
static inline int tns_outbuf_clamp(tns_outbuf *outbuf, size_t orig_size);
static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len);
static inline void tns_outbuf_free(tns_outbuf *outbuf);

#endif
