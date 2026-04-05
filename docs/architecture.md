# Architecture

## Overview

Vibe Light is a lightweight GTK4/libadwaita desktop application written in C17. It provides a three-panel interface: file browser with viewer, embedded terminal, and a prompt that sends text to the terminal. It includes SFTP/SSH support for remote file browsing and terminal access.

## Tech Stack

- **Language:** C17
- **UI Framework:** GTK 4 (>= 4.0)
- **Theme Management:** libadwaita (>= 1.0)
- **Terminal:** VTE (vte-2.91-gtk4)
- **SSH/SFTP:** System `ssh` command (no libssh)
- **Build System:** Make + GCC

## Source Structure

```
src/
  main.c         (16 lines)     Entry point, AdwApplication setup
  window.h       (80 lines)     VibeWindow struct, public API
  window.c       (~2300 lines)  Window construction, themes, file browser,
                                 file system monitoring (local + remote),
                                 SSH ControlMaster, inotifywait integration,
                                 line numbers, highlight, font intensity,
                                 SSH/SFTP file operations, remote dir chooser,
                                 prompt handler, terminal spawn
  settings.h     (68 lines)     VibeSettings + SftpConnection structs
  settings.c     (241 lines)    Load/save config and connections, locale-safe
  actions.h      (8 lines)      Action setup declaration
  actions.c      (906 lines)    Open folder, zoom, Settings dialog (5 tabs),
                                 SFTP connection dialog, async SSH connect
```

Total: ~3500 lines of C.

## Key Data Structures

### VibeWindow

Central struct holding all UI state:

- `window` -- GtkApplicationWindow
- `notebook` -- GtkNotebook with 3 tabs (Files, Terminal, Prompt)
- `file_list` -- GtkListBox for file browser
- `file_view` -- Custom VibeTextView (GtkTextView subclass) for file content
- `file_buffer` -- GtkTextBuffer for file viewer
- `intensity_tag` -- GtkTextTag for editor font intensity (foreground-rgba alpha)
- `prompt_intensity_tag` -- GtkTextTag for prompt font intensity
- `line_numbers` -- GtkTextView for line number gutter (synced scroll)
- `highlight_rgba` -- Per-instance highlight color (not global)
- `terminal` -- VteTerminal
- `prompt_view` / `prompt_buffer` -- Text input for prompt
- `ssh_host`, `ssh_user`, `ssh_port`, `ssh_key`, `ssh_remote_path`, `ssh_mount` -- SSH connection state
- `ssh_ctl_path`, `ssh_ctl_dir` -- SSH ControlMaster socket path and directory
- `cancellable` -- GCancellable for all async operations (cancelled on window destroy)
- `dir_monitor` -- GFileMonitor for local directory watching (inotify)
- `file_monitor` -- GFileMonitor for local file watching (editor auto-reload)
- `inotify_proc`, `inotify_stream` -- Remote directory watching via `ssh inotifywait`
- `remote_dir_poll_id`, `remote_file_poll_id` -- Fallback polling timers for remote
- `current_file` -- Path of file currently open in editor
- `status_label` -- Status bar (recursive file/dir count or SFTP info)
- `sftp_box`, `sftp_label`, `sftp_disconnect_btn` -- SFTP indicator in status bar
- `cursor_label` -- Cursor position (Ln/Col)
- `css_provider` -- Dynamic CSS for themes, fonts, and font intensity

### VibeSettings

Configuration struct with per-section fonts and global controls:

- **Global:** theme, font_intensity (0.3-1.0), line_spacing
- **GUI:** font, size (headerbar, tabs, menus, dialogs, path bar, status bar)
- **File Browser:** font, size
- **Editor:** font, size, line_numbers, highlight_line, wrap_lines
- **Terminal:** font, size
- **Prompt:** font, size, send_enter, switch_terminal

### SftpConnection / SftpConnections

Up to 32 saved SSH connection profiles with name, host, port, user, remote_path, use_key, key_path.

## Widget Hierarchy

```
GtkApplicationWindow
  GtkBox (vertical)
    GtkNotebook
      Tab 0 "Files": GtkPaned (horizontal)
        Left: GtkBox
          GtkLabel (.path-bar) -- clickable, opens local/remote dir chooser
          GtkSeparator
          GtkScrolledWindow > GtkListBox (.file-browser)
        Right: GtkBox (horizontal)
          GtkScrolledWindow > GtkTextView (.line-numbers)
          GtkScrolledWindow > VibeTextView (.file-viewer)
      Tab 1 "Terminal": GtkScrolledWindow > VteTerminal
      Tab 2 "Prompt": GtkBox
          GtkScrolledWindow > GtkTextView (.prompt-view)
    GtkBox (.statusbar)
      GtkLabel (file count / SFTP info)
      GtkBox (sftp_box: label + Disconnect button, hidden when not connected)
      GtkLabel (Ln/Col when in editor)
```

