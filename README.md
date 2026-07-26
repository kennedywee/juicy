# Juicy

Juicy is a focused Linux torrent video player. It accepts a magnet link,
streams a selected video before the torrent is complete, exposes audio and
subtitle tracks, and supports Anime4K shaders.

The first release targets Linux and Wayland. Video is rendered inside the
application with libmpv; torrent data is temporary and removed after the
session ends.

## Development

Required runtime and development libraries:

- Qt 6
- mpv/libmpv
- libtorrent-rasterbar 2
- CMake and Ninja

Configure and build:

```bash
scripts/configure
scripts/build
```

The helper scripts automatically use the repository-local dependency prefix
under `.deps` when it exists.

## Anime4K

The bundled Anime4K GLSL shaders are pinned to upstream revision
`7684e9586f8dcc738af08a1cdceb024cc184f426`. They are distributed under the
MIT license included in `assets/anime4k/LICENSE`.
