#include "lg_bitmap.h"
#include "lg_res.h"
#include "lg_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void usage(void)
{
    fprintf(stderr,
            "usage: lgres <command> [args]\n"
            "  info    <file.res>...        summary per file\n"
            "  list    <file.res>           chunk table\n"
            "  cat     <file.res> <id> [-o out.bin]\n"
            "  bmp     <file.res> <id> [-s sub] [-o out.pgm] [-p pal.rgb]\n"
            "  bmpscan <file.res>...        decode every bitmap frame\n"
            "  scene   <map.res> <mission.res> -o out.ppm\n"
            "  bench   <map.res> [iterations]\n");
}

static int cmd_info(int argc, char **argv)
{
    int rc = 0;
    for (int i = 0; i < argc; i++) {
        lg_res r;
        char err[256] = "";
        int e = lg_res_open(&r, argv[i], err, sizeof(err));
        if (e != LG_RES_OK) {
            fprintf(stderr, "%s: %s\n", argv[i], err[0] ? err : lg_res_error(e));
            rc = 1;
            continue;
        }
        unsigned hist[256] = {0};
        unsigned compressed = 0;
        for (uint32_t k = 0; k < r.count; k++) {
            hist[r.chunks[k].type]++;
            compressed += (r.chunks[k].flags & LG_RES_FLAG_COMPRESSED) != 0;
        }
        printf("%s: %zu bytes, %u chunks, %u compressed\n",
               argv[i], r.size, r.count, compressed);
        for (int t = 0; t < 256; t++) {
            if (hist[t])
                printf("  type 0x%02X %-8s %u\n", t, lg_res_type_name((uint8_t)t), hist[t]);
        }
        lg_res_close(&r);
    }
    return rc;
}

static int cmd_list(int argc, char **argv)
{
    if (argc != 1) {
        usage();
        return 2;
    }
    lg_res r;
    char err[256] = "";
    int e = lg_res_open(&r, argv[0], err, sizeof(err));
    if (e != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[0], err[0] ? err : lg_res_error(e));
        return 1;
    }
    printf("== %s (%u chunks) ==\n", argv[0], r.count);
    for (uint32_t i = 0; i < r.count; i++) {
        lg_res_chunk *c = &r.chunks[i];
        char flags[3] = {
            (c->flags & LG_RES_FLAG_COMPRESSED) ? 'C' : '-',
            (c->flags & LG_RES_FLAG_SUBCHUNKS) ? 'S' : '-',
            0
        };
        unsigned subs = 0;
        if (c->flags & LG_RES_FLAG_SUBCHUNKS) {
            uint32_t *offs = NULL;
            uint32_t n = 0;
            if (lg_res_subchunk_offsets(&r, c, &offs, &n, NULL, 0) == LG_RES_OK) {
                subs = n;
                free(offs);
            }
        }
        printf("%4u id=0x%04X %s type=0x%02X %-8s packed=%6u unpacked=%6u subs=%u\n",
               i, c->id, flags, c->type, lg_res_type_name(c->type),
               c->packed_len, c->unpacked_len, subs);
    }
    lg_res_close(&r);
    return 0;
}

