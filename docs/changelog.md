# Changelog

## v0.1.0 (2026-04-04)

Initial release.

### Features

- **Three-tab interface:** Files, Terminal, Prompt
- **File browser:** directory navigation with `▶` indicators for non-empty directories, root directory locked to dialog selection
- **File viewer:** read-only with blinking cursor, line numbers, current line highlight, wrap lines
- **Embedded terminal:** VTE-based, shell spawned in opened directory, themed colors and cursor
- **Prompt:** text input that sends commands to terminal, configurable send key (Ctrl+Enter or Enter), optional auto-switch to terminal tab, font intensity via GtkTextTag
- **13 themes:** System, Light, Dark, Solarized Light/Dark, Monokai, Gruvbox Light/Dark, Nord, Dracula, Tokyo Night, Catppuccin Latte/Mocha
- **Per-section fonts:** independent font family and size for GUI, File Browser, Editor, Terminal, and Prompt
- **Global font intensity:** single intensity slider (0.3-1.0) affecting all text, cursors, and UI elements across the entire application
- **5-tab Settings dialog:** GUI (theme, font, intensity), File Browser (font), Editor (font, line spacing, line numbers, highlight line, wrap lines), Terminal (font), Prompt (font, send key, show terminal)
- **Zoom:** Ctrl+Plus/Ctrl+Minus adjusts all font sizes simultaneously
- **Status bar:** shows cursor position (Ln/Col) when in editor tab
- **Auto-focus:** tab switching automatically focuses the relevant widget
- **Persistent config:** `~/.config/vibe-light/settings.conf` with locale-safe float handling

### Technical

- C17, GTK4, libadwaita, VTE (~1786 lines)
- Custom GtkTextView subclass for line highlight overlay
- Font intensity: GtkTextTag for editor/prompt, CSS rgba() for GUI/browser, VTE API for terminal
- Locale-safe: manual double parser (no atof), LC_NUMERIC=C for save, integer math for CSS rgba values
- Line numbers as synced-scroll GtkTextView
- Editor: editable=TRUE with key capture to block typing, allow navigation and blinking cursor
- Exponential realloc for directory entry collection
- Settings dialog memory managed via GPtrArray + destroy handler (no leak on X close)
- Shell fallback: $SHELL -> getpwuid -> /bin/sh
- Clean build with zero warnings (-Wall -Wextra)
