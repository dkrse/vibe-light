# Architecture

## Overview

Vibe Light is a lightweight GTK4/libadwaita desktop application written in C17. It provides a three-panel interface: file browser with syntax-highlighted viewer, embedded terminal, and an AI assistant (Claude CLI integration). It includes SFTP/SSH support for remote file browsing and terminal access.

## Tech Stack

- **Language:** C17
- **UI Framework:** GTK 4 (>= 4.0)
- **Theme Management:** libadwaita (>= 1.0)
- **Syntax Highlighting:** GtkSourceView 5 (200+ languages)
- **Terminal:** VTE (vte-2.91-gtk4)
- **SSH/SFTP:** System `ssh` command (no libssh)
- **Build System:** Make + GCC
- **Build Hardening:** `-fstack-protector-strong`, `-fPIE`, `-D_FORTIFY_SOURCE=2`

## Source Structure

```
src/
  main.c         (~16 lines)     Entry point, AdwApplication setup
  window.h       (~95 lines)     VibeWindow struct, public API
  window.c       (~2800 lines)   Window construction, themes, file browser,
                                  file system monitoring (local + remote),
                                  git status integration, lazy loading,
                                  async file loading, syntax highlighting,
                                  font intensity, AI assistant, session restore,
                                  remote dir chooser, prompt handler,
                                  terminal spawn
  ssh.h          (~90 lines)     SSH utility declarations, poll context structs
  ssh.c          (~270 lines)    SSH transport: argv builders, ControlMaster,
                                  spawn_sync, cat_file, dir/file poll threads,
                                  inotify check thread, djb2 hash
  settings.h     (~80 lines)     VibeSettings + SftpConnection structs
  settings.c     (~300 lines)    Load/save config and connections, locale-safe
  actions.h      (~8 lines)      Action setup declaration
  actions.c      (~1200 lines)   Open folder, zoom, tab switch, quit,
                                  Settings dialog (6 tabs), SFTP dialog,
                                  AI model dialog, async SSH connect
```

Total: ~4800 lines of C.

## Key Data Structures

### VibeWindow

Central struct holding all UI state:

- `window` -- GtkApplicationWindow
- `notebook` -- GtkNotebook with 3 tabs (Files, Terminal, AI-model)
- `file_list` -- GtkListBox for file browser
- `file_view` -- GtkSourceView for file content (syntax highlighting, line numbers, current line)
- `file_buffer` -- GtkSourceBuffer for file viewer
- `prompt_intensity_tag` -- GtkTextTag for prompt font intensity
- `terminal` -- VteTerminal
- `ai_output_view` / `ai_output_buffer` -- AI response display
- `ai_proc` / `ai_response_buf` -- Claude subprocess and response accumulator
- `ai_session_id` -- Session ID for `--resume`
- `ai_input_tokens` / `ai_output_tokens` -- Token usage tracking
- `prompt_view` / `prompt_buffer` -- Text input for AI prompt
- `ssh_host`, `ssh_user`, `ssh_port`, `ssh_key`, `ssh_remote_path`, `ssh_mount` -- SSH connection state
- `ssh_ctl_path`, `ssh_ctl_dir` -- SSH ControlMaster socket path and directory
- `cancellable` -- GCancellable for all async operations (cancelled on window destroy)
- `dir_monitor` -- GFileMonitor for local directory watching (inotify)
- `file_monitor` -- GFileMonitor for local file watching (editor auto-reload)
- `inotify_proc`, `inotify_stream` -- Remote directory watching via `ssh inotifywait`
- `remote_dir_poll_id`, `remote_file_poll_id` -- Fallback polling timers for remote
- `current_file` -- Path of file currently open in editor
- `git_status` -- GHashTable mapping relative paths to status characters (M/A/?/D/U/I)
- `git_root` -- Absolute path of git repository root
- `status_label` -- Status bar (recursive file/dir count or SFTP info)
- `sftp_box`, `sftp_label`, `sftp_disconnect_btn` -- SFTP indicator in status bar
- `cursor_label` -- Cursor position (Ln/Col)
- `css_provider` -- Dynamic CSS for themes, fonts, and font intensity

### VibeSettings

Configuration struct with per-section fonts and global controls:

