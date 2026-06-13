#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xrender.h>
#include <X11/keysym.h>
#include <X11/Xatom.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
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
#define MAX_ANNOTATIONS 160
#define MAX_TEXT_LEN 128
#define TOOLBAR_H 52
#define BUTTON_W 64
#define BUTTON_H 36
#define BUTTON_GAP 8
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
    Colormap root_colormap;
    unsigned long color_bg;
    unsigned long color_panel;
    unsigned long color_panel_active;
    unsigned long color_border;
    unsigned long color_accent;
    unsigned long color_accent_2;
    unsigned long color_text;
    unsigned long color_muted;
    Atom atom_clipboard;
    Atom atom_targets;
    Atom atom_png;
    Atom atom_utf8;
    Atom atom_uri_list;
    Window clipboard_window;
    char clipboard_path[PATH_MAX];
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

typedef enum {
    TOOL_BLUR,
    TOOL_ARROW,
    TOOL_RECT,
    TOOL_TEXT
} Tool;

typedef enum {
    ACTION_NONE,
    ACTION_TOOL_BLUR,
    ACTION_TOOL_ARROW,
    ACTION_TOOL_RECT,
    ACTION_TOOL_TEXT,
    ACTION_UNDO,
    ACTION_COPY,
    ACTION_SAVE
} ButtonAction;

typedef struct {
    Tool tool;
    int x1;
    int y1;
    int x2;
    int y2;
    char text[MAX_TEXT_LEN];
} Annotation;

typedef struct {
    App *app;
    Window window;
    Pixmap shot;
    Rect rect;
    Tool active_tool;
    Annotation ops[MAX_ANNOTATIONS];
    int op_count;
    int drawing;
    Annotation preview;
    int text_entry;
    int text_x;
    int text_y;
    char text_buffer[MAX_TEXT_LEN];
    int text_len;
    int win_w;
    int win_h;
    int image_x;
    int image_y;
    int done;
    int saved;
    int copied;
    char saved_display_path[PATH_MAX];
} Editor;

typedef struct {
    ButtonAction action;
    Tool tool;
    const char *label;
} ButtonSpec;

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

static Rect annotation_rect(const Annotation *annotation, int max_w, int max_h) {
    return normalize_rect(annotation->x1, annotation->y1, annotation->x2, annotation->y2, max_w, max_h);
}

