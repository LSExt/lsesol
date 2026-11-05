#include "lseai.h"
#include "globals.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <jpeglib.h>

/* Convert an XImage to an uncompressed 24-bit RGB byte buffer [R, G, B, R, G, B, ...] */
unsigned char *ximage_to_rgb24(Display *dpy, XImage *img, Colormap cmap) {
    int x, y;
    int width = img->width;
    int height = img->height;
    unsigned char *rgb = malloc(width * height * 3);
    if (!rgb) return NULL;

    unsigned char *dst = rgb;

    if (img->depth == 8) {
        /* Indexed palette: query the X colormap */
        XColor colors[256];
        for (int i = 0; i < 256; i++) {
            colors[i].pixel = i;
        }
        XQueryColors(dpy, cmap, colors, 256);

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                unsigned long pixel = XGetPixel(img, x, y);
                int idx = pixel & 0xFF;
                /* XColor values are 16-bit (0..65535), scale down to 8-bit */
                *dst++ = colors[idx].red   >> 8;
                *dst++ = colors[idx].green >> 8;
                *dst++ = colors[idx].blue  >> 8;
            }
        }
    } else {
        /* 16-bit, 24-bit, or 32-bit TrueColor */
        Visual *vis = DefaultVisual(dpy, DefaultScreen(dpy));
        unsigned long r_mask = vis->red_mask;
        unsigned long g_mask = vis->green_mask;
        unsigned long b_mask = vis->blue_mask;

        for (y = 0; y < height; y++) {
            for (x = 0; x < width; x++) {
                unsigned long pixel = XGetPixel(img, x, y);
                /* Extract channels using visual bitmasks */
                *dst++ = (pixel & r_mask) >> 16;  /* Adjust shifts if non-standard */
                *dst++ = (pixel & g_mask) >> 8;
                *dst++ = (pixel & b_mask);
            }
        }
    }

    return rgb;
}



int write_rgb_to_jpeg_file(const char *filename, unsigned char *rgb, int width, int height, int quality) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    FILE *outfile;
    JSAMPROW row_pointer[1];

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(filename, "wb")) == NULL) {
        jpeg_destroy_compress(&cinfo);
        return -1;
    }

    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = &rgb[cinfo.next_scanline * width * 3];
        (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    fclose(outfile);
    jpeg_destroy_compress(&cinfo);
    return 0;
}


/* Base64 encoding table */
static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static char *base64_encode(const unsigned char *data, size_t input_length) {
    size_t output_length = 4 * ((input_length + 2) / 3);
    char *encoded_data = (char *)malloc(output_length + 1);
    if (!encoded_data) return NULL;

    for (size_t i = 0, j = 0; i < input_length;) {
        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 16) + (octet_b << 8) + octet_c;

        encoded_data[j++] = b64_table[(triple >> 18) & 0x3F];
        encoded_data[j++] = b64_table[(triple >> 12) & 0x3F];
        encoded_data[j++] = (i > input_length + 1) ? '=' : b64_table[(triple >> 6) & 0x3F];
        encoded_data[j++] = (i > input_length) ? '=' : b64_table[triple & 0x3F];
    }
    encoded_data[output_length] = '\0';
    return encoded_data;
}

/* Reads a binary file and returns a Base64 string */
static char *file_to_base64(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *buffer = (unsigned char *)malloc(fsize);
    if (!buffer) {
        fclose(f);
        return NULL;
    }

    fread(buffer, 1, fsize, f);
    fclose(f);

    char *b64 = base64_encode(buffer, fsize);
    free(buffer);
    return b64;
}

/* Builds the JSON parts string for all files currently attached */
char *build_attachment_parts_json(void) {
    if (g_num_attachments == 0) return NULL;

    size_t total_buf_size = 1024;
    char *parts_str = (char *)malloc(total_buf_size);
    if (!parts_str) return NULL;
    parts_str[0] = '\0';

    for (int i = 0; i < g_num_attachments; i++) {
        char *b64 = file_to_base64(g_files_to_attach[i]);
        if (!b64) continue;

        /* Form the inline_data part */
        const char *part_fmt = "{\"inline_data\":{\"mime_type\":\"image/jpeg\",\"data\":\"%s\"}},";
        size_t needed = strlen(b64) + strlen(part_fmt) + 32;

        if (strlen(parts_str) + needed >= total_buf_size) {
            total_buf_size = (strlen(parts_str) + needed) * 2;
            parts_str = (char *)realloc(parts_str, total_buf_size);
        }

        char *temp_part = (char *)malloc(needed);
        sprintf(temp_part, part_fmt, b64);
        strcat(parts_str, temp_part);

        free(temp_part);
        free(b64);
    }

    return parts_str;
}

char *build_attachment_history_tag(void) {
    if (g_num_attachments == 0) return NULL;

    /* Roughly 64 bytes per filename tag */
    size_t tag_buf_size = 128 * g_num_attachments;
    char *tags = (char *)malloc(tag_buf_size);
    if (!tags) return NULL;
    tags[0] = '\0';

    for (int i = 0; i < g_num_attachments; i++) {
        char entry[256];
        snprintf(entry, sizeof(entry), "[Image Attached: %s]\n", g_files_to_attach[i]);
        strcat(tags, entry);
    }
    return tags;
}
