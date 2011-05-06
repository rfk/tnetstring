//
//  tns_core.h:  core code for a tnetstring parser in C
//
//  This is code for parsing and rendering data in the provisional
//  typed-netstring format proposed for inclusion in Mongrel2.  You can
//  think of it like a JSON library that uses a simpler wire format.
//

#ifndef _tns_core_h
#define _tns_core_h

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

//  tnetstring rendering is done using an "outbuf" struct, which combines
//  a malloced string with its allocation information.  Rendering is done
//  from back to front; the details are deliberately hidden here since
//  I'm experimenting with multiple implementations and it might change.
struct tns_outbuf_s;
typedef struct tns_outbuf_s tns_outbuf;

//  This enumeration gives the type tag for each data type in the
//  tnetstring encoding.
typedef enum tns_type_tag_e {
    tns_tag_string = ',',
    tns_tag_integer = '#',
    tns_tag_float = '^',
    tns_tag_bool = '!',
    tns_tag_null = '~',
    tns_tag_dict = '}',
    tns_tag_list = ']',
} tns_type_tag;


//  To convert between tnetstrings and the data structures of your application
//  you provide the following struct filled with function pointers.  They
//  will be called by the core parser/renderer as necessary.
//
//  Each callback is called with the containing struct as its first argument,
//  to allow a primitive type of closure.

struct tns_ops_s;
typedef struct tns_ops_s tns_ops;

struct tns_ops_s {

  //  Get the type of a data object.
  tns_type_tag (*get_type)(const tns_ops *ops, void *val);

  //  Parse various types of object from a string.
  void* (*parse_string)(const tns_ops *ops, const char *data, size_t len);
  void* (*parse_integer)(const tns_ops *ops, const char *data, size_t len);
  void* (*parse_float)(const tns_ops * ops, const char *data, size_t len);

  //  Constructors for constant primitive datatypes.
  void* (*get_null)(const tns_ops *ops);
  void* (*get_true)(const tns_ops *ops);
  void* (*get_false)(const tns_ops *ops);

  //  Render various types of object into a tns_outbuf.
  int (*render_string)(const tns_ops *ops, void *val, tns_outbuf *outbuf);
  int (*render_integer)(const tns_ops *ops, void *val, tns_outbuf *outbuf);
  int (*render_float)(const tns_ops *ops, void *val, tns_outbuf *outbuf);
  int (*render_bool)(const tns_ops *ops, void *val, tns_outbuf *outbuf);

  //  Functions for building and rendering list values.
  //  Remember that rendering is done from back to front, so
  //  you must write the last list element first.
  void* (*new_list)(const tns_ops *ops);
  int (*add_to_list)(const tns_ops *ops, void* list, void* item);
  int (*render_list)(const tns_ops *ops, void* list, tns_outbuf *outbuf);

  //  Functions for building and rendering dict values
  //  Remember that rendering is done from back to front, so
  //  you must write each value first, follow by its key.
  void* (*new_dict)(const tns_ops *ops);
  int (*add_to_dict)(const tns_ops *ops, void* dict, void* key, void* item);
  int (*render_dict)(const tns_ops *ops, void* dict, tns_outbuf *outbuf);

  //  Free values that are no longer in use
  void (*free_value)(const tns_ops *ops, void *value);

};


//  Parse an object off the front of a tnetstring.
//  Returns a pointer to the parsed object, or NULL if an error occurs.
//  The third argument is an output parameter; if non-NULL it will
//  receive the unparsed remainder of the string.
extern void* tns_parse(const tns_ops *ops, const char *data, size_t len, char** remain);

//  If you need to read the length prefix yourself, e.g. because you're
//  reading data off a socket, you can use this function to get just
//  the payload parsing logic.
extern void* tns_parse_payload(const tns_ops *ops, tns_type_tag type, const char *data, size_t len);

//  Render an object into a string.
//  On success this function returns a malloced string containing
//  the serialization of the given object.  The second argument
//  'len' is an output parameter that will receive the number of bytes in
//  the string; if NULL then the string will be null-terminated.
//  The caller is responsible for freeing the returned string.
//  On failure this function returns NULL and 'len' is unmodified.
extern char* tns_render(const tns_ops *ops, void *val, size_t *len);

//  If you need to copy the final result off somewhere else, you 
//  might like to build your own rendering function from the following.
//  It will avoid some double-copying that tns_render does internally.
//  Basic plan: Initialize an outbuf, pass it to tns_render_value, then
//  copy the bytes away using tns_outbuf_memmove.
extern int tns_render_value(const tns_ops *ops, void *val, tns_outbuf *outbuf);
extern int tns_outbuf_init(tns_outbuf *outbuf);
extern void tns_outbuf_memmove(tns_outbuf *outbuf, char *dest);

//  Use these functions for rendering into an outbuf.
extern size_t tns_outbuf_size(tns_outbuf *outbuf);
extern int tns_outbuf_putc(tns_outbuf *outbuf, char c);
extern int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len);

#endif
