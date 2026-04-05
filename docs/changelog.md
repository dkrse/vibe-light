# Changelog

## v0.3.0 (2026-04-05)

### Features

- **Live file watching (local)** -- file browser and editor automatically update when files are created, deleted, renamed, or modified on disk (uses kernel inotify via GFileMonitor, zero CPU when idle)
- **Live file watching (remote)** -- uses `ssh inotifywait` for instant detection on servers with inotify-tools, with automatic fallback to periodic polling
- **SSH ControlMaster** -- persistent multiplexed SSH connection for all remote operations (ls, cat, stat, inotifywait); near-zero overhead per command
- **Recursive file/directory count** -- status bar shows total count from root directory across all nested subdirectories (computed in background thread)
- **Expanded directory preservation** -- file browser refresh preserves which directories were expanded

### Security Fixes

- **ControlMaster socket** in `$XDG_RUNTIME_DIR` via `mkdtemp` (unpredictable path, user-private)
- **ControlPersist=60** -- SSH master auto-exits after 60s if app crashes (no orphaned sockets)
- **Symlink loop protection** -- `count_entries` uses `lstat` (no symlink following) with depth limit of 64

### Performance Fixes

- **Non-blocking remote refresh** -- remote directory listing runs in background thread (UI never freezes on SSH)
- **Debounced file reload** -- editor reacts only to `CHANGES_DONE_HINT` with 150ms debounce (prevents double reload)
- **Poll accumulation guard** -- overlapping SSH poll tasks prevented by in-flight flags
- **No race conditions** -- background threads receive copied SSH parameters, not pointers to main struct

### Bug Fixes

- **Use-after-free prevention** -- `GCancellable` shared across all async operations, cancelled on window destroy
- **inotifywait stream null check** -- callback verifies stream is still valid before reading
- **ssh_spawn_sync out_len** -- length computed before freeing stdout buffer

## v0.2.0 (2026-04-05)

### Features

- **SFTP/SSH file browsing** -- browse remote directories and view remote files via SSH (no sshfs/FUSE dependency)
- **SFTP connection dialog** -- save up to 32 connection profiles with name, host, port, user, remote path, key/password auth
- **Remote directory chooser** -- click path label to browse and switch directories on the remote server
- **SFTP status bar indicator** -- shows connection info with Disconnect button
- **SSH terminal** -- embedded terminal connects to remote host automatically on SFTP connect
- **Remote file viewing** -- view remote files via `ssh cat` (binary-safe via GSubprocess)
- **GUI font applied globally** -- GUI font setting now affects headerbar, tabs, menus, dialogs, path bar, status bar

### Security Fixes

- **Shell injection fix** -- all SSH commands now use argv arrays (g_spawn_sync / GSubprocess) instead of shell command strings
- **Config file permissions** -- `settings.conf` and `connections.conf` created with `0600` permissions
- **strncpy replaced with g_strlcpy** -- guarantees NUL termination throughout the codebase

### Performance Fixes

- **Non-blocking directory count** -- status bar counts only immediate children (no recursive traversal that could hang on large directories)
- **Async SSH connect** -- connection test runs in a GTask worker thread, UI stays responsive during the 10s timeout
- **File size limit** -- files over 10 MB show "(file too large)" instead of consuming all memory
- **realloc NULL check** -- directory entry collection handles allocation failure gracefully

### Other Fixes

- **Binary file detection** -- remote files use GSubprocess with GBytes for proper binary detection (handles embedded NUL bytes)
- **Removed dead code** -- unused `css` field from ThemeDef struct

## v0.1.0 (2026-04-04)

Initial release.

### Features

- **Three-tab interface:** Files, Terminal, Prompt
- **File browser:** directory navigation with arrow indicators for non-empty directories, root directory locked to dialog selection
- **File viewer:** read-only with blinking cursor, line numbers, current line highlight, wrap lines
- **Embedded terminal:** VTE-based, shell spawned in opened directory, themed colors and cursor
- **Prompt:** text input that sends commands to terminal, configurable send key (Ctrl+Enter or Enter), optional auto-switch to terminal tab, font intensity via GtkTextTag
- **13 themes:** System, Light, Dark, Solarized Light/Dark, Monokai, Gruvbox Light/Dark, Nord, Dracula, Tokyo Night, Catppuccin Latte/Mocha
- **Per-section fonts:** independent font family and size for GUI, File Browser, Editor, Terminal, and Prompt
- **Global font intensity:** single intensity slider (0.3-1.0) affecting all text across the entire application
- **5-tab Settings dialog:** GUI (theme, font, intensity), File Browser (font), Editor (font, line spacing, line numbers, highlight line, wrap lines), Terminal (font), Prompt (font, send key, show terminal)
- **Zoom:** Ctrl+Plus/Ctrl+Minus adjusts all font sizes simultaneously
- **Status bar:** shows cursor position (Ln/Col) when in editor tab
- **Auto-focus:** tab switching automatically focuses the relevant widget
- **Persistent config:** `~/.config/vibe-light/settings.conf` with locale-safe float handling

### Technical

- C17, GTK4, libadwaita, VTE
- Custom GtkTextView subclass for line highlight overlay
- Font intensity: GtkTextTag for editor/prompt, CSS rgba() for GUI/browser, VTE API for terminal
- Locale-safe: manual double parser (no atof), LC_NUMERIC=C for save, integer math for CSS rgba values
- Line numbers as synced-scroll GtkTextView
- Editor: editable=TRUE with key capture to block typing, allow navigation and blinking cursor
- Settings dialog memory managed via GPtrArray + destroy handler (no leak on X close)
- Shell fallback: $SHELL -> getpwuid -> /bin/sh
