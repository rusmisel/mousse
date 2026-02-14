# mousse
vim for your mouse.

## installation
[`mousse-git` on the aur](https://aur.archlinux.org/packages/mousse-git) (thanks to [@mhegreberg](https://hegreberg.io))

to install from source: `make install` (installs to `~/.local/bin` by default)

depends on `wayland-protocols`, `wlr-protocols`, `clang`, `wayland-scanner` to build, and `libwayland-client.so` at runtime

## TODO
- modifier passthrough
- redo (C-r)
- scrolling (C-e & C-y, maybe C-d & C-u)
- is zz for fullscreen zoom possible? (need to read through wayland protos more)
- multi-monitor support