## Theme System

13 themes: 3 system (system, light, dark) + 10 custom.

Custom themes use `build_theme_css()` which generates comprehensive CSS covering: window, box, scrolledwindow, textview, headerbar, notebook tabs, labels, listbox rows, separators, popovers, window controls.

Terminal colors set via `vte_terminal_set_color_foreground/background/cursor`.

Light/dark detection for custom themes uses luminance: `0.299*R + 0.587*G + 0.114*B < 0.5 = dark`

## Font System

Fonts are applied via CSS at two levels:

1. **GUI font** -- applied to `window`, `headerbar`, tabs, popovers, `.path-bar`, `.statusbar label`
2. **Per-section fonts** -- override GUI font for `.file-viewer`, `.file-browser`, `.prompt-view`, `.line-numbers`
3. **Terminal font** -- set via `vte_terminal_set_font()` (Pango)

## Font Intensity

Global font intensity (0.3-1.0) is applied everywhere through multiple mechanisms:

- **Editor** -- `GtkTextTag` with `foreground-rgba` alpha on file_buffer
- **Prompt** -- `GtkTextTag` with `foreground-rgba` alpha on prompt_buffer
- **Terminal** -- `vte_terminal_set_color_foreground` with alpha, `vte_terminal_set_color_cursor` with alpha
- **GUI elements** -- CSS `color: rgba(r,g,b,alpha)` on headerbar, notebook tabs, labels, popover items, file browser labels, path bar, status bar
- **Editor/Prompt cursor** -- CSS `caret-color: rgba(r,g,b,alpha)`
- **Line numbers** -- CSS `color: rgba(r,g,b,alpha*0.3)`

CSS rgba values use integer math (`rgba(r,g,b,0.%02d)`) to avoid locale decimal separator issues.

## Highlight Current Line

Custom GtkTextView subclass (`VibeTextView`) overrides `snapshot()`. After the parent draws all content, a semi-transparent rectangle is drawn over the current line. Dark themes use white overlay (alpha 0.06), light themes use black. Per-instance `highlight_rgba` (not global).

## File Browser

