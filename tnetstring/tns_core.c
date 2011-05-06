//
//  tns_core.c:  core code for a tnetstring parser in C
//
//  This is code for parsing and rendering data in the provisional
//  typed-netstring format proposed for inclusion in Mongrel2.  You can
//  think of it like a JSON library that uses a simpler wire format.
//

#include "dbg.h"
#include "tns_core.h"

#ifndef TNS_MAX_LENGTH
#define TNS_MAX_LENGTH 999999999
#endif

//  Current outbuf implementation writes data starting at the back of
//  the allocated buffer.  When finished we simply memmove it to the front.
//  Here *buffer points to the allocated buffer, while *head points to the
//  last characer written to the buffer (and thus decreases as we write).
struct tns_outbuf_s {
  char *buffer;
  char *head;
  size_t alloc_size;
};


//  Helper function for parsing a dict; basically parses items in a loop.
static int tns_parse_dict(const tns_ops *ops, void *dict, const char *data, size_t len);

//  Helper function for parsing a list; basically parses items in a loop.
static int tns_parse_list(const tns_ops *ops, void *list, const char *data, size_t len);

//  Helper function for writing the length prefix onto a rendered value.
static int tns_outbuf_clamp(tns_outbuf *outbuf, size_t orig_size);

//  Finalize an outbuf, turning the allocated buffer into a standard
//  char* array.  Can't use the outbuf once it has been finalized.
static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len);

//  Free the memory allocated in an outbuf.
//  Can't use the outbuf once it has been freed.
static void tns_outbuf_free(tns_outbuf *outbuf);

//  Helper function to read a base-ten integer off a string.
//  Due to additional constraints, we can do it faster than strtoi.
static size_t tns_strtosz(const char *data, size_t len, size_t *sz, char **end);


void* tns_parse(const tns_ops *ops, const char *data, size_t len, char **remain)
{
  char *valstr = NULL;
  tns_type_tag type = tns_tag_null;
  size_t vallen = 0;

  //  Read the length of the value, and verify that it ends in a colon.
  check(tns_strtosz(data, len, &vallen, &valstr) != -1,
        "Not a tnetstring: invalid length prefix.");
  check(*valstr == ':',
        "Not a tnetstring: invalid length prefix.");
  valstr++;
  check((valstr+vallen) < (data+len),
        "Not a tnetstring: invalid length prefix.");

  //  Grab the type tag from the end of the value.
  type = valstr[vallen];

  //  Output the remainder of the string if necessary.
  if(remain != NULL) {
      *remain = valstr + vallen + 1;
  }

  //  Now dispatch type parsing based on the type tag.
  return tns_parse_payload(ops, type, valstr, vallen);

error:
  return NULL;
}


//  This appears to be faster than using strncmp to compare
//  against a small string constant.  Ugly but fast.
#define STR_EQ_TRUE(s) (s[0]=='t' && s[1]=='r' && s[2]=='u' && s[3]=='e')
#define STR_EQ_FALSE(s) (s[0]=='f' && s[1]=='a' && s[2]=='l' \
                                   && s[3]=='s' && s[4] == 'e')

void* tns_parse_payload(const tns_ops *ops,tns_type_tag type, const char *data, size_t len)
{
  void *val = NULL;

  assert(ops != NULL && "ops struct cannot be NULL");

  switch(type) {
    //  Primitive type: a string blob.
    case tns_tag_string:
        val = ops->parse_string(ops, data, len);
        check(val != NULL, "Not a tnetstring: invalid string literal.");
        break;
    //  Primitive type: an integer.
    case tns_tag_integer:
        val = ops->parse_integer(ops, data, len);
        check(val != NULL, "Not a tnetstring: invalid integer literal.");
        break;
    //  Primitive type: a float.
    case tns_tag_float:
        val = ops->parse_float(ops, data, len);
        check(val != NULL, "Not a tnetstring: invalid float literal.");
        break;
    //  Primitive type: a boolean.
    //  The only acceptable values are "true" and "false".
    case tns_tag_bool:
        if(len == 4 && STR_EQ_TRUE(data)) {
            val = ops->get_true(ops);
        } else if(len == 5 && STR_EQ_FALSE(data)) {
            val = ops->get_false(ops);
        } else {
            sentinel("Not a tnetstring: invalid boolean literal.");
            val = NULL;
        }
        break;
    //  Primitive type: a null.
    //  This must be a zero-length string.
    case tns_tag_null:
        check(len == 0, "Not a tnetstring: invalid null literal.");
        val = ops->get_null(ops);
        break;
    //  Compound type: a dict.
    //  The data is written <key><value><key><value>
    case tns_tag_dict:
        val = ops->new_dict(ops);
        check(val != NULL, "Could not create dict.");
        check(tns_parse_dict(ops, val, data, len) != -1,
              "Not a tnetstring: broken dict items.");
        break;
    //  Compound type: a list.
    //  The data is written <item><item><item>
    case tns_tag_list:
        val = ops->new_list(ops);
        check(val != NULL, "Could not create list.");
        check(tns_parse_list(ops, val, data, len) != -1,
              "Not a tnetstring: broken list items.");
        break;
    //  Whoops, that ain't a tnetstring.
    default:
        sentinel("Not a tnetstring: invalid type tag.");
  }

  return val;

error:
  if(val != NULL) {
      ops->free_value(ops, val);
  }
  return NULL;
}

