# Vibe Light

A lightweight file browser, terminal, and AI assistant built with GTK4, libadwaita, and GtkSourceView. Includes SFTP/SSH support for remote file browsing, syntax highlighting for 200+ languages, and git status integration.

## Features

- **File Browser** -- Navigate directories with git status colors (modified, staged, untracked, deleted, ignored)
- **Syntax Highlighting** -- GtkSourceView-powered highlighting for 200+ languages (C, Python, Rust, Go, Makefile, JSON, YAML, and more)
- **Live File Watching** -- File browser and editor update automatically when files change on disk
- **Terminal** -- Embedded VTE terminal, spawned in the opened directory
- **AI Assistant** -- Send prompts to Claude CLI, view responses with token tracking and session management
- **SFTP/SSH** -- Browse remote directories and view remote files via SSH (no sshfs required)
- **13 Themes** -- System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Per-section Fonts** -- Independent font settings for GUI, File Browser, Editor, Terminal, and Prompt
- **Global Font Intensity** -- Single intensity control affecting all text across the entire application
- **Session Restore** -- Remembers last open file, cursor position, and active tab
- **Configurable Keybindings** -- Customize keyboard shortcuts via settings
- **Lazy Loading** -- Large directories load in batches of 500 for smooth UI
- **Async File Loading** -- Files load in background threads, UI never blocks
- **Hidden/Ignored Files** -- Toggle dotfile visibility and gitignored file display (hide or show gray)

## Dependencies

- GTK 4 (>= 4.0)
- libadwaita (>= 1.0)
- GtkSourceView 5
- VTE (vte-2.91-gtk4)
- GCC, Make, pkg-config
- OpenSSH client (for SFTP, usually pre-installed)

### Fedora (43+)

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gtksourceview5-devel gcc make pkgconf-pkg-config
```

### Ubuntu / Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev libgtksourceview-5-dev gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S gtk4 libadwaita vte4 gtksourceview5 gcc make pkg-config
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

All shortcuts are configurable via `settings.conf`.

| Shortcut | Action | Setting key |
|----------|--------|-------------|
| Ctrl+O | Open folder | `key_open_folder` |
| Ctrl+Plus / Ctrl+= | Zoom in | `key_zoom_in` |
| Ctrl+Minus | Zoom out | `key_zoom_out` |
| Alt+1 | Switch to Files tab | `key_tab_files` |
| Alt+2 | Switch to Terminal tab | `key_tab_terminal` |
| Alt+3 | Switch to AI tab | `key_tab_ai` |
| Ctrl+Q | Quit | `key_quit` |

## Git Status

Files and directories in the browser are colored by their git status:

| Color | Status |
|-------|--------|
| Orange | Modified |
| Green | Staged / Added |
| Gray | Untracked |
| Red | Deleted |
| Dark gray | Ignored (when "Show gray" is selected) |

Works both locally and over SSH/SFTP.

## SFTP/SSH

Connect to remote servers via Menu > SFTP/SSH:

- Save connection profiles (host, port, user, key/password auth)
- Browse remote directories in the file browser
- View remote files with syntax highlighting (read-only, up to 10 MB)
- SSH terminal session opens automatically
- Click path label to change remote directory (sends `cd` to terminal)
- Git status indicators work over SSH
- Disconnect via status bar button

No `sshfs` or FUSE required. Uses `ssh` commands directly (Midnight Commander-style).

SSH connections use ControlMaster multiplexing for efficient reuse of a single TCP connection. Remote file watching uses `inotifywait` on the server when available (instant notifications), with automatic fallback to periodic polling.

## AI Assistant

Built-in integration with Claude CLI (`claude` command):

- Send prompts and view responses in a dedicated tab
- Session continuity via `--resume`
- Token usage tracking (input/output/total)
- Configurable tool access (Read, Edit, Write, Glob, Grep, Bash)
- Optional CWD restriction for security
- Prompt history saved to `.LLM/prompts.json`

## Settings Tabs

| Tab | Options |
|-----|---------|
| GUI | Theme, Font, Font Intensity |
| File Browser | Font, Show Hidden Files, Gitignored Files (Hide/Show gray) |
| Editor | Font, Font Weight, Line Spacing, Line Numbers, Highlight Line, Wrap Lines |
| Terminal | Font |
| Prompt | Font, Send with (Ctrl+Enter/Enter), Show Terminal after send |
| AI Model | Full Disk Access, Tool toggles |

## Configuration

Settings are stored in `~/.config/vibe-light/settings.conf` (plain text key=value format, locale-safe).

SFTP connections are stored in `~/.config/vibe-light/connections.conf` (INI format). Both files use `0600` permissions.

## Documentation

- [Architecture](docs/architecture.md)
- [Dependencies](docs/dependencies.md)
- [Changelog](docs/changelog.md)

## Author

krse

## License

MIT
