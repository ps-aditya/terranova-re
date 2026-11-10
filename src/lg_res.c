#include "lg_res.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DICT_SIZE 16384

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(p[0] | (p[1] << 8));
}

static uint32_t read_u24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

static size_t align4(size_t n)
{
    return (n + 3u) & ~(size_t)3u;
}

static int fail(char *err, size_t errlen, int code, const char *msg)
{
    if (err && errlen)
        snprintf(err, errlen, "%s", msg);
    return code;
}

const char *lg_res_error(int code)
{
    switch (code) {
    case LG_RES_OK: return "ok";
    case LG_RES_ERR_IO: return "i/o error";
    case LG_RES_ERR_MAGIC: return "not an LG Res File v2";
    case LG_RES_ERR_TRUNCATED: return "truncated file";
    case LG_RES_ERR_RANGE: return "offset out of range";
    case LG_RES_ERR_NOMEM: return "out of memory";
    case LG_RES_ERR_DECOMPRESS: return "decompression failed";
    default: return "unknown error";
    }
}

/* 14-bit dictionary compression, as documented for System Shock 1.
 * Not present in the Terra Nova demo, implemented for retail data. */
static uint8_t *lz_decompress(const uint8_t *src, size_t src_len,
                              size_t expected, char *err, size_t errlen)
{
    uint8_t *out = calloc(1, expected ? expected : 1);
    uint32_t *offsets = NULL;
    uint32_t *lengths = NULL;
    int32_t *origins = NULL;
    size_t out_pos = 0, in_pos = 0, ntokens = 0;
    uint32_t bitbuf = 0;
    int nbits = 0;

    if (!out)
        goto oom;
    offsets = malloc(DICT_SIZE * sizeof(*offsets));
    lengths = malloc(DICT_SIZE * sizeof(*lengths));
    origins = malloc(DICT_SIZE * sizeof(*origins));
    if (!offsets || !lengths || !origins)
        goto oom;
    for (size_t i = 0; i < DICT_SIZE; i++) {
        lengths[i] = 1;
        origins[i] = -1;
    }

    while (out_pos < expected) {
        while (nbits < 14) {
            if (in_pos >= src_len) {
                memset(out + out_pos, 0, expected - out_pos);
                goto done;
            }
            bitbuf = (bitbuf << 8) | src[in_pos++];
            nbits += 8;
        }
        nbits -= 14;
        uint32_t val = (bitbuf >> (uint32_t)nbits) & 0x3FFFu;

        if (val == 0x3FFFu)
            break;
        if (val == 0x3FFEu) {
            for (size_t i = 0; i < DICT_SIZE; i++) {
                lengths[i] = 1;
                origins[i] = -1;
            }
            ntokens = 0;
            continue;
        }

        if (ntokens < DICT_SIZE) {
            offsets[ntokens] = (uint32_t)out_pos;
            if (val >= 0x100u)
                origins[ntokens] = (int32_t)(val - 0x100u);
            ntokens++;
        }

        if (val < 0x100u) {
            out[out_pos++] = (uint8_t)val;
            continue;
        }

        uint32_t idx = val - 0x100u;
        if (idx >= DICT_SIZE)
            goto bad;
        if (lengths[idx] == 1) {
            if (origins[idx] != -1)
                lengths[idx] += lengths[(uint32_t)origins[idx]];
            else
                lengths[idx] += 1;
        }
        for (uint32_t i = 0; i < lengths[idx] && out_pos < expected; i++) {
            size_t from = offsets[idx] + i;
            if (from >= out_pos)
                goto bad;
            out[out_pos++] = out[from];
        }
    }
    if (out_pos < expected)
        memset(out + out_pos, 0, expected - out_pos);

done:
    free(offsets);
    free(lengths);
    free(origins);
    return out;

oom:
    free(offsets);
    free(lengths);
    free(origins);
    free(out);
    fail(err, errlen, LG_RES_ERR_NOMEM, "out of memory");
    return NULL;

bad:
    free(offsets);
    free(lengths);
    free(origins);
    free(out);
    fail(err, errlen, LG_RES_ERR_DECOMPRESS, "corrupt compressed stream");
    return NULL;
}

