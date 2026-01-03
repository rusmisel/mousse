#include "proto/wayland-protocols/stable/xdg-shell/xdg-shell.h"
#include "proto/wlr-protocols/unstable/wlr-layer-shell-unstable-v1.h"
#include "proto/wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.h"
#include <stdio.h>
#include <string.h>
#include <wayland-client-protocol.h>
#include <wayland-client.h>

struct wl_compositor *compositor;
struct wl_seat *seat;
struct wl_shm *shm;
struct wl_surface *surface;
struct zwlr_layer_shell_v1 *layer_shell;
struct zwlr_layer_surface_v1 *layer_surface;
struct zwlr_virtual_pointer_manager_v1 *vpm;
struct zwlr_virtual_pointer_v1 *vp;
int x = 1, y = 1, xe = 2, ye = 2;

void noop() {}

void global(void *_, struct wl_registry *reg, uint32_t id, const char *iface,
            uint32_t ver) {
  if (!strcmp(wl_compositor_interface.name, iface))
    compositor = wl_registry_bind(reg, id, &wl_compositor_interface, ver);
  else if (!strcmp(wl_seat_interface.name, iface))
    seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
  else if (!strcmp(wl_shm_interface.name, iface))
    shm = wl_registry_bind(reg, id, &wl_shm_interface, ver);
  else if (!strcmp(zwlr_layer_shell_v1_interface.name, iface))
    layer_shell =
        wl_registry_bind(reg, id, &zwlr_layer_shell_v1_interface, ver);
  else if (!strcmp(zwlr_virtual_pointer_manager_v1_interface.name, iface))
    vpm = wl_registry_bind(reg, id, &zwlr_virtual_pointer_manager_v1_interface,
                           ver);
  else
    printf("%i %s %i\n", id, iface, ver);
}
void global_remove(void *_, struct wl_registry *reg, uint32_t id) {}
struct wl_registry_listener listen = {.global = global,
                                      .global_remove = global_remove};

static void layer_shell_config(void *_, struct zwlr_layer_surface_v1 *s,
                               uint32_t serial, uint32_t w, uint32_t h) {
  zwlr_layer_surface_v1_ack_configure(s, serial);
  /*
  struct wl_buffer *buffer = draw_frame(state);
  wl_surface_attach(state->wl_surface, buffer, 0, 0);
  wl_surface_commit(state->wl_surface);
  */
}

void onkey(void *d, struct wl_keyboard *keeb, uint32_t serial, uint32_t time,
           uint32_t key, uint32_t state) {
  printf("%i %i\n", key, state);
  switch (key) {}
}

void onkeymap(void *_, struct wl_keyboard *keeb,
              enum wl_keyboard_keymap_format f, int32_t fd, uint32_t s) {
  printf("foo\n");
}

int main() {
  struct wl_display *disp = wl_display_connect(NULL);
  struct wl_registry *reg = wl_display_get_registry(disp);
  wl_registry_add_listener(reg, &listen, NULL);
  wl_display_roundtrip(disp);

  surface = wl_compositor_create_surface(compositor);
  layer_surface = zwlr_layer_shell_v1_get_layer_surface(
      layer_shell, surface, 0, ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY, "mousse");
  zwlr_layer_surface_v1_add_listener(
      layer_surface,
      &(struct zwlr_layer_surface_v1_listener){.configure = layer_shell_config},
      0);
  wl_surface_commit(surface);

  vp = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(vpm, seat);
  zwlr_virtual_pointer_v1_motion_absolute(vp, 0, 1, 1, 2, 2);

  struct wl_keyboard *keeb = wl_seat_get_keyboard(seat);
  struct wl_keyboard_listener kl = {noop, noop, noop, noop, noop, noop};
  kl.key = onkey;
  kl.keymap = onkeymap;
  wl_keyboard_add_listener(keeb, &kl, 0);

  while (1)
    wl_display_dispatch(disp);
}
