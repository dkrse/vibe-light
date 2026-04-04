#define _DEFAULT_SOURCE
#include <adwaita.h>
#include "window.h"
#include "actions.h"
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>

/* ── Theme CSS ── */

typedef struct {
    const char *id;
    const char *name;
    const char *fg;
    const char *bg;
    const char *css;
} ThemeDef;

static const ThemeDef custom_themes[] = {
    {"solarized-light", "Solarized Light", "#657b83", "#fdf6e3", NULL},
    {"solarized-dark",  "Solarized Dark",  "#839496", "#002b36", NULL},
    {"monokai",         "Monokai",         "#f8f8f2", "#272822", NULL},
    {"gruvbox-light",   "Gruvbox Light",   "#3c3836", "#fbf1c7", NULL},
    {"gruvbox-dark",    "Gruvbox Dark",    "#ebdbb2", "#282828", NULL},
    {"nord",            "Nord",            "#d8dee9", "#2e3440", NULL},
    {"dracula",         "Dracula",         "#f8f8f2", "#282a36", NULL},
    {"tokyo-night",     "Tokyo Night",     "#a9b1d6", "#1a1b26", NULL},
    {"catppuccin-latte","Catppuccin Latte","#4c4f69", "#eff1f5", NULL},
    {"catppuccin-mocha","Catppuccin Mocha","#cdd6f4", "#1e1e2e", NULL},
};
#define N_CUSTOM_THEMES (sizeof(custom_themes) / sizeof(custom_themes[0]))

/* Build comprehensive CSS for a custom theme covering ALL widgets */
static void build_theme_css(char *buf, size_t bufsize, const char *fg, const char *bg) {
    snprintf(buf, bufsize,
        /* base */
        "window,window.background{background-color:%s;color:%s}"
        "box{background-color:%s;color:%s}"
        "scrolledwindow{background-color:%s}"
        /* textview */
        "textview{background-color:%s}"
        "textview text{background-color:%s;color:%s}"
        /* headerbar */
        ".titlebar,headerbar{background:%s;color:%s;box-shadow:none}"
        "headerbar button,headerbar menubutton button,headerbar menubutton"
        "{color:%s;background:transparent}"
        "headerbar button:hover,headerbar menubutton button:hover"
        "{background:alpha(%s,0.1)}"
        /* notebook tabs */
        "notebook header{background-color:%s;border-color:alpha(%s,0.2)}"
        "notebook header tab{color:alpha(%s,0.6);background-color:transparent}"
        "notebook header tab:checked{color:%s;background-color:alpha(%s,0.1)}"
        "notebook header tab:hover{background-color:alpha(%s,0.06)}"
        "notebook stack{background-color:%s}"
        /* labels */
        "label{color:%s}"
        ".dim-label{color:alpha(%s,0.5)}"
        ".path-bar{background-color:%s;color:%s}"
        /* listbox */
        "listbox,list{background-color:%s;color:%s}"
        "row{background-color:%s;color:%s}"
        "row:hover{background-color:alpha(%s,0.08)}"
        "row:selected{background-color:alpha(%s,0.15)}"
        /* separator, paned */
        "separator{background-color:alpha(%s,0.2)}"
        "paned>separator{background-color:alpha(%s,0.2)}"
        /* scrollbar */
        "scrollbar{background-color:transparent}"
        /* popover */
        "popover,popover.menu{background:transparent;box-shadow:none;border:none}"
        "popover>contents,popover.menu>contents{background-color:%s;color:%s;"
        "  border-radius:12px;border:none;box-shadow:0 2px 8px rgba(0,0,0,0.3)}"
        "popover modelbutton{color:%s}"
        "popover modelbutton:hover{background-color:alpha(%s,0.15)}"
        /* window controls */
        "windowcontrols button{color:%s}",
        bg, fg,      /* window */
        bg, fg,      /* box */
        bg,          /* scrolledwindow */
        bg,          /* textview */
        bg, fg,      /* textview text */
        bg, fg,      /* headerbar */
        fg,          /* headerbar buttons */
        fg,          /* headerbar hover */
        bg, fg,      /* notebook header */
        fg,          /* tab normal */
        fg, fg,      /* tab checked */
        fg,          /* tab hover */
        bg,          /* notebook stack */
        fg,          /* label */
        fg,          /* dim-label */
        bg, fg,      /* path-bar */
        bg, fg,      /* listbox */
        bg, fg,      /* row */
        fg,          /* row:hover */
        fg,          /* row:selected */
        fg,          /* separator */
        fg,          /* paned separator */
        bg, fg,      /* popover */
        fg,          /* popover item */
        fg,          /* popover hover */
        fg);         /* windowcontrols */
}

/* ── Apply theme ── */

