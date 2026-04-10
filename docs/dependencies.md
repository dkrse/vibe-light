# Dependencies

## Build Dependencies

| Package | Purpose | Min Version |
|---------|---------|-------------|
| GCC | C17 compiler | any recent |
| Make | Build system | any |
| pkg-config | Library discovery | any |
| GTK 4 (dev) | UI framework | 4.0 |
| libadwaita (dev) | GNOME styling, AdwApplication | 1.0 |
| GtkSourceView 5 (dev) | Syntax highlighting, line numbers | 5.0 |
| VTE (dev) | Terminal emulation (gtk4 variant) | 2.91 |
| WebKitGTK 6.0 (dev) | AI markdown rendering (WebView) | 6.0 |
| cmark-gfm (dev) | GitHub-Flavored Markdown parser | any |
| poppler-glib (dev) | PDF page number rendering (cairo) | any |

## Runtime Dependencies

| Package | Purpose | Required |
|---------|---------|----------|
| GTK 4 | UI rendering | yes |
| libadwaita | Theme management | yes |
| GtkSourceView 5 | Syntax highlighting (200+ languages) | yes |
| VTE | Embedded terminal | yes |
| WebKitGTK 6.0 | AI output WebView rendering | yes |
| cmark-gfm | Markdown to HTML conversion | yes |
| poppler-glib | PDF post-processing (page numbers) | yes |
| GLib / GIO | Subprocess, file I/O, GTask | yes (bundled with GTK) |
| OpenSSH client (`ssh`) | SFTP/SSH file browsing and terminal | for SFTP only |
| git | Git status indicators in file browser | optional |
| inotify-tools (`inotifywait`) | Instant remote file change detection | optional (on remote server) |

## Installation

### Fedora (43+)

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gtksourceview5-devel webkitgtk6.0-devel cmark-gfm-devel poppler-glib-devel gcc make pkgconf-pkg-config
```

For SFTP support (usually pre-installed):

```bash
sudo dnf install openssh-clients
```

### Ubuntu / Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev libgtksourceview-5-dev libwebkitgtk-6.0-dev libcmark-gfm-dev libpoppler-glib-dev gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S gtk4 libadwaita vte4 gtksourceview5 webkit2gtk-5.0 cmark-gfm poppler-glib gcc make pkg-config
```

## Build Hardening

The Makefile includes security hardening flags:

- `-fstack-protector-strong` -- stack buffer overflow protection
- `-fPIE` / `-pie` -- position independent executable (ASLR)
- `-D_FORTIFY_SOURCE=2` -- runtime bounds checking for string/memory functions

## Notes

- No `libssh` / `libssh2` dependency. SFTP uses the system `ssh` command directly via `g_spawn_sync` / `GSubprocess` with argv arrays (no shell parsing, immune to injection).
- No `sshfs` / FUSE dependency. Remote file browsing uses `ssh ls` and `ssh cat` commands.
- SSH ControlMaster multiplexing is used to reuse a single TCP connection for all SSH operations.
- If `inotifywait` (inotify-tools) is installed on the remote server, file change detection is instant. Otherwise falls back to periodic polling (2s interval).
- Local file watching uses kernel inotify via GLib's `GFileMonitor` (zero CPU when idle).
- Git status detection uses the system `git` command. If git is not installed or the directory is not a git repo, status indicators are simply not shown.
- GtkSourceView language detection uses `g_content_type_guess()` for files without extensions (Makefile, Dockerfile, etc.).
- Configuration files are stored in `~/.config/vibe-light/` with `0600` permissions.
