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

#include "tns_core.h"
#include "tns_outbuf_rev.c"

static void*
tns_parse(const char *data, size_t len, char **remain)
{
  void *val = NULL;
  char *valstr = NULL;
  tns_type_tag type = tns_tag_null;
  size_t vallen = 0;

  //  Read the length of the value, and verify that is ends in a colon.
  vallen = strtol(data, &valstr, 10);
  check(valstr != data, "Not a tnetstring: no length prefix.");
  check((valstr+vallen+1) < (data+len), "Not a tnetstring: bad length prefix.");
  check(*valstr == ':', "Not a tnetstring: bad length prefix.");
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
    case tns_tag_string:
        val = tns_parse_string(valstr, vallen);
        check(val != NULL, "Not a tnetstring: invalid string literal.");
        break;
    //  Primitive type: a number.
    //  I'm branching out here and allowing both floats and ints.
    case tns_tag_number:
        val = tns_parse_number(valstr, vallen);
        check(val != NULL, "Not a tnetstring: invalid number literal.");
        break;
    //  Primitive type: a boolean.
    //  The only acceptable values are "true" and "false".
    case tns_tag_bool:
        if(vallen == 4 && strncmp(valstr,"true",4) == 0) {
            val = tns_get_true();
        } else if(vallen == 5 && strncmp(valstr,"false",5) == 0) {
            val = tns_get_false();
        } else {
            sentinel("Not a tnetstring: invalid boolean literal.");
            val = NULL;
        }
        break;
    //  Primitive type: a null.
    //  This must be a zero-length string.
    case tns_tag_null:
        check(vallen == 0, "Not a tnetstring: invalid null literal");
        val = tns_get_null();
        break;
    //  Compound type: a dict.
    //  The data is written <key><value><key><value>
    case tns_tag_dict:
        val = tns_new_dict();
        check(tns_parse_dict(val,valstr,vallen) != -1,
              "Not a tnetstring: broken dict items.");
        break;
    //  Compound type: a list.
    //  The data is written <item><item><item>
    case tns_tag_list:
        val = tns_new_list();
        check(tns_parse_list(val,valstr,vallen) != -1,
              "Not a tnetstring: broken list items.");
        break;
    //  Whoops, that ain't a tnetstring.
    default:
        sentinel("Not a tnetsring: invalid type tag.");
  }

  return val;

error:
  tns_free_value(val);
  return NULL;
}


static char*
tns_render(void *val, size_t *len)
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
  orig_size = outbuf->used_size;

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


#define tns_rotate_buffer(data, remain, len, orig_len) {\
        len = len - (remain - data);\
        check(len < orig_len, "Error parsing data, buffer math is off.");\
        data = remain;\
}


static int tns_parse_list(void *val, const char *data, size_t len)
{
  void *item = NULL;
  char *remain = NULL;
  size_t orig_len = len;

  assert(value != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      item = tns_parse(data, len, &remain);
      check(item != NULL, "Failed to parse list.");
      tns_rotate_buffer(data, remain, len, orig_len);
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
  size_t orig_len = len;

  assert(value != NULL && "value cannot be NULL");
  assert(data != NULL && "data cannot be NULL");

  while(len > 0) {
      key = tns_parse(data, len, &remain);
      check(key != NULL, "Failed to parse dict key from tnetstring.");
      tns_rotate_buffer(data, remain, len, orig_len);

      item = tns_parse(data, len, &remain);
      check(item != NULL, "Failed to parse dict item from tnetstring.");
      tns_rotate_buffer(data, remain, len, orig_len);

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