int lg_res_open(lg_res *r, const char *path, char *err, size_t errlen)
{
    FILE *fp;
    long fsize;

    memset(r, 0, sizeof(*r));
    fp = fopen(path, "rb");
    if (!fp)
        return fail(err, errlen, LG_RES_ERR_IO, "cannot open file");
    if (fseek(fp, 0, SEEK_END) != 0 || (fsize = ftell(fp)) < 0 ||
        fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return fail(err, errlen, LG_RES_ERR_IO, "cannot size file");
    }
    if ((size_t)fsize < LG_RES_HEADER_SIZE) {
        fclose(fp);
        return fail(err, errlen, LG_RES_ERR_TRUNCATED, "file shorter than header");
    }
    r->size = (size_t)fsize;
    r->blob = malloc(r->size);
    if (!r->blob) {
        fclose(fp);
        return fail(err, errlen, LG_RES_ERR_NOMEM, "out of memory");
    }
    if (fread(r->blob, 1, r->size, fp) != r->size) {
        fclose(fp);
        lg_res_close(r);
        return fail(err, errlen, LG_RES_ERR_IO, "short read");
    }
    fclose(fp);

    if (memcmp(r->blob, LG_RES_MAGIC, LG_RES_MAGIC_LEN) != 0) {
        lg_res_close(r);
        return fail(err, errlen, LG_RES_ERR_MAGIC, "bad magic");
    }

    uint32_t toc = read_u32(r->blob + LG_RES_TOC_OFFSET);
    if ((size_t)toc + 6 > r->size) {
        lg_res_close(r);
        return fail(err, errlen, LG_RES_ERR_RANGE, "TOC past end of file");
    }

    uint16_t count = read_u16(r->blob + toc);
    uint32_t cursor = read_u32(r->blob + toc + 2);
    r->count = count;
    r->chunks = count ? calloc(count, sizeof(*r->chunks)) : NULL;
    if (count && !r->chunks) {
        lg_res_close(r);
        return fail(err, errlen, LG_RES_ERR_NOMEM, "out of memory");
    }

    for (uint32_t i = 0; i < count; i++) {
        size_t entry = (size_t)toc + 6 + (size_t)i * 10;
        if (entry + 10 > r->size) {
            lg_res_close(r);
            return fail(err, errlen, LG_RES_ERR_RANGE, "chunk table past EOF");
        }
        lg_res_chunk *c = &r->chunks[i];
        c->id = read_u16(r->blob + entry);
        c->unpacked_len = read_u24(r->blob + entry + 2);
        c->flags = r->blob[entry + 5];
        c->packed_len = read_u24(r->blob + entry + 6);
        c->type = r->blob[entry + 9];
        c->data_offset = cursor;
        if ((size_t)cursor + c->packed_len > r->size) {
            lg_res_close(r);
            return fail(err, errlen, LG_RES_ERR_RANGE, "chunk past EOF");
        }
        cursor = (uint32_t)align4((size_t)cursor + c->packed_len);
    }
    return LG_RES_OK;
}

void lg_res_close(lg_res *r)
{
    if (!r)
        return;
    if (r->chunks) {
        for (uint32_t i = 0; i < r->count; i++)
            free(r->chunks[i].data);
        free(r->chunks);
    }
    free(r->blob);
    memset(r, 0, sizeof(*r));
}

const lg_res_chunk *lg_res_chunk_at(const lg_res *r, uint32_t index)
{
    return index < r->count ? &r->chunks[index] : NULL;
}

const lg_res_chunk *lg_res_chunk_by_id(const lg_res *r, uint16_t id)
{
    for (uint32_t i = 0; i < r->count; i++)
        if (r->chunks[i].id == id)
            return &r->chunks[i];
    return NULL;
}

const uint8_t *lg_res_chunk_data(lg_res *r, lg_res_chunk *c, size_t *len,
                                 char *err, size_t errlen)
{
    if (c->data_state < 0)
        return NULL;
    if (!c->data_state) {
        const uint8_t *packed = r->blob + c->data_offset;
        if (c->flags & LG_RES_FLAG_COMPRESSED) {
            c->data = lz_decompress(packed, c->packed_len, c->unpacked_len,
                                    err, errlen);
            if (!c->data) {
                c->data_state = -1;
                return NULL;
            }
        } else {
            c->data = malloc(c->packed_len ? c->packed_len : 1);
            if (!c->data) {
                c->data_state = -1;
                fail(err, errlen, LG_RES_ERR_NOMEM, "out of memory");
                return NULL;
            }
            memcpy(c->data, packed, c->packed_len);
        }
        c->data_state = 1;
    }
    if (len)
        *len = c->flags & LG_RES_FLAG_COMPRESSED ? c->unpacked_len : c->packed_len;
    return c->data;
}

int lg_res_subchunk_offsets(lg_res *r, lg_res_chunk *c, uint32_t **offsets,
                            uint32_t *count, char *err, size_t errlen)
{
    size_t len = 0;
    const uint8_t *data = lg_res_chunk_data(r, c, &len, err, errlen);
    if (!data)
        return LG_RES_ERR_DECOMPRESS;
    if (!(c->flags & LG_RES_FLAG_SUBCHUNKS))
        return fail(err, errlen, LG_RES_ERR_RANGE, "chunk has no subchunks");
    if (len < 6)
        return fail(err, errlen, LG_RES_ERR_TRUNCATED, "short subchunk header");

    uint32_t n = read_u16(data);
    if (n > 4096)
        return fail(err, errlen, LG_RES_ERR_RANGE, "implausible subchunk count");
    size_t need = 2 + 4 * ((size_t)n + 1);
    if (need > len)
        return fail(err, errlen, LG_RES_ERR_TRUNCATED, "short subchunk table");

    uint32_t *out = malloc(((size_t)n + 1) * sizeof(*out));
    if (!out)
        return fail(err, errlen, LG_RES_ERR_NOMEM, "out of memory");
    for (uint32_t i = 0; i <= n; i++) {
        out[i] = read_u32(data + 2 + (size_t)i * 4);
        if (i && (out[i] < out[i - 1] || out[i] > len)) {
            free(out);
            return fail(err, errlen, LG_RES_ERR_RANGE, "bad subchunk offset");
        }
    }
    *offsets = out;
    *count = n;
    return LG_RES_OK;
}

const char *lg_res_type_name(uint8_t type)
{
    switch (type) {
    case 0x00: return "raw";
    case 0x01: return "text";
    case 0x02: return "bitmap";
    case 0x03: return "font";
    case 0x04: return "video?";
    case 0x06: return "palette";
    case 0x07: return "sound";
    case 0x08: return "tn08";
    case 0x0C: return "tn0C";
    case 0x0D: return "tn0D";
    case 0x0F: return "model3d";
    case 0x10: return "tn10";
    case 0x11: return "audio-log";
    case 0x12: return "tn12";
    case 0x30: return "map";
    default: return "?";
    }
}
