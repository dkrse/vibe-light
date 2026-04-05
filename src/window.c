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
} ThemeDef;

static const ThemeDef custom_themes[] = {
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

    char font_css[8192];
    snprintf(font_css, sizeof(font_css),
             /* GUI font — headerbar, tabs, labels, menus, dialogs, status bar */
             "window,headerbar,.titlebar{font-family:%s;font-size:%dpt}"
             "headerbar button,headerbar menubutton button,headerbar menubutton"
             "{font-family:%s;font-size:%dpt}"
             "notebook header tab{font-family:%s;font-size:%dpt}"
             "popover modelbutton{font-family:%s;font-size:%dpt}"
             "popover>contents,popover.menu>contents{font-family:%s;font-size:%dpt}"
             ".path-bar{font-family:%s;font-size:%dpt}"
             ".statusbar label{font-family:%s;font-size:%dpt}"
             /* per-section fonts */
             ".file-viewer{font-family:%s;font-size:%dpt;caret-color:%s}"
             ".file-browser{font-family:%s;font-size:%dpt}"
             ".file-browser label{color:%s}"
             ".prompt-view{font-family:%s;font-size:%dpt;caret-color:%s}"
             /* colors */
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
             /* GUI font args */
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             /* per-section font args */
             win->settings.editor_font, win->settings.editor_font_size, fg_full,
             win->settings.browser_font, win->settings.browser_font_size,
             fg_full,
             win->settings.prompt_font, win->settings.prompt_font_size, fg_full,
             /* color args */
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

    char full_css[16384];
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
static void update_file_status(VibeWindow *win);

/* Create a label row for the tree browser.
   depth = indentation level, is_dir = directory flag,
   full_path = absolute path to file/dir, name = display name */
static gboolean path_is_remote(const char *path) {
    return (strstr(path, "/vibe-light-sftp-") != NULL ||
            strstr(path, "/vibe-sftp-") != NULL);
}

static GtkWidget *create_tree_row(const char *full_path, const char *name,
                                   gboolean is_dir, int depth) {
    char label[512];
    /* build indentation */
    char indent[128] = "";
    for (int i = 0; i < depth; i++) strcat(indent, "  ");

    if (is_dir) {
        /* skip dir_has_children on remote mounts — too many network roundtrips */
        gboolean has_kids = path_is_remote(full_path) ? TRUE : dir_has_children(full_path);
        snprintf(label, sizeof(label), "%s%s %s", indent,
                 has_kids ? "▶" : " ", name);
    } else {
        snprintf(label, sizeof(label), "%s  %s", indent, name);
    }

    GtkWidget *lbl = gtk_label_new(label);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_widget_set_margin_start(lbl, 8);
    gtk_widget_set_margin_end(lbl, 8);
    gtk_widget_set_margin_top(lbl, 4);
    gtk_widget_set_margin_bottom(lbl, 4);

    /* store metadata */
    g_object_set_data_full(G_OBJECT(lbl), "full-path", g_strdup(full_path), g_free);
    g_object_set_data(G_OBJECT(lbl), "is-dir", GINT_TO_POINTER(is_dir));
    g_object_set_data(G_OBJECT(lbl), "depth", GINT_TO_POINTER(depth));
    g_object_set_data(G_OBJECT(lbl), "expanded", GINT_TO_POINTER(FALSE));

    return lbl;
}

/* Insert sorted directory entries after a given row position (or at end if after_row is NULL).
   Returns number of inserted rows. */
static int insert_children(VibeWindow *win, const char *dir_path, int depth,
                            int insert_pos) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    struct dirent **entries = NULL;
    int count = 0, capacity = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (count >= capacity) {
            capacity = capacity ? capacity * 2 : 64;
            struct dirent **tmp = realloc(entries, sizeof(struct dirent *) * capacity);
            if (!tmp) {
                for (int i = 0; i < count; i++) free(entries[i]);
                free(entries);
                closedir(dir);
                return 0;
            }
            entries = tmp;
        }
        entries[count] = malloc(sizeof(struct dirent));
        memcpy(entries[count], ent, sizeof(struct dirent));
        count++;
    }
    closedir(dir);

    if (!entries) return 0;

    qsort(entries, count, sizeof(struct dirent *), entry_compare);

    for (int i = 0; i < count; i++) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entries[i]->d_name);

        /* d_type is DT_UNKNOWN on FUSE/sshfs — fall back to stat() */
        gboolean is_dir_entry;
        if (entries[i]->d_type != DT_UNKNOWN)
            is_dir_entry = (entries[i]->d_type == DT_DIR);
        else {
            struct stat st;
            is_dir_entry = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        }

        GtkWidget *lbl = create_tree_row(full, entries[i]->d_name, is_dir_entry, depth);
        gtk_list_box_insert(win->file_list, lbl, insert_pos + i);

        free(entries[i]);
    }
    free(entries);
    return count;
}

/* ── SSH ls-based directory listing (avoids FUSE blocking) ── */

/* Convert local mount path to remote path.
   e.g. /tmp/vibe-light-sftp-123-user@host/subdir → /opt/subdir
   (if ssh_mount="/tmp/vibe-light-sftp-123-user@host" and ssh_remote_path="/opt") */
static const char *to_remote_path(VibeWindow *win, const char *local_path,
                                   char *buf, size_t buflen) {
    size_t mlen = strlen(win->ssh_mount);
    const char *suffix = local_path + mlen; /* e.g. "" or "/subdir" */
    snprintf(buf, buflen, "%s%s", win->ssh_remote_path, suffix);
    return buf;
}

/* Build SSH base argv — caller must free with g_ptr_array_unref().
   Does NOT add trailing NULL — caller adds command args + NULL.
   Uses ControlMaster multiplexing when a control socket exists. */