static int cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }
    const char *out_path = NULL;
    if (argc >= 4 && strcmp(argv[2], "-o") == 0)
        out_path = argv[3];
    else if (argc != 2) {
        usage();
        return 2;
    }

    char *end = NULL;
    unsigned long id = strtoul(argv[1], &end, 0);
    if (!end || *end || id > 0xFFFF) {
        fprintf(stderr, "bad chunk id: %s\n", argv[1]);
        return 2;
    }

    lg_res r;
    char err[256] = "";
    int e = lg_res_open(&r, argv[0], err, sizeof(err));
    if (e != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[0], err[0] ? err : lg_res_error(e));
        return 1;
    }
    lg_res_chunk *c = (lg_res_chunk *)lg_res_chunk_by_id(&r, (uint16_t)id);
    if (!c) {
        fprintf(stderr, "no chunk with id 0x%04lX\n", id);
        lg_res_close(&r);
        return 1;
    }
    size_t len = 0;
    const uint8_t *data = lg_res_chunk_data(&r, c, &len, err, sizeof(err));
    if (!data) {
        fprintf(stderr, "decode failed: %s\n", err[0] ? err : "?");
        lg_res_close(&r);
        return 1;
    }
    if (out_path) {
        FILE *fp = fopen(out_path, "wb");
        if (!fp || fwrite(data, 1, len, fp) != len) {
            fprintf(stderr, "cannot write %s\n", out_path);
            if (fp)
                fclose(fp);
            lg_res_close(&r);
            return 1;
        }
        fclose(fp);
        printf("wrote %zu bytes to %s\n", len, out_path);
    } else {
        fwrite(data, 1, len, stdout);
    }
    lg_res_close(&r);
    return 0;
}

/* Return the i-th frame of a bitmap chunk. Frame 0 of a chunk without
 * subchunks is the whole payload. */
static const uint8_t *frame_span(lg_res *r, lg_res_chunk *c, unsigned index,
                                 size_t *len)
{
    uint32_t *offs = NULL;
    uint32_t n = 0;
    size_t total = 0;
    const uint8_t *data = lg_res_chunk_data(r, c, &total, NULL, 0);
    if (!data)
        return NULL;
    if (!(c->flags & LG_RES_FLAG_SUBCHUNKS)) {
        if (index != 0)
            return NULL;
        *len = total;
        return data;
    }
    if (lg_res_subchunk_offsets(r, c, &offs, &n, NULL, 0) != LG_RES_OK)
        return NULL;
    const uint8_t *span = NULL;
    if (index < n) {
        *len = offs[index + 1] - offs[index];
        span = data + offs[index];
    }
    free(offs);
    return span;
}

static int write_pgm(const char *path, const lg_bitmap *bmp)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fprintf(fp, "P5\n%d %d\n255\n", bmp->width, bmp->height);
    size_t n = (size_t)bmp->width * (size_t)bmp->height;
    int ok = fwrite(bmp->pixels, 1, n, fp) == n;
    fclose(fp);
    return ok ? 0 : -1;
}

static int write_ppm(const char *path, const lg_bitmap *bmp, const uint8_t *pal)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return -1;
    fprintf(fp, "P6\n%d %d\n255\n", bmp->width, bmp->height);
    size_t n = (size_t)bmp->width * (size_t)bmp->height;
    int ok = 1;
    for (size_t i = 0; i < n && ok; i++) {
        const uint8_t *rgb = pal + (size_t)bmp->pixels[i] * 3;
        ok = fwrite(rgb, 1, 3, fp) == 3;
    }
    fclose(fp);
    return ok ? 0 : -1;
}

