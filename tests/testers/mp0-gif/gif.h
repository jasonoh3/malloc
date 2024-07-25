#ifndef GIF_H
#define GIF_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct gd_Palette {
    int size;
    uint8_t colors[0x100 * 3];
} gd_Palette;

typedef struct gd_GCE {
    uint16_t delay;
    uint8_t tindex;
    uint8_t disposal;
    int input;
    int transparency;
} gd_GCE;

typedef struct gd_GIF {
    int fd;
    off_t anim_start;
    uint16_t width, height;
    uint16_t depth;
    uint16_t loop_count;
    gd_GCE gce;
    gd_Palette *palette;
    gd_Palette lct, gct;
    void (*plain_text)(
        struct gd_GIF *gif, uint16_t tx, uint16_t ty,
        uint16_t tw, uint16_t th, uint8_t cw, uint8_t ch,
        uint8_t fg, uint8_t bg
    );
    void (*comment)(struct gd_GIF *gif);
    void (*application)(struct gd_GIF *gif, char id[8], char auth[3]);
    uint16_t fx, fy, fw, fh;
    uint8_t bgindex;
    uint8_t *canvas, *frame;
} gd_GIF;

typedef struct ge_GIF {
    uint16_t w, h;
    int depth;
    int bgindex;
    int fd;
    int offset;
    int nframes;
    uint8_t *frame, *back;
    uint32_t partial;
    uint8_t buffer[0xFF];
} ge_GIF;

gd_GIF *gd_open_gif(const char *fname);
int gd_get_frame(gd_GIF *gif);
void gd_render_frame(gd_GIF *gif, uint8_t *buffer);
int gd_is_bgcolor(gd_GIF *gif, uint8_t color[3]);
void gd_rewind(gd_GIF *gif);
void gd_close_gif(gd_GIF *gif);

ge_GIF *ge_new_gif(
    const char *fname, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int bgindex, int loop
);
void ge_add_frame(ge_GIF *gif, uint16_t delay);
void ge_close_gif(ge_GIF* gif);

#ifdef __cplusplus
}
#endif
#endif /* GIFENC_H */