static GPtrArray *ssh_argv_new(VibeWindow *win) {
    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", win->ssh_port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    if (win->ssh_ctl_path[0]) {
        g_ptr_array_add(av, g_strdup("-o"));
        g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", win->ssh_ctl_path));
    }
    if (win->ssh_key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(win->ssh_key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", win->ssh_user, win->ssh_host));
    return av;
}

/* Also build argv from plain params (for use in background threads) */
static GPtrArray *ssh_argv_from_params(const char *host, const char *user,
                                        int port, const char *key,
                                        const char *ctl_path) {
    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=5"));
    if (ctl_path[0]) {
        g_ptr_array_add(av, g_strdup("-o"));
        g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", ctl_path));
    }
    if (key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    return av;
}

/* ── SSH ControlMaster lifecycle ── */

static void ssh_ctl_start(VibeWindow *win) {
    /* Use XDG_RUNTIME_DIR (user-private, tmpfs) or fall back to /tmp with mkdtemp */
    const char *runtime = g_get_user_runtime_dir(); /* XDG_RUNTIME_DIR or fallback */
    snprintf(win->ssh_ctl_dir, sizeof(win->ssh_ctl_dir),
             "%s/vibe-ssh-XXXXXX", runtime);
    if (!mkdtemp(win->ssh_ctl_dir)) {
        win->ssh_ctl_dir[0] = '\0';
        win->ssh_ctl_path[0] = '\0';
        return;
    }
    snprintf(win->ssh_ctl_path, sizeof(win->ssh_ctl_path),
             "%s/ctl", win->ssh_ctl_dir);

    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", win->ssh_port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ControlMaster=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", win->ssh_ctl_path));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ControlPersist=60"));
    if (win->ssh_key[0]) {
        g_ptr_array_add(av, g_strdup("-i"));
        g_ptr_array_add(av, g_strdup(win->ssh_key));
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", win->ssh_user, win->ssh_host));
    g_ptr_array_add(av, g_strdup("-fN")); /* fork to background, no command */
    g_ptr_array_add(av, NULL);

    g_spawn_sync(NULL, (char **)av->pdata, NULL,
                 G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_ptr_array_unref(av);
}

static void ssh_ctl_stop(VibeWindow *win) {
    if (!win->ssh_ctl_path[0]) return;

    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup_printf("ControlPath=%s", win->ssh_ctl_path));
    g_ptr_array_add(av, g_strdup("-O"));
    g_ptr_array_add(av, g_strdup("exit"));
    g_ptr_array_add(av, g_strdup_printf("%s@%s", win->ssh_user, win->ssh_host));
    g_ptr_array_add(av, NULL);

    g_spawn_sync(NULL, (char **)av->pdata, NULL,
                 G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, NULL);
    g_ptr_array_unref(av);

    unlink(win->ssh_ctl_path);
    win->ssh_ctl_path[0] = '\0';
    if (win->ssh_ctl_dir[0]) {
        rmdir(win->ssh_ctl_dir);
        win->ssh_ctl_dir[0] = '\0';
    }
}

/* Run SSH command synchronously with argv (no shell — immune to injection).
   Returns TRUE on success. out_stdout/out_len may be NULL. */
static gboolean ssh_spawn_sync(GPtrArray *argv, char **out_stdout,
                                gsize *out_len) {
    g_ptr_array_add(argv, NULL);

    char *stdout_buf = NULL;
    GError *err = NULL;
    gint status = 0;

    gboolean ok = g_spawn_sync(
        NULL, (char **)argv->pdata, NULL,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, &stdout_buf, NULL, &status, &err);

    /* remove trailing NULL so caller can reuse argv */
    g_ptr_array_set_size(argv, argv->len - 1);

    if (!ok) {
        if (err) g_error_free(err);
        return FALSE;
    }
    if (!g_spawn_check_wait_status(status, NULL)) {
        g_free(stdout_buf);
        return FALSE;
    }

    if (out_len)
        *out_len = stdout_buf ? strlen(stdout_buf) : 0;

    if (out_stdout)
        *out_stdout = stdout_buf;
    else
        g_free(stdout_buf);

    return TRUE;
}

/* Run SSH ls synchronously and populate file list */
static void ssh_ls_populate(VibeWindow *win, const char *local_dir_path,
                             int depth, int insert_pos) {
    char remote[4096];
    to_remote_path(win, local_dir_path, remote, sizeof(remote));

    GPtrArray *av = ssh_argv_new(win);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(remote));

    char *stdout_buf = NULL;
    if (!ssh_spawn_sync(av, &stdout_buf, NULL)) {
        g_ptr_array_unref(av);
        return;
    }
    g_ptr_array_unref(av);

    /* parse output */
    char **lines = g_strsplit(stdout_buf, "\n", -1);
    g_free(stdout_buf);

    int nlines = 0;
    for (char **p = lines; *p && **p; p++) nlines++;

    int inserted = 0;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < nlines; i++) {
            char *name = lines[i];
            int len = (int)strlen(name);
            if (!len) continue;
            gboolean is_dir = (name[len - 1] == '/');
            if ((pass == 0 && !is_dir) || (pass == 1 && is_dir)) continue;

            char clean_name[512];
            g_strlcpy(clean_name, name, sizeof(clean_name));
            if (is_dir && clean_name[0])
                clean_name[strlen(clean_name) - 1] = '\0';

            char full[4096];
            snprintf(full, sizeof(full), "%s/%s", local_dir_path, clean_name);
            GtkWidget *lbl = create_tree_row(full, clean_name, is_dir, depth);
            int pos = insert_pos >= 0 ? insert_pos + inserted : -1;
            gtk_list_box_insert(win->file_list, lbl, pos);
            inserted++;
        }
    }
    g_strfreev(lines);
}

static void count_entries(const char *path, int *files, int *dirs);
static void load_file_content(VibeWindow *win, const char *filepath);

/* ── File system monitoring ── */

static void stop_inotify_proc(VibeWindow *win) {
    if (win->inotify_stream) {
        g_input_stream_close(G_INPUT_STREAM(win->inotify_stream), NULL, NULL);
        g_object_unref(win->inotify_stream);
        win->inotify_stream = NULL;
    }
    if (win->inotify_proc) {
        g_subprocess_force_exit(win->inotify_proc);
        g_object_unref(win->inotify_proc);
        win->inotify_proc = NULL;
    }
}

static void stop_file_monitor(VibeWindow *win) {
    if (win->file_monitor) {
        g_file_monitor_cancel(win->file_monitor);
        g_object_unref(win->file_monitor);
        win->file_monitor = NULL;
    }
    if (win->file_reload_id) {
        g_source_remove(win->file_reload_id);
        win->file_reload_id = 0;
    }
    if (win->remote_file_poll_id) {
        g_source_remove(win->remote_file_poll_id);
        win->remote_file_poll_id = 0;
    }
    win->remote_file_mtime = 0;
    win->file_poll_in_flight = FALSE;
}

static void stop_dir_monitor(VibeWindow *win) {
    if (win->fs_refresh_id) {
        g_source_remove(win->fs_refresh_id);
        win->fs_refresh_id = 0;
    }
    if (win->dir_monitor) {
        g_file_monitor_cancel(win->dir_monitor);
        g_object_unref(win->dir_monitor);
        win->dir_monitor = NULL;
    }
    if (win->remote_dir_poll_id) {
        g_source_remove(win->remote_dir_poll_id);
        win->remote_dir_poll_id = 0;
    }
    stop_inotify_proc(win);
    win->remote_dir_hash = 0;
    win->dir_poll_in_flight = FALSE;
}

/* --- Async file/dir count (runs in thread, updates label on main thread) --- */

typedef struct {
    VibeWindow *win;
    char        path[4096];
} CountCtx;

static void count_thread_func(GTask *task, gpointer src, gpointer data,
                               GCancellable *cancel) {
    (void)src; (void)cancel;
    CountCtx *ctx = data;
    int files = 0, dirs = 0;
    count_entries(ctx->path, &files, &dirs);
    gint64 result = ((gint64)files << 32) | (guint32)dirs;
    g_task_return_int(task, result);
}

static void count_finished_cb(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    CountCtx *ctx = data;
    VibeWindow *win = ctx->win;
    GError *err = NULL;
    gint64 result = g_task_propagate_int(G_TASK(res), &err);
    if (err) {
        g_error_free(err);
        g_free(ctx);
        return;
    }
    if (g_cancellable_is_cancelled(win->cancellable)) {
        g_free(ctx);
        return;
    }
    int files = (int)(result >> 32);
    int dirs  = (int)(result & 0xFFFFFFFF);
    char buf[128];
    snprintf(buf, sizeof(buf), "%d files, %d dirs", files, dirs);
    gtk_label_set_text(win->status_label, buf);
    g_free(ctx);
}

