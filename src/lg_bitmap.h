#ifndef LG_BITMAP_H
#define LG_BITMAP_H

#include <stddef.h>
#include <stdint.h>

enum {
    LG_BITMAP_OK = 0,
    LG_BITMAP_ERR_HEADER,
    LG_BITMAP_ERR_CODEC,
    LG_BITMAP_ERR_SIZE,
    LG_BITMAP_ERR_NOMEM,
    LG_BITMAP_ERR_STREAM
};

/* One frame. pixels is width * height 8-bit palette indices, owned. */
typedef struct {
    int width;
    int height;
    int codec;          /* 2 = raw, 4 = RLE */
    uint8_t *pixels;
} lg_bitmap;

/* Decode one frame from a bitmap subchunk (28-byte header + payload).
 * On stream errors the frame is still produced, zero padded, and
 * LG_BITMAP_ERR_STREAM is returned; callers may choose to keep it. */
int lg_bitmap_decode(const uint8_t *data, size_t len, lg_bitmap *out);

void lg_bitmap_free(lg_bitmap *bmp);

const char *lg_bitmap_error(int code);

#endif /* LG_BITMAP_H */
