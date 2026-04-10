#include <adwaita.h>
#include <gtksourceview/gtksource.h>
#include <string.h>
#include "editor.h"
#include "ssh.h"

/* Defined in window.c */
extern void update_status_bar(VibeWindow *win);
extern GtkWidget *vibe_dialog_new(VibeWindow *win, const char *title, int width, int height);

#define path_is_remote(p) ssh_path_is_remote(p)

void save_current_file(VibeWindow *win) {
    if (!win->current_file[0] || !win->file_modified) return;

    if (path_is_remote(win->current_file)) {
        vibe_window_toast(win, "Cannot save remote files");
        return;
    }

    GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);
    GtkTextIter start, end;
    gtk_text_buffer_get_bounds(tbuf, &start, &end);
    char *text = gtk_text_buffer_get_text(tbuf, &start, &end, FALSE);

    GError *err = NULL;
    if (g_file_set_contents(win->current_file, text, -1, &err)) {
        win->file_modified = FALSE;
        update_status_bar(win);

        const char *bname = strrchr(win->current_file, '/');
        bname = bname ? bname + 1 : win->current_file;
        char *msg = g_strdup_printf("Saved %s", bname);
        vibe_window_toast(win, msg);
        g_free(msg);
    } else {
        char msg[512];
        snprintf(msg, sizeof(msg), "Save failed: %s", err->message);
        vibe_window_toast(win, msg);
        g_error_free(err);
    }
    g_free(text);
}

void on_search_changed(GtkEditable *editable, gpointer data) {
    VibeWindow *win = data;
    const char *text = gtk_editable_get_text(editable);
    GtkSourceSearchSettings *ss = gtk_source_search_context_get_settings(win->search_ctx);
    gtk_source_search_settings_set_search_text(ss, text[0] ? text : NULL);
}

void on_search_next(GtkButton *btn, gpointer data) {
    (void)btn;
    VibeWindow *win = data;
    GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);
    GtkTextMark *mark = gtk_text_buffer_get_insert(tbuf);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(tbuf, &iter, mark);
    GtkTextIter match_start, match_end;
    gboolean has_wrap;
    if (gtk_source_search_context_forward(win->search_ctx, &iter,
                                           &match_start, &match_end, &has_wrap)) {
        gtk_text_buffer_select_range(tbuf, &match_start, &match_end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(win->file_view),
                                      &match_start, 0.1, FALSE, 0, 0);
    }
}

void on_search_prev(GtkButton *btn, gpointer data) {
    (void)btn;
    VibeWindow *win = data;
    GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);
    GtkTextMark *mark = gtk_text_buffer_get_insert(tbuf);
    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_mark(tbuf, &iter, mark);
    GtkTextIter match_start, match_end;
    gboolean has_wrap;
    if (gtk_source_search_context_backward(win->search_ctx, &iter,
                                            &match_start, &match_end, &has_wrap)) {
        gtk_text_buffer_select_range(tbuf, &match_start, &match_end);
        gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(win->file_view),
                                      &match_start, 0.1, FALSE, 0, 0);
    }
}

void on_search_close(GtkButton *btn, gpointer data) {
    (void)btn;
    VibeWindow *win = data;
    gtk_widget_set_visible(win->search_bar, FALSE);
    GtkSourceSearchSettings *ss = gtk_source_search_context_get_settings(win->search_ctx);
    gtk_source_search_settings_set_search_text(ss, NULL);
    gtk_widget_grab_focus(GTK_WIDGET(win->file_view));
}

gboolean on_search_key(GtkEventControllerKey *ctrl, guint keyval,
                        guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode; (void)state;
    VibeWindow *win = data;
    if (keyval == GDK_KEY_Escape) {
        on_search_close(NULL, win);
        return TRUE;
    }
    if (keyval == GDK_KEY_Return) {
        on_search_next(NULL, win);
        return TRUE;
    }
    return FALSE;
}

