#include "proto/wayland-protocols/stable/xdg-shell/xdg-shell.h"
#include "proto/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.h"
#include "proto/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.h"
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

enum mode {
  MODE_NORMAL,
  MODE_VISUAL,
  MODECOUNT,
};

static const char* SHM_NAME = "/mousse";
static const char* modecolorvars[MODECOUNT] = {"color1", "color7"};
static uint32_t modecolors[MODECOUNT] = {0xffff0000, 0xff0000ff};
static struct wl_compositor* compositor = NULL;
static struct wl_seat* seat = NULL;
static struct wl_shm* wlshm = NULL;
static struct wl_surface* surface = NULL;
static struct wl_buffer* buf = NULL;
static struct zwlr_layer_shell_v1* layer_shell = NULL;
static struct zwlr_layer_surface_v1* layer_surface = NULL;
static struct zwlr_virtual_pointer_manager_v1* vpm = NULL;
static struct zwlr_virtual_pointer_v1* vp = NULL;
static int shmfd;
static uint32_t *frame = NULL, fw = 0, fh = 0, fs = 0;
static size_t fsz = 0;
static bool framewritable = false, immediateredraw = false;
static bool enterbeenpressed = false, anchorbeensunk = false;
static enum mode mode = MODE_NORMAL;
static uint32_t modifiers = 0;
static uint32_t x = 1, y = 1, xe = 2, ye = 2;
static uint8_t ui = 0;
static bool uvert[UINT8_MAX];
static bool upos[UINT8_MAX];
static char* err = NULL;
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
                                ? modecolors[mode]
                                : 0x00000000;
  zwlr_virtual_pointer_v1_motion_absolute(vp, 0, x, y, xe, ye);
  framewritable = false;
  wl_surface_attach(surface, buf, 0, 0);
  wl_surface_damage_buffer(surface, 0, 0, fw, fh);
  wl_surface_commit(surface);
}

