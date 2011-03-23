//
//  tns_core.c:  core code for a tnetstring parser in C
//
//  This is code for parsing and rendering data in the provisional
//  typed-netstring format proposed for inclusion in Mongrel2.  You can
//  think of it like a JSON library that uses a simpler wire format.
//
//  This code is *not* designed to be compiled as a standalone library.
//  Instead, you provide a suite of low-level data manipulation functions
//  and then #include this code to stitch them into a tnetstring parser.
//

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

typedef struct tns_outbuf_s {
  char *buffer;
  size_t used_size;
  size_t alloc_size;
} tns_outbuf;

typedef enum tns_type_tag_e {
    tns_string_tag = ',',
    tns_number_tag = '#',
    tns_bool_tag = '!',
    tns_null_tag = '~',
    tns_dict_tag = '}',
    tns_list_tag = ']',
} tns_type_tag;

//  After #including this code, you must provide implementations for the
//  following functions.  They provide the low-level data manipulation
//  routines from which we can build a parser and renderer.

//  Functions called to describe an error situation.
static void tns_parse_error(const char *errstr);
static void tns_render_error(const char *errstr);

//  Functions to introspect the type of a data object.
static tns_type_tag tns_get_type(void *val);

//  Functions for parsing and rendering primitive datatypes.
static void *tns_parse_string(const char *data, size_t len);
static int tns_render_string(void *val, tns_outbuf *outbuf);
static void *tns_parse_integer(const char *data, size_t len);
static void *tns_parse_float(const char *data, size_t len);
static int tns_render_number(void *val, tns_outbuf *outbuf);
static int tns_render_bool(void *val, tns_outbuf *outbuf);

//  Constructors to get constant primitive datatypes.
static void *tns_get_null(void);
static void *tns_get_true(void);
static void *tns_get_false(void);

//  Functions for manipulating compound datatypes.
static void *tns_new_dict(void);
static void tns_free_dict(void*);
static int tns_add_to_dict(void *dict, void *key, void *item);
static int tns_render_dict(void *dict, tns_outbuf *outbuf);
static void *tns_new_list(void);
static void tns_free_list(void*);
static int tns_add_to_list(void *list, void *item);
static int tns_render_list(void *dict, tns_outbuf *outbuf);


//  The rest of ths code stitches the above functions together into a
//  tnetstring parser and renderer.  You get the following functions:

//  Parse an object off the front of a tnetstring.
//  Returns a pointer to the parsed object, or NULL if an error occurs.
//  The third argument is an output parameter; if non-NULL it will
//  receive the unparsed remainder of the string.
static void*
tns_parse(const char *data, size_t len, char** remain);

//  Render an object into a string.
//  On success this function returns a malloced string containing
//  the serialization of the given object.  If the second argument
//  'len' is non-NULL it will receive the number of bytes in the string.
//  The caller is responsible for freeing the returned string.
//  On failure this function returns NULL.
static char*
tns_render(void *val, size_t *len);
  
//  Render an object into a string, in reverse.
//  This is just like tns_render but the output string contains the
//  rendered data in reverse.  This is actually how the internal routines
//  produce it since it involves less copying of data.  If you need to
//  copy the string off somewhere anyway, call tns_render_reversed and
//  save yourself the cost of reversing it in-place.
static char*
tns_render_reversed(void *val, size_t *len);


//  The rest of these are for internal use only.

static inline int
tns_str_is_float(const char *data, size_t len);

static inline int
tns_parse_dict(void *dict, const char *data, size_t len);

static inline int
tns_parse_list(void *list, const char *data, size_t len);

static int
tns_render_value(void *val, tns_outbuf *outbuf);

static inline int
tns_outbuf_itoa(size_t n, tns_outbuf *outbuf);

static inline int
tns_outbuf_init(tns_outbuf *outbuf);

static inline void
tns_outbuf_free(tns_outbuf *outbuf);

static inline int
tns_outbuf_extend(tns_outbuf *outbuf);

static inline int
tns_outbuf_putc(tns_outbuf *outbuf, char c);

static inline int
tns_outbuf_rputs(tns_outbuf *outbuf, const char *data, size_t len);

static void
tns_inplace_reverse(char *data, size_t len);


