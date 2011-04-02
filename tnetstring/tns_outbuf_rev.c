//
//  tns_outbuf_rev:  tns_outbuf implemented by writing in reverse.
//
//  This outbuf implementation writes data starting at the front of the
//  allocated buffer, but writes it in reverse.  To finalize it you do
//  an in-place reverse before returning the buffer.
//
//  The advantage of this scheme is that the data is at the beginning
//  of the allocated buffer, so you can extend it using realloc and pass
//  it around as if it were a normal string.  However, the final copying
//  of the data has to be done in reverse so we can't use memmove.

struct tns_outbuf_s {
  char *buffer;
  size_t used_size;
  size_t alloc_size;
};


static inline size_t tns_outbuf_size(tns_outbuf *outbuf)
{
  return outbuf->used_size;
}


static inline void tns_inplace_reverse(char *data, size_t len)
{
  char *dend = NULL;
  char c = '0';

  dend = data + len - 1;
  while(dend > data) {
      c = *data;
      *data = *dend;
      *dend = c;
      data++;
      dend--;
  }
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


static inline int tns_outbuf_extend(tns_outbuf *outbuf, size_t free_size)
{
  char *new_buf = NULL;
  size_t new_size = outbuf->alloc_size * 2;

  while(new_size < free_size + outbuf->used_size) {
      new_size = new_size * 2;
  }

  new_buf = realloc(outbuf->buffer, new_size);
  check_mem(new_buf);

  outbuf->buffer = new_buf;
  outbuf->alloc_size = new_size;

  return 0;

error:
  return -1;
}


static inline int tns_outbuf_putc(tns_outbuf *outbuf, char c)
{
  if(outbuf->alloc_size == outbuf->used_size) {
      check(tns_outbuf_extend(outbuf, 1) != -1, "Failed to extend buffer");
  }

  outbuf->buffer[outbuf->used_size++] = c;

  return 0;

error:
  return -1;
}


static int tns_outbuf_puts(tns_outbuf *outbuf, const char *data, size_t len)
{
  const char *dend = NULL;
  char *buffer = NULL;

  //  Make sure we have enough room.
  if(outbuf->alloc_size - outbuf->used_size < len) {
      check(tns_outbuf_extend(outbuf, len) != -1, "Failed to extend buffer");
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

error:
  return -1;
}

static char* tns_outbuf_finalize(tns_outbuf *outbuf, size_t *len)
{
  tns_inplace_reverse(outbuf->buffer, outbuf->used_size);
  if(len == NULL) {
      tns_outbuf_putc(outbuf, '\0');
  } else {
      *len = outbuf->used_size;
  }
  return outbuf->buffer;
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
  char *buffer = outbuf->buffer + outbuf->used_size - 1;
  while(buffer >= outbuf->buffer) {
      *dest = *buffer;
      dest++;
      buffer--;
  }
}

