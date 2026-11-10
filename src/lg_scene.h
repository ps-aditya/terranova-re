#ifndef LG_SCENE_H
#define LG_SCENE_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int width;
    int height;
    uint8_t *rgb;      /* width * height * 3 */
} lg_image;

enum {
    LG_SCENE_OK = 0,
    LG_SCENE_ERR_NOMEM,
    LG_SCENE_ERR_ARG,
    LG_SCENE_ERR_IO
};

int lg_image_create(lg_image *img, int width, int height);
void lg_image_free(lg_image *img);
void lg_image_fill(lg_image *img, uint8_t r, uint8_t g, uint8_t b);
void lg_image_put(lg_image *img, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void lg_image_cross(lg_image *img, int x, int y, int radius,
                    uint8_t r, uint8_t g, uint8_t b);
void lg_image_line(lg_image *img, int x0, int y0, int x1, int y1,
                   uint8_t r, uint8_t g, uint8_t b);
void lg_image_circle(lg_image *img, int x, int y, int radius,
                     uint8_t r, uint8_t g, uint8_t b);

/* Shaded heightfield. heights is n*n samples; scale enlarges the output. */
int lg_scene_terrain(lg_image *img, const uint8_t *heights, int n, int scale);

/* Blit a paletted sprite frame. Index 0 is transparent, others are drawn as
 * grey until the palette is recovered. */
int lg_scene_sprite(lg_image *img, const uint8_t *pixels, int sw, int sh,
                    int dx, int dy, int scale);

int lg_image_write_ppm(const lg_image *img, const char *path);

#endif /* LG_SCENE_H */
