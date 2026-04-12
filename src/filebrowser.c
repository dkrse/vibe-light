#define _DEFAULT_SOURCE
#include <adwaita.h>
#include "filebrowser.h"
#include "window.h"
#include "ssh.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

/* External helpers from window.c */
extern GtkWidget *vibe_dialog_new(VibeWindow *win, const char *title, int width, int height);
extern void vibe_window_refresh_current_dir(VibeWindow *win);

/*
 * Strategy: every button callback copies what it needs to stack locals,
 * then calls gtk_popover_popdown(). The "closed" signal handler does
 * gtk_widget_unparent + g_free(ctx).  No action code ever runs after
 * ctx is freed, because popdown → closed fires synchronously only when
 * the popover is visible, and we always popdown first.
 *
 * For actions that open a dialog (delete, rename), the dialog owns its
 * own context struct allocated from the stack copies — no dependency on
 * CtxMenuData after popdown.
 */

typedef struct {
    VibeWindow *win;
    char        path[4096];
    gboolean    is_dir;
    GtkWidget  *popover;
} CtxMenuData;

/* Called from "closed" signal — safe because we're not inside the popover's
   own button handler anymore (popdown → closed is deferred by GTK). */
static gboolean destroy_popover_idle(gpointer data) {
    GtkWidget *popover = data;
    gtk_widget_unparent(popover);
    return G_SOURCE_REMOVE;
}

static void on_popover_closed(GtkPopover *popover, gpointer data) {
    (void)popover;
    CtxMenuData *ctx = data;
    /* Schedule unparent for idle — we might still be inside a signal chain */
    g_idle_add(destroy_popover_idle, ctx->popover);
    ctx->popover = NULL;
    g_free(ctx);
}

/* Dismiss: just popdown. "closed" signal will free everything. */
static void popdown(CtxMenuData *ctx) {
    if (ctx->popover)
        gtk_popover_popdown(GTK_POPOVER(ctx->popover));
    /* Don't touch ctx after this — on_popover_closed freed it */
}

/* ── Inline name entry (for New File, New Folder, Rename) ── */

typedef struct {
    VibeWindow *win;
    GtkListBoxRow *row;
    GtkWidget *original_label; /* NULL for new-file/new-folder */
    GtkWidget *entry;
    GtkEventController *key_ctrl;
    GtkEventController *focus_ctrl;
    char parent_dir[4096];
    gboolean is_dir;
    gboolean is_new;
    gboolean finished;
    int depth;
} InlineEditCtx;

static void inline_edit_ctx_free(InlineEditCtx *ctx) {
    if (!ctx) return;
    if (ctx->original_label) {
        g_object_unref(ctx->original_label);
        ctx->original_label = NULL;
    }
    g_free(ctx);
}