/* Collect paths of expanded directories so we can restore state after refresh */
static GPtrArray *collect_expanded_paths(VibeWindow *win) {
    GPtrArray *paths = g_ptr_array_new_with_free_func(g_free);
    for (int i = 0; ; i++) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, i);
        if (!row) break;
        GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
        gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));
        gboolean expanded = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "expanded"));
        if (is_dir && expanded) {
            const char *fp = g_object_get_data(G_OBJECT(lbl), "full-path");
            if (fp) g_ptr_array_add(paths, g_strdup(fp));
        }
    }
    return paths;
}

/* Re-expand previously expanded directories after a refresh */
static void restore_expanded(VibeWindow *win, GPtrArray *paths) {
    for (guint p = 0; p < paths->len; p++) {
        const char *want = g_ptr_array_index(paths, p);
        /* Find row with this path */
        for (int i = 0; ; i++) {
            GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, i);
            if (!row) break;
            GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
            const char *fp = g_object_get_data(G_OBJECT(lbl), "full-path");
            gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));
            if (!is_dir || !fp || strcmp(fp, want) != 0) continue;

            /* Expand it */
            int depth = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "depth"));
            if (path_is_remote(fp) && win->ssh_host[0])
                ssh_ls_populate(win, fp, depth + 1, i + 1);
            else
                insert_children(win, fp, depth + 1, i + 1);
            g_object_set_data(G_OBJECT(lbl), "expanded", GINT_TO_POINTER(TRUE));

            const char *name = strrchr(fp, '/');
            name = name ? name + 1 : fp;
            char indent[128] = "";
            for (int d = 0; d < depth; d++) strcat(indent, "  ");
            char buf[512];
            snprintf(buf, sizeof(buf), "%s▼ %s", indent, name);
            gtk_label_set_text(GTK_LABEL(lbl), buf);
            break;
        }
    }
}

/* Repopulate the file list, preserving expanded dirs (local — runs on main thread) */
static void refresh_file_list_local(VibeWindow *win) {
    GPtrArray *expanded = collect_expanded_paths(win);

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(win->file_list))))
        gtk_list_box_remove(win->file_list, child);

    insert_children(win, win->current_dir, 0, -1);
    restore_expanded(win, expanded);
    g_ptr_array_unref(expanded);
    update_file_status(win);
}

/* Remote refresh via GTask — ssh_ls_populate runs in callback on main thread
   after the async "which" pattern. But ssh_ls_populate is sync SSH...
   To avoid blocking, we fetch ls output in a thread and apply it on main. */

typedef struct {
    VibeWindow *win;
    char       *ls_output;   /* owned */
    char        local_dir[4096];
    char        current_dir_snapshot[2048]; /* to verify dir didn't change */
    GPtrArray  *expanded;    /* paths to re-expand, owned */
} RemoteRefreshCtx;

static void remote_refresh_thread(GTask *task, gpointer src, gpointer data,
                                   GCancellable *cancel) {
    (void)src; (void)cancel;
    RemoteRefreshCtx *ctx = data;
    VibeWindow *win = ctx->win;

    /* Build SSH argv from win — safe because we only read immutable-during-connection fields
       (ssh_host/user/port/key/ctl_path don't change while connected) */
    GPtrArray *av = ssh_argv_from_params(win->ssh_host, win->ssh_user,
                                          win->ssh_port, win->ssh_key,
                                          win->ssh_ctl_path);
    char remote[4096];
    to_remote_path(win, ctx->local_dir, remote, sizeof(remote));

    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(remote));

    char *stdout_buf = NULL;
    ssh_spawn_sync(av, &stdout_buf, NULL);
    g_ptr_array_unref(av);

    ctx->ls_output = stdout_buf; /* may be NULL on failure */
    g_task_return_boolean(task, stdout_buf != NULL);
}

static void remote_refresh_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    RemoteRefreshCtx *ctx = data;
    VibeWindow *win = ctx->win;

    GError *err = NULL;
    g_task_propagate_boolean(G_TASK(res), &err);
    if (err) g_error_free(err);

    if (g_cancellable_is_cancelled(win->cancellable) ||
        strcmp(win->current_dir, ctx->current_dir_snapshot) != 0) {
        /* Window destroyed or dir changed — discard */
        g_free(ctx->ls_output);
        g_ptr_array_unref(ctx->expanded);
        g_free(ctx);
        return;
    }

    /* Clear and repopulate from ls output */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(win->file_list))))
        gtk_list_box_remove(win->file_list, child);

    if (ctx->ls_output) {
        char **lines = g_strsplit(ctx->ls_output, "\n", -1);
        int nlines = 0;
        for (char **p = lines; *p && **p; p++) nlines++;

        int inserted = 0;
        for (int pass = 0; pass < 2; pass++) {
            for (int i = 0; i < nlines; i++) {
                char *name = lines[i];
                int len = (int)strlen(name);
                if (!len) continue;
                gboolean is_dir = (name[len - 1] == '/');
                if ((pass == 0 && !is_dir) || (pass == 1 && is_dir)) continue;

                char clean_name[512];
                g_strlcpy(clean_name, name, sizeof(clean_name));
                if (is_dir && clean_name[0])
                    clean_name[strlen(clean_name) - 1] = '\0';

                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", ctx->local_dir, clean_name);
                GtkWidget *lbl = create_tree_row(full, clean_name, is_dir, 0);
                gtk_list_box_insert(win->file_list, lbl, -1);
                inserted++;
            }
        }
        g_strfreev(lines);
        g_free(ctx->ls_output);
    }

    restore_expanded(win, ctx->expanded);
    g_ptr_array_unref(ctx->expanded);
    update_file_status(win);
    g_free(ctx);
}

/* Debounced refresh callback */
static gboolean fs_refresh_cb(gpointer data) {
    VibeWindow *win = data;
    win->fs_refresh_id = 0;

    const char *path = win->current_dir;
    if (!path[0]) return G_SOURCE_REMOVE;

    if (path_is_remote(path) && win->ssh_host[0]) {
        /* Async remote refresh — don't block UI */
        RemoteRefreshCtx *ctx = g_new0(RemoteRefreshCtx, 1);
        ctx->win = win;
        g_strlcpy(ctx->local_dir, path, sizeof(ctx->local_dir));
        g_strlcpy(ctx->current_dir_snapshot, path, sizeof(ctx->current_dir_snapshot));
        ctx->expanded = collect_expanded_paths(win);

        GTask *task = g_task_new(NULL, NULL, remote_refresh_done, ctx);
        g_task_set_task_data(task, ctx, NULL);
        g_task_run_in_thread(task, remote_refresh_thread);
        g_object_unref(task);
    } else {
        refresh_file_list_local(win);
    }
    return G_SOURCE_REMOVE;
}

static void on_dir_changed(GFileMonitor *mon, GFile *file, GFile *other,
                            GFileMonitorEvent event, gpointer data) {
    (void)mon; (void)file; (void)other;
    VibeWindow *win = data;

    switch (event) {
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_DELETED:
        case G_FILE_MONITOR_EVENT_RENAMED:
        case G_FILE_MONITOR_EVENT_MOVED_IN:
        case G_FILE_MONITOR_EVENT_MOVED_OUT:
            break;
        default:
            return;
    }

    if (win->fs_refresh_id)
        g_source_remove(win->fs_refresh_id);
    win->fs_refresh_id = g_timeout_add(200, fs_refresh_cb, win);
}