static void global(void* _, struct wl_registry* reg, uint32_t id,
                   const char* iface, uint32_t ver) {
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
static void global_remove(void* _, struct wl_registry* reg, uint32_t id) {
  // TODO: panic if we depend on global
}
static struct wl_registry_listener reg_listener = {
    .global = global, .global_remove = global_remove};

static void buf_release(void* _, struct wl_buffer* b) {
  framewritable = true;
  if (immediateredraw) {
    immediateredraw = false;
    draw();
  }
}
static struct wl_buffer_listener buf_listener = {.release = buf_release};

static void layer_shell_config(void* _, struct zwlr_layer_surface_v1* s,
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
  fs = fw * 4;
  fsz = fs * fh;
  zwlr_layer_surface_v1_ack_configure(s, serial);
  zwlr_layer_surface_v1_set_layer(s, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY);
  zwlr_layer_surface_v1_set_keyboard_interactivity(
      s, ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_EXCLUSIVE);
  if (ftruncate(shmfd, fsz) < 0) {
    err = "failed to resize shm";
    done = true;
    return;
  }
  frame = mmap(NULL, fsz, PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);
  if (frame == MAP_FAILED) {
    perror("");
    err = "failed to mmap shm";
    done = true;
    return;
  }
  framewritable = true;
  struct wl_shm_pool* pool = wl_shm_create_pool(wlshm, shmfd, fsz);
  buf = wl_shm_pool_create_buffer(pool, 0, fw, fh, fs, WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener(buf, &buf_listener, NULL);
  wl_shm_pool_destroy(pool);
  draw();
}
static void layer_shell_closed(void* _, struct zwlr_layer_surface_v1* s) {
  done = true;
}
static struct zwlr_layer_surface_v1_listener lsl = {
    .configure = layer_shell_config, .closed = layer_shell_closed};

void onenter(void* _, struct wl_keyboard* keeb, uint32_t serial,
             struct wl_surface* s, struct wl_array* keys) {}
void onleave(void* _, struct wl_keyboard* keeb, uint32_t serial,
             struct wl_surface* s) {}
void onmodifiers(void* _, struct wl_keyboard* keeb, uint32_t serial,
                 uint32_t depressed, uint32_t latched, uint32_t locked,
                 uint32_t keyboard) {
  modifiers = depressed | latched | locked;
}

void onrepeatinfo(void* _, struct wl_keyboard* keeb, int32_t rate,
                  int32_t delay) {}
void onkeymap(void* _, struct wl_keyboard* keeb, uint32_t fmt, int32_t fd,
              uint32_t size) {}
static void onkey(void* d, struct wl_keyboard* keeb, uint32_t serial,
                  uint32_t time, uint32_t key, uint32_t state) {
  switch (key) {
  case KEY_ESC: {
    done = true;
    return;
  }
  case KEY_SPACE:
  case KEY_ENTER: {
    int btn = key == KEY_ENTER ? BTN_LEFT : BTN_RIGHT;
    if (mode == MODE_NORMAL) {
      zwlr_virtual_pointer_v1_button(vp, 0, btn, state);
      done = enterbeenpressed && state == WL_KEYBOARD_KEY_STATE_RELEASED;
    } else if (mode == MODE_VISUAL && state) {
      x = 1;
      y = 1;
      xe = 2;
      ye = 2;
      ui = 0;
      done = anchorbeensunk;
      zwlr_virtual_pointer_v1_button(vp, 0, btn,
                                     anchorbeensunk = !anchorbeensunk);
    }
    zwlr_virtual_pointer_v1_frame(vp);
    if (!enterbeenpressed && state == WL_KEYBOARD_KEY_STATE_PRESSED)
      enterbeenpressed = true;
    break;
  }
  case KEY_H: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    x = x * 2 - 1;
    xe *= 2;
    uvert[ui] = false;
    upos[ui] = false;
    ui++;
    if (!ui)
      done = true;
    break;
  }
  case KEY_J: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    y = y * 2 + 1;
    ye *= 2;
    uvert[ui] = true;
    upos[ui] = true;
    ui++;
    if (!ui)
      done = true;
    break;
  }
  case KEY_K: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    y = y * 2 - 1;
    ye *= 2;
    uvert[ui] = true;
    upos[ui] = false;
    ui++;
    if (!ui)
      done = true;
    break;
  }
  case KEY_L: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    x = x * 2 + 1;
    xe *= 2;
    uvert[ui] = false;
    upos[ui] = true;
    ui++;
    if (!ui)
      done = true;
    break;
  }
  case KEY_U: {
    if (!ui || state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    ui--;
    *(uvert[ui] ? &y : &x) += upos[ui] ? -1 : 1;
    *(uvert[ui] ? &y : &x) /= 2;
    *(uvert[ui] ? &ye : &xe) /= 2;
    break;
  }
  case KEY_V: {
    if (state != WL_KEYBOARD_KEY_STATE_PRESSED)
      return;
    done = anchorbeensunk;
    mode = mode == MODE_VISUAL ? MODE_NORMAL : MODE_VISUAL;
  }
  }
  draw();
}
static struct wl_keyboard_listener kl = {.enter = onenter,
                                         .leave = onleave,
                                         .modifiers = onmodifiers,
                                         .repeat_info = onrepeatinfo,
                                         .keymap = onkeymap,
                                         .key = onkey};

int main() {
  for (int i = 0; i < MODECOUNT; i++) {
    char* hex = getenv(modecolorvars[i]);
    if (!hex)
      continue;
    if (hex[0] == '#')
      hex++;
    uint32_t color = strtol(hex, NULL, 16);
    modecolors[i] = (color & 0xff000000) ? color : color | 0xff000000;
  }
  shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, 0600);
  struct wl_display* disp = wl_display_connect(NULL);
  struct wl_registry* reg = wl_display_get_registry(disp);
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
    struct wl_region* region = wl_compositor_create_region(compositor);
    wl_region_add(region, 0, 0, 0, 0);
    wl_surface_set_input_region(surface, region);
    wl_surface_commit(surface);
    vp = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(vpm, seat);
    struct wl_keyboard* keeb = wl_seat_get_keyboard(seat);
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
  munmap(frame, fsz);
  close(shmfd);
  shm_unlink(SHM_NAME);
  if (err) {
    fprintf(stderr, "ERROR: %s\n", err);
    return 1;
  }
}