static int cmd_bmp(int argc, char **argv)
{
    if (argc < 2) {
        usage();
        return 2;
    }
    unsigned sub = 0;
    const char *out_path = NULL;
    const char *pal_path = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc)
            sub = (unsigned)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out_path = argv[++i];
        else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc)
            pal_path = argv[++i];
        else {
            usage();
            return 2;
        }
    }

    char *end = NULL;
    unsigned long id = strtoul(argv[1], &end, 0);
    if (!end || *end || id > 0xFFFF) {
        fprintf(stderr, "bad chunk id: %s\n", argv[1]);
        return 2;
    }

    lg_res r;
    char err[256] = "";
    int e = lg_res_open(&r, argv[0], err, sizeof(err));
    if (e != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[0], err[0] ? err : lg_res_error(e));
        return 1;
    }
    lg_res_chunk *c = (lg_res_chunk *)lg_res_chunk_by_id(&r, (uint16_t)id);
    if (!c) {
        fprintf(stderr, "no chunk with id 0x%04lX\n", id);
        lg_res_close(&r);
        return 1;
    }
    size_t len = 0;
    const uint8_t *frame = frame_span(&r, c, sub, &len);
    if (!frame) {
        fprintf(stderr, "no frame %u in chunk 0x%04lX\n", sub, id);
        lg_res_close(&r);
        return 1;
    }
    lg_bitmap bmp;
    int rc = lg_bitmap_decode(frame, len, &bmp);
    printf("chunk 0x%04lX frame %u: %dx%d codec=%d %s\n", id, sub,
           bmp.width, bmp.height, bmp.codec, lg_bitmap_error(rc));
    if (bmp.pixels) {
        if (pal_path) {
            uint8_t pal[768];
            FILE *pf = fopen(pal_path, "rb");
            size_t got = pf ? fread(pal, 1, sizeof(pal), pf) : 0;
            if (pf)
                fclose(pf);
            if (got != sizeof(pal)) {
                fprintf(stderr, "palette must be exactly 768 bytes\n");
                rc = 1;
            } else if (out_path && write_ppm(out_path, &bmp, pal) == 0) {
                printf("wrote %s\n", out_path);
            }
        } else if (out_path && write_pgm(out_path, &bmp) == 0) {
            printf("wrote %s\n", out_path);
        }
    }
    lg_bitmap_free(&bmp);
    lg_res_close(&r);
    return rc == LG_BITMAP_OK ? 0 : 1;
}

static const char *base_name(const char *path)
{
    const char *slash = strrchr(path, '/');
    const char *back = strrchr(path, '\\');
    const char *cut = slash > back ? slash : back;
    return cut ? cut + 1 : path;
}

