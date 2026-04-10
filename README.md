# Vibe Light

A lightweight file browser, terminal, and AI assistant built with GTK4, libadwaita, and GtkSourceView. Includes SFTP/SSH support for remote file browsing, syntax highlighting for 200+ languages, and git status integration.

## Features

- **File Browser** -- Navigate directories with git status colors, context menu (rename, delete, new file/dir), drag & drop
- **Syntax Highlighting** -- GtkSourceView-powered highlighting for 200+ languages (C, Python, Rust, Go, Makefile, JSON, YAML, and more)
- **File Editing** -- Full editing with undo/redo (Ctrl+Z/Ctrl+Shift+Z), save (Ctrl+S), modified indicator
- **Search** -- Ctrl+F find with highlighting and navigation, Ctrl+G go to line
- **Live File Watching** -- File browser and editor update automatically when files change on disk
- **Terminal** -- Embedded VTE terminal, spawned in the opened directory
- **AI Assistant** -- Send prompts to Claude CLI, view responses in WebKitWebView with full HTML markdown rendering, token tracking
- **SFTP/SSH** -- Browse remote directories and view remote files via SSH, save multiple connection profiles
- **13 Themes** -- System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Per-section Fonts** -- Independent font settings for GUI, File Browser, Editor, Terminal, AI Model, and Prompt
- **Per-section Font Intensity** -- Independent intensity control (0.3-1.0) for each section with live preview
- **PDF Export** -- Save as PDF via Menu or Ctrl+P with configurable margins, landscape, and page numbers (n or n/total) rendered via poppler+cairo
- **Session Restore** -- Remembers last open file, cursor position, and active tab
- **Configurable Keybindings** -- Customize keyboard shortcuts via settings
- **Lazy Loading** -- Large directories load in batches of 500 for smooth UI
- **Async File Loading** -- Files load in background threads, UI never blocks
- **Hidden/Ignored Files** -- Toggle dotfile visibility and gitignored file display (hide or show gray)
- **Toast Notifications** -- Non-intrusive feedback for save, connect, rename, delete operations
- **Drag & Drop** -- Drop files or folders from file manager to open them

## Dependencies

- GTK 4 (>= 4.0)
- libadwaita (>= 1.0)
- GtkSourceView 5
- VTE (vte-2.91-gtk4)
- WebKitGTK 6.0 (for AI markdown rendering)
- cmark-gfm (GitHub-Flavored Markdown parser)
- poppler-glib (PDF page number rendering)
- GCC, Make, pkg-config
- OpenSSH client (for SFTP, usually pre-installed)

### Fedora (43+)

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gtksourceview5-devel webkitgtk6.0-devel cmark-gfm-devel poppler-glib-devel gcc make pkgconf-pkg-config
```

### Ubuntu / Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev libgtksourceview-5-dev libwebkitgtk-6.0-dev libcmark-gfm-dev libpoppler-glib-dev gcc make pkg-config
```

### Arch Linux

```bash
sudo pacman -S gtk4 libadwaita vte4 gtksourceview5 webkit2gtk-5.0 cmark-gfm poppler-glib gcc make pkg-config
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
| Ctrl+S | Save file | -- |
| Ctrl+F | Find in editor | -- |
| Ctrl+G | Go to line | -- |
| Ctrl+Z | Undo | -- |
| Ctrl+Shift+Z / Ctrl+Y | Redo | -- |
| Ctrl+O | Open folder | `key_open_folder` |
| Ctrl+Plus / Ctrl+= | Zoom in (active section only) | `key_zoom_in` |
| Ctrl+Minus | Zoom out (active section only) | `key_zoom_out` |
| Ctrl+P | Print / Save as PDF | `key_print_pdf` |
| Alt+1 | Switch to Files tab | `key_tab_files` |
| Alt+2 | Switch to Terminal tab | `key_tab_terminal` |
| Alt+3 | Switch to AI tab | `key_tab_ai` |
| Ctrl+Q | Quit | `key_quit` |

## File Browser Context Menu

Right-click on any file or directory:

- **Copy Path** -- copy full path to clipboard
- **Rename...** -- rename dialog with filename pre-selected (without extension)
- **Delete...** -- confirmation dialog, recursive delete for directories
- **New File** -- create untitled file in selected directory
- **New Directory** -- create new_folder in selected directory

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

- **Save multiple connection profiles** (up to 32) with name, host, port, user, key/password auth
- New / Save / Delete / Connect buttons for managing profiles
- Browse remote directories in the file browser
- View remote files with syntax highlighting (read-only, up to 10 MB)
- SSH terminal session opens automatically
- Click path label to change remote directory (sends `cd` to terminal)
- Git status indicators work over SSH
- Disconnect via status bar button
- Toast notifications for connect/disconnect

No `sshfs` or FUSE required. Uses `ssh` commands directly (Midnight Commander-style).

SSH connections use ControlMaster multiplexing for efficient reuse of a single TCP connection. Remote file watching uses `inotifywait` on the server when available (instant notifications), with automatic fallback to periodic polling.

## AI Assistant

Built-in integration with Claude CLI (`claude` command):

- Send prompts and view responses in a dedicated tab
- **Full HTML markdown rendering** -- WebKitWebView with cmark-gfm: tables, code blocks, headings, bold, italic, links, blockquotes, lists, horizontal rules
- **LaTeX to Unicode** -- math expressions like `$E = mc^2$` rendered as `E = mc²`, `$\sum$` as `∑`
- **Theme-aware** -- AI output follows app theme (dark/light CSS)
- Session continuity via `--resume` with automatic session persistence across restarts
- **Session auto-recovery** -- if a saved session expires, automatically retries with a new session
- Token usage tracking (input/output/total)
- Configurable tool access (Read, Edit, Write, Glob, Grep, Bash)
- Optional CWD restriction for security
- **Conversation logging** to `.LLM/prompts.json` with model, session, tokens, timestamps

## Prompt Log Format

Prompts and responses are logged to `{project}/.LLM/prompts.json`:

```json
[
  {"id": 1, "type": "input", "timestamp": "2026-04-09T14:30:22", "session": "abc123", "model": "claude-sonnet-4-6", "text": "explain this code"},
  {"id": 2, "type": "output", "timestamp": "2026-04-09T14:30:28", "session": "abc123", "model": "claude-sonnet-4-6", "input_tokens": 1250, "output_tokens": 830, "total_tokens": 2080, "elapsed_seconds": 6.84, "text": "This code..."}
]
```

## Settings Tabs

| Tab | Options |
|-----|---------|
| GUI | Theme, Font, Font Intensity |
| File Browser | Font, Font Intensity, Show Hidden Files, Gitignored Files (Hide/Show gray) |
| Editor | Font, Font Intensity, Font Weight, Line Spacing, Line Numbers, Highlight Line, Wrap Lines |
| Terminal | Font, Font Intensity |
| Prompt | Font, Send with (Ctrl+Enter/Enter), Show Terminal after send |
| AI Model | Font, Font Intensity, Full Disk Access, Tool toggles, Session resume |
| PDF | Left/Right/Top/Bottom margins (mm), Landscape, Page numbers (None/n/n÷total) |

## Configuration

Settings are stored in `~/.config/vibe-light/settings.conf` (plain text key=value format, locale-safe).

SFTP connections are stored in `~/.config/vibe-light/connections.conf` (INI format, up to 32 profiles). Both files use `0600` permissions.

## Documentation

- [Architecture](docs/architecture.md)
- [Dependencies](docs/dependencies.md)
- [Changelog](docs/changelog.md)

## Author

krse

## License

MIT