- Opens directory via `GtkFileDialog` (Ctrl+O) or path label click
- Root directory locked to dialog selection (can't navigate above)
- Subdirectories navigable by clicking
- Directories with children show triangle arrow
- Sorted: directories first, then alphabetical (case-insensitive)
- Remote directories listed via SSH (`ssh ls -1pA`)
- File size limit: 10 MB
- **Status bar** shows recursive count of all files and directories from root (computed in background thread)

### Live File Watching

The file browser and editor automatically react to filesystem changes:

**Local (inotify):**
- Directory changes detected via `GFileMonitor` (`g_file_monitor_directory`) -- uses kernel inotify, zero CPU when idle
- Open file changes detected via `GFileMonitor` (`g_file_monitor_file`) -- only reacts to `CHANGES_DONE_HINT` to avoid double reload
- All changes debounced (200ms for directory, 150ms for file) to coalesce rapid events
- Expanded directory state preserved across refreshes (`collect_expanded_paths` / `restore_expanded`)

**Remote (SSH):**
- Primary: `ssh inotifywait -m -q -e create,delete,move,modify,attrib` -- persistent SSH subprocess streaming kernel events from the server. Instant detection, zero network traffic when idle.
- Fallback: periodic `ssh ls -1pA` polling (2s interval) with djb2 hash comparison -- only refreshes UI when output actually changes. Used when `inotifywait` is not installed on the server.
- Availability check: `ssh which inotifywait` runs in background thread on directory open; result determines which strategy to use.
- Open file watching: `ssh stat -c %Y` polling (1s interval) checks mtime, reloads only on change.
- All remote operations run in `GTask` background threads -- UI never blocks on SSH.
- Guard flags (`dir_poll_in_flight`, `file_poll_in_flight`) prevent accumulation of overlapping poll tasks on slow networks.

## SFTP/SSH

### Architecture

No FUSE mount, no libssh. Uses system `ssh` command directly, similar to Midnight Commander's FISH protocol:

- **Directory listing:** `ssh user@host -- ls -1pA /path` via `g_spawn_sync` with argv array
- **File reading:** `ssh user@host -- cat /path` via `GSubprocess` + `g_subprocess_communicate` (binary-safe, returns `GBytes` with proper length)
- **Connection test:** `ssh user@host -- echo ok` via `GTask` (async, doesn't block UI)
- **Terminal:** `vte_terminal_spawn_async` with ssh as the command
- **File watching:** `ssh inotifywait -m` via `GSubprocess` with async stdout reading, or `ssh stat -c %Y` / `ssh ls` polling via `GTask`

### SSH ControlMaster

On SFTP connect, a persistent SSH ControlMaster connection is established:

- **Socket location:** `$XDG_RUNTIME_DIR/vibe-ssh-XXXXXX/ctl` (created via `mkdtemp`, user-private)
- **All subsequent SSH commands** (ls, cat, stat, inotifywait, which) multiplex through this single TCP connection -- no repeated handshakes, near-zero overhead per command
- **ControlPersist=60** -- if the app crashes, the master process auto-exits after 60 seconds (prevents orphaned sockets)
- **Cleanup:** `ssh -O exit` sent on disconnect or window destroy; socket file and directory removed

### Security

- All SSH commands use argv arrays (`g_spawn_sync` / `GSubprocess`), never shell command strings -- immune to shell injection
- `StrictHostKeyChecking=accept-new` -- auto-accepts new hosts
- `BatchMode=yes` -- no interactive prompts, fails immediately on auth failure
- ControlMaster socket in `$XDG_RUNTIME_DIR` with `mkdtemp` -- unpredictable path, user-private directory (no symlink attacks)
- ControlPersist timeout prevents orphaned SSH processes on crash
- Connection profiles stored in `~/.config/vibe-light/connections.conf` with `0600` permissions
- Passwords are never saved to disk (only held in memory during the connection dialog)

### Path Mapping

Remote paths use a virtual mount prefix: `/tmp/vibe-light-sftp-{PID}-{user}@{host}`. The `path_is_remote()` function detects this prefix. `to_remote_path()` converts virtual paths back to actual remote paths by stripping the prefix and prepending the configured remote root.

### Remote Directory Chooser

Clicking the path label while connected opens a remote directory browser dialog. It lists directories via SSH, supports navigation (double-click to enter, `..` to go up), and a path entry with Go button. Selecting a directory sends `cd` to the existing terminal session and refreshes the file browser.

## Editor (File Viewer)

- Read-only content display with blinking cursor
- `editable=TRUE` with key handler that blocks typing but allows navigation (arrows, Home, End, PgUp/PgDn) and shortcuts (Ctrl+C, Ctrl+A, zoom)
- Line numbers as separate synced-scroll GtkTextView
- File click places cursor at start and focuses editor
- Binary detection: scan first 8 KB for NUL bytes
- File size limit: 10 MB (shows "(file too large)" for larger files)

## Terminal

- Shell spawned via `vte_terminal_spawn_async`
- Shell detection: `$SHELL` env -> `getpwuid()->pw_shell` -> `/bin/sh`
- Working directory set to opened folder
- Colors and cursor follow active theme and intensity
- For SFTP: spawns `ssh` instead of local shell

## Prompt

- Text sent to terminal via `vte_terminal_feed_child`
- Configurable send key: Ctrl+Enter (default) or Enter
- Configurable: switch to terminal tab after send
- Font intensity via GtkTextTag (same approach as editor)

## Tab Focus

`switch-page` signal on notebook auto-focuses:
- Files tab -> editor (file_view)
- Terminal tab -> VteTerminal
- Prompt tab -> prompt text view

## Settings Dialog

5-tab GtkNotebook dialog: GUI, File Browser, Editor, Terminal, Prompt.

Memory management: `GPtrArray` with `g_free` tracks heap-allocated callback contexts (`FontCtx`, `IntensityCtx`). Freed on dialog `destroy` signal (handles X button close, Apply, and Cancel).

## SFTP Connection Dialog

Two-panel layout: saved connections list (left) + form (right). Supports name, host, port, user, remote path, auth type (password or private key with file browser). Connection test runs async via `GTask` -- button shows "Connecting..." and UI stays responsive.

## Config Persistence

### settings.conf

Plain text key=value at `~/.config/vibe-light/settings.conf`.

- **Locale-safe save:** forces `LC_NUMERIC=C` during `fprintf`
- **Locale-safe load:** custom `parse_double()` with manual integer parsing (avoids `atof` locale dependency)
- **Clamping:** font_intensity clamped to 0.3-1.0 after load
- **Backwards compatible:** old per-section intensity keys map to global `font_intensity`
- **Permissions:** `0600` (user-only read/write)
- **Saved on:** settings apply, zoom, window close

### connections.conf

INI-style sections at `~/.config/vibe-light/connections.conf`.

- Up to 32 connection profiles
- **Permissions:** `0600`
- Passwords are NOT stored

## Memory Safety

- `g_strlcpy` used throughout (guarantees NUL termination)
- `realloc` checked for NULL (falls back gracefully)
- `GSubprocess` for binary-safe SSH file reading
- `GTask` for async operations (prevents use-after-free on dialog close)
- `GCancellable` shared across all async operations -- cancelled on window destroy, all callbacks check before accessing `VibeWindow`
- Background threads receive copied SSH parameters (not pointers to `VibeWindow`) to avoid race conditions with disconnect
- Guard flags prevent accumulation of overlapping async polls on slow networks
- `count_entries` uses `lstat` (no symlink following) and depth limit (64) to prevent infinite recursion
- File size capped at 10 MB to prevent OOM