static int cmd_bmpscan(int argc, char **argv)
{
    int failures = 0, verbose = 0;
    unsigned files = 0, chunks = 0, frames = 0, raw = 0, rle = 0, errs = 0;
    unsigned long long pixels = 0;
    for (int a = 0; a < argc; a++) {
        if (strcmp(argv[a], "-v") == 0) {
            verbose = 1;
            continue;
        }
        lg_res r;
        char err[256] = "";
        int e = lg_res_open(&r, argv[a], err, sizeof(err));
        if (e != LG_RES_OK) {
            fprintf(stderr, "%s: %s\n", argv[a], err[0] ? err : lg_res_error(e));
            failures++;
            continue;
        }
        files++;
        for (uint32_t i = 0; i < r.count; i++) {
            lg_res_chunk *c = &r.chunks[i];
            if (c->type != 0x02)
                continue;
            chunks++;
            unsigned n = 1;
            uint32_t *offs = NULL;
            if (c->flags & LG_RES_FLAG_SUBCHUNKS) {
                if (lg_res_subchunk_offsets(&r, c, &offs, &n, NULL, 0) != LG_RES_OK) {
                    errs++;
                    continue;
                }
                free(offs);
            }
            for (unsigned s = 0; s < n; s++) {
                size_t len = 0;
                const uint8_t *frame = frame_span(&r, c, s, &len);
                if (!frame) {
                    errs++;
                    continue;
                }
                lg_bitmap bmp;
                int rc = lg_bitmap_decode(frame, len, &bmp);
                frames++;
                if (verbose)
                    printf("%s %04X %u %d %d %d %d\n", base_name(argv[a]), c->id, s,
                           bmp.width, bmp.height, bmp.codec, rc);
                if (rc == LG_BITMAP_OK) {
                    pixels += (unsigned long long)bmp.width * bmp.height;
                    if (bmp.codec == 2 || bmp.codec == 5)
                        raw++;
                    else
                        rle++;
                } else {
                    errs++;
                }
                lg_bitmap_free(&bmp);
            }
        }
        lg_res_close(&r);
    }
    printf("files=%u bitmap_chunks=%u frames=%u raw=%u rle=%u errors=%u "
           "decoded_pixels=%llu\n", files, chunks, frames, raw, rle, errs, pixels);
    return (failures || errs) ? 1 : 0;
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* The terrain chunk is 3 bytes per vertex on a square grid. */
static uint8_t *load_heights(lg_res *r, int *side_out)
{
    lg_res_chunk *c = (lg_res_chunk *)lg_res_chunk_by_id(r, 0x0056);
    if (!c)
        return NULL;
    size_t len = 0;
    const uint8_t *data = lg_res_chunk_data(r, c, &len, NULL, 0);
    if (!data || len % 3)
        return NULL;
    size_t verts = len / 3;
    int n = 1;
    while ((size_t)n * (size_t)n < verts)
        n++;
    if ((size_t)n * (size_t)n != verts)
        return NULL;
    uint8_t *heights = malloc(verts);
    if (!heights)
        return NULL;
    for (size_t i = 0; i < verts; i++)
        heights[i] = data[i * 3 + 2];
    *side_out = n;
    return heights;
}

static const uint8_t TEAM_COLOUR[5][3] = {
    {255, 70, 70}, {80, 255, 80}, {80, 180, 255}, {255, 240, 80}, {255, 90, 255}
};

static int draw_scene(lg_image *img, lg_res *map, lg_res *mission)
{
    int n = 0;
    uint8_t *heights = load_heights(map, &n);
    if (!heights)
        return LG_SCENE_ERR_ARG;

    int scale = img->width / n;
    if (scale < 1)
        scale = 1;
    lg_scene_terrain(img, heights, n, scale);
    free(heights);

    if (mission) {
        lg_res_chunk *wp = (lg_res_chunk *)lg_res_chunk_by_id(mission, 0x00B0);
        if (wp) {
            size_t len = 0;
            const uint8_t *p = lg_res_chunk_data(mission, wp, &len, NULL, 0);
            if (p) {
                for (size_t i = 0; i + 8 <= len; i += 8) {
                    uint32_t fx = le32(p + i), fy = le32(p + i + 4);
                    if (!fx && !fy)
                        continue;
                    int x = (int)((fx >> 16) * (unsigned)scale);
                    int y = (int)((fy >> 16) * (unsigned)scale);
                    lg_image_circle(img, x, y, 4, 255, 220, 60);
                }
            }
        }
    }

    for (uint32_t cid = 0x78; cid <= 0x7C; cid++) {
        lg_res_chunk *chunk = (lg_res_chunk *)lg_res_chunk_by_id(map, (uint16_t)cid);
        if (!chunk)
            continue;
        uint32_t *offs = NULL;
        uint32_t count = 0;
        if (lg_res_subchunk_offsets(map, chunk, &offs, &count, NULL, 0) != LG_RES_OK)
            continue;
        size_t total = 0;
        const uint8_t *data = lg_res_chunk_data(map, chunk, &total, NULL, 0);
        for (uint32_t i = 0; i < count; i++) {
            uint32_t off = offs[i], len = offs[i + 1] - offs[i];
            if (len < 50 || !data)
                continue;
            const uint8_t *b = data + off;
            unsigned team = b[1] % 5;
            uint32_t a = (uint32_t)b[8] | ((uint32_t)b[9] << 8) | ((uint32_t)b[10] << 16);
            uint32_t c = (uint32_t)b[12] | ((uint32_t)b[13] << 8) | ((uint32_t)b[14] << 16);
            int ax = (int)(a & 0x1FF) * scale, ay = (int)((a >> 9) & 0x1FF) * scale;
            int bx = (int)(c & 0x1FF) * scale, by = (int)((c >> 9) & 0x1FF) * scale;
            lg_image_line(img, ax, ay, bx, by,
                          TEAM_COLOUR[team][0], TEAM_COLOUR[team][1], TEAM_COLOUR[team][2]);
            lg_image_cross(img, ax, ay, 4,
                           TEAM_COLOUR[team][0], TEAM_COLOUR[team][1], TEAM_COLOUR[team][2]);
            lg_image_circle(img, bx, by, 4,
                            TEAM_COLOUR[team][0], TEAM_COLOUR[team][1], TEAM_COLOUR[team][2]);
        }
        free(offs);
    }
    return LG_SCENE_OK;
}

static int cmd_scene(int argc, char **argv)
{
    if (argc < 4) {
        usage();
        return 2;
    }
    const char *out = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else {
            usage();
            return 2;
        }
    }
    if (!out)
        return 2;

    lg_res map, mission;
    char err[256] = "";
    if (lg_res_open(&map, argv[0], err, sizeof(err)) != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        return 1;
    }
    if (lg_res_open(&mission, argv[1], err, sizeof(err)) != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[1], err);
        lg_res_close(&map);
        return 1;
    }

    int n = 0;
    uint8_t *heights = load_heights(&map, &n);
    if (!heights) {
        fprintf(stderr, "map has no usable terrain chunk\n");
        lg_res_close(&map);
        lg_res_close(&mission);
        return 1;
    }
    free(heights);

    lg_image img;
    if (lg_image_create(&img, n * 2, n * 2) != LG_SCENE_OK) {
        lg_res_close(&map);
        lg_res_close(&mission);
        return 1;
    }
    if (draw_scene(&img, &map, &mission) != LG_SCENE_OK ||
        lg_image_write_ppm(&img, out) != LG_SCENE_OK) {
        fprintf(stderr, "render failed\n");
        lg_image_free(&img);
        lg_res_close(&map);
        lg_res_close(&mission);
        return 1;
    }
    printf("wrote %s (%dx%d)\n", out, img.width, img.height);
    lg_image_free(&img);
    lg_res_close(&map);
    lg_res_close(&mission);
    return 0;
}