/* Simple djb2 hash for comparing ls output */
static guint32 djb2_hash(const char *str) {
    guint32 h = 5381;
    for (; *str; str++)
        h = ((h << 5) + h) + (unsigned char)*str;
    return h;
}

/* ── Remote inotifywait-based directory watching ──
   Spawns: ssh ... inotifywait -m -e create,delete,move,modify --format '%e' <dir>
   Reads stdout line-by-line asynchronously. Falls back to polling if
   inotifywait is not available on the server. */

static void inotifywait_read_line_cb(GObject *src, GAsyncResult *res, gpointer data);

static void schedule_inotifywait_read(VibeWindow *win) {
    if (!win->inotify_stream) return;
    g_data_input_stream_read_line_async(win->inotify_stream,
                                         G_PRIORITY_DEFAULT, NULL,
                                         inotifywait_read_line_cb, win);
}

static gboolean debounced_file_reload_cb(gpointer data) {
    VibeWindow *win = data;
    win->file_reload_id = 0;
    if (win->current_file[0] && !g_cancellable_is_cancelled(win->cancellable))
        load_file_content(win, win->current_file);
    return G_SOURCE_REMOVE;
}

static void inotifywait_read_line_cb(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    VibeWindow *win = data;

    if (g_cancellable_is_cancelled(win->cancellable)) return;
    if (!win->inotify_stream) return;

    gsize len = 0;
    GError *err = NULL;
    char *line = g_data_input_stream_read_line_finish(
                     win->inotify_stream, res, &len, &err);

    if (!line) {
        if (err) g_error_free(err);
        return;
    }

    /* Debounce directory refresh */
    if (win->fs_refresh_id)
        g_source_remove(win->fs_refresh_id);
    win->fs_refresh_id = g_timeout_add(200, fs_refresh_cb, win);

    /* Debounce file reload on MODIFY */
    if (win->current_file[0] && strstr(line, "MODIFY")) {
        if (win->file_reload_id)
            g_source_remove(win->file_reload_id);
        win->file_reload_id = g_timeout_add(200, debounced_file_reload_cb, win);
    }

    g_free(line);
    schedule_inotifywait_read(win);
}

static void start_remote_inotifywait(VibeWindow *win, const char *remote_dir);

/* Fallback polling — used when inotifywait is not available */

typedef struct {
    VibeWindow *win;
    char        remote_path[4096];
    char        local_dir[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[256];
} RemoteDirPollCtx;

static void remote_dir_poll_thread(GTask *task, gpointer src, gpointer data,
                                    GCancellable *cancel) {
    (void)src; (void)cancel;
    RemoteDirPollCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(ctx->remote_path));

    char *stdout_buf = NULL;
    if (ssh_spawn_sync(av, &stdout_buf, NULL) && stdout_buf) {
        guint32 h = djb2_hash(stdout_buf);
        g_task_return_int(task, (gint64)h);
        g_free(stdout_buf);
    } else {
        g_task_return_int(task, 0);
    }
    g_ptr_array_unref(av);
}

static void remote_dir_poll_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    RemoteDirPollCtx *ctx = data;
    VibeWindow *win = ctx->win;
    win->dir_poll_in_flight = FALSE;

    GError *err = NULL;
    gint64 h = g_task_propagate_int(G_TASK(res), &err);
    if (err) { g_error_free(err); g_free(ctx); return; }
    if (g_cancellable_is_cancelled(win->cancellable)) { g_free(ctx); return; }

    if (h != 0 && (guint32)h != win->remote_dir_hash &&
        g_strcmp0(win->current_dir, ctx->local_dir) == 0) {
        win->remote_dir_hash = (guint32)h;
        if (!win->fs_refresh_id)
            win->fs_refresh_id = g_timeout_add(50, fs_refresh_cb, win);
    }
    g_free(ctx);
}

static gboolean remote_dir_poll_tick(gpointer data) {
    VibeWindow *win = data;
    if (!win->ssh_host[0] || !win->current_dir[0]) return G_SOURCE_REMOVE;
    if (win->dir_poll_in_flight) return G_SOURCE_CONTINUE; /* skip if previous still running */

    RemoteDirPollCtx *ctx = g_new0(RemoteDirPollCtx, 1);
    ctx->win = win;
    g_strlcpy(ctx->local_dir, win->current_dir, sizeof(ctx->local_dir));
    g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
    g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
    ctx->ssh_port = win->ssh_port;
    g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
    g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));

    char remote[4096];
    to_remote_path(win, win->current_dir, remote, sizeof(remote));
    g_strlcpy(ctx->remote_path, remote, sizeof(ctx->remote_path));

    win->dir_poll_in_flight = TRUE;
    GTask *task = g_task_new(NULL, NULL, remote_dir_poll_done, ctx);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, remote_dir_poll_thread);
    g_object_unref(task);

    return G_SOURCE_CONTINUE;
}

static void start_remote_fallback_poll(VibeWindow *win) {
    win->remote_dir_hash = 0;
    remote_dir_poll_tick(win);
    win->remote_dir_poll_id = g_timeout_add_seconds(2, remote_dir_poll_tick, win);
}

/* Try to spawn inotifywait; if it fails (exit code != 0 quickly), fall back to polling */

typedef struct {
    VibeWindow *win;
    char        remote_dir[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[512];
} InotifyCheckCtx;

static void inotifywait_check_thread(GTask *task, gpointer src, gpointer data,
                                      GCancellable *cancel) {
    (void)src; (void)cancel;
    InotifyCheckCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("which"));
    g_ptr_array_add(av, g_strdup("inotifywait"));

    gboolean has_it = ssh_spawn_sync(av, NULL, NULL);
    g_ptr_array_unref(av);

    g_task_return_boolean(task, has_it);
}

static void inotifywait_check_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    InotifyCheckCtx *ctx = data;
    VibeWindow *win = ctx->win;
    GError *err = NULL;
    gboolean has_it = g_task_propagate_boolean(G_TASK(res), &err);
    if (err) g_error_free(err);
    if (g_cancellable_is_cancelled(win->cancellable)) { g_free(ctx); return; }

    if (has_it) {
        fprintf(stderr, "WATCH: using inotifywait for %s\n", ctx->remote_dir);
        start_remote_inotifywait(win, ctx->remote_dir);
    } else {
        fprintf(stderr, "WATCH: inotifywait not found, falling back to polling\n");
        start_remote_fallback_poll(win);
    }
    g_free(ctx);
}

static void start_remote_inotifywait(VibeWindow *win, const char *remote_dir) {
    GPtrArray *av = ssh_argv_new(win);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("inotifywait"));
    g_ptr_array_add(av, g_strdup("-m"));
    g_ptr_array_add(av, g_strdup("-q"));
    g_ptr_array_add(av, g_strdup("-e"));
    g_ptr_array_add(av, g_strdup("create,delete,move,modify,attrib"));
    g_ptr_array_add(av, g_strdup("--format"));
    g_ptr_array_add(av, g_strdup("%e %f"));
    g_ptr_array_add(av, g_strdup(remote_dir));
    g_ptr_array_add(av, NULL);

    GError *err = NULL;
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE);

    win->inotify_proc = g_subprocess_launcher_spawnv(launcher,
        (const char * const *)av->pdata, &err);
    g_object_unref(launcher);
    g_ptr_array_unref(av);

    if (!win->inotify_proc) {
        if (err) g_error_free(err);
        start_remote_fallback_poll(win);
        return;
    }

    GInputStream *stdout_stream = g_subprocess_get_stdout_pipe(win->inotify_proc);
    win->inotify_stream = g_data_input_stream_new(stdout_stream);
    schedule_inotifywait_read(win);
}

