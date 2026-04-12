# Changelog

## v0.10.1 (2026-04-12)

### Features

- **Session browser** -- "Open Session…" button in status bar popover opens a full window listing all sessions with summary, date, model, turns, and token usage (in/out formatted as k/M). Click a session to load it with full conversation reconstruction from JSONL files.
- **Session picker in status bar** -- click "sessions ▾" / "session: xxx… ▾" in status bar to access New Session and Open Session actions
- **Session state restoration** -- loading a session restores conversation text, token counts (input/output including cache), turn count, and model name from JSONL data
- **Sessions directory setting** -- configurable sessions directory in AI Model dialog with Browse… button (GtkFileDialog folder picker). Stored in `ai_sessions_dir` setting. Empty = auto-detected from CWD.
- **Prompt via stdin pipe** -- multi-line prompts sent via stdin instead of `-p` argument, eliminating CLI argument parsing issues with special characters
- **Deferred markdown render** -- streaming text deltas are accumulated silently; markdown render happens once when model finishes (no incremental JS DOM updates)
- **Silent failure error display** -- when claude process exits with no response, stderr is captured and displayed as error message instead of silent freeze

### Bug Fixes

- **Inline rename crash on click away** -- clicking another file/directory during rename caused use-after-free crash. Focus-out callback accessed freed `InlineEditCtx`. Fixed by deferring focus-out cancel via `g_idle_add` and tracking active edit context on `VibeWindow`. `filebrowser_cancel_inline_edit()` is called in `on_file_row_activated` and `refresh_file_list_local` before any row destruction.
- **Gitignored directories not styled** -- newly created directories inside gitignored parents were not marked with italic/gray styling. Root cause: `git status --porcelain -u --ignored` does not report empty ignored directories. Fixed by using `--ignored=matching` which reports ignored directory patterns (e.g. `!! build/`) regardless of contents.
- **Git status styling delay after refresh** -- after file list rebuild, git status styling was only applied after async fetch completed. Now cached git status is applied immediately via `apply_git_status_to_rows()` in `refresh_file_list_local`.
- **Focus-out during rename now cancels** -- clicking away from inline rename entry cancels the rename (restores original name) instead of confirming it.
- **"Thinking…" stuck in status bar** -- when model finished without valid JSON result, status bar stayed on "thinking… Xs". Now falls back to "ready".
- **Session file filter** -- UUID.jsonl filenames (42 chars) were incorrectly filtered out by `nlen < 43` check. Fixed to `< 42`.

### Architecture

- `VibeWindow.inline_edit_ctx` -- new field tracks active inline edit context, enabling safe cancellation from any code path
- `filebrowser_cancel_inline_edit()` -- new public API to safely cancel any active inline edit
- `inline_edit_finish()` no longer frees `InlineEditCtx` -- ctx lifetime managed by `filebrowser_cancel_inline_edit` / `start_inline_edit` to prevent use-after-free from deferred GTK signals
- `get_sessions_dir()` -- helper to resolve sessions directory (custom setting or auto-detected from CWD)
- `SessionEntry` struct -- holds session metadata (sid, summary, mtime, turns, input/output tokens, model)
- AI prompt sent via `G_SUBPROCESS_FLAGS_STDIN_PIPE` + `g_output_stream_write_all` instead of `-p` CLI argument
- `ai_sessions_dir` setting in `VibeSettings` for custom sessions path

## v0.10.0 (2026-04-11)

### Features

- **Zed-style file browser context menu** -- completely rewritten context menu using plain GtkPopover with button widgets (replaces broken GMenu/GAction approach that crashed on activation)
- **Inline rename** -- Rename replaces the row label with an editable GtkEntry in-place (Zed-style). Filename selected without extension. Enter confirms, Escape cancels.
- **Inline new file/folder** -- New File and New Folder insert a temporary row with an inline entry to type the name. Enter creates, Escape cancels. No more auto-generated "untitled" names.
- **Copy Relative Path** -- new context menu action, copies path relative to project root
- **Drag & drop file moving** -- drag files within the file tree to move them between directories. Drop on a directory moves into it; drop on a file moves to the same parent.

### Architecture

