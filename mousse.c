#include "proto/wayland-protocols/stable/xdg-shell/xdg-shell.h"
#include "proto/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.h"
#include "proto/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.h"
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

static const char *SHM_NAME = "/mousse";
static struct wl_compositor *compositor = NULL;
static struct wl_seat *seat = NULL;
static struct wl_shm *wlshm = NULL;
static struct wl_surface *surface = NULL;
static struct wl_buffer *buf = NULL;
static struct zwlr_layer_shell_v1 *layer_shell = NULL;
static struct zwlr_layer_surface_v1 *layer_surface = NULL;
static struct zwlr_virtual_pointer_manager_v1 *vpm = NULL;
static struct zwlr_virtual_pointer_v1 *vp = NULL;
static int shmfd;
static uint32_t *frame = NULL;
static size_t frame_size = 0;
static bool framewritable = false, immediateredraw = false;
static uint32_t fw = 0, fh = 0, frame_stride = 0;
static uint32_t x = 1, y = 1, xe = 2, ye = 2;
static char *err = NULL;
static bool done = false;

static void draw() {
  if (!framewritable) {
    immediateredraw = true;
    return;
  }
  for (int yp = 0; yp < fh; yp++)
    for (int xp = 0; xp < fw; xp++)
      frame[yp * fw + xp] = ((xp == x * fw / xe && yp > (y - 1) * fh / ye &&
                              yp < (y + 1) * fh / ye) ||
                             (yp == y * fh / ye && xp > (x - 1) * fw / xe &&
                              xp < (x + 1) * fw / xe))
                                ? 0xffff0000
                                : 0x00000000;
  zwlr_virtual_pointer_v1_motion_absolute(vp, 0, x, y, xe, ye);
  framewritable = false;
  wl_surface_attach(surface, buf, 0, 0);
  wl_surface_damage_buffer(surface, 0, 0, fw, fh);
  wl_surface_commit(surface);
}

static void global(void *_, struct wl_registry *reg, uint32_t id,
                   const char *iface, uint32_t ver) {
  if (!strcmp(wl_compositor_interface.name, iface))
    compositor = wl_registry_bind(reg, id, &wl_compositor_interface, ver);
  else if (!strcmp(wl_seat_interface.name, iface))
    seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
  else if (!strcmp(wl_shm_interface.name, iface))
    wlshm = wl_registry_bind(reg, id, &wl_shm_interface, ver);
  else if (!strcmp(zwlr_layer_shell_v1_interface.name, iface))
    layer_shell =
        wl_registry_bind(reg, id, &zwlr_layer_shell_v1_interface, ver);
  else if (!strcmp(zwlr_virtual_pointer_manager_v1_interface.name, iface))
    vpm = wl_registry_bind(reg, id, &zwlr_virtual_pointer_manager_v1_interface,
                           ver);
}
static void global_remove(void *_, struct wl_registry *reg, uint32_t id) {
  // TODO: panic if we depend on global
}
static struct wl_registry_listener reg_listener = {
    .global = global, .global_remove = global_remove};

static void buf_release(void *_, struct wl_buffer *b) {
  framewritable = true;
  if (immediateredraw) {
    immediateredraw = false;
    draw();
  }
}
static struct wl_buffer_listener buf_listener = {.release = buf_release};

