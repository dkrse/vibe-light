#define _DEFAULT_SOURCE
#include <adwaita.h>
#include "window.h"
#include "actions.h"
#include "ssh.h"
#include "prompt_log.h"
#include "theme.h"
#include "editor.h"
#include "ai.h"
#include "filebrowser.h"
#include <webkit/webkit.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <math.h>
#include <pwd.h>
#include <unistd.h>

/* Build indentation string safely (max depth capped to prevent overflow) */
static void build_indent(char *buf, size_t bufsize, int depth) {
    buf[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < depth && pos + 2 < bufsize; i++) {
        buf[pos++] = ' ';
        buf[pos++] = ' ';
    }
    buf[pos] = '\0';
}


/* Theme functions in theme.c */

static gboolean intensity_idle_cb(gpointer data) {
    VibeWindow *win = data;
    win->intensity_idle_id = 0;
    /* Only prompt needs per-buffer tag intensity; file viewer uses CSS opacity */
    if (win->settings.editor_font_intensity < 0.99)
        apply_font_intensity(win);
    return G_SOURCE_REMOVE;
}

void update_status_bar(VibeWindow *win);
static void update_status_bar_page(VibeWindow *win, int page);

static void on_cursor_moved(GtkTextBuffer *buffer, GParamSpec *pspec, gpointer data) {
    (void)buffer; (void)pspec;
    VibeWindow *win = data;
    update_status_bar(win);
}

static void on_file_buffer_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    VibeWindow *win = data;
    if (win->settings.editor_font_intensity < 0.99 && win->intensity_idle_id == 0)
        win->intensity_idle_id = g_idle_add(intensity_idle_cb, win);
}

static void on_prompt_buffer_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    VibeWindow *win = data;
    if (win->settings.editor_font_intensity < 0.99) {
        GtkTextIter ps, pe;
        gtk_text_buffer_get_bounds(win->prompt_buffer, &ps, &pe);
        gtk_text_buffer_apply_tag(win->prompt_buffer, win->prompt_intensity_tag, &ps, &pe);
    }
}

/* Current line highlighting and line numbers are handled by GtkSourceView */

/* ── Apply all settings ── */

void vibe_window_apply_settings(VibeWindow *win) {
    apply_theme(win);

    /* Re-render AI webview with updated theme colors */
    ai_refresh_output(win);

    /* terminal font */
    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, win->settings.terminal_font);
    pango_font_description_set_size(fd, win->settings.terminal_font_size * PANGO_SCALE);
    vte_terminal_set_font(win->terminal, fd);
    pango_font_description_free(fd);

    /* wrap mode (editor only) */
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(win->file_view),
        win->settings.wrap_lines ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);

    /* line spacing */
    int extra = (int)((win->settings.line_spacing - 1.0) * win->settings.editor_font_size * 0.5);
    if (extra < 0) extra = 0;
    gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(win->file_view), extra);
    gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(win->file_view), extra);
    gtk_text_view_set_pixels_above_lines(win->prompt_view, extra);
    gtk_text_view_set_pixels_below_lines(win->prompt_view, extra);

    /* GtkSourceView: line numbers + current line highlight + style scheme */
    gtk_source_view_set_show_line_numbers(win->file_view, win->settings.show_line_numbers);
    gtk_source_view_set_highlight_current_line(win->file_view, win->settings.highlight_current_line);

    /* Editor font intensity via widget opacity */
    gtk_widget_set_opacity(GTK_WIDGET(win->file_view), win->settings.editor_font_intensity);

    /* Update source style scheme to match theme */
    GtkSourceStyleSchemeManager *ssm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = NULL;
    if (is_dark_theme(win->settings.theme))
        scheme = gtk_source_style_scheme_manager_get_scheme(ssm, "Adwaita-dark");
    else
        scheme = gtk_source_style_scheme_manager_get_scheme(ssm, "Adwaita");
    if (scheme)
        gtk_source_buffer_set_style_scheme(win->file_buffer, scheme);

    /* font intensity (editor) */
    apply_font_intensity(win);

    /* terminal colors + intensity */
    apply_terminal_colors(win);
    gtk_widget_set_opacity(GTK_WIDGET(win->terminal), win->settings.terminal_font_intensity);

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

    /* compute rgba color strings for per-section intensity */
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

    /* locale-safe rgba helper: alpha 1.0 → "1", else "0.XX" */
    #define MAKE_RGBA(buf, alpha) do { \
        if ((alpha) >= 0.995) \
            snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,1)", r, g, b); \
        else \
            snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,0.%02d)", r, g, b, (int)((alpha)*100)); \
    } while(0)

    char fg_gui[64], fg_browser[64], fg_editor[64];
    MAKE_RGBA(fg_gui, win->settings.gui_font_intensity);
    MAKE_RGBA(fg_browser, win->settings.browser_font_intensity);
    MAKE_RGBA(fg_editor, win->settings.editor_font_intensity);
    #undef MAKE_RGBA

    char *font_css = g_strdup_printf(
             /* GUI font — headerbar, tabs, labels, menus, dialogs, status bar */
             "window,headerbar,.titlebar{font-family:%s;font-size:%dpt;}"
             "headerbar button,headerbar menubutton button,headerbar menubutton"
             "{font-family:%s;font-size:%dpt;}"
             "notebook header tab{font-family:%s;font-size:%dpt;}"
             "popover modelbutton{font-family:%s;font-size:%dpt;}"
             "popover>contents,popover.menu>contents{font-family:%s;font-size:%dpt;}"
             ".path-bar{font-family:%s;font-size:%dpt;}"
             ".statusbar label{font-family:%s;font-size:%dpt;}"
             /* per-section fonts */
             ".file-viewer{font-family:%s;font-size:%dpt;font-weight:%d;caret-color:%s;}"
             ".file-browser{font-family:%s;font-size:%dpt;}"
             ".file-browser label{color:%s;}"
             ".prompt-view{font-family:%s;font-size:%dpt;caret-color:%s;}"
             /* colors — GUI intensity for chrome elements */
             ".path-bar{color:%s;}"
             ".statusbar label{color:%s;}"
             "headerbar{color:%s;}"
             "headerbar button,headerbar menubutton button,headerbar menubutton{color:%s;}"
             "notebook header tab{color:%s;}"
             "popover modelbutton{color:%s;}"
             "popover>contents,popover.menu>contents{color:%s;}"
             "window label{color:%s;}"
             "window checkbutton label{color:%s;}",
             /* GUI font args */
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             win->settings.gui_font, win->settings.gui_font_size,
             /* per-section font args */
             win->settings.editor_font, win->settings.editor_font_size, win->settings.editor_font_weight, fg_editor,
             win->settings.browser_font, win->settings.browser_font_size,
             fg_browser,
             win->settings.prompt_font, win->settings.prompt_font_size, fg_editor,
             /* color args — GUI intensity */
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui,
             fg_gui);

    char *full_css = g_strconcat(theme_css, font_css, NULL);
    gtk_css_provider_load_from_string(win->css_provider, full_css);
    g_free(full_css);
    g_free(font_css);

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

/* Use ssh_path_is_remote() from ssh.h — alias for readability */
#define path_is_remote(p) ssh_path_is_remote(p)

/* ── Git status ── */

/* Status characters: M=modified, A=staged, ?=untracked, D=deleted, U=conflict, I=ignored */
/* Colors for Pango markup */
#define GIT_COLOR_MODIFIED  "#e8a838"
#define GIT_COLOR_STAGED    "#73c991"
#define GIT_COLOR_UNTRACKED "#888888"
#define GIT_COLOR_DELETED   "#f14c4c"
#define GIT_COLOR_CONFLICT  "#e51400"
#define GIT_COLOR_IGNORED   "#555555"

static const char *git_status_color(int status) {
    switch (status) {
        case 'M': return GIT_COLOR_MODIFIED;
        case 'A': return GIT_COLOR_STAGED;
        case '?': return GIT_COLOR_UNTRACKED;
        case 'D': return GIT_COLOR_DELETED;
        case 'U': return GIT_COLOR_CONFLICT;
        case 'I': return GIT_COLOR_IGNORED;
        default:  return NULL;
    }
}

/* Compute relative path from git root. Returns static/stack buffer — caller must use immediately. */
static const char *git_rel_path(VibeWindow *win, const char *full_path,
                                 char *buf, size_t buflen) {
    if (path_is_remote(full_path)) {
        char remote[4096];
        ssh_to_remote_path(win->ssh_mount, win->ssh_remote_path,
                           full_path, remote, sizeof(remote));
        size_t root_len = strlen(win->git_root);
        if (strncmp(remote, win->git_root, root_len) == 0) {
            const char *r = remote + root_len;
            if (r[0] == '/') r++;
            g_strlcpy(buf, r, buflen);
            return buf;
        }
    } else {
        size_t root_len = strlen(win->git_root);
        if (strncmp(full_path, win->git_root, root_len) == 0) {
            const char *r = full_path + root_len;
            if (r[0] == '/') r++;
            return r; /* points into full_path — stable */
        }
    }
    return NULL;
}

/* Check if a path or any of its parent dirs is ignored.
   Checks both "dir/" entries and individual file entries. */
static gboolean is_path_ignored(VibeWindow *win, const char *rel) {
    if (!rel || !win->git_status) return FALSE;

    /* Direct lookup: exact file "path" */
    gpointer val = g_hash_table_lookup(win->git_status, rel);
    if (val && GPOINTER_TO_INT(val) == 'I') return TRUE;

    /* Check if any parent dir is ignored: "dir/" entries */
    char parent[4096];
    g_strlcpy(parent, rel, sizeof(parent));
    for (;;) {
        char *slash = strrchr(parent, '/');
        if (!slash) break;
        slash[1] = '\0'; /* keep trailing slash: "dir/" */
        val = g_hash_table_lookup(win->git_status, parent);
        if (val && GPOINTER_TO_INT(val) == 'I') return TRUE;
        slash[0] = '\0'; /* remove slash for next iteration */
    }
    return FALSE;
}

/* Check if a directory contains only ignored entries (all children are 'I') */
static gboolean is_dir_all_ignored(VibeWindow *win, const char *prefix) {
    if (!win->git_status) return FALSE;
    size_t plen = strlen(prefix);
    gboolean found_any = FALSE;

    GHashTableIter iter;
    gpointer key, val;
    g_hash_table_iter_init(&iter, win->git_status);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
        const char *k = key;
        if (strncmp(k, prefix, plen) == 0 && k[plen] == '/') {
            found_any = TRUE;
            if (GPOINTER_TO_INT(val) != 'I') return FALSE;
        }
    }
    return found_any; /* TRUE only if all children are ignored */
}

/* Look up git status for a file. Returns 0 if no status (clean). */
static int git_status_for_path(VibeWindow *win, const char *full_path) {
    if (!win->git_status || !win->git_root[0]) return 0;

    char buf[4096];
    const char *rel = git_rel_path(win, full_path, buf, sizeof(buf));
    if (!rel || !rel[0]) return 0;

    /* Check ignored (including parent dirs) */
    if (is_path_ignored(win, rel)) return 'I';

    gpointer val = g_hash_table_lookup(win->git_status, rel);
    return val ? GPOINTER_TO_INT(val) : 0;
}

