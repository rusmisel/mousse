CC=clang
WLPROTOS=wlr-protocols/unstable/wlr-layer-shell-unstable-v1.xml wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml wayland-protocols/stable/xdg-shell/xdg-shell.xml
WLCODE=$(patsubst %.xml,proto/%.c,$(WLPROTOS))
WLHEADERS=$(patsubst %.xml,proto/%.h,$(WLPROTOS))
mousse: mousse.c $(WLCODE) $(WLHEADERS)
	$(CC) -o mousse mousse.c $(WLCODE) -lwayland-client # -lwlroots-0.19
proto/%.c: /usr/share/%.xml
	@mkdir -p $(shell dirname $@)
	wayland-scanner private-code $< $@
proto/%.h: /usr/share/%.xml
	@mkdir -p $(shell dirname $@)
	wayland-scanner client-header $< $@