- **Global:** theme, font_intensity (0.3-1.0), line_spacing
- **GUI:** font, size (headerbar, tabs, menus, dialogs, path bar, status bar)
- **File Browser:** font, size, show_hidden, show_gitignored (0=hide, 1=gray)
- **Editor:** font, size, weight, line_numbers, highlight_line, wrap_lines
- **Terminal:** font, size
- **Prompt:** font, size, send_enter, switch_terminal
- **AI Model:** full_disk_access, per-tool toggles (read, edit, write, glob, grep, bash)
- **Session:** last_file, last_cursor_line, last_cursor_col, last_tab
- **Keybindings:** key_open_folder, key_zoom_in, key_zoom_out, key_tab_files, key_tab_terminal, key_tab_ai, key_quit

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
        Right: GtkScrolledWindow > GtkSourceView (.file-viewer)
      Tab 1 "Terminal": GtkScrolledWindow > VteTerminal
      Tab 2 "AI-model": GtkBox
          GtkBox (ai_status_bar: model label + token label)
          GtkSeparator
          GtkPaned (vertical)
            GtkScrolledWindow > GtkTextView (.prompt-view) -- output
            GtkScrolledWindow > GtkTextView (.prompt-view) -- input
    GtkBox (.statusbar)
      GtkLabel (file count / SFTP info)
      GtkBox (sftp_box: label + Disconnect button, hidden when not connected)
      GtkLabel (Ln/Col when in editor)
```

## Theme System

13 themes: 3 system (system, light, dark) + 10 custom.

Custom themes use `build_theme_css()` which generates comprehensive CSS covering: window, box, scrolledwindow, textview (excluding .file-viewer to preserve syntax highlighting), headerbar, notebook tabs, labels, listbox rows, separators, popovers, window controls.

GtkSourceView uses Adwaita/Adwaita-dark style schemes, auto-selected based on theme luminance.

Terminal colors set via `vte_terminal_set_color_foreground/background/cursor`.

Light/dark detection for custom themes uses luminance: `0.299*R + 0.587*G + 0.114*B < 0.5 = dark`

## Syntax Highlighting

GtkSourceView 5 provides syntax highlighting for 200+ languages. Language detection uses:

1. `g_content_type_guess()` -- detects MIME type from filename (handles extensionless files like Makefile, Dockerfile)
2. `gtk_source_language_manager_guess_language()` -- maps filename + content type to a language definition

Style schemes auto-switch between Adwaita (light) and Adwaita-dark based on the active theme. Line numbers and current line highlighting are handled natively by GtkSourceView.

## Font System

Fonts are applied via CSS at two levels:

1. **GUI font** -- applied to `window`, `headerbar`, tabs, popovers, `.path-bar`, `.statusbar label`
2. **Per-section fonts** -- override GUI font for `.file-viewer`, `.file-browser`, `.prompt-view`
3. **Terminal font** -- set via `vte_terminal_set_font()` (Pango)

## Font Intensity

Global font intensity (0.3-1.0) is applied everywhere through multiple mechanisms:

- **Editor** -- CSS `opacity` on `.file-viewer` widget (preserves syntax highlighting colors)
- **Prompt** -- `GtkTextTag` with `foreground-rgba` alpha on prompt_buffer
- **Terminal** -- `vte_terminal_set_color_foreground` with alpha, `vte_terminal_set_color_cursor` with alpha
- **GUI elements** -- CSS `color: rgba(r,g,b,alpha)` on headerbar, notebook tabs, labels, popover items, file browser labels, path bar, status bar
- **Editor/Prompt cursor** -- CSS `caret-color: rgba(r,g,b,alpha)`

CSS rgba values use integer math (`rgba(r,g,b,0.%02d)`) to avoid locale decimal separator issues. Intensity changes apply live while dragging the slider.

## Git Status Integration

File browser shows git status with colored labels (Pango markup):

| Color | Status |
|-------|--------|
| `#e8a838` (orange) | Modified (M) |
| `#73c991` (green) | Staged/Added (A) |
| `#888888` (gray) | Untracked (?) |
| `#f14c4c` (red) | Deleted (D) |
| `#e51400` (bright red) | Conflict (U) |
| `#555555` (dark gray) | Ignored (I) -- when show_gitignored=1 |

### Implementation

- `git rev-parse --show-toplevel` finds the repo root
- `git status --porcelain -u --ignored` fetches all statuses
- Results stored in `GHashTable` (relative path -> status char)
- Directory status: highest-priority status of children (conflict > modified > staged > untracked > deleted). Directories where ALL children are ignored are marked as ignored.
- Runs asynchronously via GTask (background thread)
- Refreshes on directory open and filesystem change events
- Works over SSH: `ssh ... git -C <dir> status --porcelain -u --ignored`

### Gitignored Files

Controlled by `show_gitignored` setting:
- **0 (Hide):** rows with 'I' status are hidden via `gtk_widget_set_visible(FALSE)`
- **1 (Show gray):** rows display with dark gray colored text

Detection handles both `!! file` (individual ignored files) and `!! dir/` (ignored directories) entries. `is_dir_all_ignored()` detects directories where all children are individually ignored (e.g. `build/*.o`).

## File Browser

