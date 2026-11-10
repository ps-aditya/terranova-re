#include "lg_bitmap.h"

#include <stdlib.h>
#include <string.h>

#define HEADER_SIZE 28

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

const char *lg_bitmap_error(int code)
{
    switch (code) {
    case LG_BITMAP_OK: return "ok";
    case LG_BITMAP_ERR_HEADER: return "short bitmap header";
    case LG_BITMAP_ERR_CODEC: return "unknown bitmap codec";
    case LG_BITMAP_ERR_SIZE: return "implausible bitmap size";
    case LG_BITMAP_ERR_NOMEM: return "out of memory";
    case LG_BITMAP_ERR_STREAM: return "truncated or malformed pixel stream";
    default: return "unknown error";
    }
}

/* RLE, as documented for System Shock bitmaps and confirmed on Terra Nova
 * sprite banks:
 *   00 nn xx        repeat xx nn times
 *   nn (0 < nn < 0x80)  copy nn literal bytes
 *   80 00 00        end of stream
 *   80 mm nn (nn < 0x80)  skip nn*256+mm transparent pixels
 *   80 mm nn (nn >= 0x80) copy ((nn & 0x3f) << 8 | mm) literal bytes
 *   nn (nn > 0x80)  skip nn & 0x7f transparent pixels
 * Trailing pixels are transparent. */
static int decode_rle(const uint8_t *src, size_t len, uint8_t *out, size_t n)
{
    size_t i = 0, o = 0;
    int rc = LG_BITMAP_OK;

    while (o < n) {
        if (i >= len) {
            rc = LG_BITMAP_ERR_STREAM;
            break;
        }
        uint8_t b = src[i++];

        if (b == 0x00) {
            if (i + 2 > len) {
                rc = LG_BITMAP_ERR_STREAM;
                break;
            }
            unsigned count = src[i];
            uint8_t value = src[i + 1];
            i += 2;
            while (count-- && o < n)
                out[o++] = value;
        } else if (b < 0x80) {
            if (i + b > len) {
                rc = LG_BITMAP_ERR_STREAM;
                break;
            }
            size_t take = b;
            if (take > n - o)
                take = n - o;
            memcpy(out + o, src + i, take);
            o += take;
            i += b;
        } else if (b == 0x80) {
            if (i + 2 > len) {
                rc = LG_BITMAP_ERR_STREAM;
                break;
            }
            unsigned m = src[i];
            unsigned k = src[i + 1];
            i += 2;
            if (m == 0 && k == 0)
                break;
            if (k < 0x80) {
                size_t skip = k * 256u + m;
                while (skip-- && o < n)
                    out[o++] = 0;
            } else {
                size_t count = ((k & 0x3fu) << 8) | m;
                if (i >= len) {
                    rc = LG_BITMAP_ERR_STREAM;
                    break;
                }
                uint8_t value = src[i++];
                while (count-- && o < n)
                    out[o++] = value;
            }
        } else {
            size_t skip = b & 0x7fu;
            while (skip-- && o < n)
                out[o++] = 0;
        }
    }
    return rc;
}

int lg_bitmap_decode(const uint8_t *data, size_t len, lg_bitmap *out)
{
    memset(out, 0, sizeof(*out));
    if (!data || len < HEADER_SIZE)
        return LG_BITMAP_ERR_HEADER;

    int codec = rd16(data + 4);
    int width = rd16(data + 8);
    int height = rd16(data + 10);
    if (codec != 2 && codec != 4 && codec != 5)
        return LG_BITMAP_ERR_CODEC;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return LG_BITMAP_ERR_SIZE;

    size_t n = (size_t)width * (size_t)height;
    if (n > 16u * 1024u * 1024u)
        return LG_BITMAP_ERR_SIZE;

    uint8_t *pixels = calloc(n ? n : 1, 1);
    if (!pixels)
        return LG_BITMAP_ERR_NOMEM;

    int rc = LG_BITMAP_OK;
    const uint8_t *payload = data + HEADER_SIZE;
    size_t payload_len = len - HEADER_SIZE;

    if (codec == 2 || codec == 5) {
        size_t take = payload_len < n ? payload_len : n;
        memcpy(pixels, payload, take);
    } else {
        rc = decode_rle(payload, payload_len, pixels, n);
    }

    out->width = width;
    out->height = height;
    out->codec = codec;
    out->pixels = pixels;
    return rc;
}

void lg_bitmap_free(lg_bitmap *bmp)
{
    if (!bmp)
        return;
    free(bmp->pixels);
    memset(bmp, 0, sizeof(*bmp));
}