/* --- Remote file mtime polling (for open file in editor) --- */

typedef struct {
    VibeWindow *win;
    char        remote_path[4096];
    char        local_path[4096];
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[256];
} RemoteFilePollCtx;

static void remote_file_poll_thread(GTask *task, gpointer src, gpointer data,
                                     GCancellable *cancel) {
    (void)src; (void)cancel;
    RemoteFilePollCtx *ctx = data;

    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("stat"));
    g_ptr_array_add(av, g_strdup("-c"));
    g_ptr_array_add(av, g_strdup("%Y"));
    g_ptr_array_add(av, g_strdup(ctx->remote_path));

    char *stdout_buf = NULL;
    if (ssh_spawn_sync(av, &stdout_buf, NULL) && stdout_buf) {
        gint64 mtime = g_ascii_strtoll(stdout_buf, NULL, 10);
        g_task_return_int(task, mtime);
        g_free(stdout_buf);
    } else {
        g_task_return_int(task, 0);
    }
    g_ptr_array_unref(av);
}

static void remote_file_poll_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    RemoteFilePollCtx *ctx = data;
    VibeWindow *win = ctx->win;
    win->file_poll_in_flight = FALSE;

    GError *err = NULL;
    gint64 mtime = g_task_propagate_int(G_TASK(res), &err);
    if (err) { g_error_free(err); g_free(ctx); return; }
    if (g_cancellable_is_cancelled(win->cancellable)) { g_free(ctx); return; }

    if (mtime > 0 && mtime != win->remote_file_mtime &&
        g_strcmp0(win->current_file, ctx->local_path) == 0) {
        win->remote_file_mtime = mtime;
        load_file_content(win, win->current_file);
    }
    g_free(ctx);
}

static gboolean remote_file_poll_tick(gpointer data) {
    VibeWindow *win = data;
    if (!win->ssh_host[0] || !win->current_file[0]) return G_SOURCE_REMOVE;
    if (win->file_poll_in_flight) return G_SOURCE_CONTINUE;

    RemoteFilePollCtx *ctx = g_new0(RemoteFilePollCtx, 1);
    ctx->win = win;
    g_strlcpy(ctx->local_path, win->current_file, sizeof(ctx->local_path));
    g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
    g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
    ctx->ssh_port = win->ssh_port;
    g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
    g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));

    char remote[4096];
    to_remote_path(win, win->current_file, remote, sizeof(remote));
    g_strlcpy(ctx->remote_path, remote, sizeof(ctx->remote_path));

    win->file_poll_in_flight = TRUE;
    GTask *task = g_task_new(NULL, NULL, remote_file_poll_done, ctx);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, remote_file_poll_thread);
    g_object_unref(task);

    return G_SOURCE_CONTINUE;
}

/* --- Start/stop monitors (local inotify OR remote inotifywait/polling) --- */

static void start_dir_monitor(VibeWindow *win, const char *path) {
    stop_dir_monitor(win);

    if (path_is_remote(path) && win->ssh_host[0]) {
        /* Remote: try inotifywait first, fall back to polling */
        char remote[4096];
        to_remote_path(win, path, remote, sizeof(remote));

        InotifyCheckCtx *ctx = g_new0(InotifyCheckCtx, 1);
        ctx->win = win;
        g_strlcpy(ctx->remote_dir, remote, sizeof(ctx->remote_dir));
        g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
        g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
        ctx->ssh_port = win->ssh_port;
        g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
        g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));

        GTask *task = g_task_new(NULL, NULL, inotifywait_check_done, ctx);
        g_task_set_task_data(task, ctx, NULL);
        g_task_run_in_thread(task, inotifywait_check_thread);
        g_object_unref(task);
        return;
    }

    GFile *dir = g_file_new_for_path(path);
    GError *err = NULL;
    win->dir_monitor = g_file_monitor_directory(dir, G_FILE_MONITOR_WATCH_MOVES,
                                                 NULL, &err);
    g_object_unref(dir);

    if (win->dir_monitor) {
        g_signal_connect(win->dir_monitor, "changed",
                         G_CALLBACK(on_dir_changed), win);
    } else {
        if (err) g_error_free(err);
    }
}

/* --- File content monitor (reload editor when external change) --- */

static void on_file_changed(GFileMonitor *mon, GFile *file, GFile *other,
                             GFileMonitorEvent event, gpointer data) {
    (void)mon; (void)file; (void)other;
    VibeWindow *win = data;

    /* Only react to final write (avoids double reload from CHANGED+DONE pair) */
    if (event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT)
        return;

    if (!win->current_file[0]) return;
    /* Debounce: some editors do multiple write cycles */
    if (win->file_reload_id)
        g_source_remove(win->file_reload_id);
    win->file_reload_id = g_timeout_add(150, debounced_file_reload_cb, win);
}

static void start_file_monitor(VibeWindow *win, const char *filepath) {
    stop_file_monitor(win);
    g_strlcpy(win->current_file, filepath, sizeof(win->current_file));

    if (path_is_remote(filepath) && win->ssh_host[0]) {
        /* Remote: poll mtime every 1s (uses ControlMaster — near zero overhead) */
        win->remote_file_mtime = 0;
        remote_file_poll_tick(win);
        win->remote_file_poll_id = g_timeout_add_seconds(1, remote_file_poll_tick, win);
        return;
    }

    GFile *f = g_file_new_for_path(filepath);
    GError *err = NULL;
    win->file_monitor = g_file_monitor_file(f, G_FILE_MONITOR_NONE, NULL, &err);
    g_object_unref(f);

    if (win->file_monitor) {
        g_signal_connect(win->file_monitor, "changed",
                         G_CALLBACK(on_file_changed), win);
    } else {
        if (err) g_error_free(err);
    }
}

void vibe_window_open_directory(VibeWindow *win, const char *path) {
    g_strlcpy(win->current_dir, path, sizeof(win->current_dir));
    g_strlcpy(win->settings.last_directory, path, sizeof(win->settings.last_directory));

    /* show directory name in header */
    if (path_is_remote(path) && win->ssh_host[0]) {
        /* remote: show just the directory name */
        char remote[4096];
        to_remote_path(win, path, remote, sizeof(remote));
        const char *base = strrchr(remote, '/');
        if (base && *(base + 1)) base = base + 1;
        else base = remote;
        gtk_label_set_text(win->path_label, base);
    } else if (win->root_dir[0]) {
        const char *base = strrchr(win->root_dir, '/');
        base = base ? base + 1 : win->root_dir;
        gtk_label_set_text(win->path_label, base);
    } else {
        gtk_label_set_text(win->path_label, path);
    }

    /* clear existing rows */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(win->file_list))))
        gtk_list_box_remove(win->file_list, child);

    fprintf(stderr, "OPENDIR: populate remote=%d\n", path_is_remote(path) && win->ssh_host[0]);
    if (path_is_remote(path) && win->ssh_host[0]) {
        ssh_ls_populate(win, path, 0, -1);
    } else {
        insert_children(win, path, 0, -1);
    }
    fprintf(stderr, "OPENDIR: done\n");
    update_file_status(win);
    start_dir_monitor(win, path);

    gtk_notebook_set_current_page(win->notebook, 0);
}