#undef STR_EQ_TRUE
#undef STR_EQ_FALSE


char* tns_render(const tns_ops *ops, void *val, size_t *len)
{
  tns_outbuf outbuf;

  check(tns_outbuf_init(&outbuf) != -1, "Failed to initialize outbuf.");
  check(tns_render_value(ops, val, &outbuf) != -1, "Failed to render value.");

  return tns_outbuf_finalize(&outbuf, len);
  
error:
  tns_outbuf_free(&outbuf);
  return NULL;
}


int tns_render_value(const tns_ops *ops, void *val, tns_outbuf *outbuf)
{
  tns_type_tag type = tns_tag_null;
  int res = -1;
  size_t orig_size = 0;

  assert(ops != NULL && "ops struct cannot be NULL");

  //  Find out the type tag for the given value.
  type = ops->get_type(ops, val);
  check(type != 0, "type not serializable.");

  tns_outbuf_putc(outbuf, type);
  orig_size = tns_outbuf_size(outbuf);

  //  Render it into the output buffer using callbacks.
  switch(type) {
    case tns_tag_string:
      res = ops->render_string(ops, val, outbuf);
      break;
    case tns_tag_integer:
      res = ops->render_integer(ops, val, outbuf);
      break;
    case tns_tag_float:
      res = ops->render_float(ops, val, outbuf);
      break;
    case tns_tag_bool:
      res = ops->render_bool(ops, val, outbuf);
      break;
    case tns_tag_null:
      res = 0;
      break;
    case tns_tag_dict:
      res = ops->render_dict(ops, val, outbuf);
      break;
    case tns_tag_list:
      res = ops->render_list(ops, val, outbuf);
      break;
    default:
      sentinel("unknown type tag: '%c'.", type);
  }

  check(res == 0, "Failed to render value of type '%c'.", type);
  return tns_outbuf_clamp(outbuf, orig_size);

error:
  return -1;
}


static int tns_parse_list(const tns_ops *ops, void *val, const char *data, size_t len)
{
  void *item = NULL;
  char *remain = NULL;

  assert(value != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      item = tns_parse(ops, data, len, &remain);
      check(item != NULL, "Failed to parse list.");
      len = len - (remain - data);
      data = remain;
      check(ops->add_to_list(ops, val, item) != -1,
            "Failed to add item to list.");
      item = NULL;
  }

  return 0;

error:
  if(item) {
      ops->free_value(ops, item);
  }
  return -1;
}


static int tns_parse_dict(const tns_ops *ops, void *val, const char *data, size_t len)
{
  void *key = NULL;
  void *item = NULL;
  char *remain = NULL;

  assert(val != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      key = tns_parse(ops, data, len, &remain);
      check(key != NULL, "Failed to parse dict key from tnetstring.");
      len = len - (remain - data);
      data = remain;

      item = tns_parse(ops, data, len, &remain);
      check(item != NULL, "Failed to parse dict item from tnetstring.");
      len = len - (remain - data);
      data = remain;

      check(ops->add_to_dict(ops, val, key, item) != -1,
            "Failed to add element to dict.");

      key = NULL;
      item = NULL;
  }

  return 0;

error:
  if(key) {
      ops->free_value(ops, key);
  }
  if(item) {
      ops->free_value(ops, item);
  }
  return -1;
}