static int cmd_bench(int argc, char **argv)
{
    if (argc < 1) {
        usage();
        return 2;
    }
    int iterations = argc > 1 ? atoi(argv[1]) : 300;
    int scale = argc > 2 ? atoi(argv[2]) : 2;
    if (iterations < 1)
        iterations = 1;
    if (scale < 1)
        scale = 1;

    lg_res map;
    char err[256] = "";
    if (lg_res_open(&map, argv[0], err, sizeof(err)) != LG_RES_OK) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        return 1;
    }
    int n = 0;
    uint8_t *heights = load_heights(&map, &n);
    if (!heights) {
        fprintf(stderr, "map has no usable terrain chunk\n");
        lg_res_close(&map);
        return 1;
    }
    lg_image img;
    if (lg_image_create(&img, n * scale, n * scale) != LG_SCENE_OK) {
        free(heights);
        lg_res_close(&map);
        return 1;
    }

    clock_t start = clock();
    for (int i = 0; i < iterations; i++)
        draw_scene(&img, &map, NULL);
    double seconds = (double)(clock() - start) / CLOCKS_PER_SEC;

    printf("terrain %dx%d -> %dx%d, %d frames in %.3f s = %.1f fps\n",
           n, n, img.width, img.height, iterations, seconds,
           seconds > 0 ? iterations / seconds : 0.0);
    lg_image_free(&img);
    free(heights);
    lg_res_close(&map);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        usage();
        return 2;
    }
    if (strcmp(argv[1], "info") == 0)
        return cmd_info(argc - 2, argv + 2);
    if (strcmp(argv[1], "list") == 0)
        return cmd_list(argc - 2, argv + 2);
    if (strcmp(argv[1], "cat") == 0)
        return cmd_cat(argc - 2, argv + 2);
    if (strcmp(argv[1], "bmp") == 0)
        return cmd_bmp(argc - 2, argv + 2);
    if (strcmp(argv[1], "bmpscan") == 0)
        return cmd_bmpscan(argc - 2, argv + 2);
    if (strcmp(argv[1], "scene") == 0)
        return cmd_scene(argc - 2, argv + 2);
    if (strcmp(argv[1], "bench") == 0)
        return cmd_bench(argc - 2, argv + 2);
    usage();
    return 2;
}