void vibe_window_set_root_directory(VibeWindow *win, const char *path) {
    g_strlcpy(win->root_dir, path, sizeof(win->root_dir));

    fprintf(stderr, "SETROOT: remote=%d host=%s\n", path_is_remote(path), win->ssh_host);
    if (path_is_remote(path) && win->ssh_host[0]) {
        /* Start SSH ControlMaster for multiplexed connections */
        ssh_ctl_start(win);
        /* spawn ssh session in terminal */
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", win->ssh_port);
        char userhost[512];
        snprintf(userhost, sizeof(userhost), "%s@%s", win->ssh_user, win->ssh_host);

        GPtrArray *av = g_ptr_array_new();
        g_ptr_array_add(av, (gchar *)"ssh");
        g_ptr_array_add(av, (gchar *)"-p");
        g_ptr_array_add(av, (gchar *)port_str);
        g_ptr_array_add(av, (gchar *)"-o");
        g_ptr_array_add(av, (gchar *)"StrictHostKeyChecking=accept-new");
        if (win->ssh_key[0]) {
            g_ptr_array_add(av, (gchar *)"-i");
            g_ptr_array_add(av, (gchar *)win->ssh_key);
        }
        g_ptr_array_add(av, (gchar *)userhost);
        g_ptr_array_add(av, NULL);

        fprintf(stderr, "SETROOT: spawning ssh terminal\n");
        vte_terminal_spawn_async(
            win->terminal, VTE_PTY_DEFAULT, NULL,
            (char **)av->pdata, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, -1, NULL, NULL, NULL);
        g_ptr_array_free(av, TRUE);
        fprintf(stderr, "SETROOT: ssh terminal spawned\n");
    } else {
        /* local shell */
        const char *shell = g_getenv("SHELL");
        if (!shell) {
            struct passwd *pw = getpwuid(getuid());
            shell = (pw && pw->pw_shell) ? pw->pw_shell : "/bin/sh";
        }
        char *argv[] = {(char *)shell, NULL};

        vte_terminal_spawn_async(
            win->terminal, VTE_PTY_DEFAULT, path,
            argv, NULL, G_SPAWN_DEFAULT,
            NULL, NULL, NULL, -1, NULL, NULL, NULL);
    }

    vibe_window_open_directory(win, path);
}

#define MAX_FILE_SIZE (10 * 1024 * 1024) /* 10 MB */

/* Read remote file via SSH cat using GSubprocess (binary-safe). */
static gboolean ssh_cat_file(VibeWindow *win, const char *local_path,
                              char **out_contents, gsize *out_len) {
    char remote[4096];
    to_remote_path(win, local_path, remote, sizeof(remote));

    GPtrArray *av = ssh_argv_new(win);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("cat"));
    g_ptr_array_add(av, g_strdup(remote));
    g_ptr_array_add(av, NULL);

    GError *err = NULL;
    GSubprocess *proc = g_subprocess_newv(
        (const char * const *)av->pdata,
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_SILENCE,
        &err);
    g_ptr_array_unref(av);

    if (!proc) {
        if (err) g_error_free(err);
        return FALSE;
    }

    GBytes *stdout_bytes = NULL;
    gboolean ok = g_subprocess_communicate(proc, NULL, NULL, &stdout_bytes, NULL, &err);

    if (!ok || !g_subprocess_get_successful(proc)) {
        if (stdout_bytes) g_bytes_unref(stdout_bytes);
        g_object_unref(proc);
        if (err) g_error_free(err);
        return FALSE;
    }

    gsize len = 0;
    const char *data = g_bytes_get_data(stdout_bytes, &len);

    if (len > MAX_FILE_SIZE) {
        g_bytes_unref(stdout_bytes);
        g_object_unref(proc);
        *out_contents = g_strdup("(file too large)");
        *out_len = strlen(*out_contents);
        return TRUE;
    }

    /* copy with extra NUL for text safety */
    *out_contents = g_malloc(len + 1);
    if (data && len) memcpy(*out_contents, data, len);
    (*out_contents)[len] = '\0';
    *out_len = len;

    g_bytes_unref(stdout_bytes);
    g_object_unref(proc);
    return TRUE;
}

static void load_file_content(VibeWindow *win, const char *filepath) {
    char *contents = NULL;
    gsize len = 0;
    gboolean ok = FALSE;

    if (path_is_remote(filepath) && win->ssh_host[0]) {
        ok = ssh_cat_file(win, filepath, &contents, &len);
    } else {
        ok = g_file_get_contents(filepath, &contents, &len, NULL);
    }

    if (ok && contents) {
        if (len > MAX_FILE_SIZE) {
            gtk_text_buffer_set_text(win->file_buffer, "(file too large)", -1);
        } else {
            /* binary check — scan first 8 KB for NUL bytes */
            gboolean is_binary = FALSE;
            gsize check = len < 8192 ? len : 8192;
            for (gsize i = 0; i < check; i++) {
                if (contents[i] == '\0') { is_binary = TRUE; break; }
            }
            if (is_binary)
                gtk_text_buffer_set_text(win->file_buffer, "(binary file)", -1);
            else
                gtk_text_buffer_set_text(win->file_buffer, contents, (gssize)len);
        }
        g_free(contents);
    } else {
        gtk_text_buffer_set_text(win->file_buffer, "(cannot read file)", -1);
    }

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(win->file_buffer, &start);
    gtk_text_buffer_place_cursor(win->file_buffer, &start);
    apply_font_intensity(win);
}

/* Wrapper that also starts file monitor — called from row activation */
static void open_and_watch_file(VibeWindow *win, const char *filepath) {
    load_file_content(win, filepath);
    start_file_monitor(win, filepath);
}

static void on_file_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    VibeWindow *win = data;

    GtkWidget *label = gtk_widget_get_first_child(GTK_WIDGET(row));
    const char *full_path = g_object_get_data(G_OBJECT(label), "full-path");
    gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "is-dir"));
    int depth = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "depth"));
    gboolean expanded = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "expanded"));

    if (!full_path) return;

    if (is_dir) {
        int row_idx = gtk_list_box_row_get_index(row);

        if (expanded) {
            /* collapse: remove all children with depth > this row's depth */
            int next_idx = row_idx + 1;
            for (;;) {
                GtkListBoxRow *child_row = gtk_list_box_get_row_at_index(win->file_list, next_idx);
                if (!child_row) break;
                GtkWidget *child_lbl = gtk_widget_get_first_child(GTK_WIDGET(child_row));
                int child_depth = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(child_lbl), "depth"));
                if (child_depth <= depth) break;
                gtk_list_box_remove(win->file_list, GTK_WIDGET(child_row));
            }
            g_object_set_data(G_OBJECT(label), "expanded", GINT_TO_POINTER(FALSE));

            /* update arrow: ▶ */
            const char *name = strrchr(full_path, '/');
            name = name ? name + 1 : full_path;
            gboolean has_kids = path_is_remote(full_path) ? TRUE : dir_has_children(full_path);
            char indent[128] = "";
            for (int i = 0; i < depth; i++) strcat(indent, "  ");
            char buf[512];
            snprintf(buf, sizeof(buf), "%s%s %s", indent, has_kids ? "▶" : " ", name);
            gtk_label_set_text(GTK_LABEL(label), buf);
        } else {
            /* expand: insert children after this row */
            if (path_is_remote(full_path) && win->ssh_host[0])
                ssh_ls_populate(win, full_path, depth + 1, row_idx + 1);
            else
                insert_children(win, full_path, depth + 1, row_idx + 1);
            g_object_set_data(G_OBJECT(label), "expanded", GINT_TO_POINTER(TRUE));

            /* update arrow: ▼ */
            const char *name = strrchr(full_path, '/');
            name = name ? name + 1 : full_path;
            char indent[128] = "";
            for (int i = 0; i < depth; i++) strcat(indent, "  ");
            char buf[512];
            snprintf(buf, sizeof(buf), "%s▼ %s", indent, name);
            gtk_label_set_text(GTK_LABEL(label), buf);
        }
        update_file_status(win);
    } else {
        open_and_watch_file(win, full_path);
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

/* Recursively count all non-hidden files and directories.
   Uses lstat to avoid following symlinks (prevents infinite loops).
   Depth-limited to 64 levels. */
#define COUNT_MAX_DEPTH 64
static void count_entries_r(const char *path, int *files, int *dirs, int depth) {
    if (depth > COUNT_MAX_DEPTH) return;
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;

        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);

        /* Use lstat — don't follow symlinks to avoid cycles */
        struct stat st;
        if (lstat(full, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            (*dirs)++;
            count_entries_r(full, files, dirs, depth + 1);
        } else if (S_ISREG(st.st_mode)) {
            (*files)++;
        }
    }
    closedir(d);
}

