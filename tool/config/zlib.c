// checks if system provides a working zlib
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define unassert(x)          \
  do {                       \
    if (!(x)) {              \
      exit(__COUNTER__ + 1); \
    }                        \
  } while (0)

void *Deflate(const void *data, unsigned size, unsigned *out_size) {
  void *res;
  uLong bound;
  z_stream zs = {0};
  bound = compressBound(size);
  unassert(res = malloc(bound));
  unassert(deflateInit2(&zs, 4, Z_DEFLATED, -MAX_WBITS, 8,
                        Z_DEFAULT_STRATEGY) == Z_OK);
  zs.next_in = (z_const Bytef *)data;
  zs.avail_in = size;
  zs.avail_out = bound;
  zs.next_out = (Bytef *)res;
  unassert(deflate(&zs, Z_FINISH) == Z_STREAM_END);
  unassert(deflateEnd(&zs) == Z_OK);
  unassert(res = realloc(res, zs.total_out));
  *out_size = zs.total_out;
  return res;
}

void Inflate(void *out, unsigned outsize, const void *in, unsigned insize) {
  z_stream zs;
  zs.next_in = (z_const Bytef *)in;
  zs.avail_in = insize;
  zs.total_in = insize;
  zs.next_out = (Bytef *)out;
  zs.avail_out = outsize;
  zs.total_out = outsize;
  zs.zalloc = Z_NULL;
  zs.zfree = Z_NULL;
  unassert(inflateInit2(&zs, -MAX_WBITS) == Z_OK);
  unassert(inflate(&zs, Z_FINISH) == Z_STREAM_END);
  unassert(inflateEnd(&zs) == Z_OK);
}

int main(int argc, char *argv[]) {
  char plain[64];
  void *compressed;
  const char *golden;
  unsigned compressed_size;
  memset(plain, 0, sizeof(plain));
  compressed = Deflate("hello world", 12, &compressed_size);
  Inflate(plain, sizeof(plain), compressed, compressed_size);
  unassert(!strcmp(plain, "hello world"));
  free(compressed);
  return 0;
}
