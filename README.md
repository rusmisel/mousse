# mousse
vim for your mouse.

## installation
[`mousse-git` on the aur](https://aur.archlinux.org/packages/mousse-git) (thanks to [@mhegreberg](https://hegreberg.io))

to install from source: `make install` (installs to `~/.local/bin` by default)

depends on `wayland-protocols`, `wlr-protocols`, `clang`, `wayland-scanner` to build, and `libwayland-client.so` at runtime

## TODO
- visual mode (drag & drop)
- modifier keys (shift, alt, ctrl clicks & drags)
- redo (C-r)
- colorschemes (from envvars. follow wal convention for ease of integration)
- scrolling (C-e & C-y, maybe C-d & C-u)
- configurable cursors
- is zz for screen zoom possible?
- multi-monitor support
