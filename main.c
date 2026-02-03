#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <stdbool.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

/* ================= globals ================= */

static struct wl_display *display;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct wl_keyboard *keyboard;
static struct zwlr_layer_shell_v1 *layer_shell;

static struct wl_surface *surface;
static struct zwlr_layer_surface_v1 *layer_surface;

/* xkb */
static struct xkb_context *xkb_ctx;
static struct xkb_keymap *xkb_keymap;
static struct xkb_state *xkb_state;

/* surface size */
static int width, height;

/* vim modes */
enum mode { MODE_NORMAL, MODE_VISUAL };
static enum mode mode = MODE_NORMAL;

/* cursor */
static int vx, vy;

/* selection */
static bool selecting = false;
static int ax, ay, cx, cy;

/* ================= buffer lifecycle ================= */

static void buffer_release(void *data, struct wl_buffer *buffer)
{
    (void)data;
    wl_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release,
};

/* ================= SHM ================= */

static int create_shm_file(size_t size)
{
    int fd = memfd_create("wl-shm", MFD_CLOEXEC);
    if (fd < 0) return -1;
    ftruncate(fd, size);
    return fd;
}

static void draw(void)
{
    if (width == 0 || height == 0) return;

    size_t stride = width * 4;
    size_t size = stride * height;

    int fd = create_shm_file(size);
    if (fd < 0) return;

    uint32_t *px = mmap(NULL, size,
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED, fd, 0);

    /* dim background */
    for (int i = 0; i < width * height; i++)
        px[i] = 0x88000000;

    /* cursor */
    px[vy * width + vx] = 0xFFFFFFFF;

    /* selection rectangle */
    if (selecting) {
        int minx = ax < cx ? ax : cx;
        int maxx = ax > cx ? ax : cx;
        int miny = ay < cy ? ay : cy;
        int maxy = ay > cy ? ay : cy;

        for (int x = minx; x <= maxx; x++) {
            px[miny * width + x] = 0xFFFFFFFF;
            px[maxy * width + x] = 0xFFFFFFFF;
        }
        for (int y = miny; y <= maxy; y++) {
            px[y * width + minx] = 0xFFFFFFFF;
            px[y * width + maxx] = 0xFFFFFFFF;
        }
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
    struct wl_buffer *buf =
        wl_shm_pool_create_buffer(pool, 0,
                                  width, height,
                                  stride,
                                  WL_SHM_FORMAT_ARGB8888);

    wl_buffer_add_listener(buf, &buffer_listener, NULL);

    wl_surface_attach(surface, buf, 0, 0);

    /* 🔴 THIS WAS MISSING */
    wl_surface_damage_buffer(surface, 0, 0, width, height);

    wl_surface_commit(surface);

    wl_shm_pool_destroy(pool);
    munmap(px, size);
    close(fd);

    fprintf(stderr,
        "[draw] cursor=%d,%d selecting=%d\n",
        vx, vy, selecting);
}

/* ================= cursor ================= */

static void move_cursor(int dx, int dy)
{
    vx += dx;
    vy += dy;

    if (vx < 0) vx = 0;
    if (vy < 0) vy = 0;
    if (vx >= width)  vx = width - 1;
    if (vy >= height) vy = height - 1;

    if (mode == MODE_VISUAL) {
        cx = vx;
        cy = vy;
    }

    draw();
}

/* ================= keyboard ================= */

static void keyboard_keymap(void *data, struct wl_keyboard *kbd,
                            uint32_t format, int fd, uint32_t size)
{
    (void)data; (void)kbd; (void)format;

    char *map = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    xkb_keymap = xkb_keymap_new_from_string(
        xkb_ctx, map,
        XKB_KEYMAP_FORMAT_TEXT_V1,
        XKB_KEYMAP_COMPILE_NO_FLAGS);

    xkb_state = xkb_state_new(xkb_keymap);

    munmap(map, size);
    close(fd);
}

static void keyboard_enter(void *d, struct wl_keyboard *k,
                           uint32_t s, struct wl_surface *sf,
                           struct wl_array *keys)
{
    (void)d; (void)k; (void)s; (void)sf; (void)keys;
}

static void keyboard_leave(void *d, struct wl_keyboard *k,
                           uint32_t s, struct wl_surface *sf)
{
    (void)d; (void)k; (void)s; (void)sf;
}

static void keyboard_key(void *d, struct wl_keyboard *k,
                         uint32_t serial, uint32_t time,
                         uint32_t key, uint32_t state)
{
    (void)d; (void)k; (void)serial; (void)time;

    if (state != WL_KEYBOARD_KEY_STATE_PRESSED) return;
    if (!xkb_state) return;

    xkb_keysym_t sym =
        xkb_state_key_get_one_sym(xkb_state, key + 8);

    switch (sym) {
    case XKB_KEY_h: move_cursor(-10, 0); break;
    case XKB_KEY_l: move_cursor( 10, 0); break;
    case XKB_KEY_k: move_cursor(0, -10); break;
    case XKB_KEY_j: move_cursor(0,  10); break;

    case XKB_KEY_v:
        mode = MODE_VISUAL;
        selecting = true;
        ax = cx = vx;
        ay = cy = vy;
        draw();
        break;

    case XKB_KEY_Return:
        printf("%d,%d %dx%d\n",
               ax < cx ? ax : cx,
               ay < cy ? ay : cy,
               abs(cx - ax),
               abs(cy - ay));
        fflush(stdout);
        exit(0);

    case XKB_KEY_Escape:
    case XKB_KEY_q:
        exit(0);
    }
}

static void keyboard_modifiers(void *d, struct wl_keyboard *k,
                               uint32_t s,
                               uint32_t dep,
                               uint32_t lat,
                               uint32_t lock,
                               uint32_t grp)
{
    (void)d; (void)k; (void)s;
    if (xkb_state)
        xkb_state_update_mask(
            xkb_state,
            dep, lat, lock,
            0, 0, grp);
}

static void keyboard_repeat_info(void *d, struct wl_keyboard *k,
                                 int32_t r, int32_t del)
{
    (void)d; (void)k; (void)r; (void)del;
}

static const struct wl_keyboard_listener keyboard_listener = {
    .keymap = keyboard_keymap,
    .enter = keyboard_enter,
    .leave = keyboard_leave,
    .key = keyboard_key,
    .modifiers = keyboard_modifiers,
    .repeat_info = keyboard_repeat_info,
};

/* ================= layer ================= */

static void layer_configure(void *d,
                            struct zwlr_layer_surface_v1 *s,
                            uint32_t serial,
                            uint32_t w,
                            uint32_t h)
{
    (void)d;
    zwlr_layer_surface_v1_ack_configure(s, serial);

    width = w;
    height = h;

    vx = width / 2;
    vy = height / 2;
    cx = vx;
    cy = vy;
    selecting = false;
    mode = MODE_NORMAL;

    draw();
}

static const struct zwlr_layer_surface_v1_listener layer_listener = {
    .configure = layer_configure,
};

/* ================= registry ================= */

static void registry_add(void *d, struct wl_registry *r,
                         uint32_t name, const char *iface,
                         uint32_t v)
{
    (void)d; (void)v;

    if (!strcmp(iface, wl_compositor_interface.name))
        compositor = wl_registry_bind(r, name,
            &wl_compositor_interface, 4);
    else if (!strcmp(iface, wl_shm_interface.name))
        shm = wl_registry_bind(r, name,
            &wl_shm_interface, 1);
    else if (!strcmp(iface, wl_seat_interface.name)) {
        seat = wl_registry_bind(r, name,
            &wl_seat_interface, 1);
        keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(
            keyboard, &keyboard_listener, NULL);
    }
    else if (!strcmp(iface,
        zwlr_layer_shell_v1_interface.name))
        layer_shell = wl_registry_bind(
            r, name,
            &zwlr_layer_shell_v1_interface, 4);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_add,
};

/* ================= main ================= */

int main(void)
{
    display = wl_display_connect(NULL);
    struct wl_registry *reg =
        wl_display_get_registry(display);

    wl_registry_add_listener(reg,
        &registry_listener, NULL);
    wl_display_roundtrip(display);

    surface = wl_compositor_create_surface(compositor);
    layer_surface =
        zwlr_layer_shell_v1_get_layer_surface(
            layer_shell,
            surface,
            NULL,
            ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
            "vim-screenshot");

    zwlr_layer_surface_v1_set_anchor(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    zwlr_layer_surface_v1_set_exclusive_zone(layer_surface, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layer_surface,
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);

    zwlr_layer_surface_v1_add_listener(
        layer_surface, &layer_listener, NULL);

    wl_surface_commit(surface);

    while (wl_display_dispatch(display) != -1) {}
}