static void inline_edit_finish(InlineEditCtx *ctx, const char *name) {
    if (ctx->finished) return;
    ctx->finished = TRUE;

    /* Disconnect ALL signals to prevent re-entrant calls when
       refresh or row removal triggers focus-out on the entry */
    if (ctx->entry)
        g_signal_handlers_disconnect_matched(ctx->entry, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL, ctx);
    if (ctx->key_ctrl)
        g_signal_handlers_disconnect_matched(ctx->key_ctrl, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL, ctx);
    if (ctx->focus_ctrl)
        g_signal_handlers_disconnect_matched(ctx->focus_ctrl, G_SIGNAL_MATCH_DATA,
                                              0, 0, NULL, NULL, ctx);

    VibeWindow *win = ctx->win;
    gboolean did_something = FALSE;

    if (name && name[0] && !strchr(name, '/')) {
        char full[8192];
        snprintf(full, sizeof(full), "%s/%s", ctx->parent_dir, name);

        if (ctx->is_new) {
            /* Create new file or directory */
            if (ctx->is_dir) {
                if (g_mkdir_with_parents(full, 0755) == 0) {
                    char *msg = g_strdup_printf("Created %s/", name);
                    vibe_window_toast(win, msg);
                    g_free(msg);
                    did_something = TRUE;
                } else {
                    vibe_window_toast(win, "Failed to create directory");
                }
            } else {
                FILE *f = fopen(full, "w");
                if (f) {
                    fclose(f);
                    char *msg = g_strdup_printf("Created %s", name);
                    vibe_window_toast(win, msg);
                    g_free(msg);
                    did_something = TRUE;
                } else {
                    vibe_window_toast(win, "Failed to create file");
                }
            }
        } else {
            /* Rename existing */
            char old_path[8192];
            const char *old_name = g_object_get_data(G_OBJECT(ctx->original_label), "full-path");
            if (old_name) {
                g_strlcpy(old_path, old_name, sizeof(old_path));
                if (strcmp(old_path, full) != 0) {
                    if (rename(old_path, full) == 0) {
                        char msg[300];
                        snprintf(msg, sizeof(msg), "Renamed to %s", name);
                        vibe_window_toast(win, msg);
                        did_something = TRUE;
                    } else {
                        vibe_window_toast(win, "Rename failed");
                    }
                }
            }
        }
    }

    /* NOTE: do NOT g_free(ctx) here — focus-out callbacks may still
       reference it after this function returns.  ctx is freed later
       by filebrowser_cancel_inline_edit / start_inline_edit. */

    if (did_something) {
        if (ctx->original_label) {
            g_object_unref(ctx->original_label);
            ctx->original_label = NULL;
        }
        vibe_window_refresh_current_dir(win);
    } else if (ctx->is_new) {
        /* Remove temporary row */
        gtk_list_box_remove(win->file_list, GTK_WIDGET(ctx->row));
    } else if (ctx->original_label) {
        /* Restore original label (rename cancelled) */
        GtkWidget *orig = ctx->original_label;
        ctx->original_label = NULL;
        gtk_list_box_row_set_child(ctx->row, orig);
        g_object_unref(orig);
    }
}

static void on_edit_activate(GtkEntry *entry, gpointer data) {
    InlineEditCtx *ctx = data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(entry));
    inline_edit_finish(ctx, text);
}

static gboolean on_edit_key(GtkEventControllerKey *ctrl, guint keyval,
                             guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode; (void)state;
    if (keyval == GDK_KEY_Escape) {
        inline_edit_finish(data, NULL);
        return TRUE;
    }
    return FALSE;
}

static gboolean idle_cancel_edit(gpointer data) {
    VibeWindow *win = data;
    filebrowser_cancel_inline_edit(win);
    return G_SOURCE_REMOVE;
}

static void on_edit_focus_out(GtkEventControllerFocus *ctrl, gpointer data) {
    (void)ctrl;
    InlineEditCtx *ctx = data;
    if (ctx->finished) return;
    /* Defer cancel to next main-loop iteration so the current click
       event finishes processing before we touch the row tree. */
    g_idle_add(idle_cancel_edit, ctx->win);
}

static void start_inline_edit(VibeWindow *win, GtkListBoxRow *row,
                               GtkWidget *original_label, const char *parent_dir,
                               const char *prefill, gboolean is_dir, gboolean is_new,
                               int depth) {
    /* Cancel any previous inline edit before starting a new one */
    filebrowser_cancel_inline_edit(win);

    InlineEditCtx *ctx = g_new0(InlineEditCtx, 1);
    ctx->win = win;
    ctx->row = row;
    ctx->original_label = original_label;
    ctx->is_dir = is_dir;
    ctx->is_new = is_new;
    ctx->depth = depth;
    g_strlcpy(ctx->parent_dir, parent_dir, sizeof(ctx->parent_dir));
    win->inline_edit_ctx = ctx;

    if (original_label)
        g_object_ref(original_label);

    GtkWidget *entry = gtk_entry_new();
    if (prefill)
        gtk_editable_set_text(GTK_EDITABLE(entry), prefill);
    gtk_widget_set_margin_start(entry, 8 + depth * 16);
    gtk_widget_set_margin_end(entry, 8);
    gtk_widget_set_margin_top(entry, 1);
    gtk_widget_set_margin_bottom(entry, 1);

    ctx->entry = entry;
    g_signal_connect(entry, "activate", G_CALLBACK(on_edit_activate), ctx);

    ctx->key_ctrl = gtk_event_controller_key_new();
    g_signal_connect(ctx->key_ctrl, "key-pressed", G_CALLBACK(on_edit_key), ctx);
    gtk_widget_add_controller(entry, ctx->key_ctrl);

    ctx->focus_ctrl = gtk_event_controller_focus_new();
    g_signal_connect(ctx->focus_ctrl, "leave", G_CALLBACK(on_edit_focus_out), ctx);
    gtk_widget_add_controller(entry, ctx->focus_ctrl);

    gtk_list_box_row_set_child(row, entry);
    gtk_widget_grab_focus(entry);

    /* For rename: select name without extension */
    if (!is_new && prefill) {
        const char *dot = strrchr(prefill, '.');
        int sel_len = (dot && dot != prefill && !is_dir)
                      ? (int)(dot - prefill) : (int)strlen(prefill);
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, sel_len);
    }
}

