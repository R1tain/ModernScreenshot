#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef VERSION
#define VERSION "0.1.0"
#endif

#define HOTKEY_KEYS "Ctrl+1"
#define MAX_CAPTURE_BUFFER (512 * 1024)
#define TOAST_MS 1200

typedef struct {
    int x;
    int y;
    int w;
    int h;
} Rect;

typedef struct {
    Display *display;
    int screen;
    Window root;
    int root_w;
    int root_h;
    Visual *root_visual;
    int root_depth;
    Visual *overlay_visual;
    int overlay_depth;
    Colormap overlay_colormap;
    XRenderPictFormat *overlay_format;
    int has_render;
    int has_argb;
    XFontStruct *font;
    int hotkey_code;
} App;

typedef struct {
    unsigned long mask;
    int shift;
    int bits;
} Channel;

typedef struct {
    Channel r;
    Channel g;
    Channel b;
} PixelFormat;

typedef struct {
    FILE *fp;
    uint32_t adler;
    int first_block;
} PngWriter;

static volatile sig_atomic_t keep_running = 1;
static int grab_failed = 0;

static void handle_signal(int signal_number) {
    (void)signal_number;
    keep_running = 0;
}

static void sleep_ms(long milliseconds) {
    struct timespec delay;
    delay.tv_sec = milliseconds / 1000;
    delay.tv_nsec = (milliseconds % 1000) * 1000000L;
    while (nanosleep(&delay, &delay) != 0 && errno == EINTR) {
    }
}

static int x_error_handler(Display *display, XErrorEvent *event) {
    (void)display;
    if (event->error_code == BadAccess) {
        grab_failed = 1;
    }
    return 0;
}

static int clamp_int(int value, int min, int max) {
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static Rect normalize_rect(int x1, int y1, int x2, int y2, int max_w, int max_h) {
    Rect rect;
    rect.x = x1 < x2 ? x1 : x2;
    rect.y = y1 < y2 ? y1 : y2;
    rect.w = abs(x2 - x1);
    rect.h = abs(y2 - y1);
    rect.x = clamp_int(rect.x, 0, max_w - 1);
    rect.y = clamp_int(rect.y, 0, max_h - 1);
    if (rect.x + rect.w > max_w) {
        rect.w = max_w - rect.x;
    }
    if (rect.y + rect.h > max_h) {
        rect.h = max_h - rect.y;
    }
    return rect;
}

static int mkdir_recursive(const char *path) {
    char buffer[PATH_MAX];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof(buffer)) {
        return -1;
    }

    memcpy(buffer, path, len + 1);
    for (char *cursor = buffer + 1; *cursor; cursor++) {
        if (*cursor != '/') {
            continue;
        }

        *cursor = '\0';
        if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
        *cursor = '/';
    }

    if (mkdir(buffer, 0755) != 0 && errno != EEXIST) {
        return -1;
    }
    return 0;
}

static int build_output_path(char *path, size_t path_size, char *display_path, size_t display_size) {
    const char *home = getenv("HOME");
    if (!home || !*home) {
        return -1;
    }

    char dir[PATH_MAX];
    if (snprintf(dir, sizeof(dir), "%s/Pictures/Screenshots", home) >= (int)sizeof(dir)) {
        return -1;
    }
    if (mkdir_recursive(dir) != 0) {
        return -1;
    }

    time_t now = time(NULL);
    struct tm local_time;
    if (!localtime_r(&now, &local_time)) {
        return -1;
    }

    char stamp[32];
    if (strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local_time) == 0) {
        return -1;
    }

    if (snprintf(path, path_size, "%s/Screenshot-%s.png", dir, stamp) >= (int)path_size) {
        return -1;
    }
    snprintf(display_path, display_size, "~/Pictures/Screenshots/Screenshot-%s.png", stamp);
    return 0;
}