- **filebrowser.c / filebrowser.h** -- file browser context menu, inline editing, and internal DnD extracted from window.c into a dedicated module (~470 lines)
- **vibe_window_refresh_current_dir()** -- new public API (was static `refresh_current_dir`)
- Context menu uses direct `g_signal_connect("clicked")` on GtkButtons — no GMenu, GAction, or GSimpleActionGroup (eliminates action resolution timing issues)
- Popover lifecycle: button callbacks copy data to stack locals, call `popdown()`, "closed" signal frees ctx via `g_idle_add(unparent)` — prevents use-after-free
- Inline edit uses `finished` guard flag + signal disconnection on entry/key/focus controllers to prevent re-entrant calls from focus-out during refresh

### Bug Fixes

- **Context menu crash** -- old GMenu+GSimpleActionGroup approach had multiple issues: action group freed before popover resolved actions, popover not unparented on close, multiple FileMenuCtx allocations leaked. Replaced entirely.
- **Inline edit crash on directory operations** -- `vibe_window_refresh_current_dir` destroyed entry widget, triggering focus-out callback on freed context. Fixed by disconnecting all signals before any widget destruction.

## v0.9.0 (2026-04-11)

### Features

- **Interactive streaming output** -- AI responses now stream in real-time (line-by-line via `stream-json` format) instead of waiting for the complete response. Text appears as it's generated via JS DOM append (`insertAdjacentText`), with full cmark markdown re-render on stream completion. Configurable in AI Model dialog (toggle between interactive streaming and batch mode).
- **Tool-use confirmation dialogs** -- in streaming mode, tool_use events are intercepted and shown in a modal GTK dialog with tool name, key parameters (file_path, command, pattern, etc.), and Allow/Deny buttons. Logic: auto_accept ON + tool enabled = auto-allow; auto_accept ON + tool disabled = dialog; auto_accept OFF = always dialog. Deny kills the AI process. Parameters displayed as compact markup labels (no raw JSON), long values truncated at 200 chars.
- **Auto-accept allowed tools** -- new toggle in AI Model dialog; when off, every tool use shows a confirmation dialog instead of auto-accepting
- **Session info popover** -- click session label in status bar (AI tab) to see session ID (selectable), started date/time, duration, turns count, token usage (in/out), and mode (streaming/batch)
- **Session statistics persistence** -- session start time and turn count saved to settings.conf, restored on restart
- **Application icon** -- window icon set via `gtk_window_set_icon_name("com.vibe.light")` with icons at 16/32/48/64/128/256px + scalable SVG
- **Desktop entry** -- `com.vibe.light.desktop` for system application launcher integration
- **Prompt key capture fix** -- key controller set to `GTK_PHASE_CAPTURE` so Enter/Ctrl+Enter handling works reliably in prompt view

### Performance

- **Streaming JS append** -- during streaming, text deltas appended directly to WebKit DOM via `webkit_web_view_evaluate_javascript` + `insertAdjacentText`. Full cmark-gfm re-render only on stream finalize. Eliminates O(n) re-parse on every delta for long conversations.
- **Conversation memory cap** -- `ai_conversation_md` capped at 256KB. When exceeded, first half trimmed at nearest newline with `"(earlier conversation trimmed)"` marker. Prevents unbounded RAM growth in long sessions.
- **Font CSS heap allocation** -- `font_css` switched from 8KB stack buffer to `g_strdup_printf` (heap). Prevents truncation with very long font family names.

### Security

- **XSS prevention** -- removed `CMARK_OPT_UNSAFE` from cmark-gfm parser and renderer. Raw HTML in AI responses is now sanitized (escaped) instead of passed through to WebKit.
- **Symlink-safe delete** -- `delete_recursive()` now checks `S_ISLNK` via `lstat` and removes symlinks with `unlink()` instead of following them. Prevents accidental deletion outside the target directory.
- **Shell escape in remote cd** -- `cd` command sent to terminal now escapes single quotes (`'` → `'\''`) via GString builder instead of bare `snprintf`. Prevents shell injection from directory names containing quotes.
- **Atomic prompt log writes** -- `prompt_log.c` now writes to a `.tmp` file and `rename()`s into place. Prevents `prompts.json` corruption if the app crashes mid-write.

### Bug Fixes

- **Remote refresh race condition** -- `remote_refresh_thread` now copies all SSH parameters (`ssh_host`, `ssh_user`, `ssh_port`, `ssh_key`, `ssh_ctl_path`, `ssh_mount`, `ssh_remote_path`) into `RemoteRefreshCtx` instead of reading from `VibeWindow` in the background thread. Prevents use-after-free if SFTP disconnects during a pending refresh.