static inline size_t
tns_strtosz(const char *data, size_t len, size_t *sz, char **end)
{
  char c;
  const char *pos, *eod;
  size_t value = 0;

  pos = data;
  eod = data + len;

  //  The first character must be a digit.
  //  The netstring spec explicitly forbits padding zeros.
  //  So if it's a zero, it must be the only char in the string.
  c = *pos++;
  switch(c) {
    case '0':
      *sz = 0;
      *end = (char*) pos;
      return 0;
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      value = c - '0';
      break;
    default:
      return -1;
  }

  //  Consume the remaining digits, up to maximum value length.
  while(pos < eod) {
      c = *pos;
      if(c < '0' || c > '9') {
          *sz = value;
          *end = (char*) pos;
          return 0; 
      }
      value = (value * 10) + (c - '0');
      check(value <= TNS_MAX_LENGTH,
            "Not a tnetstring: absurdly large length prefix");
      pos++;
  }

  // If we consume the entire string, that's an error.

error:
  return -1;
}

size_t tns_outbuf_size(tns_outbuf *outbuf)
{
  return outbuf->alloc_size - (outbuf->head - outbuf->buffer);
}


static inline int tns_outbuf_itoa(tns_outbuf *outbuf, size_t n)
{
  do {
      check(tns_outbuf_putc(outbuf, n%10+'0') != -1,
            "Failed to write int to tnetstring buffer.");
      n = n / 10;
  } while(n > 0);

  return 0;

error:
  return -1;
}


int tns_outbuf_init(tns_outbuf *outbuf)
{
  outbuf->buffer = malloc(64);
  check_mem(outbuf->buffer);

  outbuf->head = outbuf->buffer + 64;
  outbuf->alloc_size = 64;
  return 0;

error:
  outbuf->head = NULL;
  outbuf->alloc_size = 0;
  return -1;
}


static inline void tns_outbuf_free(tns_outbuf *outbuf)
{
  if(outbuf) {
      free(outbuf->buffer);
      outbuf->buffer = NULL;
      outbuf->head = 0;
      outbuf->alloc_size = 0;
  }
}


static inline int tns_outbuf_extend(tns_outbuf *outbuf, size_t free_size)
{
  char *new_buf = NULL;
  char *new_head = NULL;
  size_t new_size = outbuf->alloc_size * 2;
  size_t used_size;

  used_size = tns_outbuf_size(outbuf);

  while(new_size < free_size + used_size) {
      new_size = new_size * 2;
  }

  new_buf = malloc(new_size);
  check_mem(new_buf);
 
  new_head = new_buf + new_size - used_size;
  memmove(new_head, outbuf->head, used_size);

  free(outbuf->buffer);
  outbuf->buffer = new_buf;
  outbuf->head = new_head;
  outbuf->alloc_size = new_size;

  return 0;

error:
  return -1;
}


int tns_outbuf_putc(tns_outbuf *outbuf, char c)
{
  if(outbuf->buffer == outbuf->head) {
      check(tns_outbuf_extend(outbuf, 1) != -1, "Failed to extend buffer");
  }

  *(--outbuf->head) = c;

  return 0;

error:
  return -1;
}


int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len)
{
  if(outbuf->head - outbuf->buffer < len) {
      check(tns_outbuf_extend(outbuf, len) != -1, "Failed to extend buffer");
  }

  outbuf->head -= len;
  memmove(outbuf->head, data, len);

  return 0;

error:
  return -1;
}


static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len)
{
  char *new_buf = NULL;
  size_t used_size;

  used_size = tns_outbuf_size(outbuf);

  memmove(outbuf->buffer, outbuf->head, used_size);

  if(len != NULL) {
      *len = used_size;
  } else {
      if(outbuf->head == outbuf->buffer) {
          new_buf = realloc(outbuf->buffer, outbuf->alloc_size*2);
          check_mem(new_buf);
          outbuf->buffer = new_buf;
          outbuf->alloc_size = outbuf->alloc_size * 2;
      }
      outbuf->buffer[used_size] = '\0';
  }

  return outbuf->buffer;

error:
  free(outbuf->buffer);
  outbuf->buffer = NULL;
  outbuf->alloc_size = 0;
  return NULL;
}


static inline int tns_outbuf_clamp(tns_outbuf *outbuf, size_t orig_size)
{
    size_t datalen = tns_outbuf_size(outbuf) - orig_size;

    check(tns_outbuf_putc(outbuf, ':') != -1, "Failed to clamp outbuf");
    check(tns_outbuf_itoa(outbuf, datalen) != -1, "Failed to clamp outbuf");

    return 0;

error:
    return -1;
}


void tns_outbuf_memmove(tns_outbuf *outbuf, char *dest)
{
  memmove(dest, outbuf->head, tns_outbuf_size(outbuf));
}