static Channel channel_from_mask(unsigned long mask) {
    Channel channel = {mask, 0, 0};
    if (mask == 0) {
        return channel;
    }

    while (((mask >> channel.shift) & 1UL) == 0) {
        channel.shift++;
    }

    unsigned long shifted = mask >> channel.shift;
    while ((shifted & 1UL) != 0) {
        channel.bits++;
        shifted >>= 1;
    }
    return channel;
}

static uint8_t component_from_pixel(unsigned long pixel, Channel channel) {
    if (channel.mask == 0 || channel.bits == 0) {
        return 0;
    }

    unsigned long value = (pixel & channel.mask) >> channel.shift;
    unsigned long max_value = (1UL << channel.bits) - 1UL;
    return (uint8_t)((value * 255UL) / max_value);
}

static PixelFormat pixel_format_from_visual(Visual *visual) {
    PixelFormat format;
    format.r = channel_from_mask(visual->red_mask);
    format.g = channel_from_mask(visual->green_mask);
    format.b = channel_from_mask(visual->blue_mask);
    return format;
}

static void write_be32(FILE *fp, uint32_t value) {
    uint8_t bytes[4] = {
        (uint8_t)(value >> 24),
        (uint8_t)(value >> 16),
        (uint8_t)(value >> 8),
        (uint8_t)value,
    };
    fwrite(bytes, 1, sizeof(bytes), fp);
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    crc = ~crc;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = (uint32_t)-(int)(crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

static uint32_t adler32_update(uint32_t adler, const uint8_t *data, size_t len) {
    uint32_t s1 = adler & 0xffffU;
    uint32_t s2 = (adler >> 16) & 0xffffU;

    for (size_t i = 0; i < len; i++) {
        s1 = (s1 + data[i]) % 65521U;
        s2 = (s2 + s1) % 65521U;
    }
    return (s2 << 16) | s1;
}

static int png_chunk(FILE *fp, const char type[4], const uint8_t *data, size_t len) {
    if (len > UINT32_MAX) {
        return -1;
    }

    write_be32(fp, (uint32_t)len);
    if (fwrite(type, 1, 4, fp) != 4) {
        return -1;
    }
    if (len > 0 && fwrite(data, 1, len, fp) != len) {
        return -1;
    }

    uint32_t crc = 0;
    crc = crc32_update(crc, (const uint8_t *)type, 4);
    crc = crc32_update(crc, data, len);
    write_be32(fp, crc);
    return ferror(fp) ? -1 : 0;
}

static int png_begin(PngWriter *writer, const char *path, int width, int height) {
    static const uint8_t signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    uint8_t ihdr[13];

    writer->fp = fopen(path, "wb");
    writer->adler = 1;
    writer->first_block = 1;
    if (!writer->fp) {
        return -1;
    }

    if (fwrite(signature, 1, sizeof(signature), writer->fp) != sizeof(signature)) {
        fclose(writer->fp);
        writer->fp = NULL;
        return -1;
    }

    ihdr[0] = (uint8_t)(width >> 24);
    ihdr[1] = (uint8_t)(width >> 16);
    ihdr[2] = (uint8_t)(width >> 8);
    ihdr[3] = (uint8_t)width;
    ihdr[4] = (uint8_t)(height >> 24);
    ihdr[5] = (uint8_t)(height >> 16);
    ihdr[6] = (uint8_t)(height >> 8);
    ihdr[7] = (uint8_t)height;
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    if (png_chunk(writer->fp, "IHDR", ihdr, sizeof(ihdr)) != 0) {
        fclose(writer->fp);
        writer->fp = NULL;
        return -1;
    }
    return 0;
}

static int png_write_stored_block(PngWriter *writer, const uint8_t *data, size_t len, int final) {
    uint8_t chunk[2 + 5 + 65535 + 4];
    size_t pos = 0;

    if (len > 65535) {
        return -1;
    }

    if (writer->first_block) {
        chunk[pos++] = 0x78;
        chunk[pos++] = 0x01;
        writer->first_block = 0;
    }

    chunk[pos++] = final ? 1 : 0;
    chunk[pos++] = (uint8_t)(len & 0xffU);
    chunk[pos++] = (uint8_t)((len >> 8) & 0xffU);
    chunk[pos++] = (uint8_t)(~len & 0xffU);
    chunk[pos++] = (uint8_t)((~len >> 8) & 0xffU);
    memcpy(chunk + pos, data, len);
    pos += len;

    if (final) {
        chunk[pos++] = (uint8_t)(writer->adler >> 24);
        chunk[pos++] = (uint8_t)(writer->adler >> 16);
        chunk[pos++] = (uint8_t)(writer->adler >> 8);
        chunk[pos++] = (uint8_t)writer->adler;
    }

    return png_chunk(writer->fp, "IDAT", chunk, pos);
}

static int png_write_row(PngWriter *writer, const uint8_t *row, size_t row_len, int final_row) {
    size_t offset = 0;
    writer->adler = adler32_update(writer->adler, row, row_len);

    while (offset < row_len) {
        size_t block_len = row_len - offset;
        if (block_len > 65535) {
            block_len = 65535;
        }
        int final_block = final_row && offset + block_len == row_len;
        if (png_write_stored_block(writer, row + offset, block_len, final_block) != 0) {
            return -1;
        }
        offset += block_len;
    }
    return 0;
}

static int png_finish(PngWriter *writer) {
    int status = 0;
    if (!writer->fp) {
        return -1;
    }

    if (png_chunk(writer->fp, "IEND", NULL, 0) != 0) {
        status = -1;
    }
    if (fclose(writer->fp) != 0) {
        status = -1;
    }
    writer->fp = NULL;
    return status;
}

static int find_argb_visual(App *app) {
    if (!app->has_render) {
        return 0;
    }

    XVisualInfo template;
    memset(&template, 0, sizeof(template));
    template.screen = app->screen;
    template.depth = 32;
    template.class = TrueColor;

    int count = 0;
    XVisualInfo *infos = XGetVisualInfo(app->display, VisualScreenMask | VisualDepthMask | VisualClassMask, &template, &count);
    if (!infos) {
        return 0;
    }

    for (int i = 0; i < count; i++) {
        XRenderPictFormat *format = XRenderFindVisualFormat(app->display, infos[i].visual);
        if (format && format->type == PictTypeDirect && format->direct.alphaMask) {
            app->overlay_visual = infos[i].visual;
            app->overlay_depth = infos[i].depth;
            app->overlay_format = format;
            app->overlay_colormap = XCreateColormap(app->display, app->root, app->overlay_visual, AllocNone);
            XFree(infos);
            return 1;
        }
    }

    XFree(infos);
    return 0;
}

static int app_init(App *app) {
    memset(app, 0, sizeof(*app));
    app->display = XOpenDisplay(NULL);
    if (!app->display) {
        fprintf(stderr, "modern-screenshot: cannot open X display\n");
        return -1;
    }

    app->screen = DefaultScreen(app->display);
    app->root = RootWindow(app->display, app->screen);
    app->root_w = DisplayWidth(app->display, app->screen);
    app->root_h = DisplayHeight(app->display, app->screen);
    app->root_visual = DefaultVisual(app->display, app->screen);
    app->root_depth = DefaultDepth(app->display, app->screen);
    app->overlay_visual = app->root_visual;
    app->overlay_depth = app->root_depth;
    app->overlay_colormap = DefaultColormap(app->display, app->screen);
    app->hotkey_code = XKeysymToKeycode(app->display, XK_1);

    int event_base = 0;
    int error_base = 0;
    app->has_render = XRenderQueryExtension(app->display, &event_base, &error_base);
    app->has_argb = find_argb_visual(app);
    if (!app->has_argb && app->has_render) {
        app->overlay_format = XRenderFindVisualFormat(app->display, app->overlay_visual);
    }

    app->font = XLoadQueryFont(app->display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso10646-1");
    if (!app->font) {
        app->font = XLoadQueryFont(app->display, "fixed");
    }
    return 0;
}

static void app_destroy(App *app) {
    if (!app->display) {
        return;
    }
    if (app->font) {
        XFreeFont(app->display, app->font);
    }
    if (app->has_argb && app->overlay_colormap) {
        XFreeColormap(app->display, app->overlay_colormap);
    }
    XCloseDisplay(app->display);
}

static void render_fill(Display *display, Picture picture, unsigned short r, unsigned short g, unsigned short b,
                        unsigned short a, int x, int y, unsigned int w, unsigned int h) {
    XRenderColor color = {r, g, b, a};
    XRenderFillRectangle(display, PictOpSrc, picture, &color, x, y, w, h);
}

static void draw_label(App *app, Drawable drawable, const char *label, int x, int y) {
    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    if (app->font) {
        XSetFont(app->display, gc, app->font->fid);
    }
    XSetForeground(app->display, gc, app->has_argb ? 0xffffffffUL : WhitePixel(app->display, app->screen));
    XDrawString(app->display, drawable, gc, x, y, label, (int)strlen(label));
    XFreeGC(app->display, gc);
}

static void draw_overlay_xrender(App *app, Window overlay, Rect selection, int has_selection) {
    Picture picture = XRenderCreatePicture(app->display, overlay, app->overlay_format, 0, NULL);
    if (!picture) {
        return;
    }

    render_fill(app->display, picture, 0, 0, 0, 0, 0, 0, app->root_w, app->root_h);
    render_fill(app->display, picture, 0x0500, 0x0700, 0x0900, 0x9900, 0, 0, app->root_w, app->root_h);

    if (has_selection && selection.w > 0 && selection.h > 0) {
        render_fill(app->display, picture, 0, 0, 0, 0, selection.x, selection.y, selection.w, selection.h);

        const int border = 2;
        const int handle = 9;
        render_fill(app->display, picture, 0x1a00, 0xd600, 0xbb00, 0xffff, selection.x, selection.y, selection.w, border);
        render_fill(app->display, picture, 0x1a00, 0xd600, 0xbb00, 0xffff, selection.x, selection.y + selection.h - border, selection.w, border);
        render_fill(app->display, picture, 0x1a00, 0xd600, 0xbb00, 0xffff, selection.x, selection.y, border, selection.h);
        render_fill(app->display, picture, 0x1a00, 0xd600, 0xbb00, 0xffff, selection.x + selection.w - border, selection.y, border, selection.h);

        int corners[4][2] = {
            {selection.x - handle / 2, selection.y - handle / 2},
            {selection.x + selection.w - handle / 2, selection.y - handle / 2},
            {selection.x - handle / 2, selection.y + selection.h - handle / 2},
            {selection.x + selection.w - handle / 2, selection.y + selection.h - handle / 2},
        };
        for (int i = 0; i < 4; i++) {
            render_fill(app->display, picture, 0xffff, 0xffff, 0xffff, 0xffff, corners[i][0], corners[i][1], handle, handle);
        }

        char label[64];
        snprintf(label, sizeof(label), "%d x %d", selection.w, selection.h);
        int label_w = 82;
        int label_h = 26;
        int label_x = clamp_int(selection.x, 10, app->root_w - label_w - 10);
        int label_y = selection.y - label_h - 10;
        if (label_y < 10) {
            label_y = selection.y + 10;
        }
        render_fill(app->display, picture, 0x0900, 0x0d00, 0x1200, 0xee00, label_x, label_y, label_w, label_h);
        XRenderFreePicture(app->display, picture);
        draw_label(app, overlay, label, label_x + 12, label_y + 18);
        XFlush(app->display);
        return;
    }

    XRenderFreePicture(app->display, picture);
    XFlush(app->display);
}

static void draw_overlay_fallback(App *app, Window overlay, Rect selection, int has_selection) {
    GC gc = XCreateGC(app->display, overlay, 0, NULL);
    XSetForeground(app->display, gc, BlackPixel(app->display, app->screen));
    XFillRectangle(app->display, overlay, gc, 0, 0, app->root_w, app->root_h);

    if (has_selection && selection.w > 0 && selection.h > 0) {
        XSetForeground(app->display, gc, WhitePixel(app->display, app->screen));
        XDrawRectangle(app->display, overlay, gc, selection.x, selection.y, selection.w, selection.h);
        char label[64];
        snprintf(label, sizeof(label), "%d x %d", selection.w, selection.h);
        draw_label(app, overlay, label, selection.x + 8, selection.y + 18);
    }

    XFreeGC(app->display, gc);
    XFlush(app->display);
}

static Window create_overlay(App *app) {
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.colormap = app->overlay_colormap;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | KeyPressMask;

    unsigned long mask = CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    Window overlay = XCreateWindow(app->display, app->root, 0, 0, app->root_w, app->root_h, 0, app->overlay_depth,
                                   InputOutput, app->overlay_visual, mask, &attrs);
    if (!overlay) {
        return 0;
    }

    Cursor cursor = XCreateFontCursor(app->display, XC_crosshair);
    if (cursor) {
        XDefineCursor(app->display, overlay, cursor);
        XFreeCursor(app->display, cursor);
    }

    XStoreName(app->display, overlay, "Modern Screenshot");
    XMapRaised(app->display, overlay);
    XSync(app->display, False);
    return overlay;
}

static void draw_overlay(App *app, Window overlay, Rect selection, int has_selection) {
    if (app->has_argb && app->overlay_format) {
        draw_overlay_xrender(app, overlay, selection, has_selection);
    } else {
        draw_overlay_fallback(app, overlay, selection, has_selection);
    }
}

static int write_capture_png(App *app, Rect rect, const char *path) {
    PngWriter writer;
    PixelFormat format = pixel_format_from_visual(app->root_visual);

    if (rect.w <= 0 || rect.h <= 0) {
        return -1;
    }

    if (png_begin(&writer, path, rect.w, rect.h) != 0) {
        return -1;
    }

    int rows_per_tile = MAX_CAPTURE_BUFFER / (rect.w * 4);
    if (rows_per_tile < 1) {
        rows_per_tile = 1;
    }
    if (rows_per_tile > 64) {
        rows_per_tile = 64;
    }

    XImage *image = XCreateImage(app->display, app->root_visual, (unsigned int)app->root_depth, ZPixmap, 0, NULL,
                                 (unsigned int)rect.w, (unsigned int)rows_per_tile, 32, 0);
    if (!image) {
        png_finish(&writer);
        return -1;
    }

    image->data = calloc((size_t)image->bytes_per_line, (size_t)rows_per_tile);
    uint8_t *png_row = malloc((size_t)rect.w * 3U + 1U);
    if (!image->data || !png_row) {
        free(png_row);
        XDestroyImage(image);
        png_finish(&writer);
        return -1;
    }

    for (int y = 0; y < rect.h; y += rows_per_tile) {
        int rows = rect.h - y;
        if (rows > rows_per_tile) {
            rows = rows_per_tile;
        }

        image->height = rows;
        memset(image->data, 0, (size_t)image->bytes_per_line * (size_t)rows_per_tile);
        if (!XGetSubImage(app->display, app->root, rect.x, rect.y + y, (unsigned int)rect.w, (unsigned int)rows,
                          AllPlanes, ZPixmap, image, 0, 0)) {
            XDestroyImage(image);
            free(png_row);
            png_finish(&writer);
            return -1;
        }

        for (int row = 0; row < rows; row++) {
            png_row[0] = 0;
            for (int x = 0; x < rect.w; x++) {
                unsigned long pixel = XGetPixel(image, x, row);
                size_t index = 1U + (size_t)x * 3U;
                png_row[index] = component_from_pixel(pixel, format.r);
                png_row[index + 1] = component_from_pixel(pixel, format.g);
                png_row[index + 2] = component_from_pixel(pixel, format.b);
            }

            int final_row = y + row == rect.h - 1;
            if (png_write_row(&writer, png_row, (size_t)rect.w * 3U + 1U, final_row) != 0) {
                XDestroyImage(image);
                free(png_row);
                png_finish(&writer);
                return -1;
            }
        }
    }

    XDestroyImage(image);
    free(png_row);
    return png_finish(&writer);
}

static void draw_toast(App *app, Window toast, int width, int height, const char *message) {
    if (app->has_argb && app->overlay_format) {
        Picture picture = XRenderCreatePicture(app->display, toast, app->overlay_format, 0, NULL);
        if (picture) {
            render_fill(app->display, picture, 0, 0, 0, 0, 0, 0, width, height);
            render_fill(app->display, picture, 0x0800, 0x0d00, 0x1100, 0xee00, 0, 0, width, height);
            render_fill(app->display, picture, 0x1a00, 0xd600, 0xbb00, 0xffff, 0, 0, 4, height);
            XRenderFreePicture(app->display, picture);
        }
    } else {
        GC gc = XCreateGC(app->display, toast, 0, NULL);
        XSetForeground(app->display, gc, BlackPixel(app->display, app->screen));
        XFillRectangle(app->display, toast, gc, 0, 0, width, height);
        XFreeGC(app->display, gc);
    }

    draw_label(app, toast, message, 18, 32);
    XFlush(app->display);
}

static void show_toast(App *app, const char *message) {
    const int width = 360;
    const int height = 52;
    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;
    attrs.background_pixel = 0;
    attrs.border_pixel = 0;
    attrs.colormap = app->overlay_colormap;
    attrs.event_mask = ExposureMask;

    Window toast = XCreateWindow(app->display, app->root, app->root_w - width - 18, 24, width, height, 0,
                                 app->overlay_depth, InputOutput, app->overlay_visual,
                                 CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &attrs);
    if (!toast) {
        return;
    }

    XMapRaised(app->display, toast);
    draw_toast(app, toast, width, height, message);
    sleep_ms(TOAST_MS);
    XDestroyWindow(app->display, toast);
    XFlush(app->display);
}

static int interactive_capture(App *app) {
    Window overlay = create_overlay(app);
    if (!overlay) {
        return -1;
    }

    if (XGrabPointer(app->display, overlay, False,
                     ButtonPressMask | ButtonReleaseMask | PointerMotionMask, GrabModeAsync, GrabModeAsync,
                     overlay, None, CurrentTime) != GrabSuccess) {
        XDestroyWindow(app->display, overlay);
        return -1;
    }
    XGrabKeyboard(app->display, overlay, False, GrabModeAsync, GrabModeAsync, CurrentTime);

    int selecting = 0;
    int done = 0;
    int cancelled = 0;
    int start_x = 0;
    int start_y = 0;
    Rect selection = {0, 0, 0, 0};

    draw_overlay(app, overlay, selection, 0);

    while (!done && keep_running) {
        XEvent event;
        XNextEvent(app->display, &event);

        switch (event.type) {
        case Expose:
            draw_overlay(app, overlay, selection, selecting);
            break;
        case KeyPress: {
            KeySym sym = XLookupKeysym(&event.xkey, 0);
            if (sym == XK_Escape) {
                cancelled = 1;
                done = 1;
            }
            break;
        }
        case ButtonPress:
            if (event.xbutton.button == Button1) {
                selecting = 1;
                start_x = event.xbutton.x;
                start_y = event.xbutton.y;
                selection = normalize_rect(start_x, start_y, start_x, start_y, app->root_w, app->root_h);
                draw_overlay(app, overlay, selection, 1);
            }
            break;
        case MotionNotify:
            if (selecting) {
                while (XCheckTypedWindowEvent(app->display, overlay, MotionNotify, &event)) {
                }
                selection = normalize_rect(start_x, start_y, event.xmotion.x, event.xmotion.y, app->root_w, app->root_h);
                draw_overlay(app, overlay, selection, 1);
            }
            break;
        case ButtonRelease:
            if (selecting && event.xbutton.button == Button1) {
                selection = normalize_rect(start_x, start_y, event.xbutton.x, event.xbutton.y, app->root_w, app->root_h);
                done = 1;
            }
            break;
        default:
            break;
        }
    }

    XUngrabKeyboard(app->display, CurrentTime);
    XUngrabPointer(app->display, CurrentTime);
    XUnmapWindow(app->display, overlay);
    XDestroyWindow(app->display, overlay);
    XSync(app->display, False);
    sleep_ms(60);

    if (cancelled || selection.w < 4 || selection.h < 4) {
        return 0;
    }

    char path[PATH_MAX];
    char display_path[PATH_MAX];
    if (build_output_path(path, sizeof(path), display_path, sizeof(display_path)) != 0) {
        show_toast(app, "Save failed");
        return -1;
    }

    if (write_capture_png(app, selection, path) != 0) {
        show_toast(app, "Save failed");
        return -1;
    }

    char message[256];
    snprintf(message, sizeof(message), "Saved %.220s", display_path);
    show_toast(app, message);
    return 0;
}

static int capture_root(App *app, const char *path) {
    Rect rect = {0, 0, app->root_w, app->root_h};
    return write_capture_png(app, rect, path);
}

static int grab_hotkey(App *app) {
    unsigned int masks[] = {
        ControlMask,
        ControlMask | LockMask,
        ControlMask | Mod2Mask,
        ControlMask | LockMask | Mod2Mask,
    };

    int (*old_handler)(Display *, XErrorEvent *) = XSetErrorHandler(x_error_handler);
    grab_failed = 0;
    for (size_t i = 0; i < sizeof(masks) / sizeof(masks[0]); i++) {
        XGrabKey(app->display, app->hotkey_code, masks[i], app->root, False, GrabModeAsync, GrabModeAsync);
    }
    XSync(app->display, False);
    XSetErrorHandler(old_handler);

    if (grab_failed) {
        fprintf(stderr, "modern-screenshot: hotkey %s is already in use\n", HOTKEY_KEYS);
        return -1;
    }
    return 0;
}

static int run_daemon(App *app) {
    if (grab_hotkey(app) != 0) {
        return -1;
    }

    fprintf(stdout, "modern-screenshot %s listening on %s\n", VERSION, HOTKEY_KEYS);
    fflush(stdout);

    while (keep_running) {
        XEvent event;
        XNextEvent(app->display, &event);
        if (event.type != KeyPress) {
            continue;
        }

        XKeyEvent *key = &event.xkey;
        if ((int)key->keycode == app->hotkey_code && (key->state & ControlMask)) {
            interactive_capture(app);
        }
    }

    return 0;
}

static void usage(FILE *stream) {
    fprintf(stream,
            "modern-screenshot %s\n"
            "Usage:\n"
            "  modern-screenshot             Run hotkey daemon (%s)\n"
            "  modern-screenshot --once      Start one interactive capture\n"
            "  modern-screenshot --capture-root PATH\n"
            "  modern-screenshot --version\n",
            VERSION, HOTKEY_KEYS);
}

int main(int argc, char **argv) {
    if (argc > 1) {
        if (strcmp(argv[1], "--version") == 0) {
            printf("modern-screenshot %s\n", VERSION);
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            usage(stdout);
            return 0;
        }
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    App app;
    if (app_init(&app) != 0) {
        return 1;
    }

    int status = 0;
    if (argc > 1 && strcmp(argv[1], "--once") == 0) {
        status = interactive_capture(&app) == 0 ? 0 : 1;
    } else if (argc > 2 && strcmp(argv[1], "--capture-root") == 0) {
        status = capture_root(&app, argv[2]) == 0 ? 0 : 1;
    } else if (argc == 1) {
        status = run_daemon(&app) == 0 ? 0 : 1;
    } else {
        usage(stderr);
        status = 2;
    }

    app_destroy(&app);
    return status;
}
