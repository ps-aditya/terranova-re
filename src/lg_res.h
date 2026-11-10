#ifndef LG_RES_H
#define LG_RES_H

#include <stddef.h>
#include <stdint.h>

#define LG_RES_MAGIC "LG Res File v2\r\n\x1a"
#define LG_RES_MAGIC_LEN 18
#define LG_RES_HEADER_SIZE 128
#define LG_RES_TOC_OFFSET 0x7C

enum {
    LG_RES_OK = 0,
    LG_RES_ERR_IO,
    LG_RES_ERR_MAGIC,
    LG_RES_ERR_TRUNCATED,
    LG_RES_ERR_RANGE,
    LG_RES_ERR_NOMEM,
    LG_RES_ERR_DECOMPRESS
};

enum {
    LG_RES_FLAG_COMPRESSED = 0x01,
    LG_RES_FLAG_SUBCHUNKS = 0x02
};

typedef struct {
    uint16_t id;
    uint8_t type;
    uint8_t flags;
    uint32_t packed_len;
    uint32_t unpacked_len;
    size_t data_offset;
    uint8_t *data;      /* lazily decoded, owned by the chunk */
    int data_state;     /* 0 = not loaded, 1 = loaded, < 0 = error */
} lg_res_chunk;

typedef struct {
    uint8_t *blob;
    size_t size;
    lg_res_chunk *chunks;
    uint32_t count;
} lg_res;

/* Open and index a resource file. Returns LG_RES_OK or an LG_RES_ERR_*. */
int lg_res_open(lg_res *r, const char *path, char *err, size_t errlen);

void lg_res_close(lg_res *r);

const char *lg_res_error(int code);

const lg_res_chunk *lg_res_chunk_at(const lg_res *r, uint32_t index);
const lg_res_chunk *lg_res_chunk_by_id(const lg_res *r, uint16_t id);

/* Payload of a chunk, decompressed if needed. The pointer stays valid until
 * lg_res_close. */
const uint8_t *lg_res_chunk_data(lg_res *r, lg_res_chunk *c, size_t *len,
                                 char *err, size_t errlen);

/* Parse a subchunk directory. On success *offsets receives count + 1 entries
 * relative to the chunk payload, and the caller frees it. The last entry is
 * the end of the final subchunk. */
int lg_res_subchunk_offsets(lg_res *r, lg_res_chunk *c, uint32_t **offsets,
                            uint32_t *count, char *err, size_t errlen);

const char *lg_res_type_name(uint8_t type);

#endif /* LG_RES_H */