/* ── Cancel active inline edit (public) ── */

void filebrowser_cancel_inline_edit(VibeWindow *win) {
    InlineEditCtx *ctx = win->inline_edit_ctx;
    if (!ctx) return;
    if (!ctx->finished)
        inline_edit_finish(ctx, NULL);
    win->inline_edit_ctx = NULL;
    inline_edit_ctx_free(ctx);
}

/* ── Inline rename (public) ── */

void filebrowser_inline_rename(VibeWindow *win, GtkListBoxRow *row) {
    GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!lbl) return;
    const char *full_path = g_object_get_data(G_OBJECT(lbl), "full-path");
    if (!full_path) return;

    const char *base = strrchr(full_path, '/');
    base = base ? base + 1 : full_path;
    char *parent = g_path_get_dirname(full_path);

    gboolean is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));
    int depth = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "depth"));

    start_inline_edit(win, row, lbl, parent, base, is_dir, FALSE, depth);
    g_free(parent);
}

/* ── Context menu button callbacks ── */

static void on_btn_new_file(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char dir[4096];
    g_strlcpy(dir, ctx->is_dir ? ctx->path : win->current_dir, sizeof(dir));
    popdown(ctx); /* ctx is freed after this */

    /* Insert a temporary empty row at position 0 with an inline entry */
    GtkWidget *placeholder = gtk_label_new("");
    gtk_list_box_insert(win->file_list, placeholder, 0);
    GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, 0);
    start_inline_edit(win, row, NULL, dir, NULL, FALSE, TRUE, 0);
}

static void on_btn_new_dir(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char dir[4096];
    g_strlcpy(dir, ctx->is_dir ? ctx->path : win->current_dir, sizeof(dir));
    popdown(ctx);

    GtkWidget *placeholder = gtk_label_new("");
    gtk_list_box_insert(win->file_list, placeholder, 0);
    GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, 0);
    start_inline_edit(win, row, NULL, dir, NULL, TRUE, TRUE, 0);
}

static void on_btn_rename(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char path_copy[4096];
    g_strlcpy(path_copy, ctx->path, sizeof(path_copy));
    popdown(ctx);

    if (ssh_path_is_remote(path_copy)) {
        vibe_window_toast(win, "Cannot rename remote files");
        return;
    }

    for (int i = 0; ; i++) {
        GtkListBoxRow *row = gtk_list_box_get_row_at_index(win->file_list, i);
        if (!row) break;
        GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
        if (!lbl) continue;
        const char *fp = g_object_get_data(G_OBJECT(lbl), "full-path");
        if (fp && strcmp(fp, path_copy) == 0) {
            filebrowser_inline_rename(win, row);
            break;
        }
    }
}

static void on_btn_copy_path(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char path_copy[4096];
    g_strlcpy(path_copy, ctx->path, sizeof(path_copy));
    popdown(ctx);

    GdkClipboard *clip = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clip, path_copy);
    vibe_window_toast(win, "Path copied");
}

static void on_btn_copy_relative_path(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char path_copy[4096];
    g_strlcpy(path_copy, ctx->path, sizeof(path_copy));
    popdown(ctx);

    const char *rel = path_copy;
    size_t root_len = strlen(win->root_dir);
    if (root_len && strncmp(path_copy, win->root_dir, root_len) == 0) {
        rel = path_copy + root_len;
        if (rel[0] == '/') rel++;
    }

    GdkClipboard *clip = gdk_display_get_clipboard(gdk_display_get_default());
    gdk_clipboard_set_text(clip, rel);
    vibe_window_toast(win, "Relative path copied");
}