### Architecture

- `ai.c` expanded (~1330 lines, was ~863) with streaming JSON parser and tool-use dialogs:
  - `json_extract_string()` / `json_extract_int()` / `json_extract_raw_object()` -- lightweight JSON field extractors with full escape handling (including `\uXXXX` and UTF-16 surrogate pairs, nested object extraction)
  - `on_ai_stream_line_ready()` -- async line-by-line reader, processes `text_delta` events (appends to conversation), `tool_use` events (confirmation dialog), and `result` events (extracts metadata)
  - `ai_tool_use_confirm()` -- modal dialog with nested `g_main_loop` to pause stream reading until user responds
  - `tool_params_summary()` -- extracts human-readable parameter summary per tool type
  - `ai_stream_finalize()` -- extracts session ID, model, tokens from final result event; logs prompt+response
  - In streaming mode, `--allowed-tools` always passes all 6 tools (Read, Edit, Write, Glob, Grep, Bash) so CLI never blocks on stdin; GUI handles confirmation
  - Session expiry detection works in both streaming and batch modes (checks for "No conversation found" in result or empty EOF)
- `VibeWindow` gains `ai_stream` (GDataInputStream), `ai_session_start`, `ai_session_turns` fields
- `VibeSettings` gains `ai_streaming`, `ai_auto_accept`, `ai_session_start`, `ai_session_turns` fields
- Total: ~8530 lines of C (10 source files + 8 headers)

## v0.8.0 (2026-04-10)

### Architecture

- **Source split** -- `window.c` (~4640 lines) split into 4 modules for maintainability:
  - `theme.c` (~154 lines) -- theme CSS, apply_theme, terminal colors, font intensity
  - `editor.c` (~205 lines) -- file save, search, go to line, editor key handler
  - `ai.c` (~863 lines) -- LaTeX→Unicode, markdown rendering, AI prompt/response, JSON parsing
  - `window.c` (~3100 lines) -- file browser, git status, monitoring, window construction
- **Dead code removed** -- ~520 lines of old `#if 0` GtkTextBuffer markdown renderer removed
- **Debug output removed** -- 7 `fprintf(stderr)` debug statements removed from production code

### Bug Fixes

- **malloc NULL check** -- `insert_children()` now checks `malloc()` return for NULL before `memcpy()` (prevented potential segfault on allocation failure)
- **strcat buffer overflow** -- 6 `strcat()` loops building indentation strings replaced with bounds-checked `build_indent()` function (prevented overflow with directory depth >63)
- **LaTeX placeholder O(n²)** -- `ai_refresh_output()` replaced `strstr()`+`g_string_erase()`+`g_string_insert()` loop with single-pass scan using `strncmp()`/`strtoul()` (linear time instead of quadratic)
- **strcpy unsafe** -- 2 `strcpy()` calls in remote directory chooser replaced with `g_strlcpy()`
- **strncpy unsafe** -- 2 `strncpy()` calls replaced with `g_strlcpy()` (guaranteed NUL termination)
- **Format truncation warnings** -- toast and title strings now use `g_strdup_printf()` instead of fixed-size `snprintf()` buffers; path construction buffers increased where needed
- **Zero compiler warnings** -- all `-Wformat-truncation` and `-Wcomment` warnings resolved

### Fedora 43 Support

- **cmark-gfm** -- documented that Fedora does not package `cmark-gfm-devel`; added build-from-source instructions in README and docs/dependencies.md
- **ldconfig** -- documented `/usr/local/lib` registration for dynamic linker

## v0.7.0 (2026-04-10)

### Features