static unsigned long alloc_named_color(App *app, const char *name, unsigned long fallback) {
    XColor color;
    if (XParseColor(app->display, app->root_colormap, name, &color) &&
        XAllocColor(app->display, app->root_colormap, &color)) {
        return color.pixel;
    }
    return fallback;
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
    app->root_colormap = DefaultColormap(app->display, app->screen);
    app->overlay_visual = app->root_visual;
    app->overlay_depth = app->root_depth;
    app->overlay_colormap = app->root_colormap;
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

    app->color_bg = alloc_named_color(app, "#101418", BlackPixel(app->display, app->screen));
    app->color_panel = alloc_named_color(app, "#161d22", BlackPixel(app->display, app->screen));
    app->color_panel_active = alloc_named_color(app, "#213138", BlackPixel(app->display, app->screen));
    app->color_border = alloc_named_color(app, "#3b4a50", WhitePixel(app->display, app->screen));
    app->color_accent = alloc_named_color(app, "#17c6a3", WhitePixel(app->display, app->screen));
    app->color_accent_2 = alloc_named_color(app, "#ffcf5a", WhitePixel(app->display, app->screen));
    app->color_text = alloc_named_color(app, "#f5fbf8", WhitePixel(app->display, app->screen));
    app->color_muted = alloc_named_color(app, "#94a3a2", WhitePixel(app->display, app->screen));

    app->atom_clipboard = XInternAtom(app->display, "CLIPBOARD", False);
    app->atom_targets = XInternAtom(app->display, "TARGETS", False);
    app->atom_png = XInternAtom(app->display, "image/png", False);
    app->atom_utf8 = XInternAtom(app->display, "UTF8_STRING", False);
    app->atom_uri_list = XInternAtom(app->display, "text/uri-list", False);
    app->clipboard_window = XCreateSimpleWindow(app->display, app->root, 0, 0, 1, 1, 0, 0, 0);
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
    if (app->clipboard_window) {
        XDestroyWindow(app->display, app->clipboard_window);
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

static void draw_label_color(App *app, Drawable drawable, const char *label, int x, int y, unsigned long color) {
    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    if (app->font) {
        XSetFont(app->display, gc, app->font->fid);
    }
    XSetForeground(app->display, gc, color);
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

static int write_drawable_png(App *app, Drawable source, Rect rect, const char *path) {
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
        if (!XGetSubImage(app->display, source, rect.x, rect.y + y, (unsigned int)rect.w, (unsigned int)rows,
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

static int write_capture_png(App *app, Rect rect, const char *path) {
    return write_drawable_png(app, app->root, rect, path);
}

static const ButtonSpec editor_buttons[] = {
    {ACTION_TOOL_BLUR, TOOL_BLUR, "Blur"},
    {ACTION_TOOL_ARROW, TOOL_ARROW, "->"},
    {ACTION_TOOL_RECT, TOOL_RECT, "Box"},
    {ACTION_TOOL_TEXT, TOOL_TEXT, "Text"},
    {ACTION_UNDO, TOOL_BLUR, "Undo"},
    {ACTION_COPY, TOOL_BLUR, "Copy"},
    {ACTION_SAVE, TOOL_BLUR, "Save"},
};

static int editor_button_count(void) {
    return (int)(sizeof(editor_buttons) / sizeof(editor_buttons[0]));
}

static int editor_toolbar_width(void) {
    return 24 + editor_button_count() * BUTTON_W + (editor_button_count() - 1) * BUTTON_GAP;
}

static Rect editor_button_rect(int index) {
    Rect rect;
    rect.x = 12 + index * (BUTTON_W + BUTTON_GAP);
    rect.y = 8;
    rect.w = BUTTON_W;
    rect.h = BUTTON_H;
    return rect;
}

static int editor_point_in_rect(int x, int y, Rect rect) {
    return x >= rect.x && y >= rect.y && x < rect.x + rect.w && y < rect.y + rect.h;
}

static int editor_button_at(int x, int y, Tool *tool) {
    for (int i = 0; i < editor_button_count(); i++) {
        if (editor_point_in_rect(x, y, editor_button_rect(i))) {
            *tool = editor_buttons[i].tool;
            return editor_buttons[i].action;
        }
    }
    return ACTION_NONE;
}

static int editor_image_x(Editor *editor, int window_x) {
    return window_x - editor->image_x;
}

static int editor_image_y(Editor *editor, int window_y) {
    return window_y - editor->image_y;
}

static int editor_point_in_image(Editor *editor, int x, int y) {
    return x >= editor->image_x && y >= editor->image_y &&
           x < editor->image_x + editor->rect.w && y < editor->image_y + editor->rect.h;
}

static void editor_draw_button(Editor *editor, Drawable drawable, int index) {
    App *app = editor->app;
    Rect rect = editor_button_rect(index);
    int is_tool = editor_buttons[index].action == ACTION_TOOL_BLUR ||
                  editor_buttons[index].action == ACTION_TOOL_ARROW ||
                  editor_buttons[index].action == ACTION_TOOL_RECT ||
                  editor_buttons[index].action == ACTION_TOOL_TEXT;
    int active = is_tool && editor_buttons[index].tool == editor->active_tool;

    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSetForeground(app->display, gc, active ? app->color_panel_active : app->color_panel);
    XFillRectangle(app->display, drawable, gc, rect.x, rect.y, (unsigned int)rect.w, (unsigned int)rect.h);
    XSetForeground(app->display, gc, active ? app->color_accent : app->color_border);
    XDrawRectangle(app->display, drawable, gc, rect.x, rect.y, (unsigned int)(rect.w - 1), (unsigned int)(rect.h - 1));
    XFreeGC(app->display, gc);

    int text_w = app->font ? XTextWidth(app->font, editor_buttons[index].label,
                                        (int)strlen(editor_buttons[index].label)) : 28;
    int text_x = rect.x + (rect.w - text_w) / 2;
    int text_y = rect.y + 23;
    draw_label_color(app, drawable, editor_buttons[index].label, text_x, text_y,
                     active ? app->color_accent : app->color_text);
}

static void editor_draw_toolbar(Editor *editor, Drawable drawable) {
    App *app = editor->app;
    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSetForeground(app->display, gc, app->color_bg);
    XFillRectangle(app->display, drawable, gc, 0, 0, (unsigned int)editor->win_w, TOOLBAR_H);
    XSetForeground(app->display, gc, app->color_border);
    XDrawLine(app->display, drawable, gc, 0, TOOLBAR_H - 1, editor->win_w, TOOLBAR_H - 1);
    XFreeGC(app->display, gc);

    for (int i = 0; i < editor_button_count(); i++) {
        editor_draw_button(editor, drawable, i);
    }

    char info[64];
    snprintf(info, sizeof(info), "%d x %d", editor->rect.w, editor->rect.h);
    int info_w = app->font ? XTextWidth(app->font, info, (int)strlen(info)) : 48;
    if (editor->win_w - info_w > editor_toolbar_width() + 16) {
        draw_label_color(app, drawable, info, editor->win_w - info_w - 12, 30, app->color_muted);
    }
}

static void editor_draw_rect_outline(App *app, Drawable drawable, Rect rect, unsigned long color, int width) {
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }

    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSetForeground(app->display, gc, color);
    XSetLineAttributes(app->display, gc, (unsigned int)width, LineSolid, CapRound, JoinRound);
    XDrawRectangle(app->display, drawable, gc, rect.x, rect.y, (unsigned int)rect.w, (unsigned int)rect.h);
    XFreeGC(app->display, gc);
}

static void editor_draw_arrow(App *app, Drawable drawable, int x1, int y1, int x2, int y2, unsigned long color) {
    double dx = (double)(x2 - x1);
    double dy = (double)(y2 - y1);
    double len = sqrt(dx * dx + dy * dy);
    if (len < 2.0) {
        return;
    }

    double angle = atan2(dy, dx);
    double head_len = 16.0;
    double head_angle = 0.55;
    int hx1 = x2 - (int)(cos(angle - head_angle) * head_len);
    int hy1 = y2 - (int)(sin(angle - head_angle) * head_len);
    int hx2 = x2 - (int)(cos(angle + head_angle) * head_len);
    int hy2 = y2 - (int)(sin(angle + head_angle) * head_len);

    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSetForeground(app->display, gc, color);
    XSetLineAttributes(app->display, gc, 4, LineSolid, CapRound, JoinRound);
    XDrawLine(app->display, drawable, gc, x1, y1, x2, y2);
    XDrawLine(app->display, drawable, gc, x2, y2, hx1, hy1);
    XDrawLine(app->display, drawable, gc, x2, y2, hx2, hy2);
    XFreeGC(app->display, gc);
}

static void editor_draw_text(App *app, Drawable drawable, int x, int y, const char *text) {
    if (!text || !*text) {
        return;
    }

    int len = (int)strlen(text);
    int text_w = app->font ? XTextWidth(app->font, text, len) : len * 8;
    int text_h = app->font ? app->font->ascent + app->font->descent : 14;
    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSetForeground(app->display, gc, app->color_bg);
    XFillRectangle(app->display, drawable, gc, x - 6, y - text_h - 5,
                   (unsigned int)(text_w + 12), (unsigned int)(text_h + 10));
    XSetForeground(app->display, gc, app->color_accent_2);
    if (app->font) {
        XSetFont(app->display, gc, app->font->fid);
    }
    XDrawString(app->display, drawable, gc, x, y, text, len);
    XFreeGC(app->display, gc);
}

static void editor_apply_mosaic(App *app, Drawable drawable, Rect rect) {
    const int block = 12;
    if (rect.w <= 0 || rect.h <= 0) {
        return;
    }

    GC gc = XCreateGC(app->display, drawable, 0, NULL);
    XSync(app->display, False);

    for (int y = rect.y; y < rect.y + rect.h; y += block) {
        int bh = rect.y + rect.h - y;
        if (bh > block) {
            bh = block;
        }
        for (int x = rect.x; x < rect.x + rect.w; x += block) {
            int bw = rect.x + rect.w - x;
            if (bw > block) {
                bw = block;
            }
            int sx = x + bw / 2;
            int sy = y + bh / 2;
            XImage *sample = XGetImage(app->display, drawable, sx, sy, 1, 1, AllPlanes, ZPixmap);
            if (!sample) {
                continue;
            }
            XSetForeground(app->display, gc, XGetPixel(sample, 0, 0));
            XDestroyImage(sample);
            XFillRectangle(app->display, drawable, gc, x, y, (unsigned int)bw, (unsigned int)bh);
        }
    }

    XSetForeground(app->display, gc, app->color_accent_2);
    XSetLineAttributes(app->display, gc, 2, LineSolid, CapRound, JoinRound);
    XDrawRectangle(app->display, drawable, gc, rect.x, rect.y, (unsigned int)rect.w, (unsigned int)rect.h);
    XFreeGC(app->display, gc);
}

static void editor_draw_annotation(Editor *editor, Drawable drawable, const Annotation *annotation,
                                   int offset_x, int offset_y) {
    App *app = editor->app;
    Annotation shifted = *annotation;
    shifted.x1 += offset_x;
    shifted.y1 += offset_y;
    shifted.x2 += offset_x;
    shifted.y2 += offset_y;

    if (annotation->tool == TOOL_BLUR) {
        Rect rect = annotation_rect(&shifted, editor->win_w, editor->win_h);
        editor_apply_mosaic(app, drawable, rect);
    } else if (annotation->tool == TOOL_RECT) {
        Rect rect = annotation_rect(&shifted, editor->win_w, editor->win_h);
        editor_draw_rect_outline(app, drawable, rect, app->color_accent, 3);
    } else if (annotation->tool == TOOL_ARROW) {
        editor_draw_arrow(app, drawable, shifted.x1, shifted.y1, shifted.x2, shifted.y2, app->color_accent_2);
    } else if (annotation->tool == TOOL_TEXT) {
        editor_draw_text(app, drawable, shifted.x1, shifted.y1, shifted.text);
    }
}

static void editor_redraw(Editor *editor) {
    App *app = editor->app;
    GC gc = XCreateGC(app->display, editor->window, 0, NULL);
    XSetForeground(app->display, gc, app->color_bg);
    XFillRectangle(app->display, editor->window, gc, 0, 0,
                   (unsigned int)editor->win_w, (unsigned int)editor->win_h);
    XCopyArea(app->display, editor->shot, editor->window, gc, 0, 0,
              (unsigned int)editor->rect.w, (unsigned int)editor->rect.h,
              editor->image_x, editor->image_y);
    XSetForeground(app->display, gc, app->color_border);
    XDrawRectangle(app->display, editor->window, gc, editor->image_x, editor->image_y,
                   (unsigned int)editor->rect.w, (unsigned int)editor->rect.h);
    XFreeGC(app->display, gc);

    for (int i = 0; i < editor->op_count; i++) {
        editor_draw_annotation(editor, editor->window, &editor->ops[i], editor->image_x, editor->image_y);
    }
    if (editor->drawing) {
        editor_draw_annotation(editor, editor->window, &editor->preview, editor->image_x, editor->image_y);
    }
    if (editor->text_entry) {
        char preview[MAX_TEXT_LEN + 2];
        snprintf(preview, sizeof(preview), "%s_", editor->text_buffer);
        Annotation text = {TOOL_TEXT, editor->text_x, editor->text_y, editor->text_x, editor->text_y, {0}};
        size_t preview_len = strlen(preview);
        if (preview_len >= sizeof(text.text)) {
            preview_len = sizeof(text.text) - 1;
        }
        memcpy(text.text, preview, preview_len);
        text.text[preview_len] = '\0';
        editor_draw_annotation(editor, editor->window, &text, editor->image_x, editor->image_y);
    }

    editor_draw_toolbar(editor, editor->window);
    XFlush(app->display);
}

static void editor_add_annotation(Editor *editor, const Annotation *annotation) {
    if (editor->op_count >= MAX_ANNOTATIONS) {
        return;
    }
    editor->ops[editor->op_count++] = *annotation;
}

static Pixmap editor_compose_pixmap(Editor *editor) {
    App *app = editor->app;
    Pixmap output = XCreatePixmap(app->display, app->root, (unsigned int)editor->rect.w,
                                  (unsigned int)editor->rect.h, (unsigned int)app->root_depth);
    if (!output) {
        return 0;
    }

    GC gc = XCreateGC(app->display, output, 0, NULL);
    XCopyArea(app->display, editor->shot, output, gc, 0, 0,
              (unsigned int)editor->rect.w, (unsigned int)editor->rect.h, 0, 0);
    XFreeGC(app->display, gc);

    int old_win_w = editor->win_w;
    int old_win_h = editor->win_h;
    editor->win_w = editor->rect.w;
    editor->win_h = editor->rect.h;
    for (int i = 0; i < editor->op_count; i++) {
        editor_draw_annotation(editor, output, &editor->ops[i], 0, 0);
    }
    editor->win_w = old_win_w;
    editor->win_h = old_win_h;
    XSync(app->display, False);
    return output;
}

static int editor_export_png(Editor *editor, const char *path) {
    Pixmap output = editor_compose_pixmap(editor);
    if (!output) {
        return -1;
    }

    Rect rect = {0, 0, editor->rect.w, editor->rect.h};
    int status = write_drawable_png(editor->app, output, rect, path);
    XFreePixmap(editor->app->display, output);
    return status;
}

static int editor_temp_path(char *path, size_t path_size) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) {
        tmp = "/tmp";
    }
    return snprintf(path, path_size, "%s/modern-screenshot-%ld.png", tmp, (long)getpid()) < (int)path_size ? 0 : -1;
}

static int editor_read_file(const char *path, unsigned char **data, size_t *size) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        return -1;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return -1;
    }
    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    unsigned char *buffer = malloc((size_t)len);
    if (!buffer && len > 0) {
        fclose(fp);
        return -1;
    }
    if (len > 0 && fread(buffer, 1, (size_t)len, fp) != (size_t)len) {
        free(buffer);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    *data = buffer;
    *size = (size_t)len;
    return 0;
}

static void handle_clipboard_request(App *app, XSelectionRequestEvent *request) {
    XSelectionEvent reply;
    memset(&reply, 0, sizeof(reply));
    reply.type = SelectionNotify;
    reply.display = request->display;
    reply.requestor = request->requestor;
    reply.selection = request->selection;
    reply.target = request->target;
    reply.time = request->time;
    reply.property = None;

    Atom property = request->property ? request->property : request->target;
    if (!app->clipboard_path[0]) {
        XSendEvent(app->display, request->requestor, False, 0, (XEvent *)&reply);
        XFlush(app->display);
        return;
    }

    if (request->target == app->atom_targets) {
        Atom targets[] = {app->atom_targets, app->atom_png, app->atom_utf8, app->atom_uri_list, XA_STRING};
        XChangeProperty(app->display, request->requestor, property, XA_ATOM, 32, PropModeReplace,
                        (const unsigned char *)targets, (int)(sizeof(targets) / sizeof(targets[0])));
        reply.property = property;
    } else if (request->target == app->atom_png) {
        unsigned char *data = NULL;
        size_t size = 0;
        if (editor_read_file(app->clipboard_path, &data, &size) == 0) {
            XChangeProperty(app->display, request->requestor, property, app->atom_png, 8, PropModeReplace,
                            data, (int)size);
            reply.property = property;
            free(data);
        }
    } else if (request->target == app->atom_utf8 || request->target == XA_STRING) {
        XChangeProperty(app->display, request->requestor, property, request->target, 8, PropModeReplace,
                        (const unsigned char *)app->clipboard_path, (int)strlen(app->clipboard_path));
        reply.property = property;
    } else if (request->target == app->atom_uri_list) {
        char uri[PATH_MAX + 16];
        snprintf(uri, sizeof(uri), "file://%s\n", app->clipboard_path);
        XChangeProperty(app->display, request->requestor, property, app->atom_uri_list, 8, PropModeReplace,
                        (const unsigned char *)uri, (int)strlen(uri));
        reply.property = property;
    }

    XSendEvent(app->display, request->requestor, False, 0, (XEvent *)&reply);
    XFlush(app->display);
}

static void handle_clipboard_event(App *app, XEvent *event) {
    if (event->type == SelectionRequest) {
        handle_clipboard_request(app, &event->xselectionrequest);
    } else if (event->type == SelectionClear && event->xselectionclear.selection == app->atom_clipboard) {
        app->clipboard_path[0] = '\0';
    }
}

static Pixmap capture_selection_pixmap(App *app, Rect rect) {
    Pixmap pixmap = XCreatePixmap(app->display, app->root, (unsigned int)rect.w,
                                  (unsigned int)rect.h, (unsigned int)app->root_depth);
    if (!pixmap) {
        return 0;
    }

    GC gc = XCreateGC(app->display, pixmap, 0, NULL);
    XCopyArea(app->display, app->root, pixmap, gc, rect.x, rect.y,
              (unsigned int)rect.w, (unsigned int)rect.h, 0, 0);
    XFreeGC(app->display, gc);
    XSync(app->display, False);
    return pixmap;
}

static int editor_save(Editor *editor) {
    char path[PATH_MAX];
    char display_path[PATH_MAX];
    if (build_output_path(path, sizeof(path), display_path, sizeof(display_path)) != 0) {
        return -1;
    }
    if (editor_export_png(editor, path) != 0) {
        return -1;
    }
    snprintf(editor->saved_display_path, sizeof(editor->saved_display_path), "%s", display_path);
    editor->saved = 1;
    editor->done = 1;
    return 0;
}

static int editor_copy(Editor *editor) {
    App *app = editor->app;
    char path[PATH_MAX];
    if (editor_temp_path(path, sizeof(path)) != 0) {
        return -1;
    }
    if (editor_export_png(editor, path) != 0) {
        return -1;
    }

    snprintf(app->clipboard_path, sizeof(app->clipboard_path), "%s", path);
    XSetSelectionOwner(app->display, app->atom_clipboard, app->clipboard_window, CurrentTime);
    if (XGetSelectionOwner(app->display, app->atom_clipboard) != app->clipboard_window) {
        app->clipboard_path[0] = '\0';
        return -1;
    }

    editor->copied = 1;
    editor->done = 1;
    return 0;
}

static void editor_handle_action(Editor *editor, ButtonAction action, Tool tool) {
    if (action == ACTION_TOOL_BLUR || action == ACTION_TOOL_ARROW ||
        action == ACTION_TOOL_RECT || action == ACTION_TOOL_TEXT) {
        editor->active_tool = tool;
        editor->text_entry = 0;
    } else if (action == ACTION_UNDO) {
        if (editor->op_count > 0) {
            editor->op_count--;
        }
        editor->text_entry = 0;
    } else if (action == ACTION_SAVE) {
        editor_save(editor);
    } else if (action == ACTION_COPY) {
        editor_copy(editor);
    }
}

static void editor_commit_text(Editor *editor) {
    if (editor->text_len <= 0) {
        editor->text_entry = 0;
        return;
    }

    Annotation annotation;
    memset(&annotation, 0, sizeof(annotation));
    annotation.tool = TOOL_TEXT;
    annotation.x1 = editor->text_x;
    annotation.y1 = editor->text_y;
    annotation.x2 = editor->text_x;
    annotation.y2 = editor->text_y;
    snprintf(annotation.text, sizeof(annotation.text), "%s", editor->text_buffer);
    editor_add_annotation(editor, &annotation);
    editor->text_entry = 0;
    editor->text_len = 0;
    editor->text_buffer[0] = '\0';
}

static void editor_handle_text_key(Editor *editor, XKeyEvent *key) {
    KeySym sym = NoSymbol;
    char buffer[32];
    int count = XLookupString(key, buffer, sizeof(buffer), &sym, NULL);

    if (sym == XK_Return || sym == XK_KP_Enter) {
        editor_commit_text(editor);
    } else if (sym == XK_Escape) {
        editor->text_entry = 0;
        editor->text_len = 0;
        editor->text_buffer[0] = '\0';
    } else if (sym == XK_BackSpace) {
        if (editor->text_len > 0) {
            editor->text_buffer[--editor->text_len] = '\0';
        }
    } else if (count > 0) {
        for (int i = 0; i < count && editor->text_len < MAX_TEXT_LEN - 1; i++) {
            unsigned char ch = (unsigned char)buffer[i];
            if (ch >= 32 && ch < 127) {
                editor->text_buffer[editor->text_len++] = (char)ch;
                editor->text_buffer[editor->text_len] = '\0';
            }
        }
    }
}

static void editor_handle_key(Editor *editor, XKeyEvent *key) {
    if (editor->text_entry) {
        editor_handle_text_key(editor, key);
        return;
    }

    KeySym sym = XLookupKeysym(key, 0);
    int ctrl = (key->state & ControlMask) != 0;
    if (sym == XK_Escape) {
        editor->done = 1;
    } else if (ctrl && (sym == XK_z || sym == XK_Z)) {
        if (editor->op_count > 0) {
            editor->op_count--;
        }
    } else if (ctrl && (sym == XK_s || sym == XK_S)) {
        editor_save(editor);
    } else if (ctrl && (sym == XK_c || sym == XK_C)) {
        editor_copy(editor);
    } else if (sym == XK_b || sym == XK_B) {
        editor->active_tool = TOOL_BLUR;
    } else if (sym == XK_a || sym == XK_A) {
        editor->active_tool = TOOL_ARROW;
    } else if (sym == XK_r || sym == XK_R) {
        editor->active_tool = TOOL_RECT;
    } else if (sym == XK_t || sym == XK_T) {
        editor->active_tool = TOOL_TEXT;
    }
}

static void editor_finish_drawing(Editor *editor, int x, int y) {
    editor->preview.x2 = clamp_int(x, 0, editor->rect.w - 1);
    editor->preview.y2 = clamp_int(y, 0, editor->rect.h - 1);

    Rect rect = annotation_rect(&editor->preview, editor->rect.w, editor->rect.h);
    int keep = 0;
    if (editor->preview.tool == TOOL_ARROW) {
        int dx = editor->preview.x2 - editor->preview.x1;
        int dy = editor->preview.y2 - editor->preview.y1;
        keep = dx * dx + dy * dy > 25;
    } else {
        keep = rect.w >= 4 && rect.h >= 4;
    }

    if (keep) {
        editor_add_annotation(editor, &editor->preview);
    }
    editor->drawing = 0;
}

static Window editor_create_window(Editor *editor) {
    App *app = editor->app;
    int toolbar_w = editor_toolbar_width();
    editor->win_w = editor->rect.w > toolbar_w ? editor->rect.w : toolbar_w;
    editor->win_h = editor->rect.h + TOOLBAR_H;
    editor->image_x = (editor->win_w - editor->rect.w) / 2;
    editor->image_y = TOOLBAR_H;

    int max_x = app->root_w - editor->win_w;
    int max_y = app->root_h - editor->win_h;
    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }

    int win_x = clamp_int(editor->rect.x, 0, max_x);
    int preferred_y = editor->rect.y >= TOOLBAR_H ? editor->rect.y - TOOLBAR_H : editor->rect.y;
    int win_y = clamp_int(preferred_y, 0, max_y);

    XSetWindowAttributes attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.override_redirect = True;
    attrs.background_pixel = app->color_bg;
    attrs.border_pixel = app->color_border;
    attrs.colormap = app->root_colormap;
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | KeyPressMask | StructureNotifyMask;

    return XCreateWindow(app->display, app->root, win_x, win_y,
                         (unsigned int)editor->win_w, (unsigned int)editor->win_h, 0,
                         (unsigned int)app->root_depth, InputOutput, app->root_visual,
                         CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWColormap | CWEventMask, &attrs);
}

static int run_editor(App *app, Pixmap shot, Rect selection, int *saved, int *copied, char *display_path,
                      size_t display_path_size) {
    Editor editor;
    memset(&editor, 0, sizeof(editor));
    editor.app = app;
    editor.shot = shot;
    editor.rect = selection;
    editor.active_tool = TOOL_RECT;

    editor.window = editor_create_window(&editor);
    if (!editor.window) {
        return -1;
    }

    XStoreName(app->display, editor.window, "Modern Screenshot Editor");
    XMapRaised(app->display, editor.window);
    XSetInputFocus(app->display, editor.window, RevertToParent, CurrentTime);
    XSync(app->display, False);
    editor_redraw(&editor);

    while (!editor.done && keep_running) {
        XEvent event;
        XNextEvent(app->display, &event);

        if (event.type == SelectionRequest || event.type == SelectionClear) {
            handle_clipboard_event(app, &event);
            continue;
        }
        if (event.xany.window != editor.window) {
            continue;
        }

        switch (event.type) {
        case Expose:
            editor_redraw(&editor);
            break;
        case KeyPress:
            editor_handle_key(&editor, &event.xkey);
            editor_redraw(&editor);
            break;
        case ButtonPress: {
            Tool tool = TOOL_BLUR;
            ButtonAction action = editor_button_at(event.xbutton.x, event.xbutton.y, &tool);
            if (action != ACTION_NONE) {
                editor_handle_action(&editor, action, tool);
                editor_redraw(&editor);
                break;
            }
            if (!editor_point_in_image(&editor, event.xbutton.x, event.xbutton.y)) {
                break;
            }

            int ix = clamp_int(editor_image_x(&editor, event.xbutton.x), 0, editor.rect.w - 1);
            int iy = clamp_int(editor_image_y(&editor, event.xbutton.y), 0, editor.rect.h - 1);
            if (editor.active_tool == TOOL_TEXT) {
                editor.text_entry = 1;
                editor.text_x = ix;
                editor.text_y = iy;
                editor.text_len = 0;
                editor.text_buffer[0] = '\0';
            } else {
                memset(&editor.preview, 0, sizeof(editor.preview));
                editor.preview.tool = editor.active_tool;
                editor.preview.x1 = ix;
                editor.preview.y1 = iy;
                editor.preview.x2 = ix;
                editor.preview.y2 = iy;
                editor.drawing = 1;
            }
            editor_redraw(&editor);
            break;
        }
        case MotionNotify:
            if (editor.drawing) {
                while (XCheckTypedWindowEvent(app->display, editor.window, MotionNotify, &event)) {
                }
                int ix = clamp_int(editor_image_x(&editor, event.xmotion.x), 0, editor.rect.w - 1);
                int iy = clamp_int(editor_image_y(&editor, event.xmotion.y), 0, editor.rect.h - 1);
                editor.preview.x2 = ix;
                editor.preview.y2 = iy;
                editor_redraw(&editor);
            }
            break;
        case ButtonRelease:
            if (editor.drawing) {
                int ix = clamp_int(editor_image_x(&editor, event.xbutton.x), 0, editor.rect.w - 1);
                int iy = clamp_int(editor_image_y(&editor, event.xbutton.y), 0, editor.rect.h - 1);
                editor_finish_drawing(&editor, ix, iy);
                editor_redraw(&editor);
            }
            break;
        default:
            break;
        }
    }

    XDestroyWindow(app->display, editor.window);
    XFlush(app->display);

    *saved = editor.saved;
    *copied = editor.copied;
    if (editor.saved_display_path[0]) {
        snprintf(display_path, display_path_size, "%s", editor.saved_display_path);
    } else {
        display_path[0] = '\0';
    }
    return 0;
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

    Pixmap shot = capture_selection_pixmap(app, selection);
    if (!shot) {
        show_toast(app, "Capture failed");
        return -1;
    }

    int saved = 0;
    int copied = 0;
    char display_path[PATH_MAX];
    if (run_editor(app, shot, selection, &saved, &copied, display_path, sizeof(display_path)) != 0) {
        XFreePixmap(app->display, shot);
        show_toast(app, "Editor failed");
        return -1;
    }
    XFreePixmap(app->display, shot);

    if (saved) {
        char message[256];
        snprintf(message, sizeof(message), "Saved %.220s", display_path);
        show_toast(app, message);
    } else if (copied) {
        show_toast(app, "Copied PNG to clipboard");
    }
    return 0;
}

static int capture_root(App *app, const char *path) {
    Rect rect = {0, 0, app->root_w, app->root_h};
    return write_capture_png(app, rect, path);
}

static int self_test_export(App *app, const char *path) {
    Rect rect = {0, 0, 320, 180};
    Pixmap shot = XCreatePixmap(app->display, app->root, (unsigned int)rect.w,
                                (unsigned int)rect.h, (unsigned int)app->root_depth);
    if (!shot) {
        return -1;
    }

    GC gc = XCreateGC(app->display, shot, 0, NULL);
    XSetForeground(app->display, gc, app->color_panel);
    XFillRectangle(app->display, shot, gc, 0, 0, (unsigned int)rect.w, (unsigned int)rect.h);
    XSetForeground(app->display, gc, app->color_border);
    for (int x = 0; x < rect.w; x += 24) {
        XDrawLine(app->display, shot, gc, x, 0, x, rect.h);
    }
    for (int y = 0; y < rect.h; y += 24) {
        XDrawLine(app->display, shot, gc, 0, y, rect.w, y);
    }
    XFreeGC(app->display, gc);

    Editor editor;
    memset(&editor, 0, sizeof(editor));
    editor.app = app;
    editor.shot = shot;
    editor.rect = rect;
    editor.win_w = rect.w;
    editor.win_h = rect.h;

    Annotation blur = {TOOL_BLUR, 20, 22, 142, 86, {0}};
    Annotation arrow = {TOOL_ARROW, 38, 145, 184, 105, {0}};
    Annotation box = {TOOL_RECT, 176, 34, 294, 132, {0}};
    Annotation text = {TOOL_TEXT, 184, 154, 184, 154, "Modern"};
    editor_add_annotation(&editor, &blur);
    editor_add_annotation(&editor, &arrow);
    editor_add_annotation(&editor, &box);
    editor_add_annotation(&editor, &text);

    int status = editor_export_png(&editor, path);
    XFreePixmap(app->display, shot);
    return status;
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
        if (event.type == SelectionRequest || event.type == SelectionClear) {
            handle_clipboard_event(app, &event);
            continue;
        }
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
            "  modern-screenshot --self-test PATH\n"
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
    } else if (argc > 2 && strcmp(argv[1], "--self-test") == 0) {
        status = self_test_export(&app, argv[2]) == 0 ? 0 : 1;
    } else if (argc == 1) {
        status = run_daemon(&app) == 0 ? 0 : 1;
    } else {
        usage(stderr);
        status = 2;
    }

    app_destroy(&app);
    return status;
}