/* Recursive delete */
static gboolean delete_recursive(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return FALSE;
    if (S_ISLNK(st.st_mode)) return unlink(path) == 0;
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) return FALSE;
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
            char child[4096];
            snprintf(child, sizeof(child), "%s/%s", path, ent->d_name);
            delete_recursive(child);
        }
        closedir(d);
        return rmdir(path) == 0;
    }
    return unlink(path) == 0;
}

typedef struct {
    VibeWindow *win;
    char        path[4096];
    GtkWindow  *dialog;
} DeleteCtx;

static void on_delete_confirm(GtkButton *btn, gpointer data) {
    (void)btn;
    DeleteCtx *dctx = data;
    const char *base = strrchr(dctx->path, '/');
    base = base ? base + 1 : dctx->path;
    if (delete_recursive(dctx->path)) {
        char *msg = g_strdup_printf("Deleted %s", base);
        vibe_window_toast(dctx->win, msg);
        g_free(msg);
        vibe_window_refresh_current_dir(dctx->win);
    } else {
        vibe_window_toast(dctx->win, "Delete failed");
    }
    gtk_window_destroy(dctx->dialog);
}

static void on_delete_destroy(GtkWidget *w, gpointer data) {
    (void)w;
    g_free(data);
}

static void on_btn_delete(GtkButton *btn, gpointer data) {
    (void)btn;
    CtxMenuData *ctx = data;
    VibeWindow *win = ctx->win;
    char path_copy[4096];
    g_strlcpy(path_copy, ctx->path, sizeof(path_copy));
    gboolean is_dir = ctx->is_dir;
    popdown(ctx);

    if (ssh_path_is_remote(path_copy)) {
        vibe_window_toast(win, "Cannot delete remote files");
        return;
    }

    const char *base = strrchr(path_copy, '/');
    base = base ? base + 1 : path_copy;

    char *title = g_strdup_printf("Delete \"%s\"?", base);
    GtkWidget *dialog = vibe_dialog_new(win, title, 350, -1);
    g_free(title);

    DeleteCtx *dctx = g_new(DeleteCtx, 1);
    dctx->win = win;
    g_strlcpy(dctx->path, path_copy, sizeof(dctx->path));
    dctx->dialog = GTK_WINDOW(dialog);
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_delete_destroy), dctx);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);

    GtkWidget *label = gtk_label_new(is_dir
        ? "This directory and all contents will be permanently deleted."
        : "This file will be permanently deleted.");
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_box_append(GTK_BOX(vbox), label);

    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(cancel_btn, "clicked", G_CALLBACK(gtk_window_destroy), dialog);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_confirm), dctx);
    gtk_box_append(GTK_BOX(btn_box), del_btn);

    gtk_box_append(GTK_BOX(vbox), btn_box);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── Context menu — simple GtkPopover with buttons ── */

static GtkWidget *menu_button(const char *label) {
    GtkWidget *b = gtk_button_new_with_label(label);
    gtk_button_set_has_frame(GTK_BUTTON(b), FALSE);
    gtk_widget_set_halign(b, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(b, TRUE);
    GtkWidget *child = gtk_button_get_child(GTK_BUTTON(b));
    if (GTK_IS_LABEL(child))
        gtk_label_set_xalign(GTK_LABEL(child), 0);
    return b;
}

void filebrowser_show_context_menu(VibeWindow *win, GtkWidget *widget,
                                   const char *path, gboolean is_dir,
                                   double x, double y) {
    CtxMenuData *ctx = g_new0(CtxMenuData, 1);
    ctx->win = win;
    ctx->is_dir = is_dir;
    g_strlcpy(ctx->path, path, sizeof(ctx->path));

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_margin_top(vbox, 4);
    gtk_widget_set_margin_bottom(vbox, 4);
    gtk_widget_set_margin_start(vbox, 4);
    gtk_widget_set_margin_end(vbox, 4);

    GtkWidget *b;

    b = menu_button("New File");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_new_file), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    b = menu_button("New Folder");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_new_dir), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    b = menu_button("Rename");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_rename), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    b = menu_button("Delete");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_delete), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    gtk_box_append(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    b = menu_button("Copy Path");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_copy_path), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    b = menu_button("Copy Relative Path");
    g_signal_connect(b, "clicked", G_CALLBACK(on_btn_copy_relative_path), ctx);
    gtk_box_append(GTK_BOX(vbox), b);

    GtkWidget *pop = gtk_popover_new();
    gtk_popover_set_child(GTK_POPOVER(pop), vbox);
    gtk_popover_set_has_arrow(GTK_POPOVER(pop), FALSE);
    gtk_widget_set_parent(pop, widget);

    GdkRectangle rect = { (int)x, (int)y, 1, 1 };
    gtk_popover_set_pointing_to(GTK_POPOVER(pop), &rect);

    ctx->popover = pop;
    g_signal_connect(pop, "closed", G_CALLBACK(on_popover_closed), ctx);
    gtk_popover_popup(GTK_POPOVER(pop));
}