- **Per-section font intensity** -- independent intensity slider (0.3-1.0) for GUI, File Browser, Editor, Terminal, and AI Model (replaces single global slider)
- **Per-section zoom** -- Ctrl+Plus/Ctrl+Minus now zooms only the active section (file browser, editor, terminal, or AI model) instead of all sections at once
- **AI Model font settings** -- new "AI Model" tab in Settings with font family, size, and intensity for AI webview output
- **PDF export** -- "Save as PDF" in menu with file chooser; WebKit renders to temp PDF, poppler+cairo adds page numbers, saves to chosen location
- **PDF settings tab** -- new "PDF" tab in Settings with left/right/top/bottom margins (mm), landscape toggle, page numbers format (None / n / n÷total)
- **Ctrl+P print** -- opens system print dialog for current tab content
- **AI session persistence** -- session ID saved to settings, automatically restored on app restart
- **AI session auto-recovery** -- if a saved session expires ("No conversation found"), automatically clears session and retries with a new one
- **AI session resume UI** -- text entry + Resume button in AI Model dialog to paste and resume any session ID
- **Dynamic title bar** -- shows filename on Files tab, "Terminal" on Terminal tab, model name on AI tab
- **Dynamic status bar** -- AI tab shows model name (left) and full session ID (right) instead of empty status
- **LaTeX rendering improvements** -- `\text{}`, `\mathrm{}`, `\textbf{}`, `\mathbf{}` render as plain text (not subscript); `\frac{num}{den}` renders as `(num)/(den)` with recursive processing; nested `{}` in subscript/superscript correctly paired
- **AI error handling** -- comprehensive error reporting: process failures show stderr, empty responses detected, cancelled operations shown, exit codes checked

### Bug Fixes

- **Font intensity 100% bug** -- `rgba(...,0.100)` produced 10% opacity instead of 100%; fixed to output `rgba(...,1)` when alpha >= 0.995
- **Terminal font intensity** -- was not responding; fixed by adding `gtk_widget_set_opacity` on terminal widget
- **Tab status bar mismatch** -- GTK `switch-page` signal fires before page switch; fixed by passing `page_num` directly instead of reading `get_current_page`
- **Title bar showing wrong content** -- file load/save/modify callbacks set title independently of active tab; centralized all title updates through `update_status_bar`
- **AI model showing "unknown"** -- when JSON response lacks `modelUsage`, preserves previous model name instead of overwriting with "unknown"

### Architecture

- **poppler-glib** added as dependency for PDF page number rendering via cairo
- `update_status_bar` split into `update_status_bar_page(win, page)` + `update_status_bar(win)` wrapper
- `latex_to_unicode` handles `\text{}` family and `\frac{}{}` with recursive processing and nesting-aware brace matching
- `on_ai_communicate_done` rewritten with comprehensive error handling: checks exit status, stderr capture, invalid session detection with auto-retry
- `VibeSettings.font_intensity` replaced by 5 per-section intensity fields

### Dependencies

- **Added:** poppler-glib (`libpoppler-glib-dev`)

## v0.6.0 (2026-04-09)

### Features

- **WebKitWebView AI output** -- replaced GtkTextView markdown approximation with full HTML rendering via WebKitWebView + cmark-gfm. Proper tables, code blocks, headings, bold, italic, links, blockquotes, lists, strikethrough, horizontal rules.
- **LaTeX to Unicode** -- math expressions converted to Unicode symbols (e.g. `$\sum_{i=1}^{n}$` → `∑ᵢ₌₁ⁿ`, `$E = mc^2$` → `E = mc²`). Supports Greek letters, operators, arrows, set notation, superscripts, subscripts.
- **Theme-aware AI output** -- dark and light CSS variants, automatically switches when app theme changes.
- **Markdown rendering toggle** -- `ai_markdown` setting to switch between HTML markdown rendering and raw text output in AI Model dialog
- **GPU-less rendering** -- WebKit hardware acceleration disabled (`WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER`) for environments without GPU access.

### Bug Fixes

- **JSON parser fixes** -- fixed broken closing-quote detection for strings containing `\\"`, added `\uXXXX` Unicode escape support (including UTF-16 surrogate pairs), added missing escape sequences (`\r`, `\b`, `\f`, `\/`).
- **stderr contamination fix** -- changed subprocess flags from `STDERR_MERGE` to `STDERR_SILENCE` so claude CLI stderr output no longer corrupts JSON response parsing.

### Architecture

- **WebKitGTK 6.0** added as dependency for AI markdown rendering
- **cmark-gfm** renders markdown to HTML (with table, strikethrough, autolink extensions)
- Old GtkTextBuffer-based markdown renderer (`ensure_md_tags`, `render_cmark`, `insert_markdown`) removed

### Dependencies

- **Added:** WebKitGTK 6.0 (`webkitgtk-6.0`), cmark-gfm (`libcmark-gfm`)

## v0.5.0 (2026-04-09)

### Features