static void apply_theme(VibeWindow *win) {
    AdwStyleManager *sm = adw_style_manager_get_default();

    if (strcmp(win->settings.theme, "system") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_DEFAULT);
    } else if (strcmp(win->settings.theme, "light") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_LIGHT);
    } else if (strcmp(win->settings.theme, "dark") == 0) {
        adw_style_manager_set_color_scheme(sm, ADW_COLOR_SCHEME_FORCE_DARK);
    } else {
        /* custom theme: determine light/dark from bg luminance */
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

static gboolean is_dark_theme(const char *theme);
static void update_highlight(VibeWindow *win);

static void apply_terminal_colors(VibeWindow *win) {
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
    fg.alpha = win->settings.font_intensity;
    vte_terminal_set_color_foreground(win->terminal, &fg);
    vte_terminal_set_color_background(win->terminal, &bg);
    vte_terminal_set_color_cursor(win->terminal, &fg);
    vte_terminal_set_color_cursor_foreground(win->terminal, &bg);
}

/* ── Line numbers (ported from notes-desktop) ── */

static void update_line_numbers(VibeWindow *win) {
    int lines = gtk_text_buffer_get_line_count(win->file_buffer);
    if (lines == win->cached_line_count) return;
    win->cached_line_count = lines;

    GString *str = g_string_sized_new((gsize)(lines * 4));
    for (int i = 1; i <= lines; i++) {
        if (i > 1) g_string_append_c(str, '\n');
        g_string_append_printf(str, "%d", i);
    }
    GtkTextBuffer *ln_buf = gtk_text_view_get_buffer(win->line_numbers);
    gtk_text_buffer_set_text(ln_buf, str->str, -1);
    g_string_free(str, TRUE);

    /* measure width needed */
    int digits = 1, n = lines;
    while (n >= 10) { digits++; n /= 10; }
    if (digits < 2) digits = 2;

    char sample[16];
    memset(sample, '9', (size_t)digits);
    sample[digits] = '\0';

    PangoLayout *layout = gtk_widget_create_pango_layout(GTK_WIDGET(win->line_numbers), sample);
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, win->settings.editor_font);
    pango_font_description_set_size(fd, win->settings.editor_font_size * PANGO_SCALE);
    pango_layout_set_font_description(layout, fd);
    int pw, ph;
    pango_layout_get_pixel_size(layout, &pw, &ph);
    (void)ph;
    pango_font_description_free(fd);
    g_object_unref(layout);

    gtk_widget_set_size_request(GTK_WIDGET(win->line_numbers), pw + 12, -1);
}

/* ── Font intensity via GtkTextTag (ported from notes-desktop) ── */

static gboolean is_dark_theme(const char *theme) {
    return strcmp(theme, "dark") == 0 ||
           strcmp(theme, "solarized-dark") == 0 ||
           strcmp(theme, "monokai") == 0 ||
           strcmp(theme, "gruvbox-dark") == 0 ||
           strcmp(theme, "nord") == 0 ||
           strcmp(theme, "dracula") == 0 ||
           strcmp(theme, "tokyo-night") == 0 ||
           strcmp(theme, "catppuccin-mocha") == 0;
}

static void apply_font_intensity(VibeWindow *win) {
    double alpha = win->settings.font_intensity;
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(win->file_buffer, &start, &end);

    if (alpha >= 0.99) {
        gtk_text_buffer_remove_tag(win->file_buffer, win->intensity_tag, &start, &end);
        return;
    }

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

    g_object_set(win->intensity_tag, "foreground-rgba", &color, NULL);
    gtk_text_buffer_apply_tag(win->file_buffer, win->intensity_tag, &start, &end);

    /* same for prompt buffer */
    GtkTextIter ps, pe;
    gtk_text_buffer_get_bounds(win->prompt_buffer, &ps, &pe);
    if (alpha >= 0.99) {
        gtk_text_buffer_remove_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    } else {
        g_object_set(win->prompt_intensity_tag, "foreground-rgba", &color, NULL);
        gtk_text_buffer_apply_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    }
}

static gboolean intensity_idle_cb(gpointer data) {
    VibeWindow *win = data;
    win->intensity_idle_id = 0;
    if (win->settings.font_intensity < 0.99)
        apply_font_intensity(win);
    return G_SOURCE_REMOVE;
}

static void update_status_bar(VibeWindow *win);

static void on_cursor_moved(GtkTextBuffer *buffer, GParamSpec *pspec, gpointer data) {
    (void)buffer; (void)pspec;
    VibeWindow *win = data;
    update_highlight(win);
    update_status_bar(win);
}