/* Go to line dialog */
typedef struct { VibeWindow *win; GtkEntry *entry; GtkWindow *dialog; } GotoLineCtx;

static void on_goto_line_go(GtkButton *btn, gpointer data) {
    (void)btn;
    GotoLineCtx *ctx = data;
    const char *text = gtk_editable_get_text(GTK_EDITABLE(ctx->entry));
    int line = atoi(text);
    if (line < 1) line = 1;

    GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(ctx->win->file_buffer);
    int total = gtk_text_buffer_get_line_count(tbuf);
    if (line > total) line = total;

    GtkTextIter iter;
    gtk_text_buffer_get_iter_at_line(tbuf, &iter, line - 1);
    gtk_text_buffer_place_cursor(tbuf, &iter);
    GtkTextMark *mark = gtk_text_buffer_get_insert(tbuf);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(ctx->win->file_view), mark, 0.1, FALSE, 0, 0);

    gtk_window_destroy(ctx->dialog);
}

static void on_goto_line_destroy(GtkWidget *w, gpointer data) { (void)w; g_free(data); }

void show_goto_line(VibeWindow *win) {
    GtkWidget *dialog = vibe_dialog_new(win, "Go to Line", 250, -1);

    GotoLineCtx *ctx = g_new(GotoLineCtx, 1);
    ctx->win = win;
    ctx->dialog = GTK_WINDOW(dialog);
    g_signal_connect(dialog, "destroy", G_CALLBACK(on_goto_line_destroy), ctx);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);

    GtkWidget *entry = gtk_entry_new();
    ctx->entry = GTK_ENTRY(entry);
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Line number…");
    gtk_entry_set_input_purpose(GTK_ENTRY(entry), GTK_INPUT_PURPOSE_DIGITS);
    gtk_box_append(GTK_BOX(vbox), entry);

    GtkWidget *go_btn = gtk_button_new_with_label("Go");
    gtk_widget_add_css_class(go_btn, "suggested-action");
    gtk_widget_set_halign(go_btn, GTK_ALIGN_END);
    g_signal_connect(go_btn, "clicked", G_CALLBACK(on_goto_line_go), ctx);
    gtk_box_append(GTK_BOX(vbox), go_btn);

    gtk_window_set_child(GTK_WINDOW(dialog), vbox);
    gtk_window_present(GTK_WINDOW(dialog));
    gtk_widget_grab_focus(entry);
}

gboolean on_editor_key(GtkEventControllerKey *ctrl, guint keyval,
                       guint keycode, GdkModifierType state, gpointer data) {
    (void)ctrl; (void)keycode;
    VibeWindow *win = data;

    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_s) {
        save_current_file(win);
        return TRUE;
    }

    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_f) {
        gboolean visible = gtk_widget_get_visible(win->search_bar);
        gtk_widget_set_visible(win->search_bar, !visible);
        if (!visible)
            gtk_widget_grab_focus(GTK_WIDGET(win->search_entry));
        return TRUE;
    }

    if ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_g) {
        show_goto_line(win);
        return TRUE;
    }

    if ((state & GDK_CONTROL_MASK) && !(state & GDK_SHIFT_MASK) && keyval == GDK_KEY_z) {
        GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);
        if (gtk_text_buffer_get_can_undo(tbuf))
            gtk_text_buffer_undo(tbuf);
        return TRUE;
    }

    if (((state & GDK_CONTROL_MASK) && (state & GDK_SHIFT_MASK) && keyval == GDK_KEY_z) ||
        ((state & GDK_CONTROL_MASK) && keyval == GDK_KEY_y)) {
        GtkTextBuffer *tbuf = GTK_TEXT_BUFFER(win->file_buffer);
        if (gtk_text_buffer_get_can_redo(tbuf))
            gtk_text_buffer_redo(tbuf);
        return TRUE;
    }

    return FALSE;
}