static void*
tns_parse(const char *data, size_t len, char **remain)
{
  void *val;
  char *valstr;
  tns_type_tag type;
  size_t vallen;

  //  Read the length of the value, and verify that is ends in a colon.
  vallen = strtol(data, &valstr, 10);
  if(valstr == data) {
      tns_parse_error("not a tnetstring: no length prefix");
      return NULL;
  }
  if((valstr + vallen) >= (data + len) || *valstr != ':') {
      tns_parse_error("not a tnetstring: invalid length prefix");
      return NULL;
  }
  valstr++;

  //  Grab the type tag from the end of the value.
  type = valstr[vallen];

  //  Output the remainder of the string if necessary.
  if(remain != NULL) {
      *remain = valstr + vallen + 1;
  }

  //  Now dispatch type parsing based on the type tag.
  switch(type) {
    //  Primitive type: a string blob.
    case tns_string_tag:
        val = tns_parse_string(valstr, vallen);
        break;
    //  Primitive type: a number.
    //  I'm branching out here and allowing both floats and ints.
    case tns_number_tag:
        if(tns_str_is_float(valstr,vallen)) {
            val = tns_parse_float(valstr, vallen);
            if(val == NULL) {
                tns_parse_error("not a tnetstring: invalid float literal");
            }
        } else {
            val = tns_parse_integer(valstr, vallen);
            if(val == NULL) {
                tns_parse_error("not a tnetstring: invalid integer literal");
            }
        }
        break;
    //  Primitive type: a boolean.
    //  The only acceptable values are "true" and "false".
    case tns_bool_tag:
        if(vallen == 4 && strncmp(valstr,"true",4) == 0) {
            val = tns_get_true();
        } else if(vallen == 5 && strncmp(valstr,"false",5) == 0) {
            val = tns_get_false();
        } else {
            tns_parse_error("not a tnetstring: invalid boolean literal");
            val = NULL;
        }
        break;
    //  Primitive type: a null.
    //  This must be a zero-length string.
    case tns_null_tag:
        if(vallen != 0) {
            tns_parse_error("not a tnetstring: invalid null literal");
            val = NULL;
        } else {
            val = tns_get_null();
        }
        break;
    //  Compound type: a dict.
    //  The data is written <key><value><key><value>
    case tns_dict_tag:
        val = tns_new_dict();
        if(tns_parse_dict(val,valstr,vallen) == -1) {
            tns_free_dict(val);
            tns_parse_error("not a tnetstring: broken dict items");
            val = NULL;
        }
        break;
    //  Compound type: a list.
    //  The data is written <item><item><item>
    case tns_list_tag:
        val = tns_new_list();
        if(tns_parse_list(val,valstr,vallen) == -1) {
            tns_free_list(val);
            tns_parse_error("not a tnetstring: broken list items");
            val = NULL;
        }
        break;
    //  Whoops, that ain't a tnetstring.
    default:
      tns_parse_error("not a tnetstring: invalid type tag");
      val = NULL;
  }

  return val;
}


static char*
tns_render(void *val, size_t *len)
{
  char *output;
  output = tns_render_reversed(val, len);
  if(output != NULL) {
      tns_inplace_reverse(output, *len);
  }
  return output;
}


static char*
tns_render_reversed(void *val, size_t *len)
{
  tns_outbuf outbuf;
  
  if(tns_outbuf_init(&outbuf) == -1 ) {
      return NULL;
  }

  if(tns_render_value(val, &outbuf) == -1) {
      tns_outbuf_free(&outbuf);
  }

  *len = outbuf.used_size;
  return outbuf.buffer;
}


static int
tns_render_value(void *val, tns_outbuf *outbuf)
{
  tns_type_tag type;
  int res;
  size_t datalen;

  //  Find out the type tag for the given value.
  type = tns_get_type(val);
  if(type == 0) {
      tns_render_error("type not serializable");
      return -1;
  }
  tns_outbuf_putc(outbuf,type);
  datalen = outbuf->used_size;

  //  Render it into the output buffer, leaving space for the
  //  type tag at the end.
  switch(type) {
    case tns_string_tag:
      res = tns_render_string(val, outbuf);
      break;
    case tns_number_tag:
      res = tns_render_number(val, outbuf);
      break;
    case tns_bool_tag:
      res = tns_render_bool(val, outbuf);
      break;
    case tns_null_tag:
      res = 0;
      break;
    case tns_dict_tag:
      res = tns_render_dict(val, outbuf);
      break;
    case tns_list_tag:
      res = tns_render_list(val, outbuf);
      break;
    default:
      tns_render_error("unknown type tag");
      return -1;
  }

  //  If that succeeds, write the framing info.
  if(res == 0) {
      datalen = outbuf->used_size - datalen;
      tns_outbuf_putc(outbuf, ':');
      res = tns_outbuf_itoa(datalen, outbuf);
  }
  return res;
}