/* Check if any file under a directory path has a git status */
static int git_status_for_dir(VibeWindow *win, const char *full_path) {
    if (!win->git_status || !win->git_root[0]) return 0;

    char buf[4096];
    const char *prefix = git_rel_path(win, full_path, buf, sizeof(buf));
    if (!prefix) return 0;

    /* Check if this dir itself is listed as ignored: "dir/" */
    char dir_key[4096];
    snprintf(dir_key, sizeof(dir_key), "%s/", prefix);
    gpointer dv = g_hash_table_lookup(win->git_status, dir_key);
    if (dv && GPOINTER_TO_INT(dv) == 'I') return 'I';

    /* Check if a parent dir is ignored */
    if (is_path_ignored(win, prefix)) return 'I';

    /* Check if ALL children in this dir are ignored (e.g. build/ .o all ignored) */
    if (is_dir_all_ignored(win, prefix)) return 'I';

    size_t plen = strlen(prefix);
    int result = 0;
    GHashTableIter iter;
    gpointer key, val;
    g_hash_table_iter_init(&iter, win->git_status);
    while (g_hash_table_iter_next(&iter, &key, &val)) {
        const char *k = key;
        if (strncmp(k, prefix, plen) == 0 &&
            (plen == 0 || k[plen] == '/')) {
            int s = GPOINTER_TO_INT(val);
            if (s == 'I') continue; /* don't count ignored towards dir status color */
            /* Priority: conflict > modified > staged > untracked > deleted */
            if (s == 'U') return 'U';
            if (s == 'M' && result != 'U') result = 'M';
            else if (s == 'A' && result != 'M' && result != 'U') result = 'A';
            else if (s == '?' && !result) result = '?';
            else if (s == 'D' && !result) result = 'D';
        }
    }
    return result;
}

/* Parse `git status --porcelain` output into the hash table */
static void parse_git_status(VibeWindow *win, const char *output) {
    if (win->git_status)
        g_hash_table_remove_all(win->git_status);
    else
        win->git_status = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

    if (!output) return;

    char **lines = g_strsplit(output, "\n", -1);
    for (char **p = lines; *p && **p; p++) {
        const char *line = *p;
        if (strlen(line) < 4) continue; /* "XY filename" minimum */

        char x = line[0], y = line[1];
        const char *path = line + 3;

        /* Skip leading quotes for paths with spaces */
        if (path[0] == '"') {
            path++;
            /* Remove trailing quote if present */
        }

        int status = 0;
        if (x == '!' && y == '!')
            status = 'I'; /* ignored */
        else if (x == 'U' || y == 'U' || (x == 'A' && y == 'A') || (x == 'D' && y == 'D'))
            status = 'U'; /* conflict */
        else if (x == '?' && y == '?')
            status = '?'; /* untracked */
        else if (x == 'A' || x == 'R' || x == 'C')
            status = 'A'; /* staged */
        else if (x == 'D' || y == 'D')
            status = 'D'; /* deleted */
        else if (x == 'M' || y == 'M')
            status = 'M'; /* modified */

        if (status) {
            /* For renames "R  old -> new", use the new path */
            const char *arrow = strstr(path, " -> ");
            if (arrow) path = arrow + 4;

            /* Remove trailing quote */
            char *clean = g_strdup(path);
            size_t clen = strlen(clean);
            if (clen > 0 && clean[clen - 1] == '"') clean[clen - 1] = '\0';

            g_hash_table_insert(win->git_status, clean, GINT_TO_POINTER(status));
        }
    }
    g_strfreev(lines);
}

/* Async git status fetch context */
typedef struct {
    VibeWindow *win;
    char       *git_root;    /* result: git root path */
    char       *status_output; /* result: porcelain output */
    /* SSH params snapshot */
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[512];
    char        dir_path[4096];  /* directory to check */
    gboolean    is_remote;
} GitStatusCtx;

static void git_status_thread(GTask *task, gpointer src, gpointer data,
                               GCancellable *cancel) {
    (void)src; (void)cancel;
    GitStatusCtx *ctx = data;

    if (ctx->is_remote) {
        /* Remote: run via SSH — get git root */
        GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                              ctx->ssh_port, ctx->ssh_key,
                                              ctx->ssh_ctl_path);
        g_ptr_array_add(av, g_strdup("--"));
        g_ptr_array_add(av, g_strdup("git"));
        g_ptr_array_add(av, g_strdup("-C"));
        g_ptr_array_add(av, g_strdup(ctx->dir_path));
        g_ptr_array_add(av, g_strdup("rev-parse"));
        g_ptr_array_add(av, g_strdup("--show-toplevel"));

        char *root = NULL;
        if (ssh_spawn_sync(av, &root, NULL) && root) {
            g_strstrip(root);
            ctx->git_root = root;
        }
        g_ptr_array_unref(av);

        /* Get status if we found a git root */
        if (ctx->git_root) {
            GPtrArray *av2 = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                                    ctx->ssh_port, ctx->ssh_key,
                                                    ctx->ssh_ctl_path);
            g_ptr_array_add(av2, g_strdup("--"));
            g_ptr_array_add(av2, g_strdup("git"));
            g_ptr_array_add(av2, g_strdup("-C"));
            g_ptr_array_add(av2, g_strdup(ctx->git_root));
            g_ptr_array_add(av2, g_strdup("status"));
            g_ptr_array_add(av2, g_strdup("--porcelain"));
            g_ptr_array_add(av2, g_strdup("-u"));
            g_ptr_array_add(av2, g_strdup("--ignored"));

            char *status = NULL;
            ssh_spawn_sync(av2, &status, NULL);
            ctx->status_output = status;
            g_ptr_array_unref(av2);
        }
    } else {
        /* Local: run git directly */
        const char *root_argv[] = {"git", "-C", ctx->dir_path, "rev-parse",
                                    "--show-toplevel", NULL};
        char *root = NULL;
        gint exit_status = 0;
        if (g_spawn_sync(NULL, (char **)root_argv, NULL, G_SPAWN_SEARCH_PATH,
                          NULL, NULL, &root, NULL, &exit_status, NULL) &&
            g_spawn_check_wait_status(exit_status, NULL) && root) {
            g_strstrip(root);
            ctx->git_root = root;

            const char *status_argv[] = {"git", "-C", root, "status",
                                          "--porcelain", "-u", "--ignored=matching", NULL};
            char *status = NULL;
            if (g_spawn_sync(NULL, (char **)status_argv, NULL, G_SPAWN_SEARCH_PATH,
                              NULL, NULL, &status, NULL, &exit_status, NULL) &&
                g_spawn_check_wait_status(exit_status, NULL)) {
                ctx->status_output = status;
            } else {
                g_free(status);
            }
        } else {
            g_free(root);
        }
    }
    g_task_return_boolean(task, ctx->git_root != NULL);
}

/* Apply git status colors to existing rows in the file list */
static void apply_git_status_to_rows(VibeWindow *win);

static void refresh_git_status(VibeWindow *win);
static gboolean git_poll_cb(gpointer data);

static void git_status_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    GitStatusCtx *ctx = data;
    VibeWindow *win = ctx->win;
    win->git_status_in_flight = FALSE;

    GError *err = NULL;
    g_task_propagate_boolean(G_TASK(res), &err);
    if (err) g_error_free(err);
    if (g_cancellable_is_cancelled(win->cancellable)) {
        g_free(ctx->git_root);
        g_free(ctx->status_output);
        g_free(ctx);
        return;
    }

    if (ctx->git_root) {
        g_strlcpy(win->git_root, ctx->git_root, sizeof(win->git_root));
        parse_git_status(win, ctx->status_output);
        apply_git_status_to_rows(win);
    } else {
        win->git_root[0] = '\0';
        if (win->git_status) g_hash_table_remove_all(win->git_status);
    }

    g_free(ctx->git_root);
    g_free(ctx->status_output);
    g_free(ctx);

    /* Schedule next git status poll in 3 seconds */
    if (win->git_poll_id)
        g_source_remove(win->git_poll_id);
    win->git_poll_id = g_timeout_add(3000, git_poll_cb, win);
}

static gboolean git_poll_cb(gpointer data) {
    VibeWindow *win = data;
    win->git_poll_id = 0;
    refresh_git_status(win);
    return G_SOURCE_REMOVE;
}

static void refresh_git_status(VibeWindow *win) {
    if (win->git_status_in_flight) return;
    if (!win->current_dir[0]) return;

    GitStatusCtx *ctx = g_new0(GitStatusCtx, 1);
    ctx->win = win;
    ctx->is_remote = path_is_remote(win->current_dir) && win->ssh_host[0];

    if (ctx->is_remote) {
        char remote[4096];
        ssh_to_remote_path(win->ssh_mount, win->ssh_remote_path,
                           win->current_dir, remote, sizeof(remote));
        g_strlcpy(ctx->dir_path, remote, sizeof(ctx->dir_path));
        g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
        g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
        ctx->ssh_port = win->ssh_port;
        g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
        g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));
    } else {
        g_strlcpy(ctx->dir_path, win->root_dir[0] ? win->root_dir : win->current_dir,
                  sizeof(ctx->dir_path));
    }

    win->git_status_in_flight = TRUE;
    GTask *task = g_task_new(NULL, win->cancellable, git_status_done, ctx);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, git_status_thread);
    g_object_unref(task);
}

/* Walk all rows and update labels with git status indicators.
   Also hides ignored files when show_gitignored == 0. */
static void apply_git_status_to_rows(VibeWindow *win) {
    int show_ignored = win->settings.show_gitignored;

    for (int i = 0; ; i++) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, i);
        if (!row) break;
        GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
        if (!lbl) continue;

        const char *full_path = g_object_get_data(G_OBJECT(lbl), "full-path");
        if (!full_path) {
            /* "Show more" rows etc — always visible */
            gtk_widget_set_visible(GTK_WIDGET(row), TRUE);
            continue;
        }

        gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));
        int status = is_dir ? git_status_for_dir(win, full_path)
                            : git_status_for_path(win, full_path);

        /* Handle ignored files */
        if (status == 'I') {
            if (show_ignored == 0) {
                /* Hide ignored */
                gtk_widget_set_visible(GTK_WIDGET(row), FALSE);
                continue;
            }
            /* show_ignored == 1: show in gray (fall through to color) */
        }

        gtk_widget_set_visible(GTK_WIDGET(row), TRUE);

        const char *color = git_status_color(status);
        const char *current_text = gtk_label_get_text(GTK_LABEL(lbl));
        if (!current_text) continue;

        if (status == 'I') {
            char *escaped = g_markup_escape_text(current_text, -1);
            char markup[1024];
            snprintf(markup, sizeof(markup),
                     "<span style=\"italic\" alpha=\"50%%\">%s</span>", escaped);
            gtk_label_set_markup(GTK_LABEL(lbl), markup);
            g_free(escaped);
        } else if (color) {
            char *escaped = g_markup_escape_text(current_text, -1);
            char markup[1024];
            snprintf(markup, sizeof(markup),
                     "<span foreground=\"%s\">%s</span>", color, escaped);
            gtk_label_set_markup(GTK_LABEL(lbl), markup);
            g_free(escaped);
        } else {
            gtk_label_set_text(GTK_LABEL(lbl), current_text);
        }
    }
}

/* ── Create a label row for the tree browser ── */

