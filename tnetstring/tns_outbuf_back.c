//
//  tns_outbuf_back:  tns_outbuf implemented by writing from back of buffer.
//
//  This outbuf implementation writes data starting at the back of the
//  allocated buffer.  To finalize it you need to memmove it to the start
//  of the buffer so it can be treated like an ordinary malloced string.
//
//  The advantage of this scheme is that the data is the right way round,
//  so you can shuffle it about using memmove.  The disadvantage is that
//  reallocating the buffer is tricker as you must move the data by hand.
//  On my machines, the ability to use memmove seems to be a very slight
//  win over tns_outbuf_rev.c.
//

struct tns_outbuf_s {
  char *buffer;
  char *head;
  size_t alloc_size;
};

static inline size_t tns_outbuf_size(tns_outbuf *outbuf)
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


static inline int tns_outbuf_init(tns_outbuf *outbuf)
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


static inline int tns_outbuf_putc(tns_outbuf *outbuf, char c)
{
  if(outbuf->buffer == outbuf->head) {
      check(tns_outbuf_extend(outbuf, 1) != -1, "Failed to extend buffer");
  }

  *(--outbuf->head) = c;

  return 0;

error:
  return -1;
}


static int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len)
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


static inline void tns_outbuf_memmove(tns_outbuf *outbuf, char *dest)
{
  memmove(dest, outbuf->head, tns_outbuf_size(outbuf));
}

