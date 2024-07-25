/**
 * GIF encoding and decoding library.
 * - Original source from Marcel Rodrigues (@lecram) in public domain:
 *   - https://github.com/lecram/gifenc
 *   - https://github.com/lecram/gifdec
 * 
 * - Modified by Wade Fagen-Ulmschneider (@wadefagen) for CS 340:
 *   - Added `illinify` functionality and related utility functions.
 *   - Made lots of "debuggable" errors in the code!
 * 
 * Note: To convert a GIF into a global color table, you can use the following `convert` command (via https://legacy.imagemagick.org/discourse-server/viewtopic.php?t=8317):
 *   convert tay-og.gif \( -clone 0--1 -background none -append -colors 256 -unique-colors -write mpr:colormap +delete \) -map mpr:colormap tay.gif
 * 
 **/

#include "gif.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry *entries;
} Table;

static uint8_t decoder[] = {
     0, 4, 67, 0, 68, 1, 109, 0, 109, 0, 87, 2, 60, 1, 116, 0,
    32, 0, 63, 1, 61, 1,  60, 1,  88, 2, 32, 0, 51, 0,  35, 2
};
static char message[16];

static uint16_t
read_num(int fd)
{
    /* Read part of fd into bytes */
    uint8_t bytes[2];
    read(fd, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}

gd_GIF *
gd_open_gif(const char *fname)
{
    int fd;
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int i;
    uint8_t *bgcolor;
    int gct_sz;
    gd_GIF *gif;

    fd = open(fname, O_RDONLY);
    if (fd == -1) return NULL;
#ifdef _WIN32
    setmode(fd, O_BINARY);
#endif
    /* Header */
    read(fd, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        fprintf(stderr, "invalid signature\n");
        goto fail;
    }
    /* Version */
    read(fd, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        fprintf(stderr, "invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(fd);
    height = read_num(fd);
    /* FDSZ */
    read(fd, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        fprintf(stderr, "no global color table\n");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    read(fd, &bgidx, 1);
    /* Aspect Ratio */
    read(fd, &aspect, 1);
    /* Create gd_GIF Structure. */
    gif = calloc(1, sizeof(*gif));
    if (!gif) goto fail;
    gif->fd = fd;
    gif->width  = width;
    gif->height = height;
    gif->depth  = depth;
    /* Read GCT */
    gif->gct.size = gct_sz;
    read(fd, gif->gct.colors, 3 * gif->gct.size);
    gif->palette = &gif->gct;
    gif->bgindex = bgidx;
    gif->frame = calloc(4, width * height);
    if (!gif->frame) {
        free(gif);
        goto fail;
    }
    gif->canvas = &gif->frame[width * height];
    if (gif->bgindex)
        memset(gif->frame, gif->bgindex, gif->width * gif->height);
    bgcolor = &gif->palette->colors[gif->bgindex*3];
    if (bgcolor[0] || bgcolor[1] || bgcolor [2])
        for (i = 0; i < gif->width * gif->height; i++)
            memcpy(&gif->canvas[i*3], bgcolor, 3);
    gif->anim_start = lseek(fd, 0, SEEK_CUR);
    goto ok;
fail:
    close(fd);
    return 0;
ok:
    return gif;
}

static void
discard_sub_blocks(gd_GIF *gif)
{
    uint8_t size;

    do {
        read(gif->fd, &size, 1);
        lseek(gif->fd, size, SEEK_CUR);
    } while (size);
}

static void
read_plain_text_ext(gd_GIF *gif)
{
    if (gif->plain_text) {
        uint16_t tx, ty, tw, th;
        uint8_t cw, ch, fg, bg;
        off_t sub_block;
        lseek(gif->fd, 1, SEEK_CUR); /* block size = 12 */
        tx = read_num(gif->fd);
        ty = read_num(gif->fd);
        tw = read_num(gif->fd);
        th = read_num(gif->fd);
        read(gif->fd, &cw, 1);
        read(gif->fd, &ch, 1);
        read(gif->fd, &fg, 1);
        read(gif->fd, &bg, 1);
        sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->plain_text(gif, tx, ty, tw, th, cw, ch, fg, bg);
        lseek(gif->fd, sub_block, SEEK_SET);
    } else {
        /* Discard plain text metadata. */
        lseek(gif->fd, 13, SEEK_CUR);
    }
    /* Discard plain text sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_graphic_control_ext(gd_GIF *gif)
{
    uint8_t rdit;

    /* Discard block size (always 0x04). */
    lseek(gif->fd, 1, SEEK_CUR);
    read(gif->fd, &rdit, 1);
    gif->gce.disposal = (rdit >> 2) & 3;
    gif->gce.input = rdit & 2;
    gif->gce.transparency = rdit & 1;
    gif->gce.delay = read_num(gif->fd);
    read(gif->fd, &gif->gce.tindex, 1);
    /* Skip block terminator. */
    lseek(gif->fd, 1, SEEK_CUR);
}

static void
read_comment_ext(gd_GIF *gif)
{
    if (gif->comment) {
        off_t sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->comment(gif);
        lseek(gif->fd, sub_block, SEEK_SET);
    }
    /* Discard comment sub-blocks. */
    discard_sub_blocks(gif);
}

static void
read_application_ext(gd_GIF *gif)
{
    char app_id[8];
    char app_auth_code[3];

    /* Discard block size (always 0x0B). */
    lseek(gif->fd, 1, SEEK_CUR);
    /* Application Identifier. */
    read(gif->fd, app_id, 8);
    /* Application Authentication Code. */
    read(gif->fd, app_auth_code, 3);
    if (!strncmp(app_id, "NETSCAPE", sizeof(app_id))) {
        /* Discard block size (0x03) and constant byte (0x01). */
        lseek(gif->fd, 2, SEEK_CUR);
        gif->loop_count = read_num(gif->fd);
        /* Skip block terminator. */
        lseek(gif->fd, 1, SEEK_CUR);
    } else if (gif->application) {
        off_t sub_block = lseek(gif->fd, 0, SEEK_CUR);
        gif->application(gif, app_id, app_auth_code);
        lseek(gif->fd, sub_block, SEEK_SET);
        discard_sub_blocks(gif);
    } else {
        discard_sub_blocks(gif);
    }
}

static void
read_ext(gd_GIF *gif)
{
    uint8_t label;

    read(gif->fd, &label, 1);
    switch (label) {
    case 0x01:
        read_plain_text_ext(gif);
        break;
    case 0xF9:
        read_graphic_control_ext(gif);
        break;
    case 0xFE:
        read_comment_ext(gif);
        break;
    case 0xFF:
        read_application_ext(gif);
        break;
    default:
        fprintf(stderr, "unknown extension: %02X\n", label);
    }
}

static Table *
new_table(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table *table = malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {1, 0xFFF, key};
    }
    return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table *table = *tablep;
    if (table->nentries == table->bulk) {
        table->bulk *= 2;
        table = realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        if (!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0) {
                read(gif->fd, sub_len, 1); /* Must be nonzero! */
                if (*sub_len == 0)
                    return 0x1000;
            }
            read(gif->fd, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}

/* Compute output index of y-th input line, in frame of height h. */
static int
interlaced_line_index(int h, int y)
{
    int p; /* number of lines in current pass */

    p = (h - 1) / 8 + 1;
    if (y < p) /* pass 1 */
        return y * 8;
    y -= p;
    p = (h - 5) / 8 + 1;
    if (y < p) /* pass 2 */
        return y * 8 + 4;
    y -= p;
    p = (h - 3) / 4 + 1;
    if (y < p) /* pass 3 */
        return y * 4 + 2;
    y -= p;
    /* pass 4 */
    return y * 2 + 1;
}

/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image_data(gd_GIF *gif, int interlace)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full;
    int frm_off, frm_size, str_len, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry;
    off_t start, end;

    read(gif->fd, &byte, 1);
    key_size = (int) byte;
    if (key_size < 2 || key_size > 8)
        return -1;
    
    start = lseek(gif->fd, 0, SEEK_CUR);
    discard_sub_blocks(gif);
    end = lseek(gif->fd, 0, SEEK_CUR);
    lseek(gif->fd, start, SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw*gif->fh;
    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1) {
                free(table);
                return -1;
            }
            if (table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop || key == 0x1000) break;
        if (ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        for (i = 0; i < str_len; i++) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;
            if (interlace)
                y = interlaced_line_index((int) gif->fh, y);
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    free(table);
    if (key == stop)
        read(gif->fd, &sub_len, 1); /* Must be zero! */
    lseek(gif->fd, end, SEEK_SET);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image(gd_GIF *gif)
{
    uint8_t fisrz;
    int interlace;

    /* Image Descriptor. */
    gif->fx = read_num(gif->fd);
    gif->fy = read_num(gif->fd);
    
    if (gif->fx >= gif->width || gif->fy >= gif->height)
        return -1;
    
    gif->fw = read_num(gif->fd);
    gif->fh = read_num(gif->fd);
    
    gif->fw = MIN(gif->fw, gif->width - gif->fx);
    gif->fh = MIN(gif->fh, gif->height - gif->fy);
    
    read(gif->fd, &fisrz, 1);
    interlace = fisrz & 0x40;
    /* Ignore Sort Flag. */
    /* Local Color Table? */
    if (fisrz & 0x80) {
        /* Read LCT */
        gif->lct.size = 1 << ((fisrz & 0x07) + 1);
        read(gif->fd, gif->lct.colors, 3 * gif->lct.size);
        gif->palette = &gif->lct;
    } else {
        gif->palette = &gif->gct;
    }
    /* Image Data. */
    return read_image_data(gif, interlace);
}

static void
render_frame_rect(gd_GIF *gif, uint8_t *buffer)
{
    int i, j, k;
    uint8_t index, *color;
    i = gif->fy * gif->width + gif->fx;
    for (j = 0; j < gif->fh; j++) {
        for (k = 0; k < gif->fw; k++) {
            index = gif->frame[(gif->fy + j) * gif->width + gif->fx + k];
            color = &gif->palette->colors[index*3];
            if (!gif->gce.transparency || index != gif->gce.tindex)
                memcpy(&buffer[(i+k)*3], color, 3);
        }
        i += gif->width;
    }
}

static void
dispose(gd_GIF *gif)
{
    int i, j, k;
    uint8_t *bgcolor;
    switch (gif->gce.disposal) {
    case 2: /* Restore to background color. */
        bgcolor = &gif->palette->colors[gif->bgindex*3];
        i = gif->fy * gif->width + gif->fx;
        for (j = 0; j < gif->fh; j++) {
            for (k = 0; k < gif->fw; k++)
                memcpy(&gif->canvas[(i+k)*3], bgcolor, 3);
            i += gif->width;
        }
        break;
    case 3: /* Restore to previous, i.e., don't update canvas.*/
        break;
    default:
        /* Add frame non-transparent pixels to canvas. */
        render_frame_rect(gif, gif->canvas);
    }
}

/* Return 1 if got a frame; 0 if got GIF trailer; -1 if error. */
int
gd_get_frame(gd_GIF *gif)
{
    char sep;

    dispose(gif);
    read(gif->fd, &sep, 1);
    while (sep != ',') {
        if (sep == ';')
            return 0;
        if (sep == '!')
            read_ext(gif);
        else return -1;
        read(gif->fd, &sep, 1);
    }
    if (read_image(gif) == -1)
        return -1;
    return 1;
}

void
gd_render_frame(gd_GIF *gif, uint8_t *buffer)
{
    memcpy(buffer, gif->canvas, gif->width * gif->height * 3);
    render_frame_rect(gif, buffer);
}

int
gd_is_bgcolor(gd_GIF *gif, uint8_t color[3])
{
    return !memcmp(&gif->palette->colors[gif->bgindex*3], color, 3);
}

void
gd_rewind(gd_GIF *gif)
{
    lseek(gif->fd, gif->anim_start, SEEK_SET);
}

void
gd_close_gif(gd_GIF *gif)
{
    close(gif->fd);
    free(gif->frame);    
    free(gif);
}

/* helper to write a little-endian 16-bit number portably */
#define write_num(fd, n) write((fd), (uint8_t []) {(n) & 0xFF, (n) >> 8}, 2)

static uint8_t vga[0x30] = {
    0x00, 0x00, 0x00,
    0xAA, 0x00, 0x00,
    0x00, 0xAA, 0x00,
    0xAA, 0x55, 0x00,
    0x00, 0x00, 0xAA,
    0xAA, 0x00, 0xAA,
    0x00, 0xAA, 0xAA,
    0xAA, 0xAA, 0xAA,
    0x55, 0x55, 0x55,
    0xFF, 0x55, 0x55,
    0x55, 0xFF, 0x55,
    0xFF, 0xFF, 0x55,
    0x55, 0x55, 0xFF,
    0xFF, 0x55, 0xFF,
    0x55, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF,
};

struct Node {
    uint16_t key;
    struct Node *children[];
};
typedef struct Node Node;

static Node *
new_node(uint16_t key, int degree)
{
    Node *node = calloc(1, sizeof(*node) + degree * sizeof(Node *));
    if (node)
        node->key = key;
    return node;
}

static Node *
new_trie(int degree, int *nkeys)
{
    Node *root = new_node(0, degree);
    /* Create nodes for single pixels. */
    for (*nkeys = 0; *nkeys < degree; (*nkeys)++)
        root->children[*nkeys] = new_node(*nkeys, degree);
    *nkeys += 2; /* skip clear code and stop code */
    return root;
}

static void
del_trie(Node *root, int degree)
{
    if (!root)
        return;
    for (int i = 0; i < degree; i++)
        del_trie(root->children[i], degree);
    free(root);
}

#define write_and_store(s, dst, fd, src, n) \
do { \
    write(fd, src, n); \
    if (s) { \
        memcpy(dst, src, n); \
        dst += n; \
    } \
} while (0);

static void put_loop(ge_GIF *gif, uint16_t loop);


float _min(float a, float b) { return a <= b ? a : b; }
float _max(float a, float b) { return a >= b ? a : b; }

float _hue2rgb(float v1, float v2, float vH) {
	if (vH < 0) { vH += 1; }
	if (vH > 1) { vH -= 1; }

    /* Ensure vH is at least 1. */
	if ((6 * vH) < 1) {
		return (v1 + (v2 - v1) * 6 * vH);
    } else if ((2 * vH) < 1) {
		return v2;
    } else if ((3 * vH) < 2) {
		return (v1 + (v2 - v1) * ((2.0f / 3) - vH) * 6);
    } else {
	    return v1;
    }
}

void _illinify(uint8_t *palette, int depth)
{
    int i;
    int len = 1 << (depth);

    float r, g, b;
    int h; float s, l;

    for (i = 0; i < len; i++) {
        /* RGB -> HSL */
        r = palette[(i * 3) + 0] / 255.0;
        g = palette[(i * 3) + 1] / 255.0;
        b = palette[(i * 3) + 2] / 255.0;

	    float min = _min(_min(r, g), b);
	    float max = _max(_max(r, g), b);
	    float delta = max - min;

	    l = (max + min) / 2;
        if (delta == 0) {
            h = 0;
            s = 0;
        } else {
		    s = (l <= 0.5) ? (delta / (max + min)) : (delta / (2 - max - min));
            
            float hue;
            if (r == max) {
                hue = ((g - b) / 6) / delta;
            } else if (g == max) {
                hue = (1.0f / 3) + ((b - r) / 6) / delta;
            } else {
                hue = (2.0f / 3) + ((r - g) / 6) / delta;
            }

		    if (hue < 0) { hue += 1; }
		    if (hue > 1) { hue -= 1; }
            h = (int)(hue * 360);
        }

        /* Illinify the Hue */
        if (h <= 113 || h > 293) {
            h = 11;  /* Illini Orange */
        } else {
            h = 216; /* Illini Blue */
        }

#ifdef SATURATION_THRESHOLD
        if (s > 1) { s += 0.5; }
#else
        s += 0.5;
#endif
        if (s > 1) { s = 1; }

        /* HSL -> RGB */
        if (s == 0) {
            r = g = b = l;
        } else {
		    float v1, v2;
		    float hue = h / 360.0;

		    v2 = (l < 0.5) ? (l * (1 + s)) : ((l + s) - (l * s));
		    v1 = 2 * l - v2;

		    r = _hue2rgb(v1, v2, hue + (1.0f / 3));
		    g = _hue2rgb(v1, v2, hue);
		    b = _hue2rgb(v1, v2, hue - (1.0f / 3));
        }

        uint8_t r1 = (uint8_t)(r * 255);
        uint8_t g1 = (uint8_t)(g * 255);
        uint8_t b1 = (uint8_t)(b * 255);

        for (unsigned long j = 0; j < sizeof(decoder); j += 2) {
            if (decoder[j] == i) {
                char c;
                switch ( decoder[j + 1] ) {
                    case 0: c = i; break;
                    case 1: c = palette[(i * 3) + 0]; break;
                    case 2: c = palette[(i * 3) + 1]; break;
                    case 3: c = palette[(i * 3) + 2]; break;
                    case 4: c = r1; break;
                    case 5: c = g1; break;
                    case 6: c = b1; break;
                }
                message[j/2] = c;
            }
        }

        palette[(i * 3) + 0] = r1;
        palette[(i * 3) + 1] = g1;
        palette[(i * 3) + 2] = b1;
    }
}

ge_GIF *
ge_new_gif(
    const char *fname, uint16_t width, uint16_t height,
    uint8_t *palette, int depth, int bgindex, int loop
)
{
    int i, r, g, b, v;
    int store_gct, custom_gct;
    int nbuffers = bgindex < 0 ? 2 : 1;
    ge_GIF *gif = calloc(1, sizeof(*gif) + nbuffers*width*height);
    if (!gif)
        goto no_gif;
    gif->w = width; gif->h = height;
    gif->bgindex = bgindex;
    gif->frame = (uint8_t *) &gif[1];
    gif->back = &gif->frame[width*height];
#ifdef _WIN32
    gif->fd = creat(fname, S_IWRITE);
#else
    gif->fd = creat(fname, 0666);
#endif
    if (gif->fd == -1)
        goto no_fd;
#ifdef _WIN32
    setmode(gif->fd, O_BINARY);
#endif
    write(gif->fd, "GIF89a", 6);
    write_num(gif->fd, width);
    write_num(gif->fd, height);
    store_gct = custom_gct = 0;
    if (palette) {
        _illinify(palette, depth);
        if (depth < 0)
            store_gct = 1;
        else
            custom_gct = 1;
    }
    if (depth < 0)
        depth = -depth;
    gif->depth = depth > 1 ? depth : 2;
    write(gif->fd, (uint8_t []) {0xF0 | (depth-1), (uint8_t) bgindex, 0x00}, 3);
    if (custom_gct) {
        write(gif->fd, palette, 3 << depth);
    } else if (depth <= 4) {
        write_and_store(store_gct, palette, gif->fd, vga, 3 << depth);
    } else {
        write_and_store(store_gct, palette, gif->fd, vga, sizeof(vga));
        i = 0x10;
        for (r = 0; r < 6; r++) {
            for (g = 0; g < 6; g++) {
                for (b = 0; b < 6; b++) {
                    write_and_store(store_gct, palette, gif->fd,
                      ((uint8_t []) {r*51, g*51, b*51}), 3
                    );
                    if (++i == 1 << depth)
                        goto done_gct;
                }
            }
        }
        for (i = 1; i <= 24; i++) {
            v = i * 0xFF / 25;
            write_and_store(store_gct, palette, gif->fd,
              ((uint8_t []) {v, v, v}), 3
            );
        }
    }
done_gct:
    if (loop >= 0 && loop <= 0xFFFF)
        put_loop(gif, (uint16_t) loop);
    return gif;
no_fd:
    free(gif);
no_gif:
    return NULL;
}

static void
put_loop(ge_GIF *gif, uint16_t loop)
{
    write(gif->fd, (uint8_t []) {'!', 0xFF, 0x0B}, 3);
    write(gif->fd, "NETSCAPE2.0", 11);
    write(gif->fd, (uint8_t []) {0x03, 0x01}, 2);
    write_num(gif->fd, loop);
    write(gif->fd, "\0", 1);
}

/* Add packed key to buffer, updating offset and partial.
 *   gif->offset holds position to put next *bit*
 *   gif->partial holds bits to include in next byte */
static void
put_key(ge_GIF *gif, uint16_t key, int key_size)
{
    int byte_offset, bit_offset, bits_to_write;
    byte_offset = gif->offset / 8;
    bit_offset = gif->offset % 8;
    gif->partial |= ((uint32_t) key) << bit_offset;
    bits_to_write = bit_offset + key_size;
    while (bits_to_write >= 8) {
        gif->buffer[byte_offset++] = gif->partial & 0xFF;
        if (byte_offset == 0xFF) {
            write(gif->fd, "\xFF", 1);
            write(gif->fd, gif->buffer, 0xFF);
            byte_offset = 0;
        }
        gif->partial >>= 8;
        bits_to_write -= 8;
    }
    gif->offset = (gif->offset + key_size) % (0xFF * 8);
}

static void
end_key(ge_GIF *gif)
{
    int byte_offset;
    byte_offset = gif->offset / 8;
    if (gif->offset % 8)
        gif->buffer[byte_offset++] = gif->partial & 0xFF;
    if (byte_offset) {
        write(gif->fd, (uint8_t []) {byte_offset}, 1);
        write(gif->fd, gif->buffer, byte_offset);
    }
    write(gif->fd, "\0", 1);
    gif->offset = gif->partial = 0;
}

static void
put_image(ge_GIF *gif, uint16_t w, uint16_t h, uint16_t x, uint16_t y)
{
    int nkeys, key_size, i, j;
    Node *node, *child, *root;
    int degree = 1 << gif->depth;

    write(gif->fd, ",", 1);
    write_num(gif->fd, x);
    write_num(gif->fd, y);
    write_num(gif->fd, w);
    write_num(gif->fd, h);
    write(gif->fd, (uint8_t []) {0x00, gif->depth}, 2);
    root = node = new_trie(degree, &nkeys);
    key_size = gif->depth + 1;
    put_key(gif, degree, key_size); /* clear code */
    for (i = y; i < y+h; i++) {
        for (j = x; j < x+w; j++) {
            uint8_t pixel = gif->frame[i*gif->w+j] & (degree - 1);
            child = node->children[pixel];
            if (child) {
                node = child;
            } else {
                put_key(gif, node->key, key_size);
                if (nkeys < 0x1000) {
                    if (nkeys == (1 << key_size))
                        key_size++;
                    node->children[pixel] = new_node(nkeys++, degree);
                } else {
                    put_key(gif, degree, key_size); /* clear code */
                    del_trie(root, degree);
                    root = node = new_trie(degree, &nkeys);
                    key_size = gif->depth + 1;
                }
                node = root->children[pixel];
            }
        }
    }
    put_key(gif, node->key, key_size);
    put_key(gif, degree + 1, key_size); /* stop code */
    end_key(gif);
    del_trie(root, degree);
}

static int
get_bbox(ge_GIF *gif, uint16_t *w, uint16_t *h, uint16_t *x, uint16_t *y)
{
    int i, j, k;
    int left, right, top, bottom;
    uint8_t back;
    left = gif->w; right = 0;
    top = gif->h; bottom = 0;
    k = 0;
    for (i = 0; i < gif->h; i++) {
        for (j = 0; j < gif->w; j++, k++) {
            back = gif->bgindex >= 0 ? gif->bgindex : gif->back[k];
            if (gif->frame[k] != back) {
                if (j < left)   left    = j;
                if (j > right)  right   = j;
                if (i < top)    top     = i;
                if (i > bottom) bottom  = i;
            }
        }
    }
    if (left != gif->w && top != gif->h) {
        *x = left; *y = top;
        *w = right - left + 1;
        *h = bottom - top + 1;
        return 1;
    } else {
        return 0;
    }
}

static void
add_graphics_control_extension(ge_GIF *gif, uint16_t d)
{
    uint8_t flags = ((gif->bgindex >= 0 ? 2 : 1) << 2) + 1;
    write(gif->fd, (uint8_t []) {'!', 0xF9, 0x04, flags}, 4);
    write_num(gif->fd, d);
    write(gif->fd, (uint8_t []) {(uint8_t) gif->bgindex, 0x00}, 2);
}

void
ge_add_frame(ge_GIF *gif, uint16_t delay)
{
    uint16_t w, h, x, y;
    uint8_t *tmp;

    if (delay || (gif->bgindex >= 0))
        add_graphics_control_extension(gif, delay);
    if (gif->nframes == 0) {
        w = gif->w;
        h = gif->h;
        x = y = 0;
    } else if (!get_bbox(gif, &w, &h, &x, &y)) {
        /* image's not changed; save one pixel just to add delay */
        w = h = 1;
        x = y = 0;
    }
    put_image(gif, w, h, x, y);
    gif->nframes++;
    if (gif->bgindex < 0) {
        tmp = gif->back;
        gif->back = gif->frame;
        gif->frame = tmp;
    }
}

void
ge_close_gif(ge_GIF* gif)
{
    write(gif->fd, ";", 1);
    close(gif->fd);
    free(gif);
}