static GtkWidget *create_tree_row(const char *full_path, const char *name,
                                   gboolean is_dir, int depth) {
    char label[512];
    /* build indentation */
    char indent[128];
    build_indent(indent, sizeof(indent), depth);

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

/* Maximum entries to show before inserting a "Show more" row */
#define DIR_BATCH_SIZE 500

/* Data attached to "Show more" rows for lazy expansion */
typedef struct {
    char   **names;     /* remaining entry names (owned) */
    gboolean *is_dirs;  /* whether each entry is a dir */
    int       count;    /* number of remaining entries */
    char      dir_path[4096];
    int       depth;
} LazyLoadData;

static void lazy_load_data_free(gpointer p) {
    LazyLoadData *d = p;
    for (int i = 0; i < d->count; i++) g_free(d->names[i]);
    g_free(d->names);
    g_free(d->is_dirs);
    g_free(d);
}

/* Insert sorted directory entries after a given row position.
   Returns number of inserted rows (including possible "Show more" row). */
static int insert_children(VibeWindow *win, const char *dir_path, int depth,
                            int insert_pos) {
    DIR *dir = opendir(dir_path);
    if (!dir) return 0;

    struct dirent **entries = NULL;
    int count = 0, capacity = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        /* Always skip . and .. and .git */
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        if (strcmp(ent->d_name, ".git") == 0) continue;
        /* Skip dotfiles unless show_hidden is enabled */
        if (ent->d_name[0] == '.' && !win->settings.show_hidden) continue;
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
        if (!entries[count]) {
            for (int i = 0; i < count; i++) free(entries[i]);
            free(entries);
            closedir(dir);
            return 0;
        }
        memcpy(entries[count], ent, sizeof(struct dirent));
        count++;
    }
    closedir(dir);

    if (!entries) return 0;

    qsort(entries, count, sizeof(struct dirent *), entry_compare);

    int show = count <= DIR_BATCH_SIZE ? count : DIR_BATCH_SIZE;
    int inserted = 0;

    for (int i = 0; i < show; i++) {
        char full[4096];
        snprintf(full, sizeof(full), "%s/%s", dir_path, entries[i]->d_name);

        gboolean is_dir_entry;
        if (entries[i]->d_type != DT_UNKNOWN)
            is_dir_entry = (entries[i]->d_type == DT_DIR);
        else {
            struct stat st;
            is_dir_entry = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
        }

        GtkWidget *lbl = create_tree_row(full, entries[i]->d_name, is_dir_entry, depth);
        int pos = insert_pos >= 0 ? insert_pos + inserted : -1;
        gtk_list_box_insert(win->file_list, lbl, pos);
        inserted++;
    }

    /* If there are more entries, add a "Show more" row */
    if (count > DIR_BATCH_SIZE) {
        int remaining = count - DIR_BATCH_SIZE;
        LazyLoadData *lazy = g_new0(LazyLoadData, 1);
        lazy->count = remaining;
        lazy->depth = depth;
        g_strlcpy(lazy->dir_path, dir_path, sizeof(lazy->dir_path));
        lazy->names = g_new(char *, remaining);
        lazy->is_dirs = g_new(gboolean, remaining);

        for (int i = 0; i < remaining; i++) {
            int si = DIR_BATCH_SIZE + i;
            lazy->names[i] = g_strdup(entries[si]->d_name);
            if (entries[si]->d_type != DT_UNKNOWN)
                lazy->is_dirs[i] = (entries[si]->d_type == DT_DIR);
            else {
                char full[4096];
                snprintf(full, sizeof(full), "%s/%s", dir_path, entries[si]->d_name);
                struct stat st;
                lazy->is_dirs[i] = (stat(full, &st) == 0 && S_ISDIR(st.st_mode));
            }
        }

        char indent[128];
        build_indent(indent, sizeof(indent), depth);
        char label_text[256];
        snprintf(label_text, sizeof(label_text), "%s  ⋯ Show %d more…", indent, remaining);
        GtkWidget *lbl = gtk_label_new(label_text);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_widget_add_css_class(lbl, "dim-label");

        g_object_set_data(G_OBJECT(lbl), "is-dir", GINT_TO_POINTER(FALSE));
        g_object_set_data(G_OBJECT(lbl), "depth", GINT_TO_POINTER(depth));
        g_object_set_data(G_OBJECT(lbl), "expanded", GINT_TO_POINTER(FALSE));
        g_object_set_data_full(G_OBJECT(lbl), "lazy-load", lazy, lazy_load_data_free);

        int pos = insert_pos >= 0 ? insert_pos + inserted : -1;
        gtk_list_box_insert(win->file_list, lbl, pos);
        inserted++;
    }

    for (int i = 0; i < count; i++) free(entries[i]);
    free(entries);
    return inserted;
}

/* ── SSH ls-based directory listing (avoids FUSE blocking) ── */

/* Wrapper for ssh_to_remote_path using window state */
static const char *to_remote_path(VibeWindow *win, const char *local_path,
                                   char *buf, size_t buflen) {
    return ssh_to_remote_path(win->ssh_mount, win->ssh_remote_path,
                              local_path, buf, buflen);
}

/* Wrapper for ssh_argv_new using window state */
static GPtrArray *win_ssh_argv(VibeWindow *win) {
    return ssh_argv_new(win->ssh_host, win->ssh_user, win->ssh_port,
                        win->ssh_key, win->ssh_ctl_path);
}


/* ── SSH ControlMaster lifecycle ── */

/* Wrappers for ssh_ctl_start/stop using window state */
static void win_ssh_ctl_start(VibeWindow *win) {
    ssh_ctl_start(win->ssh_ctl_dir, sizeof(win->ssh_ctl_dir),
                  win->ssh_ctl_path, sizeof(win->ssh_ctl_path),
                  win->ssh_host, win->ssh_user, win->ssh_port, win->ssh_key);
}

static void win_ssh_ctl_stop(VibeWindow *win) {
    ssh_ctl_stop(win->ssh_ctl_path, win->ssh_ctl_dir,
                 win->ssh_host, win->ssh_user);
}

/* ssh_spawn_sync is now in ssh.c */

/* Run SSH ls synchronously and populate file list */
static void ssh_ls_populate(VibeWindow *win, const char *local_dir_path,
                             int depth, int insert_pos) {
    char remote[4096];
    to_remote_path(win, local_dir_path, remote, sizeof(remote));

    GPtrArray *av = win_ssh_argv(win);
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
            /* Always skip .git; skip other dotfiles unless show_hidden */
            if (strcmp(name, ".git") == 0 || strcmp(name, ".git/") == 0) continue;
            if (name[0] == '.' && !win->settings.show_hidden) continue;
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
    if (win->git_poll_id) {
        g_source_remove(win->git_poll_id);
        win->git_poll_id = 0;
    }
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
            char indent[128];
            build_indent(indent, sizeof(indent), depth);
            char buf[512];
            snprintf(buf, sizeof(buf), "%s▼ %s", indent, name);
            gtk_label_set_text(GTK_LABEL(lbl), buf);
            break;
        }
    }
}

/* Repopulate the file list, preserving expanded dirs (local — runs on main thread) */
static void refresh_file_list_local(VibeWindow *win) {
    filebrowser_cancel_inline_edit(win);
    GPtrArray *expanded = collect_expanded_paths(win);

    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(win->file_list))))
        gtk_list_box_remove(win->file_list, child);

    insert_children(win, win->current_dir, 0, -1);
    restore_expanded(win, expanded);
    g_ptr_array_unref(expanded);
    update_file_status(win);
    if (win->git_status)
        apply_git_status_to_rows(win);
    refresh_git_status(win);
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
    /* SSH params snapshot (thread safety — avoid reading win-> from thread) */
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[512];
    char        ssh_mount[2048];
    char        ssh_remote_path[1024];
} RemoteRefreshCtx;

static void remote_refresh_thread(GTask *task, gpointer src, gpointer data,
                                   GCancellable *cancel) {
    (void)src; (void)cancel;
    RemoteRefreshCtx *ctx = data;

    /* Use copied SSH params — safe from race with disconnect on main thread */
    GPtrArray *av = ssh_argv_from_params(ctx->ssh_host, ctx->ssh_user,
                                          ctx->ssh_port, ctx->ssh_key,
                                          ctx->ssh_ctl_path);
    char remote[4096];
    ssh_to_remote_path(ctx->ssh_mount, ctx->ssh_remote_path,
                       ctx->local_dir, remote, sizeof(remote));

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
                if (strcmp(name, ".git") == 0 || strcmp(name, ".git/") == 0) continue;
                if (name[0] == '.' && !win->settings.show_hidden) continue;
                gboolean is_dir = (name[len - 1] == '/');
                if ((pass == 0 && !is_dir) || (pass == 1 && is_dir)) continue;

                char clean_name[512];
                g_strlcpy(clean_name, name, sizeof(clean_name));
                if (is_dir && clean_name[0])
                    clean_name[strlen(clean_name) - 1] = '\0';

                char full[4608];
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
    refresh_git_status(win);
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
        g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
        g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
        ctx->ssh_port = win->ssh_port;
        g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
        g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));
        g_strlcpy(ctx->ssh_mount, win->ssh_mount, sizeof(ctx->ssh_mount));
        g_strlcpy(ctx->ssh_remote_path, win->ssh_remote_path, sizeof(ctx->ssh_remote_path));
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

/* djb2_hash is now in ssh.c as ssh_djb2_hash */

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

/* RemoteDirPollCtx and thread function are now SshDirPollCtx in ssh.c */

static void remote_dir_poll_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    SshDirPollCtx *ctx = data;
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

    SshDirPollCtx *ctx = g_new0(SshDirPollCtx, 1);
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
    g_task_run_in_thread(task, ssh_dir_poll_thread);
    g_object_unref(task);

    return G_SOURCE_CONTINUE;
}

static void start_remote_fallback_poll(VibeWindow *win) {
    win->remote_dir_hash = 0;
    remote_dir_poll_tick(win);
    win->remote_dir_poll_id = g_timeout_add_seconds(2, remote_dir_poll_tick, win);
}

/* Try to spawn inotifywait; if it fails (exit code != 0 quickly), fall back to polling */

/* InotifyCheckCtx and thread function are now SshInotifyCheckCtx in ssh.c */

static void inotifywait_check_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    SshInotifyCheckCtx *ctx = data;
    VibeWindow *win = ctx->win;
    GError *err = NULL;
    gboolean has_it = g_task_propagate_boolean(G_TASK(res), &err);
    if (err) g_error_free(err);
    if (g_cancellable_is_cancelled(win->cancellable)) { g_free(ctx); return; }

    if (has_it) {
        start_remote_inotifywait(win, ctx->remote_dir);
    } else {
        start_remote_fallback_poll(win);
    }
    g_free(ctx);
}

static void start_remote_inotifywait(VibeWindow *win, const char *remote_dir) {
    GPtrArray *av = win_ssh_argv(win);
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

/* RemoteFilePollCtx and thread function are now SshFilePollCtx in ssh.c */

static void remote_file_poll_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    SshFilePollCtx *ctx = data;
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

    SshFilePollCtx *ctx = g_new0(SshFilePollCtx, 1);
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
    g_task_run_in_thread(task, ssh_file_poll_thread);
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

        SshInotifyCheckCtx *ctx = g_new0(SshInotifyCheckCtx, 1);
        ctx->win = win;
        g_strlcpy(ctx->remote_dir, remote, sizeof(ctx->remote_dir));
        g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
        g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
        ctx->ssh_port = win->ssh_port;
        g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
        g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));

        GTask *task = g_task_new(NULL, NULL, inotifywait_check_done, ctx);
        g_task_set_task_data(task, ctx, NULL);
        g_task_run_in_thread(task, ssh_inotify_check_thread);
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

    if (path_is_remote(path) && win->ssh_host[0]) {
        ssh_ls_populate(win, path, 0, -1);
    } else {
        insert_children(win, path, 0, -1);
    }
    update_file_status(win);
    start_dir_monitor(win, path);
    refresh_git_status(win);

    gtk_notebook_set_current_page(win->notebook, 0);
}