static void on_file_buffer_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    VibeWindow *win = data;
    if (win->settings.show_line_numbers)
        update_line_numbers(win);
    if (win->settings.font_intensity < 0.99 && win->intensity_idle_id == 0)
        win->intensity_idle_id = g_idle_add(intensity_idle_cb, win);
}

static void on_prompt_buffer_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    VibeWindow *win = data;
    if (win->settings.font_intensity < 0.99) {
        GtkTextIter ps, pe;
        gtk_text_buffer_get_bounds(win->prompt_buffer, &ps, &pe);
        gtk_text_buffer_apply_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    }
}

/* ── Highlight current line (ported from notes-desktop) ── */

#define VIBE_TYPE_TEXT_VIEW (vibe_text_view_get_type())
G_DECLARE_FINAL_TYPE(VibeTextView, vibe_text_view, VIBE, TEXT_VIEW, GtkTextView)

struct _VibeTextView {
    GtkTextView parent;
    VibeWindow *win;
};

G_DEFINE_TYPE(VibeTextView, vibe_text_view, GTK_TYPE_TEXT_VIEW)

static void vibe_text_view_snapshot(GtkWidget *widget, GtkSnapshot *snapshot) {
    VibeTextView *self = VIBE_TEXT_VIEW(widget);
    VibeWindow *win = self->win;

    GTK_WIDGET_CLASS(vibe_text_view_parent_class)->snapshot(widget, snapshot);

    if (win && win->settings.highlight_current_line) {
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_line(win->file_buffer, &iter, win->highlight_line);

        GdkRectangle rect;
        gtk_text_view_get_iter_location(GTK_TEXT_VIEW(widget), &iter, &rect);

        int wx, wy;
        gtk_text_view_buffer_to_window_coords(GTK_TEXT_VIEW(widget),
            GTK_TEXT_WINDOW_WIDGET, rect.x, rect.y, &wx, &wy);

        int view_width = gtk_widget_get_width(widget);
        int h = rect.height > 0 ? rect.height : win->settings.editor_font_size + 4;
        int extra = (int)((win->settings.line_spacing - 1.0) * win->settings.editor_font_size * 0.5);
        if (extra < 0) extra = 0;

        graphene_rect_t area = GRAPHENE_RECT_INIT(0, wy - extra, view_width, h + extra * 2);
        gtk_snapshot_append_color(snapshot, &win->highlight_rgba, &area);
    }
}

static void vibe_text_view_class_init(VibeTextViewClass *klass) {
    GTK_WIDGET_CLASS(klass)->snapshot = vibe_text_view_snapshot;
}

static void vibe_text_view_init(VibeTextView *self) { (void)self; }

static void update_highlight(VibeWindow *win) {
    GtkTextMark *mark = gtk_text_buffer_get_insert(win->file_buffer);
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_mark(win->file_buffer, &cursor, mark);
    win->highlight_line = gtk_text_iter_get_line(&cursor);
    gtk_widget_queue_draw(GTK_WIDGET(win->file_view));
}

static void apply_highlight_color(VibeWindow *win) {
    if (is_dark_theme(win->settings.theme))
        win->highlight_rgba = (GdkRGBA){1.0, 1.0, 1.0, 0.06};
    else
        win->highlight_rgba = (GdkRGBA){0.0, 0.0, 0.0, 0.06};
}

/* ── Apply all settings ── */