static void layer_shell_config(void *_, struct zwlr_layer_surface_v1 *s,
                               uint32_t serial, uint32_t width,
                               uint32_t height) {
  if (fw == width && fh == height)
    return;
  if (frame && !framewritable) {
    err = "compositor musn't send configure between buffer commit and release";
    done = true;
    return;
  }
  fw = width;
  fh = height;
  frame_stride = fw * 4;
  frame_size = frame_stride * fh;
  zwlr_layer_surface_v1_ack_configure(s, serial);
  zwlr_layer_surface_v1_set_layer(s, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
  zwlr_layer_surface_v1_set_keyboard_interactivity(
      s, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
  if (ftruncate(shmfd, frame_size) < 0) {
    err = "failed to resize shm";
    done = true;
    return;
  }
  frame = mmap(NULL, frame_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
  if (frame == MAP_FAILED) {
    perror("");
    err = "failed to mmap shm";
    done = true;
    return;
  }
  framewritable = true;
  struct wl_shm_pool *pool = wl_shm_create_pool(wlshm, shmfd, frame_size);
  buf = wl_shm_pool_create_buffer(pool, 0, fw, fh, frame_stride,
                                  WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener(buf, &buf_listener, NULL);
  wl_shm_pool_destroy(pool);
  draw();
}
static void layer_shell_closed(void *_, struct zwlr_layer_surface_v1 *s) {
  done = true;
}
static struct zwlr_layer_surface_v1_listener lsl = {
    .configure = layer_shell_config, .closed = layer_shell_closed};

static void onkey(void *d, struct wl_keyboard *keeb, uint32_t serial,
                  uint32_t time, uint32_t key, uint32_t state) {
  switch (key) {
  case KEY_ESC: {
    done = true;
    return;
  }
  case KEY_ENTER: {
    zwlr_virtual_pointer_v1_button(vp, 0, BTN_LEFT, state);
    zwlr_virtual_pointer_v1_frame(vp);
    if (!state)
      done = true;
    break;
  }
  case KEY_SPACE: {
    zwlr_virtual_pointer_v1_button(vp, 0, BTN_RIGHT, state);
    zwlr_virtual_pointer_v1_frame(vp);
    if (!state)
      done = true;
    break;
  }
  case KEY_H: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    x = x * 2 - 1;
    xe *= 2;
    break;
  }
  case KEY_J: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    y = y * 2 + 1;
    ye *= 2;
    break;
  }
  case KEY_K: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    y = y * 2 - 1;
    ye *= 2;
    break;
  }
  case KEY_L: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    x = x * 2 + 1;
    xe *= 2;
    break;
  }
  }
  draw();
}
static struct wl_keyboard_listener kl = {.key = onkey};

int main() {
  shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0600);
  struct wl_display *disp = wl_display_connect(NULL);
  struct wl_registry *reg = wl_display_get_registry(disp);
  wl_registry_add_listener(reg, &reg_listener, NULL);
  wl_display_roundtrip(disp);
  err = !compositor    ? "missing global compositor"
        : !seat        ? "missing global seat"
        : !wlshm       ? "missing global shm"
        : !layer_shell ? "missing global layer shell"
        : !vpm         ? "missing global virtual pointer manager"
        : shmfd < 0    ? "mousse appears to be running already"
                       : NULL;
  if (!err) {
    surface = wl_compositor_create_surface(compositor);
    layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell, surface, NULL, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "mousse");
    zwlr_layer_surface_v1_add_listener(layer_surface, &lsl, 0);
    struct wl_region *region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, 0, 0);
    wl_surface_set_input_region(surface, region);
    wl_surface_commit(surface);
    vp = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(vpm, seat);
    struct wl_keyboard *keeb = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(keeb, &kl, 0);
    while (!done)
      wl_display_dispatch(disp);
    if (buf)
      wl_buffer_destroy(buf);
    wl_keyboard_destroy(keeb);
    zwlr_virtual_pointer_v1_destroy(vp);
    wl_region_destroy(region);
    zwlr_layer_surface_v1_destroy(layer_surface);
    wl_surface_destroy(surface);
  }
  wl_compositor_destroy(compositor);
  wl_seat_destroy(seat);
  wl_shm_destroy(wlshm);
  zwlr_layer_shell_v1_destroy(layer_shell);
  zwlr_virtual_pointer_manager_v1_destroy(vpm);
  wl_registry_destroy(reg);
  wl_display_roundtrip(disp);
  wl_display_disconnect(disp);
  munmap(frame, frame_size);
  close(shmfd);
  shm_unlink(SHM_NAME);
  if (err) {
    fprintf(stderr, "ERROR: %s\n", err);
    return 1;
  }
}