/* ── Drag & Drop ── */

GdkContentProvider *filebrowser_dnd_prepare(GtkDragSource *source, double x, double y,
                                             gpointer data) {
    (void)x; (void)y; (void)data;
    VibeWindow *win = g_object_get_data(G_OBJECT(source), "win");
    if (!win) return NULL;

    GtkListBoxRow *row = gtk_list_box_get_row_at_y(win->file_list, (int)y);
    if (!row) return NULL;

    GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!lbl) return NULL;

    const char *fpath = g_object_get_data(G_OBJECT(lbl), "full-path");
    if (!fpath) return NULL;

    GValue val = G_VALUE_INIT;
    g_value_init(&val, G_TYPE_STRING);
    g_value_set_string(&val, fpath);
    GdkContentProvider *provider = gdk_content_provider_new_for_value(&val);
    g_value_unset(&val);
    return provider;
}

static gboolean on_dnd_drop(GtkDropTarget *target, const GValue *value,
                             double x, double y, gpointer data) {
    (void)target; (void)x;
    VibeWindow *win = data;

    if (!G_VALUE_HOLDS_STRING(value)) return FALSE;
    const char *source_path = g_value_get_string(value);
    if (!source_path || !source_path[0]) return FALSE;

    GtkListBoxRow *row = gtk_list_box_get_row_at_y(win->file_list, (int)y);
    if (!row) return FALSE;

    GtkWidget *lbl = gtk_widget_get_first_child(GTK_WIDGET(row));
    if (!lbl) return FALSE;

    const char *target_path = g_object_get_data(G_OBJECT(lbl), "full-path");
    gboolean target_is_dir = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(lbl), "is-dir"));
    if (!target_path) return FALSE;

    char dest_dir[4096];
    if (target_is_dir) {
        g_strlcpy(dest_dir, target_path, sizeof(dest_dir));
    } else {
        char *parent = g_path_get_dirname(target_path);
        g_strlcpy(dest_dir, parent, sizeof(dest_dir));
        g_free(parent);
    }

    if (strncmp(dest_dir, source_path, strlen(source_path)) == 0) {
        char c = dest_dir[strlen(source_path)];
        if (c == '\0' || c == '/') return FALSE;
    }

    const char *base = strrchr(source_path, '/');
    base = base ? base + 1 : source_path;

    char new_path[8192];
    snprintf(new_path, sizeof(new_path), "%s/%s", dest_dir, base);

    if (g_file_test(new_path, G_FILE_TEST_EXISTS)) {
        char *msg = g_strdup_printf("'%s' already exists in destination", base);
        vibe_window_toast(win, msg);
        g_free(msg);
        return FALSE;
    }

    if (rename(source_path, new_path) == 0) {
        char *msg = g_strdup_printf("Moved %s", base);
        vibe_window_toast(win, msg);
        g_free(msg);
        vibe_window_refresh_current_dir(win);
        return TRUE;
    } else {
        vibe_window_toast(win, "Move failed");
        return FALSE;
    }
}

void filebrowser_setup_dnd(VibeWindow *win) {
    GtkDragSource *drag = gtk_drag_source_new();
    gtk_drag_source_set_actions(drag, GDK_ACTION_MOVE);
    g_object_set_data(G_OBJECT(drag), "win", win);
    g_signal_connect(drag, "prepare", G_CALLBACK(filebrowser_dnd_prepare), NULL);
    gtk_widget_add_controller(GTK_WIDGET(win->file_list), GTK_EVENT_CONTROLLER(drag));

    GtkDropTarget *drop = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_MOVE);
    g_signal_connect(drop, "drop", G_CALLBACK(on_dnd_drop), win);
    gtk_widget_add_controller(GTK_WIDGET(win->file_list), GTK_EVENT_CONTROLLER(drop));
}
