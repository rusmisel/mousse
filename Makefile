PREFIX:=${HOME}/.local
WLPROTOS=wlr-protocols/unstable/wlr-layer-shell-unstable-v1.xml wlr-protocols/unstable/wlr-virtual-pointer-unstable-v1.xml wayland-protocols/stable/xdg-shell/xdg-shell.xml
WLCODE=$(patsubst %.xml,proto/%.c,$(WLPROTOS))
WLHEADERS=$(patsubst %.xml,proto/%.h,$(WLPROTOS))
mousse: mousse.c $(WLCODE) $(WLHEADERS)
	$(CC) -o mousse mousse.c $(WLCODE) -lwayland-client
proto/%.c: /usr/share/%.xml
	@mkdir -p $(shell dirname $@)
	wayland-scanner private-code $< $@
proto/%.h: /usr/share/%.xml
	@mkdir -p $(shell dirname $@)
	wayland-scanner client-header $< $@
.PHONY: install
install: mousse
	install mousse "${PREFIX}/bin"
	mkdir -p "${PREFIX}/share/man/man1"
	install -m644 mousse.1 "${PREFIX}/share/man/man1"
