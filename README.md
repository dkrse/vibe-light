# Vibe Light

A lightweight file browser, terminal, and prompt application built with GTK4 and libadwaita. Includes SFTP/SSH support for remote file browsing.

## Features

- **File Browser** -- Navigate directories, view files with line numbers and current line highlight
- **Live File Watching** -- File browser and editor update automatically when files change on disk
- **Terminal** -- Embedded VTE terminal, spawned in the opened directory
- **Prompt** -- Send text commands to the terminal with configurable send key
- **SFTP/SSH** -- Browse remote directories and view remote files via SSH (no sshfs required)
- **13 Themes** -- System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Per-section Fonts** -- Independent font settings for GUI, File Browser, Editor, Terminal, and Prompt
- **Global Font Intensity** -- Single intensity control affecting all text across the entire application
- **Settings** -- Tabbed dialog (GUI, File Browser, Editor, Terminal, Prompt)

## Dependencies

- GTK 4 (>= 4.0)
- libadwaita (>= 1.0)
- VTE (vte-2.91-gtk4)
- GCC, Make, pkg-config
- OpenSSH client (for SFTP, usually pre-installed)

### Fedora (43+)

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gcc make pkgconf-pkg-config
```

### Ubuntu / Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S gtk4 libadwaita vte4 gcc make pkg-config
```

See [docs/dependencies.md](docs/dependencies.md) for full details.

## Build

```bash
make
```

## Run

```bash
./build/vibe-light
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open folder |
| Ctrl+Plus / Ctrl+= | Zoom in |
| Ctrl+Minus | Zoom out |
| Ctrl+Enter | Send prompt to terminal (default, configurable) |

## SFTP/SSH

Connect to remote servers via Menu > SFTP/SSH:

- Save connection profiles (host, port, user, key/password auth)
- Browse remote directories in the file browser
- View remote files (read-only, up to 10 MB)
- SSH terminal session opens automatically
- Click path label to change remote directory (sends `cd` to terminal)
- Disconnect via status bar button

No `sshfs` or FUSE required. Uses `ssh` commands directly (Midnight Commander-style).

SSH connections use ControlMaster multiplexing for efficient reuse of a single TCP connection. Remote file watching uses `inotifywait` on the server when available (instant notifications), with automatic fallback to periodic polling.

## Settings Tabs

| Tab | Options |
|-----|---------|
| GUI | Theme, Font, Font Intensity |
| File Browser | Font |
| Editor | Font, Line Spacing, Line Numbers, Highlight Line, Wrap Lines |
| Terminal | Font |
| Prompt | Font, Send with (Ctrl+Enter/Enter), Show Terminal after send |

## Configuration

Settings are stored in `~/.config/vibe-light/settings.conf` (plain text key=value format, locale-safe).

SFTP connections are stored in `~/.config/vibe-light/connections.conf` (INI format). Both files use `0600` permissions.

## Documentation

- [Architecture](docs/architecture.md)
- [Dependencies](docs/dependencies.md)
- [Changelog](docs/changelog.md)

## License

MIT
