# Architecture

## Overview

Vibe Light is a lightweight GTK4/libadwaita desktop application written in C17. It provides a three-panel interface: file browser with viewer, embedded terminal, and a prompt that sends text to the terminal.

## Tech Stack

- **Language:** C17
- **UI Framework:** GTK 4 (>= 4.0)
- **Theme Management:** libadwaita (>= 1.0)
- **Terminal:** VTE (vte-2.91-gtk4)
- **Build System:** Make + GCC

## Source Structure

```
src/
  main.c         (16 lines)    Entry point, AdwApplication setup
  window.h       (47 lines)    VibeWindow struct, public API
  window.c       (1033 lines)  Window construction, themes, file browser,
                                line numbers, highlight, font intensity,
                                prompt handler, terminal spawn
  settings.h     (47 lines)    VibeSettings struct
  settings.c     (179 lines)   Load/save config, locale-safe double parsing
  actions.h      (8 lines)     Action setup declaration
  actions.c      (456 lines)   Open folder, zoom, Settings dialog (5 tabs)
```

Total: ~1786 lines of C.

## Key Data Structures

### VibeWindow

Central struct holding all UI state:

- `window` — GtkApplicationWindow
- `notebook` — GtkNotebook with 3 tabs (Files, Terminal, Prompt)
- `file_list` — GtkListBox for file browser
- `file_view` — Custom VibeTextView (GtkTextView subclass) for file content
- `file_buffer` — GtkTextBuffer for file viewer
- `intensity_tag` — GtkTextTag for editor font intensity (foreground-rgba alpha)
- `prompt_intensity_tag` — GtkTextTag for prompt font intensity
- `line_numbers` — GtkTextView for line number gutter (synced scroll)
- `highlight_rgba` — Per-instance highlight color (not global)
- `terminal` — VteTerminal
- `prompt_view` / `prompt_buffer` — Text input for prompt
- `status_label` — Status bar (Ln/Col in editor)
- `css_provider` — Dynamic CSS for themes and font intensity

### VibeSettings

Configuration struct with per-section fonts and global controls:

- **Global:** theme, font_intensity (0.3-1.0), line_spacing
- **GUI:** font, size
- **File Browser:** font, size
- **Editor:** font, size, line_numbers, highlight_line, wrap_lines
- **Terminal:** font, size
- **Prompt:** font, size, send_enter, switch_terminal

## Widget Hierarchy

```
GtkApplicationWindow
  GtkBox (vertical)
    GtkNotebook
      Tab 0 "Files": GtkPaned (horizontal)
        Left: GtkBox
          GtkLabel (.path-bar)
          GtkSeparator
          GtkScrolledWindow > GtkListBox (.file-browser)
        Right: GtkBox (horizontal)
          GtkScrolledWindow > GtkTextView (.line-numbers)
          GtkScrolledWindow > VibeTextView (.file-viewer)
      Tab 1 "Terminal": GtkScrolledWindow > VteTerminal
      Tab 2 "Prompt": GtkBox
          GtkScrolledWindow > GtkTextView (.prompt-view)
    GtkBox (.statusbar)
      GtkLabel (Ln/Col when in editor)
```

## Theme System

13 themes: 3 system (system, light, dark) + 10 custom.

Custom themes use `build_theme_css()` which generates comprehensive CSS covering: window, box, scrolledwindow, textview, headerbar, notebook tabs, labels, listbox rows, separators, popovers, window controls.

Terminal colors set via `vte_terminal_set_color_foreground/background/cursor`.

Light/dark detection for custom themes uses luminance: `0.299*R + 0.587*G + 0.114*B < 0.5 = dark`

## Font Intensity

Global font intensity (0.3-1.0) is applied everywhere through multiple mechanisms:

- **Editor** — `GtkTextTag` with `foreground-rgba` alpha on file_buffer
- **Prompt** — `GtkTextTag` with `foreground-rgba` alpha on prompt_buffer
- **Terminal** — `vte_terminal_set_color_foreground` with alpha, `vte_terminal_set_color_cursor` with alpha
- **GUI elements** — CSS `color: rgba(r,g,b,alpha)` on headerbar, notebook tabs, labels, popover items, file browser labels, path bar, status bar
- **Editor/Prompt cursor** — CSS `caret-color: rgba(r,g,b,alpha)`
- **Line numbers** — CSS `color: rgba(r,g,b,alpha*0.3)`

CSS rgba values use integer math (`rgba(r,g,b,0.%02d)`) to avoid locale decimal separator issues.

## Highlight Current Line

Custom GtkTextView subclass (`VibeTextView`) overrides `snapshot()`. After the parent draws all content, a semi-transparent rectangle is drawn over the current line. Dark themes use white overlay (alpha 0.06), light themes use black. Per-instance `highlight_rgba` (not global).

## File Browser

- Opens directory via `GtkFileDialog` (Ctrl+O)
- Root directory locked to dialog selection (can't navigate above)
- Subdirectories navigable by clicking
- Directories with children show `▶` arrow
- Sorted: directories first, then alphabetical (case-insensitive)
- Exponential realloc growth for entry collection (capacity doubles)

## Editor (File Viewer)

- Read-only content display with blinking cursor
- `editable=TRUE` with key handler that blocks typing but allows navigation (arrows, Home, End, PgUp/PgDn) and shortcuts (Ctrl+C, Ctrl+A, zoom)
- Line numbers as separate synced-scroll GtkTextView
- File click places cursor at start and focuses editor

## Terminal

- Shell spawned via `vte_terminal_spawn_async`
- Shell detection: `$SHELL` env → `getpwuid()->pw_shell` → `/bin/sh`
- Working directory set to opened folder
- Colors and cursor follow active theme and intensity

## Prompt

- Text sent to terminal via `vte_terminal_feed_child`
- Configurable send key: Ctrl+Enter (default) or Enter
- Configurable: switch to terminal tab after send
- Font intensity via GtkTextTag (same approach as editor)

## Tab Focus

`switch-page` signal on notebook auto-focuses:
- Files tab → editor (file_view)
- Terminal tab → VteTerminal
- Prompt tab → prompt text view

## Settings Dialog

5-tab GtkNotebook dialog: GUI, File Browser, Editor, Terminal, Prompt.

Memory management: `GPtrArray` with `g_free` tracks heap-allocated callback contexts (`FontCtx`, `IntensityCtx`). Freed on dialog `destroy` signal (handles X button close, Apply, and Cancel).

## Config Persistence

Plain text key=value at `~/.config/vibe-light/settings.conf`.

- **Locale-safe save:** forces `LC_NUMERIC=C` during `fprintf`
- **Locale-safe load:** custom `parse_double()` with manual integer parsing (avoids `atof` locale dependency)
- **Clamping:** font_intensity clamped to 0.3-1.0 after load
- **Backwards compatible:** old per-section intensity keys map to global `font_intensity`
- **Saved on:** settings apply, zoom, window close
- **NOT saved on:** directory navigation (only on close)
