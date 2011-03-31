
#include "tns_core.h"

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


static inline int tns_outbuf_init(tns_outbuf *outbuf)
{
  outbuf->buffer = malloc(64);
  check_mem(outbuf->buffer);

  outbuf->alloc_size = 64;
  outbuf->used_size = 0;
  return 0;

error:
  outbuf->alloc_size = 0;
  outbuf->used_size = 0;
  return -1;
}


static inline void tns_outbuf_free(tns_outbuf *outbuf)
{
  if(outbuf) {
      free(outbuf->buffer);
      outbuf->buffer = NULL;
      outbuf->alloc_size = 0;
      outbuf->used_size = 0;
  }
}


static inline int tns_outbuf_extend(tns_outbuf *outbuf)
{
  char *new_buf = NULL;
  size_t new_size = outbuf->alloc_size * 2;

  new_buf = malloc(new_size);
  check_mem(new_buf);
  memmove(new_buf + new_size - outbuf->used_size,
          outbuf->buffer + outbuf->alloc_size - outbuf->used_size,
          outbuf->used_size);

  outbuf->buffer = new_buf;
  outbuf->alloc_size = new_size;

  return 0;

error:
  return -1;
}


static inline int tns_outbuf_putc(tns_outbuf *outbuf, char c)
{
  if(outbuf->alloc_size == outbuf->used_size) {
      check(tns_outbuf_extend(outbuf) != -1, "Failed to extend buffer");
  }

  outbuf->buffer[outbuf->alloc_size - ++outbuf->used_size] = c;

  return 0;

error:
  return -1;
}


static int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len)
{
  while(outbuf->alloc_size - outbuf->used_size < len) {
      check(tns_outbuf_extend(outbuf) != -1, "Failed to extend buffer");
  }

  memmove(outbuf->buffer + outbuf->alloc_size - outbuf->used_size - len,
          data, len);

  outbuf->used_size += len;

  return 0;

error:
  return -1;
}

static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len)
{
  char *new_buf = NULL;

  memmove(outbuf->buffer,
          outbuf->buffer + outbuf->alloc_size - outbuf->used_size,
          outbuf->used_size);

  if(len != NULL) {
      *len = outbuf->used_size;
  } else {
      if(outbuf->alloc_size == outbuf->used_size) {
          new_buf = realloc(outbuf->buffer, outbuf->alloc_size*2);
          check_mem(new_buf);
          outbuf->buffer = new_buf;
          outbuf->alloc_size = outbuf->alloc_size * 2;
      }
      outbuf->buffer[outbuf->used_size++] = '\0';
  }

  return outbuf->buffer;

error:
  free(outbuf->buffer);
  outbuf->buffer = NULL;
  return NULL;
}


static inline int tns_outbuf_clamp(tns_outbuf *outbuf, size_t orig_size)
{
    size_t datalen = outbuf->used_size - orig_size;

    check(tns_outbuf_putc(outbuf, ':') != -1, "Failed to clamp outbuf");
    check(tns_outbuf_itoa(outbuf, datalen) != -1, "Failed to clamp outbuf");

    return 0;

error:
    return -1;
}


static inline void tns_outbuf_memmove(tns_outbuf *outbuf, char *dest)
{
  memmove(dest,
          outbuf->buffer + outbuf->alloc_size - outbuf->used_size,
          outbuf->used_size);
}