void vibe_window_set_root_directory(VibeWindow *win, const char *path) {
    g_strlcpy(win->root_dir, path, sizeof(win->root_dir));

    if (path_is_remote(path) && win->ssh_host[0]) {
        /* Start SSH ControlMaster for multiplexed connections */
        win_ssh_ctl_start(win);
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

        vte_terminal_spawn_async(
            win->terminal, VTE_PTY_DEFAULT, NULL,
            (char **)av->pdata, NULL, G_SPAWN_SEARCH_PATH,
            NULL, NULL, NULL, -1, NULL, NULL, NULL);
        g_ptr_array_free(av, TRUE);
    } else {
        /* local shell — wrapped in `script` for terminal logging */
        const char *shell = g_getenv("SHELL");
        if (!shell) {
            struct passwd *pw = getpwuid(getuid());
            shell = (pw && pw->pw_shell) ? pw->pw_shell : "/bin/sh";
        }

        /* Ensure .LLM directory exists */
        char llmdir[2200];
        snprintf(llmdir, sizeof(llmdir), "%s/.LLM", path);
        g_mkdir_with_parents(llmdir, 0755);

        char *argv[] = {(char *)shell, NULL};
        vte_terminal_spawn_async(
            win->terminal, VTE_PTY_DEFAULT, path,
            argv, NULL, G_SPAWN_DEFAULT,
            NULL, NULL, NULL, -1, NULL, NULL, NULL);
    }

    vibe_window_open_directory(win, path);
}

#define MAX_FILE_SIZE (10 * 1024 * 1024) /* 10 MB */


/* ── Async file loading ── */

typedef struct {
    VibeWindow *win;
    char        filepath[4096];
    char       *contents;    /* owned, result from thread */
    gsize       len;
    gboolean    ok;
    /* SSH params snapshot (for thread safety) */
    char        ssh_host[256];
    char        ssh_user[128];
    int         ssh_port;
    char        ssh_key[1024];
    char        ssh_ctl_path[512];
    char        ssh_mount[2048];
    char        ssh_remote_path[1024];
} FileLoadCtx;

static void file_load_thread(GTask *task, gpointer src, gpointer data,
                              GCancellable *cancel) {
    (void)src; (void)cancel;
    FileLoadCtx *ctx = data;

    if (ssh_path_is_remote(ctx->filepath) && ctx->ssh_host[0]) {
        ctx->ok = ssh_cat_file(ctx->ssh_host, ctx->ssh_user, ctx->ssh_port,
                               ctx->ssh_key, ctx->ssh_ctl_path,
                               ctx->ssh_mount, ctx->ssh_remote_path,
                               ctx->filepath, &ctx->contents, &ctx->len,
                               MAX_FILE_SIZE);
    } else {
        ctx->ok = g_file_get_contents(ctx->filepath, &ctx->contents, &ctx->len, NULL);
    }
    g_task_return_boolean(task, ctx->ok);
}

static void file_load_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    FileLoadCtx *ctx = data;
    VibeWindow *win = ctx->win;

    GError *err = NULL;
    g_task_propagate_boolean(G_TASK(res), &err);
    if (err) g_error_free(err);

    /* Check if window is destroyed or file changed while loading */
    if (g_cancellable_is_cancelled(win->cancellable) ||
        strcmp(win->current_file, ctx->filepath) != 0) {
        g_free(ctx->contents);
        g_free(ctx);
        return;
    }

    GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);

    if (ctx->ok && ctx->contents) {
        if (ctx->len > MAX_FILE_SIZE) {
            gtk_text_buffer_set_text(tbuf, "(file too large)", -1);
        } else {
            /* binary check — scan first 8 KB for NUL bytes */
            gboolean is_binary = FALSE;
            gsize check = ctx->len < 8192 ? ctx->len : 8192;
            for (gsize i = 0; i < check; i++) {
                if (ctx->contents[i] == '\0') { is_binary = TRUE; break; }
            }
            if (is_binary) {
                gtk_text_buffer_set_text(tbuf, "(binary file)", -1);
            } else {
                gtk_text_buffer_set_text(tbuf, ctx->contents, (gssize)ctx->len);
            }
        }
    } else {
        gtk_text_buffer_set_text(tbuf, "(cannot read file)", -1);
    }

    /* Set syntax highlighting language based on filename + content type */
    GtkSourceLanguageManager *lm = gtk_source_language_manager_get_default();
    gchar *content_type = g_content_type_guess(ctx->filepath, NULL, 0, NULL);
    GtkSourceLanguage *lang = gtk_source_language_manager_guess_language(lm, ctx->filepath, content_type);
    g_free(content_type);
    gtk_source_buffer_set_language(win->file_buffer, lang);
    gtk_source_buffer_set_highlight_syntax(win->file_buffer, lang != NULL);

    GtkTextIter start;
    gtk_text_buffer_get_start_iter(tbuf, &start);
    gtk_text_buffer_place_cursor(tbuf, &start);
    apply_font_intensity(win);

    /* Reset modified state and update title */
    win->file_modified = FALSE;
    update_status_bar(win);

    g_free(ctx->contents);
    g_free(ctx);
}

static void load_file_content(VibeWindow *win, const char *filepath) {
    /* Store filepath immediately for guard checks */
    g_strlcpy(win->current_file, filepath, sizeof(win->current_file));

    /* Show loading indicator */
    gtk_text_buffer_set_text(GTK_TEXT_BUFFER(win->file_buffer), "Loading…", -1);

    FileLoadCtx *ctx = g_new0(FileLoadCtx, 1);
    ctx->win = win;
    g_strlcpy(ctx->filepath, filepath, sizeof(ctx->filepath));
    g_strlcpy(ctx->ssh_host, win->ssh_host, sizeof(ctx->ssh_host));
    g_strlcpy(ctx->ssh_user, win->ssh_user, sizeof(ctx->ssh_user));
    ctx->ssh_port = win->ssh_port;
    g_strlcpy(ctx->ssh_key, win->ssh_key, sizeof(ctx->ssh_key));
    g_strlcpy(ctx->ssh_ctl_path, win->ssh_ctl_path, sizeof(ctx->ssh_ctl_path));
    g_strlcpy(ctx->ssh_mount, win->ssh_mount, sizeof(ctx->ssh_mount));
    g_strlcpy(ctx->ssh_remote_path, win->ssh_remote_path, sizeof(ctx->ssh_remote_path));

    GTask *task = g_task_new(NULL, win->cancellable, file_load_done, ctx);
    g_task_set_task_data(task, ctx, NULL);
    g_task_run_in_thread(task, file_load_thread);
    g_object_unref(task);
}

/* Wrapper that also starts file monitor — called from row activation */
static void open_and_watch_file(VibeWindow *win, const char *filepath) {
    load_file_content(win, filepath);
    start_file_monitor(win, filepath);
}

static void vibe_toast(VibeWindow *win, const char *message);

/* Helper: create a themed modal dialog with AdwHeaderBar */
GtkWidget *vibe_dialog_new(VibeWindow *win, const char *title, int width, int height) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), title);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    if (width > 0 && height > 0)
        gtk_window_set_default_size(GTK_WINDOW(dialog), width, height);
    else if (width > 0)
        gtk_window_set_default_size(GTK_WINDOW(dialog), width, -1);
    GtkWidget *header = adw_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(dialog), header);
    return dialog;
}

/* ── File browser context menu (in filebrowser.c) ── */

void vibe_window_refresh_current_dir(VibeWindow *win) {
    if (!win->current_dir[0]) return;
    if (win->fs_refresh_id) g_source_remove(win->fs_refresh_id);
    win->fs_refresh_id = g_timeout_add(50, fs_refresh_cb, win);
}

static void on_file_list_right_click(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer data) {
    (void)n_press;
    VibeWindow *win = data;
    gtk_gesture_set_state(GTK_GESTURE(gesture), GTK_EVENT_SEQUENCE_CLAIMED);

    GtkListBoxRow *row = gtk_list_box_get_row_at_y(win->file_list, (int)y);
    if (!row) return;

    GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!lbl) return;

    const char *full_path = g_object_get_data(G_OBJECT(lbl), "full-path");
    if (!full_path) return;
    gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));

    filebrowser_show_context_menu(win, GTK_WIDGET(win->file_list), full_path, is_dir, x, y);
}

static void on_file_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    VibeWindow *win = data;

    /* Cancel any active inline edit before processing the click */
    filebrowser_cancel_inline_edit(win);

    GtkWidget *label = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!label) return;

    /* Handle "Show more" lazy-load rows */
    LazyLoadData *lazy = g_object_get_data(G_OBJECT(label), "lazy-load");
    if (lazy) {
        int row_idx = gtk_list_box_row_get_index(row);
        /* Remove this "show more" row */
        gtk_list_box_remove(win->file_list, GTK_WIDGET(row));

        /* Insert remaining entries */
        int show = lazy->count <= DIR_BATCH_SIZE ? lazy->count : DIR_BATCH_SIZE;
        for (int i = 0; i < show; i++) {
            char full[4608];
            snprintf(full, sizeof(full), "%s/%s", lazy->dir_path, lazy->names[i]);
            GtkWidget *lbl = create_tree_row(full, lazy->names[i], lazy->is_dirs[i], lazy->depth);
            gtk_list_box_insert(win->file_list, lbl, row_idx + i);
        }

        /* If still more, add another "show more" row */
        if (lazy->count > DIR_BATCH_SIZE) {
            int remaining = lazy->count - DIR_BATCH_SIZE;
            LazyLoadData *next = g_new0(LazyLoadData, 1);
            next->count = remaining;
            next->depth = lazy->depth;
            g_strlcpy(next->dir_path, lazy->dir_path, sizeof(next->dir_path));
            next->names = g_new(char *, remaining);
            next->is_dirs = g_new(gboolean, remaining);
            for (int i = 0; i < remaining; i++) {
                next->names[i] = g_strdup(lazy->names[DIR_BATCH_SIZE + i]);
                next->is_dirs[i] = lazy->is_dirs[DIR_BATCH_SIZE + i];
            }

            char indent[128] = "";
            build_indent(indent, sizeof(indent), next->depth);
            char lt[256];
            snprintf(lt, sizeof(lt), "%s  ⋯ Show %d more…", indent, remaining);
            GtkWidget *more_lbl = gtk_label_new(lt);
            gtk_label_set_xalign(GTK_LABEL(more_lbl), 0);
            gtk_widget_set_margin_start(more_lbl, 8);
            gtk_widget_set_margin_end(more_lbl, 8);
            gtk_widget_set_margin_top(more_lbl, 4);
            gtk_widget_set_margin_bottom(more_lbl, 4);
            gtk_widget_add_css_class(more_lbl, "dim-label");
            g_object_set_data(G_OBJECT(more_lbl), "is-dir", GINT_TO_POINTER(FALSE));
            g_object_set_data(G_OBJECT(more_lbl), "depth", GINT_TO_POINTER(next->depth));
            g_object_set_data(G_OBJECT(more_lbl), "expanded", GINT_TO_POINTER(FALSE));
            g_object_set_data_full(G_OBJECT(more_lbl), "lazy-load", next, lazy_load_data_free);
            gtk_list_box_insert(win->file_list, more_lbl, row_idx + show);
        }
        update_file_status(win);
        apply_git_status_to_rows(win);
        return;
    }

    const char *full_path = g_object_get_data(G_OBJECT(label), "full-path");
    if (!full_path) return;
    gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "is-dir"));
    int depth = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "depth"));
    gboolean expanded = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(label), "expanded"));

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
            char indent[128];
            build_indent(indent, sizeof(indent), depth);
            char buf[512];
            snprintf(buf, sizeof(buf), "%s%s %s", indent, has_kids ? "▶" : " ", name);
            int st = git_status_for_dir(win, full_path);
            const char *clr = git_status_color(st);
            if (clr) {
                char *esc = g_markup_escape_text(buf, -1);
                char mkup[1024];
                snprintf(mkup, sizeof(mkup), "<span foreground=\"%s\">%s</span>", clr, esc);
                gtk_label_set_markup(GTK_LABEL(label), mkup);
                g_free(esc);
            } else {
                gtk_label_set_text(GTK_LABEL(label), buf);
            }
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
            char indent[128];
            build_indent(indent, sizeof(indent), depth);
            char buf[512];
            snprintf(buf, sizeof(buf), "%s▼ %s", indent, name);
            int st = git_status_for_dir(win, full_path);
            const char *clr = git_status_color(st);
            if (clr) {
                char *esc = g_markup_escape_text(buf, -1);
                char mkup[1024];
                snprintf(mkup, sizeof(mkup), "<span foreground=\"%s\">%s</span>", clr, esc);
                gtk_label_set_markup(GTK_LABEL(label), mkup);
                g_free(esc);
            } else {
                gtk_label_set_text(GTK_LABEL(label), buf);
            }
        }
        update_file_status(win);
        apply_git_status_to_rows(win);
    } else {
        open_and_watch_file(win, full_path);
        gtk_widget_grab_focus(GTK_WIDGET(win->file_view));
    }
}

