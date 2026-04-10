#include <adwaita.h>
#include <vte/vte.h>
#include <string.h>
#include "theme.h"

const ThemeDef custom_themes[N_CUSTOM_THEMES] = {
    {"solarized-light", "Solarized Light", "#657b83", "#fdf6e3"},
    {"solarized-dark",  "Solarized Dark",  "#839496", "#002b36"},
    {"monokai",         "Monokai",         "#f8f8f2", "#272822"},
    {"gruvbox-light",   "Gruvbox Light",   "#3c3836", "#fbf1c7"},
    {"gruvbox-dark",    "Gruvbox Dark",    "#ebdbb2", "#282828"},
    {"nord",            "Nord",            "#d8dee9", "#2e3440"},
    {"dracula",         "Dracula",         "#f8f8f2", "#282a36"},
    {"tokyo-night",     "Tokyo Night",     "#a9b1d6", "#1a1b26"},
    {"catppuccin-latte","Catppuccin Latte","#4c4f69", "#eff1f5"},
    {"catppuccin-mocha","Catppuccin Mocha","#cdd6f4", "#1e1e2e"},
};

void build_theme_css(char *buf, size_t bufsize, const char *fg, const char *bg) {
    snprintf(buf, bufsize,
        "window,window.background{background-color:%s;color:%s;}"
        "box{background-color:%s;color:%s;}"
        "scrolledwindow{background-color:%s;}"
        "textview{background-color:%s;}"
        "textview:not(.file-viewer):not(.ai-output) text{background-color:%s;color:%s;}"
        ".file-viewer text{background-color:%s;}"
        ".ai-output text{background-color:%s;}"
        ".titlebar,headerbar{background:%s;color:%s;box-shadow:none;}"
        "headerbar button,headerbar menubutton button,headerbar menubutton"
        "{color:%s;background:transparent;}"
        "headerbar button:hover,headerbar menubutton button:hover"
        "{background:alpha(%s,0.1);}"
        "notebook header{background-color:%s;border-color:alpha(%s,0.2);}"
        "notebook header tab{color:alpha(%s,0.6);background-color:transparent;}"
        "notebook header tab:checked{color:%s;background-color:alpha(%s,0.1);}"
        "notebook header tab:hover{background-color:alpha(%s,0.06);}"
        "notebook stack{background-color:%s;}"
        "label{color:%s;}"
        ".dim-label{color:alpha(%s,0.5);}"
        ".path-bar{background-color:%s;color:%s;}"
        "listbox,list{background-color:%s;color:%s;}"
        "row{background-color:%s;color:%s;}"
        "row:hover{background-color:alpha(%s,0.08);}"
        "row:selected{background-color:alpha(%s,0.15);}"
        "separator{background-color:alpha(%s,0.2);}"
        "paned>separator{background-color:alpha(%s,0.2);}"
        "scrollbar{background-color:transparent;}"
        "popover,popover.menu{background:transparent;box-shadow:none;border:none;}"
        "popover>contents,popover.menu>contents{background-color:%s;color:%s;"
        "  border-radius:12px;border:none;box-shadow:0 2px 8px rgba(0,0,0,0.3);}"
        "popover modelbutton{color:%s;}"
        "popover modelbutton:hover{background-color:alpha(%s,0.15);}"
        "windowcontrols button{color:%s;}",
        bg, fg, bg, fg, bg, bg, bg, fg, bg, bg,
        bg, fg, fg, fg,
        bg, fg, fg, fg, fg, fg, bg,
        fg, fg, bg, fg, bg, fg, bg, fg, fg, fg,
        fg, fg,
        bg, fg, fg, fg, fg);
}

gboolean is_dark_theme(const char *theme) {
    return strcmp(theme, "dark") == 0 ||
           strcmp(theme, "solarized-dark") == 0 ||
           strcmp(theme, "monokai") == 0 ||
           strcmp(theme, "gruvbox-dark") == 0 ||
           strcmp(theme, "nord") == 0 ||
           strcmp(theme, "dracula") == 0 ||
           strcmp(theme, "tokyo-night") == 0 ||
           strcmp(theme, "catppuccin-mocha") == 0;
}

void apply_theme(VibeWindow *win) {
    AdwStyleManager *sm = adw_style_manager_get_default();

    if (strcmp(win->settings.theme, "system") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_DEFAULT);
    } else if (strcmp(win->settings.theme, "light") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_LIGHT);
    } else if (strcmp(win->settings.theme, "dark") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_DARK);
    } else {
        for (size_t i = 0; i < N_CUSTOM_THEMES; i++) {
            if (strcmp(win->settings.theme, custom_themes[i].id) == 0) {
                GdkRGBA c;
                gdk_rgba_parse(&c, custom_themes[i].bg);
                gboolean dark = (0.299 * c.red + 0.587 * c.green + 0.114 * c.blue) < 0.5;
                adw_style_manager_set_color_scheme(sm,
                    dark ? ADW_COLOR_SCHEME_FORCE_DARK : ADW_COLOR_SCHEME_FORCE_LIGHT);
                break;
            }
        }
    }
}

void apply_terminal_colors(VibeWindow *win) {
    const char *fg_hex = NULL, *bg_hex = NULL;

    for (size_t i = 0; i < N_CUSTOM_THEMES; i++) {
        if (strcmp(win->settings.theme, custom_themes[i].id) == 0) {
            fg_hex = custom_themes[i].fg;
            bg_hex = custom_themes[i].bg;
            break;
        }
    }

    if (!fg_hex) {
        if (is_dark_theme(win->settings.theme)) {
            fg_hex = "#d4d4d4"; bg_hex = "#1e1e1e";
        } else {
            fg_hex = "#1e1e1e"; bg_hex = "#ffffff";
        }
    }

    GdkRGBA fg, bg;
    gdk_rgba_parse(&fg, fg_hex);
    gdk_rgba_parse(&bg, bg_hex);
    fg.alpha = win->settings.terminal_font_intensity;
    vte_terminal_set_color_foreground(win->terminal, &fg);
    vte_terminal_set_color_background(win->terminal, &bg);
    vte_terminal_set_color_cursor(win->terminal, &fg);
    vte_terminal_set_color_cursor_foreground(win->terminal, &bg);
}

void apply_font_intensity(VibeWindow *win) {
    double alpha = win->settings.editor_font_intensity;

    GtkTextIter ps, pe;
    gtk_text_buffer_get_bounds(win->prompt_buffer, &ps, &pe);
    if (alpha >= 0.99) {
        gtk_text_buffer_remove_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    } else {
        const char *fg = NULL;
        for (size_t i = 0; i < N_CUSTOM_THEMES; i++) {
            if (strcmp(win->settings.theme, custom_themes[i].id) == 0) {
                fg = custom_themes[i].fg;
                break;
            }
        }

        GdkRGBA color;
        if (fg) {
            gdk_rgba_parse(&color, fg);
        } else if (is_dark_theme(win->settings.theme)) {
            color = (GdkRGBA){1.0, 1.0, 1.0, 1.0};
        } else {
            color = (GdkRGBA){0.0, 0.0, 0.0, 1.0};
        }
        color.alpha = alpha;

        g_object_set(win->prompt_intensity_tag, "foreground-rgba", &color, NULL);
        gtk_text_buffer_apply_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    }
}