void vibe_window_apply_settings(VibeWindow *win) {
    apply_theme(win);

    /* terminal font */
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, win->settings.terminal_font);
    pango_font_description_set_size(fd, win->settings.terminal_font_size * PANGO_SCALE);
    vte_terminal_set_font(win->terminal, fd);
    pango_font_description_free(fd);

    /* wrap mode (editor only) */
    gtk_text_view_set_wrap_mode(win->file_view,
        win->settings.wrap_lines ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);

    /* line spacing */
    int extra = (int)((win->settings.line_spacing - 1.0) * win->settings.editor_font_size * 0.5);
    if (extra < 0) extra = 0;
    gtk_text_view_set_pixels_above_lines(win->file_view, extra);
    gtk_text_view_set_pixels_below_lines(win->file_view, extra);
    gtk_text_view_set_pixels_above_lines(win->prompt_view, extra);
    gtk_text_view_set_pixels_below_lines(win->prompt_view, extra);
    gtk_text_view_set_pixels_above_lines(win->line_numbers, extra);
    gtk_text_view_set_pixels_below_lines(win->line_numbers, extra);

    /* line numbers visibility */
    gtk_widget_set_visible(win->ln_scrolled, win->settings.show_line_numbers);
    win->cached_line_count = 0;
    if (win->settings.show_line_numbers)
        update_line_numbers(win);

    /* highlight */
    apply_highlight_color(win);
    update_highlight(win);

    /* font intensity (editor) */
    apply_font_intensity(win);

    /* terminal colors */
    apply_terminal_colors(win);

    /* CSS: theme + per-section fonts */
    char theme_css[4096] = "";
    const ThemeDef *td = NULL;
    for (size_t i = 0; i < N_CUSTOM_THEMES; i++) {
        if (strcmp(win->settings.theme, custom_themes[i].id) == 0) {
            td = &custom_themes[i];
            break;
        }
    }
    if (td) {
        build_theme_css(theme_css, sizeof(theme_css), td->fg, td->bg);
    }

    double alpha = win->settings.font_intensity;

    /* compute rgba color string for intensity */
    const char *fg_hex = NULL;
    for (size_t i = 0; i < N_CUSTOM_THEMES; i++) {
        if (strcmp(win->settings.theme, custom_themes[i].id) == 0) {
            fg_hex = custom_themes[i].fg;
            break;
        }
    }
    if (!fg_hex)
        fg_hex = is_dark_theme(win->settings.theme) ? "#d4d4d4" : "#1e1e1e";

    GdkRGBA fg_rgba;
    gdk_rgba_parse(&fg_rgba, fg_hex);
    int r = (int)(fg_rgba.red * 255);
    int g = (int)(fg_rgba.green * 255);
    int b = (int)(fg_rgba.blue * 255);
    int a_full = (int)(alpha * 100);
    int a_dim = (int)(alpha * 30);
    char fg_full[64], fg_dim[64];
    /* use integer math to avoid locale comma in decimals */
    snprintf(fg_full, sizeof(fg_full), "rgba(%d,%d,%d,0.%02d)", r, g, b, a_full);
    snprintf(fg_dim, sizeof(fg_dim), "rgba(%d,%d,%d,0.%02d)", r, g, b, a_dim);

    char font_css[4096];
    snprintf(font_css, sizeof(font_css),
             ".file-viewer{font-family:%s;font-size:%dpt;caret-color:%s}"
             ".file-browser{font-family:%s;font-size:%dpt}"
             ".file-browser label{color:%s}"
             ".prompt-view{font-family:%s;font-size:%dpt;caret-color:%s}"
             ".path-bar{color:%s}"
             ".statusbar label{color:%s}"
             "headerbar{color:%s}"
             "headerbar button,headerbar menubutton button,headerbar menubutton{color:%s}"
             "notebook header tab{color:%s}"
             "popover modelbutton{color:%s}"
             "popover>contents,popover.menu>contents{color:%s}"
             "window label{color:%s}"
             "window checkbutton label{color:%s}"
             ".line-numbers,.line-numbers text{font-family:%s;font-size:%dpt;color:%s}",
             win->settings.editor_font, win->settings.editor_font_size, fg_full,
             win->settings.browser_font, win->settings.browser_font_size,
             fg_full,
             win->settings.prompt_font, win->settings.prompt_font_size, fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             fg_full,
             win->settings.editor_font, win->settings.editor_font_size,
             fg_dim);

    char full_css[8192];
    snprintf(full_css, sizeof(full_css), "%s%s", theme_css, font_css);
    gtk_css_provider_load_from_string(win->css_provider, full_css);

    /* status bar */
    update_status_bar(win);
}

/* ── Helper: check if directory has children ── */

static gboolean dir_has_children(const char *path) {
    DIR *d = opendir(path);
    if (!d) return FALSE;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        closedir(d);
        return TRUE;
    }
    closedir(d);
    return FALSE;
}

/* ── File Browser ── */

static int entry_compare(const void *a, const void *b) {
    const struct dirent *da = *(const struct dirent **)a;
    const struct dirent *db = *(const struct dirent **)b;

    int a_dir = (da->d_type == DT_DIR) ? 0 : 1;
    int b_dir = (db->d_type == DT_DIR) ? 0 : 1;
    if (a_dir != b_dir) return a_dir - b_dir;

    return strcasecmp(da->d_name, db->d_name);
}

static void on_file_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data);