/* ── Toast notifications ── */

static void vibe_toast(VibeWindow *win, const char *message) {
    if (!win->toast_overlay) return;
    AdwToast *toast = adw_toast_new(message);
    adw_toast_set_timeout(toast, 2);
    adw_toast_overlay_add_toast(win->toast_overlay, toast);
}

void vibe_window_toast(VibeWindow *win, const char *message) {
    vibe_toast(win, message);
}

void vibe_window_switch_ai_mode(VibeWindow *win) {
    ai_refresh_output(win);
}

/* save_current_file moved to editor.c */

static void on_file_modified_changed(GtkTextBuffer *buffer, gpointer data) {
    (void)buffer;
    VibeWindow *win = data;
    if (!win->current_file[0]) return;

    win->file_modified = TRUE;
    update_status_bar(win);
}

/* Search, goto line, and editor key handler moved to editor.c */

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
    update_status_bar_page(win, (int)page_num);
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
    win_ssh_ctl_stop(win);
    win->ssh_host[0] = '\0';
    win->ssh_user[0] = '\0';
    win->ssh_port = 0;
    win->ssh_key[0] = '\0';
    win->ssh_remote_path[0] = '\0';
    win->ssh_mount[0] = '\0';

    /* go back to home directory */
    const char *home = g_get_home_dir();
    vibe_window_set_root_directory(win, home);
    vibe_toast(win, "SFTP disconnected");
}

static void on_sftp_disconnect_clicked(GtkButton *btn, gpointer data) {
    (void)btn;
    VibeWindow *win = data;
    vibe_window_disconnect_sftp(win);
}

static void update_cursor_label(VibeWindow *win) {
    GtkTextBuffer *fb = GTK_TEXT_BUFFER(win->file_buffer);
    GtkTextMark *mark = gtk_text_buffer_get_insert(fb);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(fb, &iter, mark);
    int line = gtk_text_iter_get_line(&iter) + 1;
    int col = gtk_text_iter_get_line_offset(&iter) + 1;
    char buf[128];
    snprintf(buf, sizeof(buf), "Ln %d, Col %d", line, col);
    gtk_label_set_text(win->cursor_label, buf);
}

static void update_status_bar_page(VibeWindow *win, int page) {
    if (page == 0) {
        update_file_status(win);
        update_cursor_label(win);
        /* Title: filename or app name */
        if (win->current_file[0]) {
            const char *base = strrchr(win->current_file, '/');
            base = base ? base + 1 : win->current_file;
            if (win->file_modified) {
                char *t = g_strdup_printf("%s [modified]", base);
                gtk_window_set_title(GTK_WINDOW(win->window), t);
                g_free(t);
            } else {
                gtk_window_set_title(GTK_WINDOW(win->window), base);
            }
        } else {
            gtk_window_set_title(GTK_WINDOW(win->window), "Vibe Light");
        }
    } else if (page == 1) {
        gtk_label_set_text(win->status_label, "");
        gtk_label_set_text(win->cursor_label, "");
        gtk_window_set_title(GTK_WINDOW(win->window), "Terminal");
    } else if (page == 2) {
        /* AI tab: show model name on left, session on right */
        const char *model = gtk_label_get_text(win->ai_status_label);
        if (model && model[0] && strcmp(model, "ready") != 0
            && strncmp(model, "thinking", 8) != 0) {
            gtk_label_set_text(win->status_label, model);
            gtk_window_set_title(GTK_WINDOW(win->window), model);
        } else {
            gtk_label_set_text(win->status_label, "AI Model");
            gtk_window_set_title(GTK_WINDOW(win->window), "AI Model");
        }

        if (win->ai_session_id[0]) {
            char sid[160];
            snprintf(sid, sizeof(sid), "session: %.8s… ▾", win->ai_session_id);
            gtk_label_set_text(win->cursor_label, sid);
        } else {
            gtk_label_set_text(win->cursor_label, "sessions ▾");
        }
    } else {
        gtk_label_set_text(win->status_label, "");
        gtk_label_set_text(win->cursor_label, "");
        gtk_window_set_title(GTK_WINDOW(win->window), "Vibe Light");
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

void update_status_bar(VibeWindow *win) {
    update_status_bar_page(win, gtk_notebook_get_current_page(win->notebook));
}

/* ── Session picker helpers ── */

/* Get the sessions directory path. Uses custom setting or derives from CWD. */
static void get_sessions_dir(VibeWindow *win, char *buf, size_t bufsize) {
    buf[0] = '\0';
    if (win->settings.ai_sessions_dir[0]) {
        g_strlcpy(buf, win->settings.ai_sessions_dir, bufsize);
        return;
    }
    const char *cwd = win->ai_cwd[0] ? win->ai_cwd : win->root_dir;
    if (!cwd[0]) return;
    char safe[2048];
    g_strlcpy(safe, cwd, sizeof(safe));
    for (char *p = safe; *p; p++)
        if (*p == '/') *p = '-';
    snprintf(buf, bufsize, "%s/.claude/projects/%s", g_get_home_dir(), safe);
}

/* ── Session types ── */

typedef struct {
    char sid[128];
    char summary[200];
    gint64 mtime;
    int turns;
    int input_tokens;
    int output_tokens;
    char model[64];
} SessionEntry;

static int session_cmp(const void *a, const void *b) {
    const SessionEntry *sa = a, *sb = b;
    if (sb->mtime > sa->mtime) return 1;
    if (sb->mtime < sa->mtime) return -1;
    return 0;
}

/* ── Session browser dialog ── */

static void on_session_browser_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    VibeWindow *win = data;
    GtkWidget *child = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!child) return;
    const char *sid = g_object_get_data(G_OBJECT(child), "session-id");
    if (!sid) return;

    /* Close the dialog */
    GtkWidget *dialog = gtk_widget_get_ancestor(GTK_WIDGET(box), GTK_TYPE_WINDOW);
    if (dialog && dialog != GTK_WIDGET(win->window))
        gtk_window_destroy(GTK_WINDOW(dialog));

    /* Switch to this session (reuse on_session_row_clicked logic) */
    g_strlcpy(win->ai_session_id, sid, sizeof(win->ai_session_id));
    g_strlcpy(win->settings.ai_last_session, sid, sizeof(win->settings.ai_last_session));
    win->ai_session_turns = 0;
    win->ai_input_tokens = 0;
    win->ai_output_tokens = 0;
    win->ai_session_start = g_get_real_time();
    win->settings.ai_session_start = win->ai_session_start;
    settings_save(&win->settings);

    /* Reconstruct conversation from JSONL */
    if (win->ai_conversation_md)
        g_string_truncate(win->ai_conversation_md, 0);
    else
        win->ai_conversation_md = g_string_new(NULL);

    char sessions_dir[4096];
    get_sessions_dir(win, sessions_dir, sizeof(sessions_dir));
    char proj_path[4608] = "";
    if (sessions_dir[0])
        snprintf(proj_path, sizeof(proj_path), "%s/%s.jsonl", sessions_dir, sid);

    if (proj_path[0] && g_file_test(proj_path, G_FILE_TEST_EXISTS)) {
        FILE *f = fopen(proj_path, "r");
        if (f) {
            char line[65536];
            while (fgets(line, sizeof(line), f)) {
                /* User messages → conversation + turn count */
                if (strstr(line, "\"role\":\"user\"") && strstr(line, "\"type\":\"user\"")) {
                    win->ai_session_turns++;
                    const char *tp = strstr(line, "\"text\":\"");
                    if (tp) {
                        tp += 8;
                        GString *text = g_string_new(NULL);
                        while (*tp && !(*tp == '"' && *(tp-1) != '\\')) {
                            if (tp[0] == '\\' && tp[1] == 'n') { g_string_append_c(text, '\n'); tp += 2; }
                            else if (tp[0] == '\\' && tp[1] == '"') { g_string_append_c(text, '"'); tp += 2; }
                            else if (tp[0] == '\\' && tp[1] == '\\') { g_string_append_c(text, '\\'); tp += 2; }
                            else { g_string_append_c(text, *tp); tp++; }
                        }
                        if (text->len > 0)
                            g_string_append_printf(win->ai_conversation_md,
                                "*>>> %s*\n\n---\n\n", text->str);
                        g_string_free(text, TRUE);
                    }
                }
                /* Assistant messages → conversation text + token usage */
                if (strstr(line, "\"role\":\"assistant\"")) {
                    if (strstr(line, "\"type\":\"text\"")) {
                        const char *tp = strstr(line, "\"text\":\"");
                        if (tp) {
                            tp += 8;
                            GString *text = g_string_new(NULL);
                            while (*tp && !(*tp == '"' && *(tp-1) != '\\')) {
                                if (tp[0] == '\\' && tp[1] == 'n') { g_string_append_c(text, '\n'); tp += 2; }
                                else if (tp[0] == '\\' && tp[1] == '"') { g_string_append_c(text, '"'); tp += 2; }
                                else if (tp[0] == '\\' && tp[1] == '\\') { g_string_append_c(text, '\\'); tp += 2; }
                                else { g_string_append_c(text, *tp); tp++; }
                            }
                            if (text->len > 0)
                                g_string_append_printf(win->ai_conversation_md, "%s\n\n", text->str);
                            g_string_free(text, TRUE);
                        }
                    }
                    if (strstr(line, "\"usage\"")) {
                        const char *it = strstr(line, "\"input_tokens\":");
                        if (it) win->ai_input_tokens += atoi(it + 15);
                        const char *ot = strstr(line, "\"output_tokens\":");
                        if (ot) win->ai_output_tokens += atoi(ot + 16);
                        const char *cr = strstr(line, "\"cache_read_input_tokens\":");
                        if (cr) win->ai_input_tokens += atoi(cr + 25);
                        const char *cc = strstr(line, "\"cache_creation_input_tokens\":");
                        if (cc) win->ai_input_tokens += atoi(cc + 29);
                        /* Model name */
                        const char *mp = strstr(line, "\"model\":\"");
                        if (mp) {
                            mp += 9;
                            char model[128];
                            int mi = 0;
                            while (*mp && *mp != '"' && mi < 127)
                                model[mi++] = *mp++;
                            model[mi] = '\0';
                            gtk_label_set_text(win->ai_status_label, model);
                        }
                    }
                }
            }
            fclose(f);
        }
    }

    ai_refresh_output(win);
    update_status_bar(win);
    char msg[128];
    snprintf(msg, sizeof(msg), "Resumed session %.8s…", sid);
    vibe_window_toast(win, msg);
}

