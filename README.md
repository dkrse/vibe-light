# Vibe Light

A lightweight file browser, terminal, and prompt application built with GTK4 and libadwaita.

## Features

- **File Browser** - Navigate directories, view files with line numbers and current line highlight
- **Terminal** - Embedded VTE terminal, spawned in the opened directory
- **Prompt** - Send text commands to the terminal with configurable send key
- **13 Themes** - System, Light, Dark, Solarized, Monokai, Gruvbox, Nord, Dracula, Tokyo Night, Catppuccin
- **Per-section Fonts** - Independent font settings for GUI, File Browser, Editor, Terminal, and Prompt
- **Global Font Intensity** - Single intensity control affecting all text across the entire application
- **Settings** - Tabbed dialog (GUI, File Browser, Editor, Terminal, Prompt)

## Dependencies

- GTK 4 (>= 4.0)
- libadwaita (>= 1.0)
- VTE (vte-2.91-gtk4)
- GCC, Make, pkg-config

### Fedora

```bash
sudo dnf install gtk4-devel libadwaita-devel vte291-gtk4-devel gcc make pkg-config
```

### Ubuntu/Debian

```bash
sudo apt install libgtk-4-dev libadwaita-1-dev libvte-2.91-gtk4-dev gcc make pkg-config
```

### Arch

```bash
sudo pacman -S gtk4 libadwaita vte4 gcc make pkg-config
```

## Build

```bash
make
```

## Run

```bash
./build/vibe-light
```

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+O | Open folder |
| Ctrl+Plus / Ctrl+= | Zoom in |
| Ctrl+Minus | Zoom out |
| Ctrl+Enter | Send prompt to terminal (default, configurable) |

## Settings Tabs

| Tab | Options |
|-----|---------|
| GUI | Theme, Font, Font Intensity |
| File Browser | Font |
| Editor | Font, Line Spacing, Line Numbers, Highlight Line, Wrap Lines |
| Terminal | Font |
| Prompt | Font, Send with (Ctrl+Enter/Enter), Show Terminal after send |

## Configuration

Settings are stored in `~/.config/vibe-light/settings.conf` (plain text key=value format, locale-safe).

## Documentation

- [Architecture](docs/architecture.md)
- [Changelog](docs/changelog.md)

## License

MIT