static void
tns_inplace_reverse(char *data, size_t len)
{
  char *dend, c;

  dend = data + len - 1;
  while(dend > data) {
      c = *data;
      *data = *dend;
      *dend = c;
      data++;
      dend--;
  }
}


static inline int
tns_str_is_float(const char *data, size_t len)
{
  size_t i=0;
  while(i < len) {
      switch(data[i]) {
        case '.':
        case 'e':
        case 'E':
          return 1;
      }
      i++;
  }
  return 0;
}


static int
tns_parse_list(void *val, const char *data, size_t len)
{
    void *item;
    char *remain;
    while(len > 0) {
        item = tns_parse(data, len, &remain);
        len = len - (remain - data);
        data = remain;
        if(item == NULL) {
            return -1;
        }
        if(tns_add_to_list(val,item) == -1) {
            return -1;
        }
    }
    return 0;
}


static int
tns_parse_dict(void *val, const char *data, size_t len)
{
    void *key, *item;
    char *remain;
    while(len > 0) {
        key = tns_parse(data, len, &remain);
        len = len - (remain - data);
        data = remain;
        if(key == NULL) {
            return -1;
        }
        item = tns_parse(data, len, &remain);
        len = len - (remain - data);
        data = remain;
        if(item == NULL) {
            return -1;
        }
        if(tns_add_to_dict(val,key,item) == -1) {
            return -1;
        }
    }
    return 0;
}


static inline int
tns_outbuf_itoa(size_t n, tns_outbuf *outbuf)
{
  while(1) {
      if(tns_outbuf_putc(outbuf, n%10+'0') == -1) {
          return -1;
      }
      n = n / 10;
      if(n == 0) {
          return 0;
      }
  }
}


static inline int
tns_outbuf_init(tns_outbuf *outbuf)
{
  outbuf->buffer = malloc(64);
  if(outbuf->buffer == NULL) {
      outbuf->alloc_size = 0;
      outbuf->used_size = 0;
      return -1;
  } else {
      outbuf->alloc_size = 64;
      outbuf->used_size = 0;
      return 0;
  }
}


static inline void
tns_outbuf_free(tns_outbuf *outbuf)
{
  free(outbuf->buffer);
  outbuf->buffer = NULL;
  outbuf->alloc_size = 0;
  outbuf->used_size = 0;
}


static inline int
tns_outbuf_extend(tns_outbuf *outbuf)
{
  char *new_buf;
  size_t new_size;

  new_size = outbuf->alloc_size * 2;
  new_buf = realloc(outbuf->buffer, new_size);
  if(new_buf == NULL) {
      return -1;
  }
  outbuf->buffer = new_buf;
  outbuf->alloc_size = new_size;
  return 0;
}


static inline int
tns_outbuf_putc(tns_outbuf *outbuf, char c)
{
  if(outbuf->alloc_size == outbuf->used_size) {
      if(tns_outbuf_extend(outbuf) == -1) {
          return -1;
      }
  }
  outbuf->buffer[outbuf->used_size++] = c;
  return 0;
}


static inline int
tns_outbuf_rputs(tns_outbuf *outbuf, const char *data, size_t len)
{
  const char *dend;
  char *buffer;

  //  Make sure we have enough room.
  while(outbuf->alloc_size - outbuf->used_size < len) {
      if(tns_outbuf_extend(outbuf) == -1) {
          return -1;
      }
  }

  //  Copy the data in reverse.
  buffer = outbuf->buffer + outbuf->used_size;
  dend = data + len - 1;
  while(dend >= data) {
      *buffer = *dend;
      buffer++;
      dend--;
  }

  outbuf->used_size += len;
  return 0;
}