- Opens directory via `GtkFileDialog` (Ctrl+O) or path label click
- Root directory locked to dialog selection (can't navigate above)
- Subdirectories navigable by clicking (expand/collapse with arrows)
- Directories with children show triangle arrow
- Sorted: directories first, then alphabetical (case-insensitive)
- Remote directories listed via SSH (`ssh ls -1pA`)
- Hidden files (dotfiles) toggled via `show_hidden` setting
- File size limit: 10 MB
- **Lazy loading:** directories with >500 entries show first 500 + "Show N more..." row
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
- **Git status:** `ssh ... git status --porcelain -u --ignored` via `GTask`

### SSH Module (ssh.c)

Core SSH transport extracted into a separate module:

- `ssh_argv_new()` / `ssh_argv_from_params()` -- build SSH command argv arrays
- `ssh_ctl_start()` / `ssh_ctl_stop()` -- ControlMaster lifecycle
- `ssh_spawn_sync()` -- execute SSH command synchronously (no shell)
- `ssh_cat_file()` -- read remote file (binary-safe via GSubprocess)
- `ssh_path_is_remote()` / `ssh_to_remote_path()` -- path conversion
- `ssh_djb2_hash()` -- hash for directory change detection
- `ssh_dir_poll_thread()` / `ssh_file_poll_thread()` / `ssh_inotify_check_thread()` -- GTask thread functions

### SSH ControlMaster

On SFTP connect, a persistent SSH ControlMaster connection is established:

- **Socket location:** `$XDG_RUNTIME_DIR/vibe-ssh-XXXXXX/ctl` (created via `mkdtemp`, user-private)
- **All subsequent SSH commands** (ls, cat, stat, inotifywait, which, git) multiplex through this single TCP connection -- no repeated handshakes, near-zero overhead per command
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

Remote paths use a virtual mount prefix: `/tmp/vibe-light-sftp-{PID}-{user}@{host}`. The `ssh_path_is_remote()` function detects this prefix. `ssh_to_remote_path()` converts virtual paths back to actual remote paths by stripping the prefix and prepending the configured remote root.

### Remote Directory Chooser

Clicking the path label while connected opens a remote directory browser dialog. It lists directories via SSH, supports navigation (double-click to enter, `..` to go up), and a path entry with Go button. Selecting a directory sends `cd` to the existing terminal session and refreshes the file browser.

## Editor (File Viewer)

- GtkSourceView with syntax highlighting for 200+ languages
- Read-only content display with blinking cursor
- `editable=TRUE` with key handler that blocks typing but allows navigation (arrows, Home, End, PgUp/PgDn) and shortcuts (Ctrl+C, Ctrl+A, zoom)
- Line numbers and current line highlight handled by GtkSourceView natively
- Async file loading via GTask -- displays "Loading..." while reading, UI never blocks
- File click places cursor at start and focuses editor
- Binary detection: scan first 8 KB for NUL bytes
- File size limit: 10 MB (shows "(file too large)" for larger files)

## AI Assistant

- Spawns `claude -p "prompt" --output-format json` via GSubprocess
- Session continuity via `--resume SESSION_ID`
- Token tracking: input/output/total with dynamic formatting (k/M suffixes)
- Elapsed time display (seconds or minutes)
- Model name extracted from response JSON
- Configurable tool access: Read, Edit, Write, Glob, Grep, Bash
- CWD restriction: optional system prompt restricting file access to terminal's CWD
- Prompt history saved to `.LLM/prompts.json` (append-only JSON array)

## Terminal

- Shell spawned via `vte_terminal_spawn_async`
- Shell detection: `$SHELL` env -> `getpwuid()->pw_shell` -> `/bin/sh`
- Working directory set to opened folder
- Colors and cursor follow active theme and intensity
- For SFTP: spawns `ssh` instead of local shell

## Session Restore

On window close, the following state is saved to `settings.conf`:
- `last_file` -- path of the open file
- `last_cursor_line` / `last_cursor_col` -- cursor position
- `last_tab` -- active notebook tab (0=Files, 1=Terminal, 2=AI)

On startup, local files are restored (remote files are not auto-restored since the SSH connection is not automatic). Cursor position is restored with bounds checking.

## Configurable Keybindings

7 configurable shortcuts stored in `settings.conf` (GTK accelerator format):

| Setting | Default | Action |
|---------|---------|--------|
| `key_open_folder` | `<Control>o` | Open folder dialog |
| `key_zoom_in` | `<Control>plus` | Zoom in all fonts |
| `key_zoom_out` | `<Control>minus` | Zoom out all fonts |
| `key_tab_files` | `<Alt>1` | Switch to Files tab |
| `key_tab_terminal` | `<Alt>2` | Switch to Terminal tab |
| `key_tab_ai` | `<Alt>3` | Switch to AI tab |
| `key_quit` | `<Control>q` | Close window |

Supports `|` separator for alternative keys (e.g. `<Control>plus|<Control>equal`).

## Tab Focus

`switch-page` signal on notebook auto-focuses:
- Files tab -> editor (file_view)
- Terminal tab -> VteTerminal
- AI-model tab -> prompt text view

## Settings Dialog

6-tab GtkNotebook dialog: GUI, File Browser, Editor, Terminal, Prompt, AI Model.

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
- Lazy loading caps directory display at 500 entries per batch