void vibe_window_open_directory(VibeWindow *win, const char *path) {
    strncpy(win->current_dir, path, sizeof(win->current_dir) - 1);
    strncpy(win->settings.last_directory, path, sizeof(win->settings.last_directory) - 1);

    /* show relative path from root */
    if (win->root_dir[0] && strncmp(path, win->root_dir, strlen(win->root_dir)) == 0) {
        const char *rel = path + strlen(win->root_dir);
        if (rel[0] == '/') rel++;
        if (rel[0] == '\0')
            gtk_label_set_text(win->path_label, "/");
        else {
            char display[2048];
            snprintf(display, sizeof(display), "/%s", rel);
            gtk_label_set_text(win->path_label, display);
        }
    } else {
        gtk_label_set_text(win->path_label, path);
    }

    /* clear existing rows */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(win->file_list))))
        gtk_list_box_remove(win->file_list, child);

    /* ".." only if we're deeper than root_dir */
    if (win->root_dir[0] && strcmp(path, win->root_dir) != 0) {
        GtkWidget *lbl = gtk_label_new("  ..");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_list_box_append(win->file_list, lbl);
    }

    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent **entries = NULL;
    int count = 0, capacity = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 64;
            entries = realloc(entries, sizeof(struct dirent *) * capacity);
        }
        entries[count] = malloc(sizeof(struct dirent));
        memcpy(entries[count], ent, sizeof(struct dirent));
        count++;
    }
    closedir(dir);

    if (entries) {
        qsort(entries, count, sizeof(struct dirent *), entry_compare);

        for (int i = 0; i < count; i++) {
            gboolean is_dir = (entries[i]->d_type == DT_DIR);
            char label[512];

            if (is_dir) {
                /* check if directory has children -> show arrow */
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", path, entries[i]->d_name);
                gboolean has_kids = dir_has_children(full);
                snprintf(label, sizeof(label), "%s %s",
                         has_kids ? "▶" : " ", entries[i]->d_name);
            } else {
                snprintf(label, sizeof(label), "  %s", entries[i]->d_name);
            }

            GtkWidget *lbl = gtk_label_new(label);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0);
            gtk_widget_set_margin_start(lbl, 8);
            gtk_widget_set_margin_end(lbl, 8);
            gtk_widget_set_margin_top(lbl, 4);
            gtk_widget_set_margin_bottom(lbl, 4);
            gtk_list_box_append(win->file_list, lbl);

            free(entries[i]);
        }
        free(entries);
    }

    gtk_notebook_set_current_page(win->notebook, 0);
}

void vibe_window_set_root_directory(VibeWindow *win, const char *path) {
    strncpy(win->root_dir, path, sizeof(win->root_dir) - 1);

    /* spawn terminal in this directory */
    const char *shell = g_getenv("SHELL");
    if (!shell) {
        struct passwd *pw = getpwuid(getuid());
        shell = (pw && pw->pw_shell) ? pw->pw_shell : "/bin/sh";
    }
    char *argv[] = {(char *)shell, NULL};

    vte_terminal_spawn_async(
        win->terminal,
        VTE_PTY_DEFAULT,
        path,
        argv,
        NULL,
        G_SPAWN_DEFAULT,
        NULL, NULL, NULL,
        -1,
        NULL, NULL, NULL
    );

    vibe_window_open_directory(win, path);
}

static void load_file_content(VibeWindow *win, const char *filepath) {
    char *contents = NULL;
    gsize len = 0;
    if (g_file_get_contents(filepath, &contents, &len, NULL)) {
        /* check if binary (has null bytes in first 8K) */
        gboolean is_binary = FALSE;
        gsize check = len < 8192 ? len : 8192;
        for (gsize i = 0; i < check; i++) {
            if (contents[i] == '\0') { is_binary = TRUE; break; }
        }
        if (is_binary) {
            gtk_text_buffer_set_text(win->file_buffer, "(binary file)", -1);
        } else {
            gtk_text_buffer_set_text(win->file_buffer, contents, len);
        }
        g_free(contents);
    } else {
        gtk_text_buffer_set_text(win->file_buffer, "(cannot read file)", -1);
    }

    /* cursor to start */
    GtkTextIter start;
    gtk_text_buffer_get_start_iter(win->file_buffer, &start);
    gtk_text_buffer_place_cursor(win->file_buffer, &start);

    /* apply intensity immediately (no blink) */
    apply_font_intensity(win);
}

static void on_file_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    VibeWindow *win = data;

    GtkWidget *label = gtk_widget_get_first_child(GTK_WIDGET(row));
    const char *text = gtk_label_get_text(GTK_LABEL(label));

    /* parse the name: skip leading arrow/spaces */
    const char *name = text;
    /* skip "▶ " or "  " prefix */
    if (name[0] == ' ' && name[1] == ' ') {
        name += 2;
    } else {
        /* UTF-8 ▶ is 3 bytes + space */
        const char *sp = strchr(name, ' ');
        if (sp) name = sp + 1;
    }

    /* ".." -> go up within root */
    if (strcmp(name, "..") == 0) {
        char new_path[4096];
        strncpy(new_path, win->current_dir, sizeof(new_path));
        char *last = strrchr(new_path, '/');
        if (last && last != new_path)
            *last = '\0';
        else
            strcpy(new_path, "/");
        vibe_window_open_directory(win, new_path);
        return;
    }

    char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", win->current_dir, name);

    struct stat st;
    if (stat(full_path, &st) != 0) return;

    if (S_ISDIR(st.st_mode)) {
        vibe_window_open_directory(win, full_path);
    } else if (S_ISREG(st.st_mode)) {
        load_file_content(win, full_path);
        gtk_widget_grab_focus(GTK_WIDGET(win->file_view));
    }
}