static void count_entries(const char *path, int *files, int *dirs) {
    count_entries_r(path, files, dirs, 0);
}

static void update_file_status(VibeWindow *win) {
    if (path_is_remote(win->current_dir) && win->ssh_host[0]) {
        /* for remote: count visible rows in the list instead of traversing FS */
        int count = 0;
        for (int i = 0; ; i++) {
            GtkListBoxRow *r = gtk_list_box_get_row_at_index(win->file_list, i);
            if (!r) break;
            count++;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "%s@%s — %d entries", win->ssh_user, win->ssh_host, count);
        gtk_label_set_text(win->status_label, buf);
    } else {
        const char *count_path = win->root_dir[0] ? win->root_dir : win->current_dir;
        CountCtx *ctx = g_new(CountCtx, 1);
        ctx->win = win;
        g_strlcpy(ctx->path, count_path, sizeof(ctx->path));
        GTask *task = g_task_new(NULL, NULL, count_finished_cb, ctx);
        g_task_set_task_data(task, ctx, NULL);
        g_task_run_in_thread(task, count_thread_func);
        g_object_unref(task);
    }
}

void vibe_window_disconnect_sftp(VibeWindow *win) {
    stop_dir_monitor(win);
    stop_file_monitor(win);
    ssh_ctl_stop(win);
    win->ssh_host[0] = '\0';
    win->ssh_user[0] = '\0';
    win->ssh_port = 0;
    win->ssh_key[0] = '\0';
    win->ssh_remote_path[0] = '\0';
    win->ssh_mount[0] = '\0';

    /* go back to home directory */
    const char *home = g_get_home_dir();
    vibe_window_set_root_directory(win, home);
}

static void on_sftp_disconnect_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    VibeWindow *win = data;
    vibe_window_disconnect_sftp(win);
}

static void update_cursor_label(VibeWindow *win) {
    GtkTextMark *mark = gtk_text_buffer_get_insert(win->file_buffer);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(win->file_buffer, &iter, mark);
    int line = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;
    char buf[128];
    snprintf(buf, sizeof(buf), "Ln %d, Col %d", line, col);
    gtk_label_set_text(win->cursor_label, buf);
}

static void update_status_bar(VibeWindow *win) {
    int page = gtk_notebook_get_current_page(win->notebook);
    if (page == 0) {
        update_file_status(win);
        update_cursor_label(win);
    } else {
        gtk_label_set_text(win->status_label, "");
        gtk_label_set_text(win->cursor_label, "");
    }

    /* SFTP indicator */
    if (win->sftp_box) {
        if (win->ssh_host[0]) {
            char buf[512];
            snprintf(buf, sizeof(buf), "SFTP: %s@%s",
                     win->ssh_user, win->ssh_host);
            gtk_label_set_text(win->sftp_label, buf);
            gtk_widget_set_visible(win->sftp_box, TRUE);
        } else {
            gtk_widget_set_visible(win->sftp_box, FALSE);
        }
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

    /* Cancel all in-flight async operations first */
    g_cancellable_cancel(win->cancellable);

    stop_dir_monitor(win);
    stop_file_monitor(win);
    ssh_ctl_stop(win);
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
    g_object_unref(win->cancellable);
    g_free(win);
}

/* ── Build window ── */

/* Click on path label → open folder chooser */
static void on_path_folder_selected(GObject *src, GAsyncResult *res, gpointer data) {
    VibeWindow *win = data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(src);
    GFile *folder = gtk_file_dialog_select_folder_finish(dialog, res, NULL);
    if (folder) {
        char *path = g_file_get_path(folder);
        if (path) {
            vibe_window_set_root_directory(win, path);
            g_free(path);
        }
        g_object_unref(folder);
    }
}

/* ── Remote directory chooser dialog ── */

typedef struct {
    VibeWindow *win;
    GtkWindow  *dialog;
    GtkListBox *dir_list;
    GtkEntry   *path_entry;
    char        current_remote[4096];
} RemoteDirCtx;

/* List remote directories via SSH ls */
static void remote_dir_populate(RemoteDirCtx *ctx) {
    VibeWindow *win = ctx->win;

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(ctx->dir_list))))
        gtk_list_box_remove(ctx->dir_list, child);

    /* add ".." entry unless at root */
    if (strcmp(ctx->current_remote, "/") != 0) {
        GtkWidget *lbl = gtk_label_new("  ..");
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        g_object_set_data_full(G_OBJECT(lbl), "dir-name", g_strdup(".."), g_free);
        gtk_list_box_append(ctx->dir_list, lbl);
    }

    GPtrArray *av = ssh_argv_new(win);
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("ls"));
    g_ptr_array_add(av, g_strdup("-1pA"));
    g_ptr_array_add(av, g_strdup(ctx->current_remote));

    char *stdout_buf = NULL;
    if (!ssh_spawn_sync(av, &stdout_buf, NULL)) {
        g_ptr_array_unref(av);
        return;
    }
    g_ptr_array_unref(av);

    char **lines = g_strsplit(stdout_buf, "\n", -1);
    g_free(stdout_buf);

    for (char **p = lines; *p && **p; p++) {
        int len = strlen(*p);
        if (len < 1) continue;
        if ((*p)[len - 1] != '/') continue; /* only directories */

        char name[512];
        strncpy(name, *p, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        name[strlen(name) - 1] = '\0'; /* strip trailing / */

        char display[540];
        snprintf(display, sizeof(display), "  %s", name);
        GtkWidget *lbl = gtk_label_new(display);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        g_object_set_data_full(G_OBJECT(lbl), "dir-name", g_strdup(name), g_free);
        gtk_list_box_append(ctx->dir_list, lbl);
    }
    g_strfreev(lines);

    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), ctx->current_remote);
}

