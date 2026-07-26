# Juicy

Juicy is a focused Linux torrent video player. It accepts a magnet link,
streams a selected video before the torrent is complete, exposes audio and
subtitle tracks, and supports Anime4K shaders.

The first release targets Linux and Wayland. Video is rendered inside the
application with libmpv; torrent data is temporary and removed after the
session ends.

## Usage

Launch Juicy, paste a magnet link, wait for its metadata, select a video, and
press **Stream**. Playback begins as soon as the pieces required by mpv are
available.

The bottom controls provide seeking, volume, audio track selection, embedded
or external subtitles, Anime4K profiles, and fullscreen mode.

Torrent files are stored in a unique directory under the system temporary
directory. Juicy removes that directory when the application exits normally.

## Development

Required runtime and development libraries:

- Qt 6
- mpv/libmpv
- libtorrent-rasterbar 2
- CMake and Ninja

On Arch Linux:

```bash
sudo pacman -S --needed cmake ninja qt6-base mpv libtorrent-rasterbar boost
```

Configure and build:

```bash
scripts/configure
scripts/build
```

The helper scripts automatically use the repository-local dependency prefix
under `.deps` when it exists.

Run the test suite:

```bash
scripts/test
```

Build an installable Linux archive:

```bash
scripts/package
```

The resulting archive is written to `build/juicy-linux-x86_64.tar.zst` and
expects Qt 6, libmpv, and libtorrent-rasterbar to be installed on the target
system.

## Anime4K

The bundled Anime4K GLSL shaders are pinned to upstream revision
`7684e9586f8dcc738af08a1cdceb024cc184f426`. They are distributed under the
MIT license included in `assets/anime4k/LICENSE`.

## Responsible use

Juicy is a general-purpose BitTorrent media player. Only stream content you
have the legal right to access and share.