/* ── Editor key handler (block editing, allow navigation) ── */

static gboolean on_editor_key(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode; (void)data;

    /* allow navigation keys */
    switch (keyval) {
        case GDK_KEY_Left: case GDK_KEY_Right:
        case GDK_KEY_Up: case GDK_KEY_Down:
        case GDK_KEY_Home: case GDK_KEY_End:
        case GDK_KEY_Page_Up: case GDK_KEY_Page_Down:
        case GDK_KEY_Tab:
            return FALSE; /* let through */
    }

    /* allow Ctrl+C (copy), Ctrl+A (select all) */
    if (state & GDK_CONTROL_MASK) {
        if (keyval == GDK_KEY_c || keyval == GDK_KEY_a ||
            keyval == GDK_KEY_plus || keyval == GDK_KEY_equal ||
            keyval == GDK_KEY_minus || keyval == GDK_KEY_o ||
            keyval == GDK_KEY_q)
            return FALSE;
    }

    /* block everything else (typing, delete, backspace, enter) */
    return TRUE;
}

/* ── Prompt key handler ── */

static void send_prompt_to_terminal(VibeWindow *win) {
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(win->prompt_buffer, &start, &end);
    char *text = gtk_text_buffer_get_text(win->prompt_buffer, &start, &end, FALSE);

    if (text && text[0]) {
        vte_terminal_feed_child(win->terminal, text, strlen(text));
        vte_terminal_feed_child(win->terminal, "\n", 1);
        gtk_text_buffer_set_text(win->prompt_buffer, "", -1);
        if (win->settings.prompt_switch_terminal)
            gtk_notebook_set_current_page(win->notebook, 1);
    }
    g_free(text);
}

static gboolean on_prompt_key(GtkEventControllerKey *ctrl, guint keyval,
                               guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode;
    VibeWindow *win = data;

    if (keyval == GDK_KEY_Return) {
        if (win->settings.prompt_send_enter && !(state & GDK_CONTROL_MASK)) {
            /* Enter sends (without Ctrl) */
            send_prompt_to_terminal(win);
            return TRUE;
        } else if (!win->settings.prompt_send_enter && (state & GDK_CONTROL_MASK)) {
            /* Ctrl+Enter sends */
            send_prompt_to_terminal(win);
            return TRUE;
        }
    }
    return FALSE;
}

/* ── Tab switch → focus ── */

static void on_tab_switched(GtkNotebook *nb, GtkWidget *page, guint page_num, gpointer data) {
    (void)nb; (void)page;
    VibeWindow *win = data;
    switch (page_num) {
        case 0:
            gtk_widget_grab_focus(GTK_WIDGET(win->file_view));
            break;
        case 1:
            gtk_widget_grab_focus(GTK_WIDGET(win->terminal));
            break;
        case 2:
            gtk_widget_grab_focus(GTK_WIDGET(win->prompt_view));
            break;
    }
    update_status_bar(win);
}

/* ── Status bar update ── */

static void update_status_bar(VibeWindow *win) {
    int page = gtk_notebook_get_current_page(win->notebook);
    if (page == 0) {
        /* Files tab - show cursor position */
        GtkTextMark *mark = gtk_text_buffer_get_insert(win->file_buffer);
        GtkTextIter iter;
        gtk_text_buffer_get_iter_at_mark(win->file_buffer, &iter, mark);
        int line = gtk_text_iter_get_line(&iter) + 1;
        int col = gtk_text_iter_get_line_offset(&iter) + 1;
        char buf[128];
        snprintf(buf, sizeof(buf), "Ln %d, Col %d", line, col);
        gtk_label_set_text(win->status_label, buf);
    } else {
        gtk_label_set_text(win->status_label, "");
    }
}

/* ── Window close ── */

static void on_close_request(GtkWindow *window, gpointer data) {
    (void)window;
    VibeWindow *win = data;
    int w, h;
    gtk_window_get_default_size(GTK_WINDOW(win->window), &w, &h);
    win->settings.window_width = w;
    win->settings.window_height = h;
    settings_save(&win->settings);
}

static void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    VibeWindow *win = data;
    if (win->intensity_idle_id) {
        g_source_remove(win->intensity_idle_id);
        win->intensity_idle_id = 0;
    }
    if (win->css_provider) {
        gtk_style_context_remove_provider_for_display(
            gdk_display_get_default(),
            GTK_STYLE_PROVIDER(win->css_provider));
        g_object_unref(win->css_provider);
    }
    g_free(win);
}