static void on_open_sessions_clicked(GtkButton *btn, gpointer data) {
    (void)data;
    VibeWindow *win = g_object_get_data(G_OBJECT(btn), "win");
    GtkWidget *popover = g_object_get_data(G_OBJECT(btn), "popover");
    gtk_popover_popdown(GTK_POPOVER(popover));

    char proj_dir[4096];
    get_sessions_dir(win, proj_dir, sizeof(proj_dir));
    if (!proj_dir[0]) { vibe_window_toast(win, "No working directory set"); return; }

    GDir *dir = g_dir_open(proj_dir, 0, NULL);
    if (!dir) { vibe_window_toast(win, "No sessions found"); return; }

    /* Collect all sessions */
    SessionEntry entries[1000];
    int count = 0;
    const char *name;
    while ((name = g_dir_read_name(dir)) && count < 1000) {
        size_t nlen = strlen(name);
        if (nlen < 42 || strcmp(name + nlen - 6, ".jsonl") != 0) continue;

        char sid[128];
        g_strlcpy(sid, name, sizeof(sid));
        sid[nlen - 6] = '\0';

        char fullpath[4608];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", proj_dir, name);

        GFile *gf = g_file_new_for_path(fullpath);
        GFileInfo *fi = g_file_query_info(gf, G_FILE_ATTRIBUTE_TIME_MODIFIED,
                                           G_FILE_QUERY_INFO_NONE, NULL, NULL);
        g_object_unref(gf);
        if (!fi) continue;
        gint64 mtime = g_file_info_get_attribute_uint64(fi, G_FILE_ATTRIBUTE_TIME_MODIFIED);
        g_object_unref(fi);

        SessionEntry *e = &entries[count];
        g_strlcpy(e->sid, sid, sizeof(e->sid));
        e->mtime = mtime;
        e->summary[0] = '\0';
        e->turns = 0;
        e->input_tokens = 0;
        e->output_tokens = 0;
        e->model[0] = '\0';

        FILE *f = fopen(fullpath, "r");
        if (f) {
            char line[65536];
            gboolean got_summary = FALSE;
            while (fgets(line, sizeof(line), f)) {
                /* First user message → summary */
                if (!got_summary && strstr(line, "\"role\":\"user\"")) {
                    const char *tp = strstr(line, "\"text\":\"");
                    if (tp) {
                        tp += 8;
                        int i = 0;
                        while (*tp && *tp != '"' && i < 120) {
                            if (tp[0] == '\\' && tp[1] == 'n') { e->summary[i++] = ' '; tp += 2; }
                            else if (tp[0] == '\\' && tp[1]) { e->summary[i++] = tp[1]; tp += 2; }
                            else { e->summary[i++] = *tp; tp++; }
                        }
                        e->summary[i] = '\0';
                        got_summary = TRUE;
                    }
                }
                /* Count user turns */
                if (strstr(line, "\"type\":\"user\""))
                    e->turns++;
                /* Assistant messages → tokens + model */
                if (strstr(line, "\"role\":\"assistant\"") && strstr(line, "\"usage\"")) {
                    /* Model */
                    if (!e->model[0]) {
                        const char *mp = strstr(line, "\"model\":\"");
                        if (mp) {
                            mp += 9;
                            int i = 0;
                            while (*mp && *mp != '"' && i < 63)
                                e->model[i++] = *mp++;
                            e->model[i] = '\0';
                        }
                    }
                    /* input_tokens */
                    const char *it = strstr(line, "\"input_tokens\":");
                    if (it) e->input_tokens += atoi(it + 15);
                    /* output_tokens */
                    const char *ot = strstr(line, "\"output_tokens\":");
                    if (ot) e->output_tokens += atoi(ot + 16);
                    /* cache tokens count as input */
                    const char *cr = strstr(line, "\"cache_read_input_tokens\":");
                    if (cr) e->input_tokens += atoi(cr + 25);
                    const char *cc = strstr(line, "\"cache_creation_input_tokens\":");
                    if (cc) e->input_tokens += atoi(cc + 29);
                }
            }
            fclose(f);
        }
        count++;
    }
    g_dir_close(dir);

    if (count == 0) { vibe_window_toast(win, "No sessions found"); return; }
    qsort(entries, count, sizeof(SessionEntry), session_cmp);

    /* Build dialog */
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Sessions");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_titlebar(GTK_WINDOW(dialog), adw_header_bar_new());

    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_window_set_child(GTK_WINDOW(dialog), scroll);

    GtkWidget *listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_SINGLE);
    g_signal_connect(listbox, "row-activated",
                     G_CALLBACK(on_session_browser_row_activated), win);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), listbox);

    for (int i = 0; i < count; i++) {
        SessionEntry *e = &entries[i];
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_widget_set_margin_start(row_box, 8);
        gtk_widget_set_margin_end(row_box, 8);
        gtk_widget_set_margin_top(row_box, 4);
        gtk_widget_set_margin_bottom(row_box, 4);

        gboolean is_current = win->ai_session_id[0] &&
                              strcmp(e->sid, win->ai_session_id) == 0;

        /* Top row: summary + date */
        GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

        char left[256];
        if (e->summary[0])
            snprintf(left, sizeof(left), "%s%s",
                     e->summary, is_current ? "  (current)" : "");
        else
            snprintf(left, sizeof(left), "%.12s…%s",
                     e->sid, is_current ? "  (current)" : "");

        GtkWidget *left_label = gtk_label_new(left);
        gtk_label_set_xalign(GTK_LABEL(left_label), 0);
        gtk_label_set_ellipsize(GTK_LABEL(left_label), PANGO_ELLIPSIZE_END);
        gtk_widget_set_hexpand(left_label, TRUE);
        if (is_current) gtk_widget_add_css_class(left_label, "accent");
        gtk_box_append(GTK_BOX(top), left_label);

        GDateTime *dt = g_date_time_new_from_unix_local(e->mtime);
        char *date_str = dt ? g_date_time_format(dt, "%Y-%m-%d %H:%M") : g_strdup("?");
        if (dt) g_date_time_unref(dt);
        GtkWidget *date_label = gtk_label_new(date_str);
        gtk_widget_add_css_class(date_label, "dim-label");
        g_free(date_str);
        gtk_box_append(GTK_BOX(top), date_label);

        gtk_box_append(GTK_BOX(row_box), top);

        /* Bottom row: model, turns, tokens */
        char info[256];
        char in_s[16], out_s[16];
        #define FMT_K(buf, n) do { \
            if ((n) >= 1000000) snprintf(buf, sizeof(buf), "%.1fM", (n)/1e6); \
            else if ((n) >= 1000) snprintf(buf, sizeof(buf), "%.1fk", (n)/1e3); \
            else snprintf(buf, sizeof(buf), "%d", (n)); \
        } while(0)
        FMT_K(in_s, e->input_tokens);
        FMT_K(out_s, e->output_tokens);
        #undef FMT_K
        snprintf(info, sizeof(info), "%s   %d turns   in: %s  out: %s",
                 e->model[0] ? e->model : "?", e->turns, in_s, out_s);
        GtkWidget *info_label = gtk_label_new(info);
        gtk_label_set_xalign(GTK_LABEL(info_label), 0);
        gtk_widget_add_css_class(info_label, "dim-label");
        gtk_box_append(GTK_BOX(row_box), info_label);

        g_object_set_data_full(G_OBJECT(row_box), "session-id",
                               g_strdup(e->sid), g_free);
        gtk_list_box_insert(GTK_LIST_BOX(listbox), row_box, -1);
    }

    gtk_window_present(GTK_WINDOW(dialog));
}

static void on_new_session_clicked(GtkButton *btn, gpointer data) {
    (void)data;
    VibeWindow *win = g_object_get_data(G_OBJECT(btn), "win");
    GtkWidget *popover = g_object_get_data(G_OBJECT(btn), "popover");
    gtk_popover_popdown(GTK_POPOVER(popover));

    win->ai_session_id[0] = '\0';
    win->settings.ai_last_session[0] = '\0';
    win->ai_session_turns = 0;
    win->ai_input_tokens = 0;
    win->ai_output_tokens = 0;
    win->ai_session_start = 0;
    settings_save(&win->settings);
    if (win->ai_conversation_md)
        g_string_truncate(win->ai_conversation_md, 0);
    ai_refresh_output(win);
    update_status_bar(win);
    vibe_window_toast(win, "New session started");
}


/* ── Session info popover ── */