- **File editing with save** -- editor is now fully editable, Ctrl+S saves to disk, title shows `[modified]` indicator
- **Undo/Redo** -- Ctrl+Z undo, Ctrl+Shift+Z / Ctrl+Y redo (GTK4 native text buffer undo)
- **Find in editor (Ctrl+F)** -- search bar with GtkSourceSearchContext highlighting, prev/next navigation, Enter to find next, Escape to close
- **Go to line (Ctrl+G)** -- dialog to jump to a specific line number
- **File browser context menu** -- right-click for Copy Path, Rename, Delete, New File, New Directory
- **Drag & drop** -- drop files or folders from file manager onto the window to open them
- **Markdown rendering in AI output** -- code blocks (dark background), **bold**, *italic*, `inline code`, # headings, [links](url) via GtkTextTags
- **Toast notifications** -- AdwToast feedback for save, SFTP connect/disconnect, rename, delete, copy path, errors
- **SFTP multiple connections** -- New button in SFTP dialog to create additional connection profiles, clear form for new entry
- **Conversation logging** -- prompts.json now logs both input and output entries with model, session ID, token counts, and elapsed time. Input logged after response so model/session are correct.

### Architecture

- **Prompt log module** -- `prompt_log.c` / `prompt_log.h` extracted from window.c. Handles JSON escaping, append-only log file, input/output entry types with full metadata.
- **AdwHeaderBar on all dialogs** -- Settings, SFTP, AI Model, Remote Dir, Go to Line, Rename, Delete dialogs now use AdwHeaderBar titlebar (follows app theme, no light panel mismatch)
- **GtkDialog removed** -- all dialogs converted from deprecated GtkDialog to plain GtkWindow + AdwHeaderBar with custom button layouts

## v0.4.0 (2026-04-09)

### Features

- **Syntax highlighting** -- GtkSourceView 5 integration with 200+ language support (C, Python, Rust, Go, Makefile, JSON, YAML, assembly, and more). Language auto-detected from filename and content type.
- **Git status indicators** -- file browser shows colored labels for modified (orange), staged (green), untracked (gray), deleted (red), conflict (bright red), and ignored (dark gray) files. Works locally and over SSH/SFTP.
- **Gitignored file handling** -- configurable in Settings > File Browser: hide (default) or show in gray. Detects both `!! file` entries and directories where all children are ignored.
- **Show/hide hidden files** -- toggle dotfile visibility in Settings > File Browser. Works locally and over SSH.
- **Lazy loading** -- directories with >500 entries show first batch + "Show N more..." row. Prevents UI freeze on large directories.
- **Async file loading** -- files load in GTask background threads with "Loading..." indicator. UI never blocks, guard checks prevent stale loads.
- **Configurable keyboard shortcuts** -- 7 shortcuts configurable via `settings.conf`: open folder, zoom in/out, tab switching (Alt+1/2/3), quit. Supports `|` separator for alternative keys.
- **Session restore** -- saves and restores last open file, cursor position (line + column), and active tab across sessions. Only restores local files.
- **Tab switching shortcuts** -- Alt+1 (Files), Alt+2 (Terminal), Alt+3 (AI) actions with configurable keybindings.
- **Quit action** -- Ctrl+Q closes the window (configurable).
- **Live intensity preview** -- font intensity slider updates editor/UI in real-time while dragging.

### Architecture

- **SSH module extracted** -- `ssh.c` / `ssh.h` contain all SSH transport functions: argv builders, ControlMaster lifecycle, spawn_sync, cat_file, poll threads. Decoupled from window.c.
- **GtkSourceView replaces custom code** -- removed ~130 lines of custom line numbers (update_line_numbers, Pango measurement, synced scroll), custom VibeTextView subclass (snapshot override for line highlight), and related widget hierarchy. All handled natively by GtkSourceView.
- **Font intensity via CSS opacity** -- editor uses CSS `opacity` on `.file-viewer` widget instead of GtkTextTag (preserves syntax highlighting colors).
- **Content type detection** -- `g_content_type_guess()` enables language detection for extensionless files (Makefile, Dockerfile, etc.).

### Security

- **Build hardening flags** -- `-fstack-protector-strong`, `-fPIE`/`-pie`, `-D_FORTIFY_SOURCE=2`
- **Stack buffer replaced** -- 16KB CSS stack buffer replaced with heap allocation (`g_strconcat`)

### Dependencies

- **Added:** GtkSourceView 5 (`gtksourceview-5`)

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
