#include "lseai.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>

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


#include <stdio.h>
#include <jpeglib.h>

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