static void on_session_label_clicked(GtkGestureClick *gesture, int n_press,
                                      double x, double y, gpointer data) {
    (void)n_press; (void)x; (void)y;
    VibeWindow *win = data;

    /* Only show on AI tab */
    if (gtk_notebook_get_current_page(win->notebook) != 2) return;

    GtkWidget *popover = gtk_popover_new();
    gtk_widget_set_parent(popover, gtk_event_controller_get_widget(
        GTK_EVENT_CONTROLLER(gesture)));

    GtkWidget *grid = NULL;
    GtkWidget *lbl;
    int row = 0;

    if (win->ai_session_id[0]) {
    grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_widget_set_margin_start(grid, 12);
    gtk_widget_set_margin_end(grid, 12);
    gtk_widget_set_margin_top(grid, 8);
    gtk_widget_set_margin_bottom(grid, 8);

    /* Session ID */
    lbl = gtk_label_new("Session:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 1);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    lbl = gtk_label_new(win->ai_session_id);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_label_set_selectable(GTK_LABEL(lbl), TRUE);
    gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

    /* Started */
    if (win->ai_session_start > 0) {
        GDateTime *dt = g_date_time_new_from_unix_local(win->ai_session_start / G_USEC_PER_SEC);
        if (dt) {
            char *started = g_date_time_format(dt, "%Y-%m-%d %H:%M:%S");
            lbl = gtk_label_new("Started:");
            gtk_label_set_xalign(GTK_LABEL(lbl), 1);
            gtk_widget_add_css_class(lbl, "dim-label");
            gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
            lbl = gtk_label_new(started);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0);
            gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

            /* Duration */
            GDateTime *now = g_date_time_new_now_local();
            GTimeSpan span = g_date_time_difference(now, dt);
            gint64 mins = span / G_TIME_SPAN_MINUTE;
            char dur[64];
            if (mins >= 60)
                snprintf(dur, sizeof(dur), "%ldh %ldm", (long)(mins / 60), (long)(mins % 60));
            else if (mins > 0)
                snprintf(dur, sizeof(dur), "%ldm", (long)mins);
            else
                snprintf(dur, sizeof(dur), "%lds", (long)(span / G_TIME_SPAN_SECOND));
            lbl = gtk_label_new("Duration:");
            gtk_label_set_xalign(GTK_LABEL(lbl), 1);
            gtk_widget_add_css_class(lbl, "dim-label");
            gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
            lbl = gtk_label_new(dur);
            gtk_label_set_xalign(GTK_LABEL(lbl), 0);
            gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

            g_free(started);
            g_date_time_unref(now);
            g_date_time_unref(dt);
        }
    }

    /* Turns */
    char turns[32];
    snprintf(turns, sizeof(turns), "%d", win->ai_session_turns);
    lbl = gtk_label_new("Turns:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 1);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    lbl = gtk_label_new(turns);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

    /* Tokens */
    char tok[64];
    snprintf(tok, sizeof(tok), "%d in / %d out",
             win->ai_input_tokens, win->ai_output_tokens);
    lbl = gtk_label_new("Tokens:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 1);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    lbl = gtk_label_new(tok);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

    /* Mode */
    lbl = gtk_label_new("Mode:");
    gtk_label_set_xalign(GTK_LABEL(lbl), 1);
    gtk_widget_add_css_class(lbl, "dim-label");
    gtk_grid_attach(GTK_GRID(grid), lbl, 0, row, 1, 1);
    lbl = gtk_label_new(win->settings.ai_streaming ? "interactive (streaming)" : "batch");
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    gtk_grid_attach(GTK_GRID(grid), lbl, 1, row++, 1, 1);

    } /* end if (ai_session_id[0]) */

    /* ── Separator + session picker ── */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    if (grid)
        gtk_box_append(GTK_BOX(vbox), grid);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    if (grid)
        gtk_box_append(GTK_BOX(vbox), sep);

    /* Buttons row */
    GtkWidget *btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_start(btn_row, 8);
    gtk_widget_set_margin_end(btn_row, 8);

    GtkWidget *new_btn = gtk_button_new_with_label("New Session");
    gtk_widget_add_css_class(new_btn, "flat");
    g_object_set_data(G_OBJECT(new_btn), "win", win);
    g_object_set_data(G_OBJECT(new_btn), "popover", popover);
    g_signal_connect(new_btn, "clicked", G_CALLBACK(on_new_session_clicked), NULL);
    gtk_box_append(GTK_BOX(btn_row), new_btn);

    GtkWidget *open_btn = gtk_button_new_with_label("Open Session…");
    gtk_widget_add_css_class(open_btn, "flat");
    g_object_set_data(G_OBJECT(open_btn), "win", win);
    g_object_set_data(G_OBJECT(open_btn), "popover", popover);
    g_signal_connect(open_btn, "clicked", G_CALLBACK(on_open_sessions_clicked), NULL);
    gtk_box_append(GTK_BOX(btn_row), open_btn);

    gtk_box_append(GTK_BOX(vbox), btn_row);

    gtk_popover_set_child(GTK_POPOVER(popover), vbox);
    gtk_popover_popup(GTK_POPOVER(popover));
}

/* ── Window close ── */

static void on_close_request(GtkWindow *window, gpointer data) {
    (void)window;
    VibeWindow *win = data;

    int w, h;
    gtk_window_get_default_size(GTK_WINDOW(win->window), &w, &h);
    win->settings.window_width = w;
    win->settings.window_height = h;

    /* Save session state */
    g_strlcpy(win->settings.last_file, win->current_file, sizeof(win->settings.last_file));
    win->settings.last_tab = gtk_notebook_get_current_page(win->notebook);

    /* Save cursor position */
    GtkTextBuffer *fb = GTK_TEXT_BUFFER(win->file_buffer);
    GtkTextMark *mark = gtk_text_buffer_get_insert(fb);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(fb, &iter, mark);
    win->settings.last_cursor_line = gtk_text_iter_get_line(&iter);
    win->settings.last_cursor_col = gtk_text_iter_get_line_offset(&iter);

    settings_save(&win->settings);
}

static void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    VibeWindow *win = data;

    /* Cancel all in-flight async operations first */
    g_cancellable_cancel(win->cancellable);

    /* Clean up AI process */
    if (win->ai_timer_id) { g_source_remove(win->ai_timer_id); win->ai_timer_id = 0; }
    g_free(win->ai_last_prompt);
    if (win->ai_response_buf) { g_string_free(win->ai_response_buf, TRUE); win->ai_response_buf = NULL; }
    if (win->ai_conversation_md) { g_string_free(win->ai_conversation_md, TRUE); win->ai_conversation_md = NULL; }
    if (win->ai_proc) {
        g_subprocess_force_exit(win->ai_proc);
        g_clear_object(&win->ai_proc);
    }

    stop_dir_monitor(win);
    stop_file_monitor(win);
    win_ssh_ctl_stop(win);
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
    if (win->git_status)
        g_hash_table_destroy(win->git_status);
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

    GPtrArray *av = win_ssh_argv(win);
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
        g_strlcpy(name, *p, sizeof(name));
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
            g_strlcpy(ctx->current_remote, "/", sizeof(ctx->current_remote));
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
        g_strlcpy(ctx->current_remote, text, sizeof(ctx->current_remote));
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

    /* send cd to existing terminal session — escape single quotes for shell safety */
    GString *cd_cmd = g_string_new("cd '");
    for (const char *c = ctx->current_remote; *c; c++) {
        if (*c == '\'')
            g_string_append(cd_cmd, "'\\''");
        else
            g_string_append_c(cd_cmd, *c);
    }
    g_string_append(cd_cmd, "'\n");
    vte_terminal_feed_child(win->terminal, cd_cmd->str, cd_cmd->len);
    g_string_free(cd_cmd, TRUE);

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
    g_strlcpy(ctx->current_remote, win->ssh_remote_path, sizeof(ctx->current_remote));
    if (!ctx->current_remote[0]) g_strlcpy(ctx->current_remote, "/", sizeof(ctx->current_remote));

    GtkWidget *dialog = vibe_dialog_new(win, "Remote Directory", 420, 400);
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
    g_menu_append(menu, "AI Model", "win.ai-model");
    g_menu_append(menu, "Save as PDF", "win.save-pdf");
    g_menu_append(menu, "Settings", "win.settings");

    GtkWidget *btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(btn), "open-menu-symbolic");
    gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(btn), G_MENU_MODEL(menu));
    g_object_unref(menu);
    return btn;
}

/* ── Drag & drop ── */

static gboolean on_drop(GtkDropTarget *target, const GValue *value,
                          double x, double y, gpointer data) {
    (void)target; (void)x; (void)y;
    VibeWindow *win = data;

    if (!G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST)) return FALSE;

    GSList *files = g_value_get_boxed(value);
    if (!files) return FALSE;

    /* Use the first dropped file/directory */
    GFile *file = files->data;
    char *path = g_file_get_path(file);
    if (!path) return FALSE;

    struct stat st;
    if (stat(path, &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            vibe_window_set_root_directory(win, path);
        } else if (S_ISREG(st.st_mode)) {
            /* Open the file's parent directory, then open the file */
            char *dir = g_path_get_dirname(path);
            if (!win->root_dir[0] || strncmp(path, win->root_dir, strlen(win->root_dir)) != 0)
                vibe_window_set_root_directory(win, dir);
            open_and_watch_file(win, path);
            gtk_widget_grab_focus(GTK_WIDGET(win->file_view));
            g_free(dir);
        }
    }
    g_free(path);
    return TRUE;
}

