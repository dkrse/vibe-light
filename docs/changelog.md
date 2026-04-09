# Changelog

## v0.6.0 (2026-04-09)

### Features

- **WebKitWebView AI output** -- replaced GtkTextView markdown approximation with full HTML rendering via WebKitWebView + cmark-gfm. Proper tables, code blocks, headings, bold, italic, links, blockquotes, lists, strikethrough, horizontal rules.
- **LaTeX to Unicode** -- math expressions converted to Unicode symbols (e.g. `$\sum_{i=1}^{n}$` → `∑ᵢ₌₁ⁿ`, `$E = mc^2$` → `E = mc²`). Supports Greek letters, operators, arrows, set notation, superscripts, subscripts.
- **Theme-aware AI output** -- dark and light CSS variants, automatically switches when app theme changes.
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