static void on_remote_dir_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    RemoteDirCtx *ctx = data;
    GtkWidget *label = gtk_widget_get_first_child(GTK_WIDGET(row));
    const char *name = g_object_get_data(G_OBJECT(label), "dir-name");
    if (!name) return;

    if (strcmp(name, "..") == 0) {
        /* go up */
        char *slash = strrchr(ctx->current_remote, '/');
        if (slash && slash != ctx->current_remote) {
            *slash = '\0';
        } else {
            strcpy(ctx->current_remote, "/");
        }
    } else {
        /* go into subdir */
        size_t len = strlen(ctx->current_remote);
        if (len > 1) /* not "/" */
            snprintf(ctx->current_remote + len, sizeof(ctx->current_remote) - len, "/%s", name);
        else
            snprintf(ctx->current_remote + len, sizeof(ctx->current_remote) - len, "%s", name);
    }
    remote_dir_populate(ctx);
}

static void on_remote_dir_go(GtkButton *btn, gpointer data) {
    (void)btn;
    RemoteDirCtx *ctx = data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry));
    if (text[0] == '/') {
        strncpy(ctx->current_remote, text, sizeof(ctx->current_remote) - 1);
        remote_dir_populate(ctx);
    }
}

static void on_remote_dir_open(GtkButton *btn, gpointer data) {
    (void)btn;
    RemoteDirCtx *ctx = data;
    VibeWindow *win = ctx->win;

    /* update remote path and virtual mount */
    g_strlcpy(win->ssh_remote_path, ctx->current_remote, sizeof(win->ssh_remote_path));
    snprintf(win->ssh_mount, sizeof(win->ssh_mount),
             "/tmp/vibe-light-sftp-%d-%s@%s", (int)getpid(), win->ssh_user, win->ssh_host);
    g_strlcpy(win->root_dir, win->ssh_mount, sizeof(win->root_dir));

    /* send cd to existing terminal session */
    char cd_cmd[4200];
    snprintf(cd_cmd, sizeof(cd_cmd), "cd '%s'\n", ctx->current_remote);
    vte_terminal_feed_child(win->terminal, cd_cmd, strlen(cd_cmd));

    gtk_window_destroy(ctx->dialog);

    /* refresh file browser only (terminal already has the session) */
    vibe_window_open_directory(win, win->ssh_mount);
}

static void on_remote_dir_destroy(GtkWidget *w, gpointer data) {
    (void)w;
    g_free(data);
}

static void show_remote_dir_chooser(VibeWindow *win) {
    RemoteDirCtx *ctx = g_new0(RemoteDirCtx, 1);
    ctx->win = win;
    strncpy(ctx->current_remote, win->ssh_remote_path, sizeof(ctx->current_remote) - 1);
    if (!ctx->current_remote[0]) strcpy(ctx->current_remote, "/");

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Remote Directory");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 420, 400);
    ctx->dialog = GTK_WINDOW(dialog);
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_remote_dir_destroy), ctx);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_start(vbox, 8);
    gtk_widget_set_margin_end(vbox, 8);
    gtk_widget_set_margin_top(vbox, 8);
    gtk_widget_set_margin_bottom(vbox, 8);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    /* path entry + Go button */
    GtkWidget *path_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    ctx->path_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->path_entry), TRUE);
    gtk_box_append(GTK_BOX(path_box), GTK_WIDGET(ctx->path_entry));
    GtkWidget *go_btn = gtk_button_new_with_label("Go");
    g_signal_connect(go_btn, "clicked", G_CALLBACK(on_remote_dir_go), ctx);
    gtk_box_append(GTK_BOX(path_box), go_btn);
    gtk_box_append(GTK_BOX(vbox), path_box);

    /* directory list */
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(scroll, TRUE);
    ctx->dir_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->dir_list, GTK_SELECTION_SINGLE);
    g_signal_connect(ctx->dir_list, "row-activated", G_CALLBACK(on_remote_dir_activated), ctx);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(ctx->dir_list));
    gtk_box_append(GTK_BOX(vbox), scroll);

    /* Open button */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 4);
    GtkWidget *open_btn = gtk_button_new_with_label("Open");
    gtk_widget_add_css_class(open_btn, "suggested-action");
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_remote_dir_open), ctx);
    gtk_box_append(GTK_BOX(btn_box), open_btn);
    gtk_box_append(GTK_BOX(vbox), btn_box);

    remote_dir_populate(ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_path_label_clicked(GtkGestureClick *gesture, int n_press,
                                   double x, double y, gpointer data) {
    (void)gesture; (void)n_press; (void)x; (void)y;
    VibeWindow *win = data;

    /* if connected via SFTP, show remote directory chooser */
    if (win->ssh_host[0] && path_is_remote(win->current_dir)) {
        show_remote_dir_chooser(win);
        return;
    }

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Folder");

    if (win->root_dir[0]) {
        GFile *init = g_file_new_for_path(win->root_dir);
        gtk_file_dialog_set_initial_folder(dialog, init);
        g_object_unref(init);
    }

    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(win->window), NULL,
                                   on_path_folder_selected, win);
}

static GtkWidget *build_menu_button(void) {
    GMenu *menu = g_menu_new();
    g_menu_append(menu, "Open Folder", "win.open-folder");
    g_menu_append(menu, "SFTP/SSH", "win.sftp");
    g_menu_append(menu, "Settings", "win.settings");

    GtkWidget *btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn), G_MENU_MODEL(menu));
    g_object_unref(menu);
    return btn;
}

VibeWindow *vibe_window_new(GtkApplication *app) {
    VibeWindow *win = g_new0(VibeWindow, 1);
    win->cancellable = g_cancellable_new();
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
    gtk_widget_set_cursor_from_name(GTK_WIDGET(win->path_label), "pointer");

    /* click on path label → open folder dialog */
    GtkGesture *path_click = gtk_gesture_click_new();
    g_signal_connect(path_click, "released", G_CALLBACK(on_path_label_clicked), win);
    gtk_widget_add_controller(GTK_WIDGET(win->path_label), GTK_EVENT_CONTROLLER(path_click));

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

    /* SFTP indicator + disconnect button */
    win->sftp_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_visible(win->sftp_box, FALSE);

    win->sftp_label = GTK_LABEL(gtk_label_new(""));
    gtk_widget_add_css_class(GTK_WIDGET(win->sftp_label), "dim-label");
    gtk_box_append(GTK_BOX(win->sftp_box), GTK_WIDGET(win->sftp_label));

    win->sftp_disconnect_btn = GTK_BUTTON(gtk_button_new_with_label("Disconnect"));
    gtk_widget_add_css_class(GTK_WIDGET(win->sftp_disconnect_btn), "flat");
    gtk_widget_add_css_class(GTK_WIDGET(win->sftp_disconnect_btn), "destructive-action");
    g_signal_connect(win->sftp_disconnect_btn, "clicked",
                     G_CALLBACK(on_sftp_disconnect_clicked), win);
    gtk_box_append(GTK_BOX(win->sftp_box), GTK_WIDGET(win->sftp_disconnect_btn));

    gtk_box_append(GTK_BOX(status_bar), win->sftp_box);

    win->cursor_label = GTK_LABEL(gtk_label_new(""));
    gtk_widget_set_halign(GTK_WIDGET(win->cursor_label), GTK_ALIGN_END);
    gtk_widget_add_css_class(GTK_WIDGET(win->cursor_label), "dim-label");
    gtk_box_append(GTK_BOX(status_bar), GTK_WIDGET(win->cursor_label));

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
