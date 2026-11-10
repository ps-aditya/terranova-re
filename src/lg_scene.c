#include "lg_scene.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int lg_image_create(lg_image *img, int width, int height)
{
    memset(img, 0, sizeof(*img));
    if (width <= 0 || height <= 0)
        return LG_SCENE_ERR_ARG;
    img->rgb = calloc((size_t)width * (size_t)height, 3);
    if (!img->rgb)
        return LG_SCENE_ERR_NOMEM;
    img->width = width;
    img->height = height;
    return LG_SCENE_OK;
}

void lg_image_free(lg_image *img)
{
    if (!img)
        return;
    free(img->rgb);
    memset(img, 0, sizeof(*img));
}

void lg_image_fill(lg_image *img, uint8_t r, uint8_t g, uint8_t b)
{
    size_t n = (size_t)img->width * (size_t)img->height;
    for (size_t i = 0; i < n; i++) {
        img->rgb[i * 3 + 0] = r;
        img->rgb[i * 3 + 1] = g;
        img->rgb[i * 3 + 2] = b;
    }
}

void lg_image_put(lg_image *img, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || y < 0 || x >= img->width || y >= img->height)
        return;
    size_t o = ((size_t)y * (size_t)img->width + (size_t)x) * 3;
    img->rgb[o + 0] = r;
    img->rgb[o + 1] = g;
    img->rgb[o + 2] = b;
}

void lg_image_line(lg_image *img, int x0, int y0, int x1, int y1,
                   uint8_t r, uint8_t g, uint8_t b)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        lg_image_put(img, x0, y0, r, g, b);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void lg_image_cross(lg_image *img, int x, int y, int radius,
                    uint8_t r, uint8_t g, uint8_t b)
{
    for (int d = -radius; d <= radius; d++) {
        lg_image_put(img, x + d, y, r, g, b);
        lg_image_put(img, x, y + d, r, g, b);
    }
}

void lg_image_circle(lg_image *img, int x, int y, int radius,
                     uint8_t r, uint8_t g, uint8_t b)
{
    int px = radius, py = 0, err = 0;
    while (px >= py) {
        lg_image_put(img, x + px, y + py, r, g, b);
        lg_image_put(img, x + py, y + px, r, g, b);
        lg_image_put(img, x - py, y + px, r, g, b);
        lg_image_put(img, x - px, y + py, r, g, b);
        lg_image_put(img, x - px, y - py, r, g, b);
        lg_image_put(img, x - py, y - px, r, g, b);
        lg_image_put(img, x + py, y - px, r, g, b);
        lg_image_put(img, x + px, y - py, r, g, b);
        py++;
        if (err <= 0)
            err += 2 * py + 1;
        if (err > 0) {
            px--;
            err -= 2 * px + 1;
        }
    }
}

int lg_scene_terrain(lg_image *img, const uint8_t *heights, int n, int scale)
{
    if (!img->rgb || !heights || n <= 1 || scale <= 0)
        return LG_SCENE_ERR_ARG;

    for (int y = 0; y < img->height; y++) {
        int gy = y / scale;
        if (gy >= n)
            gy = n - 1;
        int gy0 = gy > 0 ? gy - 1 : gy;
        int gy1 = gy + 1 < n ? gy + 1 : gy;
        for (int x = 0; x < img->width; x++) {
            int gx = x / scale;
            if (gx >= n)
                gx = n - 1;
            int gx0 = gx > 0 ? gx - 1 : gx;
            int gx1 = gx + 1 < n ? gx + 1 : gx;

            int h = heights[(size_t)gy * n + gx];
            int dzdx = heights[(size_t)gy * n + gx1] -
                       heights[(size_t)gy * n + gx0];
            int dzdy = heights[(size_t)gy1 * n + gx] -
                       heights[(size_t)gy0 * n + gx];
            int slope = abs(dzdx) + abs(dzdy);

            /* height ramp: low ground blue-grey, plateau sunlit green */
            int r = 40 + h * 45 / 255;
            int g = 70 + h * 90 / 255;
            int b = 45 + h * 30 / 255;

            if (h < 12) {         /* voids and water */
                r = 18; g = 28; b = 44;
            } else {
                int shade = 12 * (dzdx + dzdy) / (scale * 2);
                int light = 100 - slope * 2 - shade;
                if (light < 40) light = 40;
                if (light > 150) light = 150;
                r = r * light / 100;
                g = g * light / 100;
                b = b * light / 100;
                if (r > 255) r = 255;
                if (g > 255) g = 255;
                if (b > 255) b = 255;
            }

            size_t o = ((size_t)y * (size_t)img->width + (size_t)x) * 3;
            img->rgb[o + 0] = (uint8_t)r;
            img->rgb[o + 1] = (uint8_t)g;
            img->rgb[o + 2] = (uint8_t)b;
        }
    }
    return LG_SCENE_OK;
}

int lg_scene_sprite(lg_image *img, const uint8_t *pixels, int sw, int sh,
                    int dx, int dy, int scale)
{
    if (!img->rgb || !pixels || sw <= 0 || sh <= 0 || scale <= 0)
        return LG_SCENE_ERR_ARG;
    for (int y = 0; y < sh * scale; y++) {
        for (int x = 0; x < sw * scale; x++) {
            uint8_t v = pixels[(size_t)(y / scale) * sw + (x / scale)];
            if (v == 0)
                continue;
            uint8_t grey = (uint8_t)(40 + v * 3 / 4);
            lg_image_put(img, dx + x, dy + y, grey, grey, grey);
        }
    }
    return LG_SCENE_OK;
}

int lg_image_write_ppm(const lg_image *img, const char *path)
{
    FILE *fp = fopen(path, "wb");
    if (!fp)
        return LG_SCENE_ERR_IO;
    fprintf(fp, "P6\n%d %d\n255\n", img->width, img->height);
    size_t n = (size_t)img->width * (size_t)img->height * 3;
    int ok = fwrite(img->rgb, 1, n, fp) == n;
    fclose(fp);
    return ok ? LG_SCENE_OK : LG_SCENE_ERR_IO;
}
