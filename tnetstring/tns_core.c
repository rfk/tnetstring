//
//  tns_core.c:  core code for a tnetstring parser in C
//
//  This is code for parsing and rendering data in the provisional
//  typed-netstring format proposed for inclusion in Mongrel2.  You can
//  think of it like a JSON library that uses a simpler wire format.
//
//  This code is *not* designed to be compiled as a standalone library.
//  Instead, you provide a suite of low-level data manipulation functions
//  and then #include "tns_core.c" to stitch them into a tnetstring parser.
//

#include <stdlib.h>
#include <stddef.h>
#include <ctype.h>

#include "dbg.h"

#include "tns_core.h"

#ifndef TNS_MAX_LENGTH
#define TNS_MAX_LENGTH 999999999
#endif

//  These are our internal-use functions.

static int tns_parse_dict(void *dict, const char *data, size_t len);
static int tns_parse_list(void *list, const char *data, size_t len);
static int tns_outbuf_putc(tns_outbuf *outbuf, char c);
static int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len);
static int tns_outbuf_clamp(tns_outbuf *outbuf, size_t orig_size);
static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len);
static void tns_outbuf_free(tns_outbuf *outbuf);
static size_t tns_strtosz(const char *data, size_t len, size_t *sz, char **end);

#include "tns_outbuf_back.c"

//  This appears to be faster than using strncmp to compare
//  against a small string constant.
#define STR_EQ_TRUE(s) (s[0]=='t' && s[1]=='r' && s[2]=='u' && s[3]=='e')
#define STR_EQ_FALSE(s) (s[0]=='f' && s[1]=='a' && s[2]=='l' \
                                   && s[3]=='s' && s[4] == 'e')


static void* tns_parse(const char *data, size_t len, char **remain)
{
  char *valstr = NULL;
  tns_type_tag type = tns_tag_null;
  size_t vallen = 0;

  //  Read the length of the value, and verify that it ends in a colon.
  check(tns_strtosz(data, len, &vallen, &valstr) != -1,
        "Not a tnetstring: invalid length prefix.");
  check(*valstr == ':', "Not a tnetstring: invalid length prefix.");
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
  return tns_parse_payload(type, valstr, vallen);

error:
  return NULL;
}


static void* tns_parse_payload(tns_type_tag type, const char *data, size_t len)
{
  void *val = NULL;

  switch(type) {
    //  Primitive type: a string blob.
    case tns_tag_string:
        val = tns_parse_string(data, len);
        check(val != NULL, "Not a tnetstring: invalid string literal.");
        break;
    //  Primitive type: a number.
    //  I'm branching out here and allowing both floats and ints.
    case tns_tag_number:
        val = tns_parse_number(data, len);
        check(val != NULL, "Not a tnetstring: invalid number literal.");
        break;
    //  Primitive type: a boolean.
    //  The only acceptable values are "true" and "false".
    case tns_tag_bool:
        if(len == 4 && STR_EQ_TRUE(data)) {
            val = tns_get_true();
        } else if(len == 5 && STR_EQ_FALSE(data)) {
            val = tns_get_false();
        } else {
            sentinel("Not a tnetstring: invalid boolean literal.");
            val = NULL;
        }
        break;
    //  Primitive type: a null.
    //  This must be a zero-length string.
    case tns_tag_null:
        check(len == 0, "Not a tnetstring: invalid null literal");
        val = tns_get_null();
        break;
    //  Compound type: a dict.
    //  The data is written <key><value><key><value>
    case tns_tag_dict:
        val = tns_new_dict();
        check(tns_parse_dict(val,data,len) != -1,
              "Not a tnetstring: broken dict items.");
        break;
    //  Compound type: a list.
    //  The data is written <item><item><item>
    case tns_tag_list:
        val = tns_new_list();
        check(tns_parse_list(val,data,len) != -1,
              "Not a tnetstring: broken list items.");
        break;
    //  Whoops, that ain't a tnetstring.
    default:
        sentinel("Not a tnetstring: invalid type tag.");
  }

  return val;

error:
  tns_free_value(val);
  return NULL;
}

#undef STR_EQ_TRUE
#undef STR_EQ_FALSE


static char* tns_render(void *val, size_t *len)
{
  tns_outbuf outbuf;

  check(tns_outbuf_init(&outbuf) != -1, "Failed to initialize outbuf.");
  check(tns_render_value(val, &outbuf) != -1, "Failed to render value.");

  return tns_outbuf_finalize(&outbuf, len);
  
error:
  tns_outbuf_free(&outbuf);
  return NULL;
}


static int tns_render_value(void *val, tns_outbuf *outbuf)
{
  tns_type_tag type = tns_tag_null;
  int res = -1;
  size_t orig_size = 0;

  //  Find out the type tag for the given value.
  type = tns_get_type(val);
  check(type != 0, "type not serializable.");

  tns_outbuf_putc(outbuf,type);
  orig_size = tns_outbuf_size(outbuf);

  //  Render it into the output buffer, leaving space for the
  //  type tag at the end.
  switch(type) {
    case tns_tag_string:
      res = tns_render_string(val, outbuf);
      break;
    case tns_tag_number:
      res = tns_render_number(val, outbuf);
      break;
    case tns_tag_bool:
      res = tns_render_bool(val, outbuf);
      break;
    case tns_tag_null:
      res = 0;
      break;
    case tns_tag_dict:
      res = tns_render_dict(val, outbuf);
      break;
    case tns_tag_list:
      res = tns_render_list(val, outbuf);
      break;
    default:
      sentinel("unknown type tag: '%c'.", type);
  }

  check(res == 0, "Failed to render value of type '%c'.", type);
  return tns_outbuf_clamp(outbuf, orig_size);

error:
  return -1;
}


static int tns_parse_list(void *val, const char *data, size_t len)
{
  void *item = NULL;
  char *remain = NULL;

  assert(value != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      item = tns_parse(data, len, &remain);
      check(item != NULL, "Failed to parse list.");
      len = len - (remain - data);
      data = remain;
      check(tns_add_to_list(val, item) != -1,
            "Failed to add item to list.");
      item = NULL;
  }

  return 0;

error:
  if(item) {
      tns_free_value(item);
  }
  return -1;
}


static int tns_parse_dict(void *val, const char *data, size_t len)
{
  void *key = NULL;
  void *item = NULL;
  char *remain = NULL;

  assert(val != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      key = tns_parse(data, len, &remain);
      check(key != NULL, "Failed to parse dict key from tnetstring.");
      len = len - (remain - data);
      data = remain;

      item = tns_parse(data, len, &remain);
      check(item != NULL, "Failed to parse dict item from tnetstring.");
      len = len - (remain - data);
      data = remain;

      check(tns_add_to_dict(val,key,item) != -1,
            "Failed to add element to dict.");

      key = NULL;
      item = NULL;
  }

  return 0;

error:
  if(key) {
      tns_free_value(key);
  }
  if(item) {
      tns_free_value(item);
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
  //  If it's a zero, it must be the only char in the string.
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

  //  Consume all other digits.
  while(pos < eod) {
      c = *pos;
      switch(c) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
          break;
        default:
          *sz = value;
          *end = (char*) pos;
          return 0; 
      }
      value = (value * 10) + (c - '0');
      pos++;
  }

  // If we consume the entire string, that's an error.
  return -1;
}