/* ── Build window ── */

static GtkWidget *build_menu_button(void) {
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Open Folder", "win.open-folder");
    g_menu_append(menu, "Settings", "win.settings");

    GtkWidget *btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn), G_MENU_MODEL(menu));
    g_object_unref(menu);
    return btn;
}

VibeWindow *vibe_window_new(GtkApplication *app) {
    VibeWindow *win = g_new0(VibeWindow, 1);
    settings_load(&win->settings);

    win->window = GTK_APPLICATION_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(win->window), "Vibe Light");
    gtk_window_set_default_size(GTK_WINDOW(win->window),
                                 win->settings.window_width,
                                 win->settings.window_height);

    GtkWidget *header = gtk_header_bar_new();
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), build_menu_button());
    gtk_window_set_titlebar(GTK_WINDOW(win->window), header);

    win->notebook = GTK_NOTEBOOK(gtk_notebook_new());

    /* ── Tab 1: File Browser (left) + File Viewer (right) ── */
    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(paned), 250);

    /* Left: browser */
    GtkWidget *browser_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_size_request(browser_box, 180, -1);

    win->path_label = GTK_LABEL(gtk_label_new("No folder opened"));
    gtk_label_set_xalign(win->path_label, 0);
    gtk_widget_set_margin_start(GTK_WIDGET(win->path_label), 8);
    gtk_widget_set_margin_end(GTK_WIDGET(win->path_label), 8);
    gtk_widget_set_margin_top(GTK_WIDGET(win->path_label), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(win->path_label), 6);
    gtk_widget_add_css_class(GTK_WIDGET(win->path_label), "path-bar");
    gtk_label_set_ellipsize(win->path_label, PANGO_ELLIPSIZE_START);
    gtk_box_append(GTK_BOX(browser_box), GTK_WIDGET(win->path_label));

    gtk_box_append(GTK_BOX(browser_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    win->file_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_widget_add_css_class(GTK_WIDGET(win->file_list), "file-browser");
    gtk_list_box_set_selection_mode(win->file_list, GTK_SELECTION_SINGLE);
    g_signal_connect(win->file_list, "row-activated", G_CALLBACK(on_file_row_activated), win);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(win->file_list));
    gtk_box_append(GTK_BOX(browser_box), scroll);

    gtk_paned_set_start_child(GTK_PANED(paned), browser_box);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    /* Right: line numbers + file viewer */
    /* Line numbers (non-editable text view, synced scroll) */
    win->line_numbers = GTK_TEXT_VIEW(gtk_text_view_new());
    gtk_text_view_set_editable(win->line_numbers, FALSE);
    gtk_text_view_set_cursor_visible(win->line_numbers, FALSE);
    gtk_widget_set_can_focus(GTK_WIDGET(win->line_numbers), FALSE);
    gtk_widget_add_css_class(GTK_WIDGET(win->line_numbers), "line-numbers");
    gtk_text_view_set_right_margin(win->line_numbers, 8);
    gtk_text_view_set_left_margin(win->line_numbers, 4);
    gtk_text_view_set_top_margin(win->line_numbers, 8);
    gtk_text_buffer_set_text(gtk_text_view_get_buffer(win->line_numbers), "1", -1);

    /* File viewer (custom subclass for highlight drawing) */
    VibeTextView *vtv = g_object_new(VIBE_TYPE_TEXT_VIEW, NULL);
    vtv->win = win;
    win->file_view = GTK_TEXT_VIEW(vtv);
    win->file_buffer = gtk_text_view_get_buffer(win->file_view);
    gtk_text_view_set_editable(win->file_view, TRUE);
    gtk_text_view_set_cursor_visible(win->file_view, TRUE);
    gtk_text_view_set_wrap_mode(win->file_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(win->file_view, 12);
    gtk_text_view_set_right_margin(win->file_view, 12);
    gtk_text_view_set_top_margin(win->file_view, 8);
    gtk_text_view_set_monospace(win->file_view, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(win->file_view), "file-viewer");

    /* Intensity tag for font opacity */
    win->intensity_tag = gtk_text_buffer_create_tag(win->file_buffer, "intensity",
                                                     "foreground-rgba", NULL, NULL);

    g_signal_connect(win->file_buffer, "changed", G_CALLBACK(on_file_buffer_changed), win);

    /* block keyboard editing but allow cursor movement and blinking */
    GtkEventController *edit_key = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(edit_key, GTK_PHASE_CAPTURE);
    g_signal_connect(edit_key, "key-pressed", G_CALLBACK(on_editor_key), win);
    gtk_widget_add_controller(GTK_WIDGET(win->file_view), edit_key);
    g_signal_connect(win->file_buffer, "notify::cursor-position", G_CALLBACK(on_cursor_moved), win);

    /* Scrolled windows */
    GtkWidget *viewer_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(viewer_scroll), GTK_WIDGET(win->file_view));
    gtk_widget_set_vexpand(viewer_scroll, TRUE);
    gtk_widget_set_hexpand(viewer_scroll, TRUE);

    win->ln_scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(win->ln_scrolled), GTK_WIDGET(win->line_numbers));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(win->ln_scrolled),
                                    GTK_POLICY_NEVER, GTK_POLICY_EXTERNAL);
    gtk_widget_set_vexpand(win->ln_scrolled, TRUE);

    /* Sync vertical scroll */
    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer_scroll));
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(win->ln_scrolled), vadj);

    /* HBox: line numbers + viewer */
    GtkWidget *viewer_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(viewer_box), win->ln_scrolled);
    gtk_box_append(GTK_BOX(viewer_box), viewer_scroll);

    gtk_paned_set_end_child(GTK_PANED(paned), viewer_box);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);

    gtk_notebook_append_page(win->notebook, paned, gtk_label_new("Files"));

    /* ── Tab 2: Terminal ── */
    win->terminal = VTE_TERMINAL(vte_terminal_new());
    vte_terminal_set_scroll_on_output(win->terminal, TRUE);
    vte_terminal_set_scrollback_lines(win->terminal, 10000);

    GtkWidget *term_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(GTK_WIDGET(win->terminal), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(win->terminal), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(term_scroll), GTK_WIDGET(win->terminal));

    gtk_notebook_append_page(win->notebook, term_scroll, gtk_label_new("Terminal"));

    /* ── Tab 3: Prompt ── */
    GtkWidget *prompt_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *prompt_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(prompt_scroll, TRUE);
    win->prompt_view = GTK_TEXT_VIEW(gtk_text_view_new());
    win->prompt_buffer = gtk_text_view_get_buffer(win->prompt_view);
    gtk_text_view_set_wrap_mode(win->prompt_view, GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(win->prompt_view, 8);
    gtk_text_view_set_right_margin(win->prompt_view, 8);
    gtk_text_view_set_top_margin(win->prompt_view, 8);
    gtk_text_view_set_monospace(win->prompt_view, TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(win->prompt_view), "prompt-view");
    win->prompt_intensity_tag = gtk_text_buffer_create_tag(win->prompt_buffer, "intensity",
                                                            "foreground-rgba", NULL, NULL);
    g_signal_connect(win->prompt_buffer, "changed", G_CALLBACK(on_prompt_buffer_changed), win);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(prompt_scroll), GTK_WIDGET(win->prompt_view));
    gtk_box_append(GTK_BOX(prompt_box), prompt_scroll);

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_prompt_key), win);
    gtk_widget_add_controller(GTK_WIDGET(win->prompt_view), key_ctrl);

    gtk_notebook_append_page(win->notebook, prompt_box, gtk_label_new("Prompt"));

    /* Status bar */
    GtkWidget *status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(status_bar, "statusbar");
    gtk_widget_set_margin_start(status_bar, 8);
    gtk_widget_set_margin_end(status_bar, 8);
    gtk_widget_set_margin_top(status_bar, 6);
    gtk_widget_set_margin_bottom(status_bar, 6);

    win->status_label = GTK_LABEL(gtk_label_new(""));
    gtk_widget_set_halign(GTK_WIDGET(win->status_label), GTK_ALIGN_START);
    gtk_widget_set_hexpand(GTK_WIDGET(win->status_label), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(win->status_label), "dim-label");
    gtk_box_append(GTK_BOX(status_bar), GTK_WIDGET(win->status_label));

    /* Main layout: notebook + status bar */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(win->notebook));
    gtk_widget_set_vexpand(GTK_WIDGET(win->notebook), TRUE);
    gtk_box_append(GTK_BOX(main_box), status_bar);
    gtk_window_set_child(GTK_WINDOW(win->window), main_box);

    /* Tab switch → focus */
    g_signal_connect(win->notebook, "switch-page", G_CALLBACK(on_tab_switched), win);

    /* CSS provider */
    win->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(win->css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_signal_connect(win->window, "close-request", G_CALLBACK(on_close_request), win);
    g_signal_connect(win->window, "destroy", G_CALLBACK(on_destroy), win);

    actions_setup(win, app);
    vibe_window_apply_settings(win);

    if (win->settings.last_directory[0]) {
        g_strlcpy(win->root_dir, win->settings.last_directory, sizeof(win->root_dir));
        vibe_window_set_root_directory(win, win->settings.last_directory);
    }

    gtk_window_present(GTK_WINDOW(win->window));
    return win;
}