VibeWindow *vibe_window_new(GtkApplication *app) {
    VibeWindow *win = g_new0(VibeWindow, 1);
    win->cancellable = g_cancellable_new();
    settings_load(&win->settings);

    win->window = GTK_APPLICATION_WINDOW(gtk_application_window_new(app));
    gtk_window_set_title(GTK_WINDOW(win->window), "Vibe Light");
    gtk_window_set_icon_name(GTK_WINDOW(win->window), "com.vibe.light");
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

    /* Right-click context menu */
    GtkGesture *right_click = gtk_gesture_click_new();
    gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(right_click), 3); /* button 3 = right */
    g_signal_connect(right_click, "pressed", G_CALLBACK(on_file_list_right_click), win);
    gtk_widget_add_controller(GTK_WIDGET(win->file_list), GTK_EVENT_CONTROLLER(right_click));
    filebrowser_setup_dnd(win);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), GTK_WIDGET(win->file_list));
    gtk_box_append(GTK_BOX(browser_box), scroll);

    gtk_paned_set_start_child(GTK_PANED(paned), browser_box);
    gtk_paned_set_shrink_start_child(GTK_PANED(paned), FALSE);

    /* Right: GtkSourceView file viewer (syntax highlighting, line numbers, current line) */
    win->file_buffer = gtk_source_buffer_new(NULL);
    win->file_view = GTK_SOURCE_VIEW(gtk_source_view_new_with_buffer(win->file_buffer));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(win->file_view), TRUE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(win->file_view), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(win->file_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(win->file_view), 12);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(win->file_view), 12);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(win->file_view), 8);
    gtk_widget_add_css_class(GTK_WIDGET(win->file_view), "file-viewer");

    /* GtkSourceView features */
    gtk_source_view_set_show_line_numbers(win->file_view, win->settings.show_line_numbers);
    gtk_source_view_set_highlight_current_line(win->file_view, win->settings.highlight_current_line);
    gtk_source_view_set_tab_width(win->file_view, 4);
    gtk_source_view_set_show_line_marks(win->file_view, FALSE);

    /* Apply source style scheme matching the app theme */
    GtkSourceStyleSchemeManager *ssm = gtk_source_style_scheme_manager_get_default();
    GtkSourceStyleScheme *scheme = NULL;
    if (is_dark_theme(win->settings.theme))
        scheme = gtk_source_style_scheme_manager_get_scheme(ssm, "Adwaita-dark");
    else
        scheme = gtk_source_style_scheme_manager_get_scheme(ssm, "Adwaita");
    if (scheme)
        gtk_source_buffer_set_style_scheme(win->file_buffer, scheme);

    g_signal_connect(win->file_buffer, "changed", G_CALLBACK(on_file_buffer_changed), win);
    g_signal_connect(win->file_buffer, "changed", G_CALLBACK(on_file_modified_changed), win);

    /* Key handler for Ctrl+S, Ctrl+F — editing is allowed */
    GtkEventController *edit_key = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(edit_key, GTK_PHASE_CAPTURE);
    g_signal_connect(edit_key, "key-pressed", G_CALLBACK(on_editor_key), win);
    gtk_widget_add_controller(GTK_WIDGET(win->file_view), edit_key);
    g_signal_connect(win->file_buffer, "notify::cursor-position", G_CALLBACK(on_cursor_moved), win);

    /* Scrolled window */
    GtkWidget *viewer_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(viewer_scroll), GTK_WIDGET(win->file_view));
    gtk_widget_set_vexpand(viewer_scroll, TRUE);
    gtk_widget_set_hexpand(viewer_scroll, TRUE);

    /* Search bar (hidden by default, shown with Ctrl+F) */
    win->search_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_visible(win->search_bar, FALSE);
    gtk_widget_set_margin_start(win->search_bar, 8);
    gtk_widget_set_margin_end(win->search_bar, 8);
    gtk_widget_set_margin_top(win->search_bar, 4);
    gtk_widget_set_margin_bottom(win->search_bar, 4);

    win->search_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_placeholder_text(win->search_entry, "Search…");
    gtk_widget_set_hexpand(GTK_WIDGET(win->search_entry), TRUE);
    gtk_box_append(GTK_BOX(win->search_bar), GTK_WIDGET(win->search_entry));

    GtkWidget *prev_btn = gtk_button_new_from_icon_name("go-up-symbolic");
    GtkWidget *next_btn = gtk_button_new_from_icon_name("go-down-symbolic");
    gtk_widget_add_css_class(prev_btn, "flat");
    gtk_widget_add_css_class(next_btn, "flat");
    gtk_box_append(GTK_BOX(win->search_bar), prev_btn);
    gtk_box_append(GTK_BOX(win->search_bar), next_btn);

    GtkWidget *close_btn = gtk_button_new_from_icon_name("window-close-symbolic");
    gtk_widget_add_css_class(close_btn, "flat");
    gtk_box_append(GTK_BOX(win->search_bar), close_btn);

    /* Search signals */
    g_signal_connect(win->search_entry, "changed", G_CALLBACK(on_search_changed), win);
    g_signal_connect(prev_btn, "clicked", G_CALLBACK(on_search_prev), win);
    g_signal_connect(next_btn, "clicked", G_CALLBACK(on_search_next), win);
    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_search_close), win);
    GtkEventController *search_key = gtk_event_controller_key_new();
    g_signal_connect(search_key, "key-pressed", G_CALLBACK(on_search_key), win);
    gtk_widget_add_controller(GTK_WIDGET(win->search_entry), search_key);

    /* GtkSourceView search context */
    GtkSourceSearchSettings *search_settings = gtk_source_search_settings_new();
    gtk_source_search_settings_set_wrap_around(search_settings, TRUE);
    win->search_ctx = gtk_source_search_context_new(win->file_buffer, search_settings);
    gtk_source_search_context_set_highlight(win->search_ctx, TRUE);
    g_object_unref(search_settings);

    /* VBox: viewer + search bar */
    GtkWidget *viewer_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(viewer_vbox), viewer_scroll);
    gtk_box_append(GTK_BOX(viewer_vbox), win->search_bar);

    gtk_paned_set_end_child(GTK_PANED(paned), viewer_vbox);
    gtk_paned_set_shrink_end_child(GTK_PANED(paned), FALSE);

    gtk_notebook_append_page(win->notebook, paned, gtk_label_new("Files"));

    /* ── Tab 2: Terminal (plain, for interactive shell) ── */
    win->terminal = VTE_TERMINAL(vte_terminal_new());
    vte_terminal_set_scroll_on_output(win->terminal, TRUE);
    vte_terminal_set_scrollback_lines(win->terminal, 10000);

    GtkWidget *term_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(GTK_WIDGET(win->terminal), TRUE);
    gtk_widget_set_hexpand(GTK_WIDGET(win->terminal), TRUE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(term_scroll), GTK_WIDGET(win->terminal));

    gtk_notebook_append_page(win->notebook, term_scroll, gtk_label_new("Terminal"));

    /* ── Tab 3: AI-model (status + output + prompt) ── */
    GtkWidget *ai_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    /* Status bar: model + tokens */
    GtkWidget *ai_status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(ai_status_bar, 8);
    gtk_widget_set_margin_end(ai_status_bar, 8);
    gtk_widget_set_margin_top(ai_status_bar, 4);
    gtk_widget_set_margin_bottom(ai_status_bar, 4);

    win->ai_status_label = GTK_LABEL(gtk_label_new("ready"));
    gtk_label_set_xalign(win->ai_status_label, 0);
    gtk_widget_add_css_class(GTK_WIDGET(win->ai_status_label), "dim-label");
    gtk_box_append(GTK_BOX(ai_status_bar), GTK_WIDGET(win->ai_status_label));

    win->ai_token_label = GTK_LABEL(gtk_label_new("in: 0  out: 0  total: 0"));
    gtk_label_set_xalign(win->ai_token_label, 1);
    gtk_widget_set_hexpand(GTK_WIDGET(win->ai_token_label), TRUE);
    gtk_widget_add_css_class(GTK_WIDGET(win->ai_token_label), "dim-label");
    gtk_box_append(GTK_BOX(ai_status_bar), GTK_WIDGET(win->ai_token_label));

    gtk_box_append(GTK_BOX(ai_box), ai_status_bar);
    gtk_box_append(GTK_BOX(ai_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    /* Output view: read-only, shows conversation */
    GtkWidget *ai_paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_vexpand(ai_paned, TRUE);
    gtk_paned_set_resize_start_child(GTK_PANED(ai_paned), TRUE);
    gtk_paned_set_resize_end_child(GTK_PANED(ai_paned), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(ai_paned), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(ai_paned), FALSE);

    /* Output: WebKitWebView for proper markdown/HTML rendering */
    win->ai_webview = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_widget_set_vexpand(GTK_WIDGET(win->ai_webview), TRUE);
    /* Disable hardware acceleration — avoids GBM/GPU errors in some environments */
    WebKitSettings *wk_settings = webkit_web_view_get_settings(win->ai_webview);
    webkit_settings_set_hardware_acceleration_policy(wk_settings,
        WEBKIT_HARDWARE_ACCELERATION_POLICY_NEVER);
    /* Transparent background so it blends with the app theme */
    GdkRGBA transparent = {0, 0, 0, 0};
    webkit_web_view_set_background_color(win->ai_webview, &transparent);

    /* Demo content */
    win->ai_conversation_md = g_string_new(
        "# Vibe Light AI\n\n"
        "Send a prompt below. Supports **Markdown** and `code`.\n\n"
        "### Features\n\n"
        "- **Bold**, *italic*, `inline code`\n"
        "- Code blocks with syntax highlighting\n"
        "- Tables, lists, blockquotes\n"
        "- LaTeX: $E = mc^2$, $$\\sum_{i=1}^{n} i$$\n\n"
        "```c\nint main(void) {\n    return 0;\n}\n```\n\n"
        "| Feature | Status |\n"
        "|---------|--------|\n"
        "| Markdown | ✓ |\n"
        "| Code | ✓ |\n\n"
        "---\n"
    );
    ai_refresh_output(win);

    /* Restore persisted session ID + stats */
    if (win->settings.ai_last_session[0]) {
        g_strlcpy(win->ai_session_id, win->settings.ai_last_session,
                   sizeof(win->ai_session_id));
        win->ai_session_start = win->settings.ai_session_start;
        win->ai_session_turns = win->settings.ai_session_turns;
    }

    gtk_paned_set_start_child(GTK_PANED(ai_paned), GTK_WIDGET(win->ai_webview));

    /* Prompt input */
    GtkWidget *prompt_scroll = gtk_scrolled_window_new();
    gtk_widget_set_size_request(prompt_scroll, -1, 80);
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

    GtkEventController *key_ctrl = gtk_event_controller_key_new();
    gtk_event_controller_set_propagation_phase(key_ctrl, GTK_PHASE_CAPTURE);
    g_signal_connect(key_ctrl, "key-pressed", G_CALLBACK(on_prompt_key), win);
    gtk_widget_add_controller(GTK_WIDGET(win->prompt_view), key_ctrl);

    gtk_paned_set_end_child(GTK_PANED(ai_paned), prompt_scroll);
    gtk_box_append(GTK_BOX(ai_box), ai_paned);

    gtk_notebook_append_page(win->notebook, ai_box, gtk_label_new("AI-model"));

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

    /* Wrap cursor label in a clickable gesture for session info */
    GtkWidget *cursor_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_append(GTK_BOX(cursor_box), GTK_WIDGET(win->cursor_label));
    GtkGesture *cursor_click = gtk_gesture_click_new();
    g_signal_connect(cursor_click, "pressed", G_CALLBACK(on_session_label_clicked), win);
    gtk_widget_add_controller(cursor_box, GTK_EVENT_CONTROLLER(cursor_click));
    gtk_widget_set_cursor_from_name(cursor_box, "pointer");
    gtk_box_append(GTK_BOX(status_bar), cursor_box);

    /* Main layout: toast overlay > notebook + status bar */
    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_append(GTK_BOX(main_box), GTK_WIDGET(win->notebook));
    gtk_widget_set_vexpand(GTK_WIDGET(win->notebook), TRUE);
    gtk_box_append(GTK_BOX(main_box), status_bar);

    win->toast_overlay = ADW_TOAST_OVERLAY(adw_toast_overlay_new());
    adw_toast_overlay_set_child(win->toast_overlay, main_box);
    gtk_window_set_child(GTK_WINDOW(win->window), GTK_WIDGET(win->toast_overlay));

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

    /* Drag & drop: accept file drops on the whole window */
    GtkDropTarget *drop = gtk_drop_target_new(GDK_TYPE_FILE_LIST, GDK_ACTION_COPY);
    g_signal_connect(drop, "drop", G_CALLBACK(on_drop), win);
    gtk_widget_add_controller(GTK_WIDGET(win->window), GTK_EVENT_CONTROLLER(drop));

    actions_setup(win, app);
    vibe_window_apply_settings(win);

    if (win->settings.last_directory[0]) {
        g_strlcpy(win->root_dir, win->settings.last_directory, sizeof(win->root_dir));
        vibe_window_set_root_directory(win, win->settings.last_directory);
    }

    /* Restore session: last open file + cursor position */
    if (win->settings.last_file[0] && !path_is_remote(win->settings.last_file)) {
        /* Only restore local files (remote connections are not auto-restored) */
        struct stat st;
        if (stat(win->settings.last_file, &st) == 0 && S_ISREG(st.st_mode)) {
            open_and_watch_file(win, win->settings.last_file);

            /* Restore cursor position (deferred to allow async load to complete) */
            if (win->settings.last_cursor_line > 0 || win->settings.last_cursor_col > 0) {
                GtkTextBuffer *fb = GTK_TEXT_BUFFER(win->file_buffer);
                GtkTextIter iter;
                int line = win->settings.last_cursor_line;
                int col = win->settings.last_cursor_col;
                int total = gtk_text_buffer_get_line_count(fb);
                if (line >= total) line = total - 1;
                gtk_text_buffer_get_iter_at_line_offset(fb, &iter, line, 0);
                /* move to column if valid */
                int line_len = gtk_text_iter_get_chars_in_line(&iter);
                if (col > 0 && col < line_len)
                    gtk_text_iter_set_line_offset(&iter, col);
                gtk_text_buffer_place_cursor(fb, &iter);

                /* Scroll to cursor */
                GtkTextMark *mark = gtk_text_buffer_get_insert(fb);
                gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(win->file_view), mark, 0.1, FALSE, 0, 0);
            }
        }
    }

    /* Restore active tab */
    if (win->settings.last_tab >= 0 && win->settings.last_tab <= 2)
        gtk_notebook_set_current_page(win->notebook, win->settings.last_tab);

    gtk_window_present(GTK_WINDOW(win->window));
    return win;
}
