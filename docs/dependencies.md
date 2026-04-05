# Dependencies

## Build Dependencies

| Package | Purpose | Min Version |
|---------|---------|-------------|
| GCC | C17 compiler | any recent |
| Make | Build system | any |
| pkg-config | Library discovery | any |
| GTK 4 (dev) | UI framework | 4.0 |
| libadwaita (dev) | GNOME styling, AdwApplication | 1.0 |
| VTE (dev) | Terminal emulation (gtk4 variant) | 2.91 |

## Runtime Dependencies

| Package | Purpose | Required |
|---------|---------|----------|
| GTK 4 | UI rendering | yes |
| libadwaita | Theme management | yes |
| VTE | Embedded terminal | yes |
| GLib / GIO | Subprocess, file I/O, GTask | yes (bundled with GTK) |
| OpenSSH client (`ssh`) | SFTP/SSH file browsing and terminal | for SFTP only |
| inotify-tools (`inotifywait`) | Instant remote file change detection | optional (on remote server) |

## Installation

### Fedora (43+)

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gcc make pkgconf-pkg-config
```

For SFTP support (usually pre-installed):

```bash
sudo dnf install openssh-clients
```

### Ubuntu / Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S gtk4 libadwaita vte4 gcc make pkg-config
```

## Notes

- No `libssh` / `libssh2` dependency. SFTP uses the system `ssh` command directly via `g_spawn_sync` / `GSubprocess` with argv arrays (no shell parsing, immune to injection).
- No `sshfs` / FUSE dependency. Remote file browsing uses `ssh ls` and `ssh cat` commands.
- SSH ControlMaster multiplexing is used to reuse a single TCP connection for all SSH operations.
- If `inotifywait` (inotify-tools) is installed on the remote server, file change detection is instant. Otherwise falls back to periodic polling (2s interval).
- Local file watching uses kernel inotify via GLib's `GFileMonitor` (zero CPU when idle).
- Configuration files are stored in `~/.config/vibe-light/` with `0600` permissions.
