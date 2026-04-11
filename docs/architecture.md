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
- **AI Markdown:** WebKitGTK 6.0 (WebView) + cmark-gfm (Markdown→HTML)
- **PDF Export:** poppler-glib + cairo (page number rendering)
- **Build System:** Make + GCC
- **Build Hardening:** `-fstack-protector-strong`, `-fPIE`, `-D_FORTIFY_SOURCE=2`

## Source Structure

```
src/
  main.c         (~16 lines)     Entry point, AdwApplication setup
  window.h       (~118 lines)    VibeWindow struct, public API
  window.c       (~3221 lines)   Window construction, apply_settings,
                                  file browser, git status integration,
                                  lazy loading, async file loading,
                                  file system monitoring (local + remote),
                                  context menu (rename/delete/new),
                                  drag & drop, toast notifications,
                                  session restore, remote dir chooser,
                                  terminal spawn, status bar,
                                  session info popover
  theme.h        (~23 lines)     Theme definitions, declarations
  theme.c        (~154 lines)    Theme CSS generation, apply_theme,
                                  terminal colors, font intensity
  editor.h       (~17 lines)     Editor function declarations
  editor.c       (~205 lines)    File save, search (Ctrl+F),
                                  go to line (Ctrl+G), undo/redo,
                                  editor key handler
  ai.h           (~16 lines)     AI assistant declarations
  ai.c           (~1330 lines)   LaTeX to Unicode conversion,
                                  markdown rendering (cmark-gfm + WebKit),
                                  AI prompt/response handling, JSON parsing,
                                  session management, token tracking,
                                  streaming JSON line-by-line parser,
                                  tool-use confirmation dialogs
  ssh.h          (~99 lines)     SSH utility declarations, poll context structs
  ssh.c          (~316 lines)    SSH transport: argv builders, ControlMaster,
                                  spawn_sync, cat_file, dir/file poll threads,
                                  inotify check thread, djb2 hash
  prompt_log.h   (~30 lines)     Prompt logging API declarations
  prompt_log.c   (~196 lines)    JSON conversation log: input/output entries
                                  with model, session, tokens, timestamps
  settings.h     (~117 lines)    VibeSettings + SftpConnection structs
  settings.c     (~398 lines)    Load/save config and connections, locale-safe
  actions.h      (~8 lines)      Action setup declaration
  actions.c      (~1793 lines)   Open folder, zoom (per-section), tab switch,
                                  quit, Settings dialog (7 tabs incl. PDF),
                                  SFTP dialog (multi-connection), AI model
                                  dialog (session resume), PDF export with
                                  poppler page numbers, async SSH connect
```

Total: ~8060 lines of C (9 source files + 7 headers).

## Key Data Structures

### VibeWindow

Central struct holding all UI state:

- `window` -- GtkApplicationWindow
- `notebook` -- GtkNotebook with 3 tabs (Files, Terminal, AI-model)
- `file_list` -- GtkListBox for file browser
- `file_view` -- GtkSourceView for file content (syntax highlighting, line numbers, current line)
- `file_buffer` -- GtkSourceBuffer for file viewer
- `file_modified` -- dirty flag for unsaved changes
- `search_bar` / `search_entry` / `search_ctx` -- Ctrl+F search UI and GtkSourceSearchContext
- `prompt_intensity_tag` -- GtkTextTag for prompt font intensity
- `terminal` -- VteTerminal
- `ai_webview` -- WebKitWebView for AI response display (HTML markdown rendering)
- `ai_proc` / `ai_response_buf` -- Claude subprocess and response accumulator
- `ai_stream` -- GDataInputStream for streaming line-by-line stdout reading
- `ai_session_id` -- Session ID for `--resume`
- `ai_session_start` -- Real time (µs) when session was created
- `ai_session_turns` -- Number of prompts sent in this session
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
- `toast_overlay` -- AdwToastOverlay for non-intrusive notifications
- `css_provider` -- Dynamic CSS for themes, fonts, and font intensity

### VibeSettings

Configuration struct with per-section fonts and global controls:

- **Global:** theme, line_spacing
- **GUI:** font, size, font_intensity (headerbar, tabs, menus, dialogs, path bar, status bar)
- **File Browser:** font, size, font_intensity, show_hidden, show_gitignored (0=hide, 1=gray)
- **Editor:** font, size, font_intensity, weight, line_numbers, highlight_line, wrap_lines
- **Terminal:** font, size, font_intensity
- **Prompt:** font, size, send_enter, switch_terminal
- **AI Model:** font_size, font_intensity, full_disk_access, per-tool toggles, ai_markdown, ai_streaming, ai_auto_accept, ai_last_session, ai_session_start, ai_session_turns
- **PDF:** margins (left/right/top/bottom mm), landscape, page_numbers (0=none, 1=n, 2=n/total)
- **Window:** window_width, window_height, last_directory
- **Session:** last_file, last_cursor_line, last_cursor_col, last_tab
- **Keybindings:** key_open_folder, key_zoom_in, key_zoom_out, key_tab_files, key_tab_terminal, key_tab_ai, key_quit, key_print_pdf

### SftpConnection / SftpConnections

Up to 32 saved SSH connection profiles with name, host, port, user, remote_path, use_key, key_path.

## Widget Hierarchy

```
GtkApplicationWindow
  AdwToastOverlay
    GtkBox (vertical)
      GtkNotebook
      Tab 0 "Files": GtkPaned (horizontal)
        Left: GtkBox
          GtkLabel (.path-bar) -- clickable, opens local/remote dir chooser
          GtkSeparator
          GtkScrolledWindow > GtkListBox (.file-browser)
        Right: GtkBox (vertical)
          GtkScrolledWindow > GtkSourceView (.file-viewer)
          GtkBox (.search-bar) -- hidden, Ctrl+F toggles
      Tab 1 "Terminal": GtkScrolledWindow > VteTerminal
      Tab 2 "AI-model": GtkBox
          GtkBox (ai_status_bar: model label + token label)
          GtkSeparator
          GtkPaned (vertical)
            WebKitWebView -- AI output (HTML markdown)
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

Per-section font intensity (0.3-1.0) with independent controls for GUI, File Browser, Editor, Terminal, and AI Model:

- **Editor** -- CSS `opacity` on `.file-viewer` widget (preserves syntax highlighting colors)
- **Prompt** -- `GtkTextTag` with `foreground-rgba` alpha on prompt_buffer
- **Terminal** -- `gtk_widget_set_opacity` on VTE widget + `vte_terminal_set_color_foreground` alpha
- **GUI elements** -- CSS `color: rgba(r,g,b,alpha)` on headerbar, notebook tabs, labels, popover items, path bar, status bar
- **File Browser** -- CSS `color: rgba(r,g,b,alpha)` on `.file-browser label`
- **AI Model** -- CSS `opacity` injected into WebKit HTML body
- **Editor/Prompt cursor** -- CSS `caret-color: rgba(r,g,b,alpha)`

CSS rgba values use integer math with special handling for alpha=1.0 (`rgba(r,g,b,1)` instead of `rgba(r,g,b,0.100)`). Intensity changes apply live while dragging the slider.

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
- **Fully editable** -- type, delete, paste; Ctrl+S saves to disk
- **Undo/Redo** -- Ctrl+Z / Ctrl+Shift+Z (GTK4 native `gtk_text_buffer_undo`/`redo`)
- **Modified indicator** -- window title shows `[modified]` for unsaved changes
- **Search (Ctrl+F)** -- search bar with GtkSourceSearchContext for highlighted matches, prev/next buttons, Enter to advance, Escape to close
- **Go to line (Ctrl+G)** -- dialog to jump to a line number with bounds checking
- Line numbers and current line highlight handled by GtkSourceView natively
- Async file loading via GTask -- displays "Loading..." while reading, UI never blocks
- File click places cursor at start and focuses editor
- Binary detection: scan first 8 KB for NUL bytes
- File size limit: 10 MB (shows "(file too large)" for larger files)
- Remote files are read-only (toast notification on save attempt)

## File Browser Context Menu

Right-click on any file or directory to access:
- **Copy Path** -- copies absolute path to GDK clipboard
- **Rename...** -- dialog with filename pre-filled, extension excluded from selection
- **Delete...** -- confirmation dialog, recursive delete for directories via `delete_recursive()`
- **New File** -- creates `untitled` (auto-numbered if exists) in the selected/current directory
- **New Directory** -- creates `new_folder` (auto-numbered if exists)

All operations show toast notifications on success/failure. Remote files are blocked with toast message.

## Drag & Drop

`GtkDropTarget` on the main window accepts `GDK_TYPE_FILE_LIST`:
- Dropping a directory opens it as root (calls `vibe_window_set_root_directory`)
- Dropping a file opens the file in the editor (sets root to parent dir if needed)

## Toast Notifications

`AdwToastOverlay` wraps the main layout. `vibe_toast()` (static) and `vibe_window_toast()` (public API for actions.c) show 2-second non-intrusive toasts for:
- File save success/failure
- SFTP connect/disconnect
- Context menu operations (rename, delete, copy path, create)
- Remote save attempts

## AI Assistant

- Spawns `claude -p "prompt" --output-format stream-json` (streaming) or `--output-format json` (batch) via GSubprocess
- **Streaming mode** reads stdout line-by-line via `GDataInputStream`, processes `text_delta` events (JS DOM append via `insertAdjacentText` for performance) and `result` events (extract metadata + full cmark re-render).
- **Batch mode** reads all stdout when process exits (legacy behavior)
- Session continuity via `--resume SESSION_ID`
- Token tracking: input/output/total with dynamic formatting (k/M suffixes)
- Elapsed time display (seconds or minutes)
- Model name extracted from response JSON
- Configurable tool access: Read, Edit, Write, Glob, Grep, Bash
- **Tool-use confirmation dialogs** (streaming mode) -- `tool_use` events in stream-json intercepted, modal dialog shown with tool name and key parameters. Auto-accept ON + tool enabled = skip dialog; auto-accept ON + tool disabled = dialog; auto_accept OFF = always dialog. Deny kills the subprocess. In streaming mode all 6 tools are always passed via `--allowed-tools` to prevent CLI stdin blocking; GUI handles the approval flow.
- Markdown toggle: switch between HTML rendering and raw text output (`ai_markdown` setting)
- CWD restriction: optional system prompt restricting file access to terminal's CWD
- **HTML markdown rendering** via WebKitWebView: cmark-gfm parses markdown to HTML (without `CMARK_OPT_UNSAFE` — raw HTML sanitized), rendered with dark/light CSS matching the app theme. LaTeX expressions (`$...$`, `$$...$$`) converted to Unicode (e.g. `\sum` → `∑`, `^2` → `²`). Hardware acceleration disabled for GPU-less environments.
- **Conversation memory cap** -- `ai_conversation_md` GString capped at 256KB; first half trimmed at nearest newline when exceeded. Prevents OOM in long sessions.
- Full support for tables, code blocks, headings, bold, italic, links, blockquotes, lists, strikethrough, horizontal rules
- **LaTeX \text{} support** -- `\text{}`, `\mathrm{}`, `\textbf{}`, `\mathbf{}` render as plain text; `\frac{a}{b}` renders as `(a)/(b)` with recursive processing
- **Session persistence** -- session ID, start time, and turn count saved to settings.conf, restored on startup, auto-cleared when expired
- **Session info popover** -- clicking session label in status bar shows session ID, started date/time, duration, turns, tokens, and mode
- **Error handling** -- stderr captured, exit codes checked, empty/malformed responses reported in conversation
- **Conversation logging** via `prompt_log.c` -- both input and output entries logged to `.LLM/prompts.json` with model, session, token counts, elapsed time. Input deferred until response arrives so model/session are accurate. Writes are atomic (temp file + `rename()`).

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

8 configurable shortcuts stored in `settings.conf` (GTK accelerator format):

| Setting | Default | Action |
|---------|---------|--------|
| `key_open_folder` | `<Control>o` | Open folder dialog |
| `key_zoom_in` | `<Control>plus` | Zoom in active section |
| `key_zoom_out` | `<Control>minus` | Zoom out active section |
| `key_tab_files` | `<Alt>1` | Switch to Files tab |
| `key_tab_terminal` | `<Alt>2` | Switch to Terminal tab |
| `key_tab_ai` | `<Alt>3` | Switch to AI tab |
| `key_quit` | `<Control>q` | Close window |
| `key_print_pdf` | `<Control>p` | Print / Save as PDF |

Supports `|` separator for alternative keys (e.g. `<Control>plus|<Control>equal`).

## Tab Focus

`switch-page` signal on notebook auto-focuses:
- Files tab -> editor (file_view)
- Terminal tab -> VteTerminal
- AI-model tab -> prompt text view

## Dialog Theming

All dialogs use `AdwHeaderBar` as titlebar (via `vibe_dialog_new()` helper or direct `gtk_window_set_titlebar(dialog, adw_header_bar_new())`). This ensures titlebars follow the active theme (dark/light) and match the main window. The deprecated `GtkDialog` is not used.

## Settings Dialog

7-tab GtkNotebook dialog: GUI, File Browser, Editor, Terminal, Prompt, AI Model, PDF.

Memory management: `GPtrArray` with `g_free` tracks heap-allocated callback contexts (`FontCtx`, `IntensityCtx`). Freed on dialog `destroy` signal (handles X button close, Apply, and Cancel).

## SFTP Connection Dialog

Two-panel layout: saved connections list (left) + form (right). Supports up to 32 named connection profiles with host, port, user, remote path, auth type (password or private key with file browser). Buttons: New (clear form), Save (create/update), Delete, Connect. Connection test runs async via `GTask` -- button shows "Connecting..." and UI stays responsive.

## Config Persistence

### settings.conf

Plain text key=value at `~/.config/vibe-light/settings.conf`.

- **Locale-safe save:** forces `LC_NUMERIC=C` during `fprintf`
- **Locale-safe load:** custom `parse_double()` with manual integer parsing (avoids `atof` locale dependency)
- **Clamping:** all font_intensity values clamped to 0.3-1.0 after load
- **Backwards compatible:** old single `font_intensity` key sets all per-section intensities
- **Permissions:** `0600` (user-only read/write)
- **Saved on:** settings apply, zoom, window close

### connections.conf

INI-style sections at `~/.config/vibe-light/connections.conf`.

- Up to 32 connection profiles
- **Permissions:** `0600`
- Passwords are NOT stored

## Memory Safety

- `g_strlcpy` used throughout (guarantees NUL termination) -- no `strcpy` or bare `strncpy` in codebase
- `malloc` return values checked for NULL (graceful cleanup on allocation failure)
- `realloc` checked for NULL (falls back gracefully)
- Indentation strings built via bounds-checked `build_indent()` (prevents buffer overflow with deep directory trees)
- Toast/title strings use `g_strdup_printf` (no fixed-size buffer truncation)
- `GSubprocess` for binary-safe SSH file reading
- `GTask` for async operations (prevents use-after-free on dialog close)
- `GCancellable` shared across all async operations -- cancelled on window destroy, all callbacks check before accessing `VibeWindow`
- Background threads receive copied SSH parameters (not pointers to `VibeWindow`) to avoid race conditions with disconnect
- Guard flags prevent accumulation of overlapping async polls on slow networks
- `count_entries` uses `lstat` (no symlink following) and depth limit (64) to prevent infinite recursion
- `delete_recursive` uses `lstat` to detect symlinks — removes them with `unlink()` instead of following (prevents traversal outside target directory)
- File size capped at 10 MB to prevent OOM
- AI conversation buffer capped at 256KB to prevent OOM in long sessions
- Lazy loading caps directory display at 500 entries per batch
- Font CSS generated via `g_strdup_printf` (heap-allocated, no fixed buffer truncation)
- Prompt log uses atomic writes (temp file + `rename()`) to prevent corruption on crash
- Remote directory `cd` command escapes single quotes to prevent shell injection
- cmark-gfm runs without `CMARK_OPT_UNSAFE` — raw HTML in AI responses is sanitized
