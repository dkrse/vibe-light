#include "actions.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

/* ── Open Folder ── */

static void on_folder_selected(GObject *src, GAsyncResult *res, gpointer data) {
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

static void on_open_folder(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    VibeWindow *win = data;

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open Folder");

    if (win->current_dir[0]) {
        GFile *init = g_file_new_for_path(win->current_dir);
        gtk_file_dialog_set_initial_folder(dialog, init);
        g_object_unref(init);
    }

    gtk_file_dialog_select_folder(dialog, GTK_WINDOW(win->window), NULL,
                                   on_folder_selected, win);
}

/* ── Zoom ── */

static void on_zoom_in(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    VibeWindow *win = data;
    if (win->settings.editor_font_size < 72) {
        win->settings.editor_font_size += 2;
        win->settings.browser_font_size += 2;
        win->settings.terminal_font_size += 2;
        win->settings.prompt_font_size += 2;
        win->settings.gui_font_size += 2;
        settings_save(&win->settings);
        vibe_window_apply_settings(win);
    }
}

static void on_zoom_out(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    VibeWindow *win = data;
    if (win->settings.editor_font_size > 6) {
        win->settings.editor_font_size -= 2;
        win->settings.browser_font_size -= 2;
        win->settings.terminal_font_size -= 2;
        win->settings.prompt_font_size -= 2;
        win->settings.gui_font_size -= 2;
        settings_save(&win->settings);
        vibe_window_apply_settings(win);
    }
}

/* ── Settings callbacks ── */

static const char *theme_ids[] = {
    "system", "light", "dark",
    "solarized-light", "solarized-dark", "monokai",
    "gruvbox-light", "gruvbox-dark", "nord", "dracula",
    "tokyo-night", "catppuccin-latte", "catppuccin-mocha",
    NULL
};
static const char *theme_names[] = {
    "System", "Light", "Dark",
    "Solarized Light", "Solarized Dark", "Monokai",
    "Gruvbox Light", "Gruvbox Dark", "Nord", "Dracula",
    "Tokyo Night", "Catppuccin Latte", "Catppuccin Mocha",
    NULL
};

static void on_theme_changed(GtkDropDown *dd, GParamSpec *ps, gpointer data) {
    (void)ps;
    VibeWindow *win = data;
    guint idx = gtk_drop_down_get_selected(dd);
    if (theme_ids[idx])
        g_strlcpy(win->settings.theme, theme_ids[idx], sizeof(win->settings.theme));
}

static void on_spacing_changed(GtkDropDown *dd, GParamSpec *ps, gpointer data) {
    (void)ps;
    VibeWindow *win = data;
    guint idx = gtk_drop_down_get_selected(dd);
    double spacings[] = {1.0, 1.2, 1.5, 1.8, 2.0};
    if (idx < 5)
        win->settings.line_spacing = spacings[idx];
}

/* generic font callback — stores into a char[256] pointed to by user_data */
typedef struct { VibeWindow *win; char *font_field; int *size_field; } FontCtx;

static void on_font_changed(GtkFontDialogButton *btn, GParamSpec *ps, gpointer data) {
    (void)ps;
    FontCtx *ctx = data;
    PangoFontDescription *fd = gtk_font_dialog_button_get_font_desc(btn);
    if (!fd) return;
    const char *family = pango_font_description_get_family(fd);
    if (family)
        g_strlcpy(ctx->font_field, family, 256);
    int size = pango_font_description_get_size(fd) / PANGO_SCALE;
    if (size > 0)
        *ctx->size_field = size;
}

/* generic intensity callback */
typedef struct { VibeWindow *win; double *field; } IntensityCtx;

static void on_intensity_changed(GtkRange *range, gpointer data) {
    IntensityCtx *ctx = data;
    *ctx->field = gtk_range_get_value(range);
}

static void on_line_numbers_toggled(GtkCheckButton *btn, gpointer data) {
    VibeWindow *win = data;
    win->settings.show_line_numbers = gtk_check_button_get_active(btn);
}

static void on_highlight_line_toggled(GtkCheckButton *btn, gpointer data) {
    VibeWindow *win = data;
    win->settings.highlight_current_line = gtk_check_button_get_active(btn);
}

static void on_wrap_lines_toggled(GtkCheckButton *btn, gpointer data) {
    VibeWindow *win = data;
    win->settings.wrap_lines = gtk_check_button_get_active(btn);
}

static void on_send_key_changed(GtkDropDown *dd, GParamSpec *ps, gpointer data) {
    (void)ps;
    VibeWindow *win = data;
    win->settings.prompt_send_enter = (gtk_drop_down_get_selected(dd) == 1);
}

static void on_switch_terminal_toggled(GtkCheckButton *btn, gpointer data) {
    VibeWindow *win = data;
    win->settings.prompt_switch_terminal = gtk_check_button_get_active(btn);
}

/* ── Settings dialog helpers ── */

static GtkWidget *make_label(const char *text) {
    GtkWidget *lbl = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0);
    return lbl;
}

static GtkWidget *make_font_row(GtkWidget *grid, int *row, const char *label,
                                 char *font_field, int *size_field, VibeWindow *win,
                                 GPtrArray *allocs) {
    (void)win;
    gtk_grid_attach(GTK_GRID(grid), make_label(label), 0, *row, 1, 1);

    PangoFontDescription *fd = pango_font_description_new();
    pango_font_description_set_family(fd, font_field);
    pango_font_description_set_size(fd, *size_field * PANGO_SCALE);
    GtkFontDialog *dlg = gtk_font_dialog_new();
    GtkWidget *btn = gtk_font_dialog_button_new(dlg);
    gtk_font_dialog_button_set_font_desc(GTK_FONT_DIALOG_BUTTON(btn), fd);
    pango_font_description_free(fd);

    FontCtx *ctx = g_new(FontCtx, 1);
    ctx->win = win;
    ctx->font_field = font_field;
    ctx->size_field = size_field;
    g_ptr_array_add(allocs, ctx);

    g_signal_connect(btn, "notify::font-desc", G_CALLBACK(on_font_changed), ctx);
    gtk_widget_set_hexpand(btn, TRUE);
    gtk_grid_attach(GTK_GRID(grid), btn, 1, (*row)++, 1, 1);
    return btn;
}

static GtkWidget *make_intensity_row(GtkWidget *grid, int *row, const char *label,
                                      double *field, VibeWindow *win, GPtrArray *allocs) {
    gtk_grid_attach(GTK_GRID(grid), make_label(label), 0, *row, 1, 1);

    GtkWidget *scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.3, 1.0, 0.01);
    gtk_range_set_value(GTK_RANGE(scale), *field);

    IntensityCtx *ctx = g_new(IntensityCtx, 1);
    ctx->win = win;
    ctx->field = field;
    g_ptr_array_add(allocs, ctx);

    g_signal_connect(scale, "value-changed", G_CALLBACK(on_intensity_changed), ctx);
    gtk_widget_set_hexpand(scale, TRUE);
    gtk_grid_attach(GTK_GRID(grid), scale, 1, (*row)++, 1, 1);
    return scale;
}

/* ── Settings Apply/Cancel ── */

typedef struct {
    VibeWindow *win;
    GtkWindow *dialog;
    GPtrArray *allocs;
} SettingsCtx;

static void on_settings_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    SettingsCtx *ctx = data;
    g_ptr_array_unref(ctx->allocs);
    g_free(ctx);
}

static void on_settings_apply(GtkButton *btn, gpointer data) {
    (void)btn;
    SettingsCtx *ctx = data;
    settings_save(&ctx->win->settings);
    vibe_window_apply_settings(ctx->win);
    gtk_window_close(ctx->dialog);
}

static void on_settings_cancel(GtkButton *btn, gpointer data) {
    (void)btn;
    SettingsCtx *ctx = data;
    settings_load(&ctx->win->settings);
    vibe_window_apply_settings(ctx->win);
    gtk_window_close(ctx->dialog);
}

/* ── Settings dialog ── */

static void on_settings(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    VibeWindow *win = data;

    /* allocs array to free on close */
    GPtrArray *allocs = g_ptr_array_new_with_free_func(g_free);

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Settings");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 420);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    /* Notebook with tabs */
    GtkWidget *nb = gtk_notebook_new();
    gtk_widget_set_vexpand(nb, TRUE);
    gtk_box_append(GTK_BOX(vbox), nb);

    /* ── Tab: GUI ── */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_widget_set_margin_start(grid, 12);
        gtk_widget_set_margin_end(grid, 12);
        gtk_widget_set_margin_top(grid, 12);
        gtk_widget_set_margin_bottom(grid, 12);
        int row = 0;

        /* Theme */
        gtk_grid_attach(GTK_GRID(grid), make_label("Theme:"), 0, row, 1, 1);
        GtkWidget *theme_dd = gtk_drop_down_new_from_strings(theme_names);
        gtk_widget_set_hexpand(theme_dd, TRUE);
        for (int i = 0; theme_ids[i]; i++) {
            if (strcmp(win->settings.theme, theme_ids[i]) == 0) {
                gtk_drop_down_set_selected(GTK_DROP_DOWN(theme_dd), i);
                break;
            }
        }
        g_signal_connect(theme_dd, "notify::selected", G_CALLBACK(on_theme_changed), win);
        gtk_grid_attach(GTK_GRID(grid), theme_dd, 1, row++, 1, 1);

        /* Font */
        make_font_row(grid, &row, "Font:", win->settings.gui_font, &win->settings.gui_font_size, win, allocs);

        /* Font Intensity */
        make_intensity_row(grid, &row, "Font Intensity:", &win->settings.font_intensity, win, allocs);

        gtk_notebook_append_page(GTK_NOTEBOOK(nb), grid, gtk_label_new("GUI"));
    }

    /* ── Tab: File Browser ── */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_widget_set_margin_start(grid, 12);
        gtk_widget_set_margin_end(grid, 12);
        gtk_widget_set_margin_top(grid, 12);
        gtk_widget_set_margin_bottom(grid, 12);
        int row = 0;

        make_font_row(grid, &row, "Font:", win->settings.browser_font, &win->settings.browser_font_size, win, allocs);

        gtk_notebook_append_page(GTK_NOTEBOOK(nb), grid, gtk_label_new("File Browser"));
    }

    /* ── Tab: Editor ── */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_widget_set_margin_start(grid, 12);
        gtk_widget_set_margin_end(grid, 12);
        gtk_widget_set_margin_top(grid, 12);
        gtk_widget_set_margin_bottom(grid, 12);
        int row = 0;

        make_font_row(grid, &row, "Font:", win->settings.editor_font, &win->settings.editor_font_size, win, allocs);

        /* Line Spacing */
        gtk_grid_attach(GTK_GRID(grid), make_label("Line Spacing:"), 0, row, 1, 1);
        const char *spacings[] = {"1", "1.2", "1.5", "1.8", "2", NULL};
        GtkWidget *sp_dd = gtk_drop_down_new_from_strings(spacings);
        guint sp_idx = 0;
        if (win->settings.line_spacing >= 1.9) sp_idx = 4;
        else if (win->settings.line_spacing >= 1.7) sp_idx = 3;
        else if (win->settings.line_spacing >= 1.4) sp_idx = 2;
        else if (win->settings.line_spacing >= 1.1) sp_idx = 1;
        gtk_drop_down_set_selected(GTK_DROP_DOWN(sp_dd), sp_idx);
        g_signal_connect(sp_dd, "notify::selected", G_CALLBACK(on_spacing_changed), win);
        gtk_grid_attach(GTK_GRID(grid), sp_dd, 1, row++, 1, 1);

        /* Line Numbers */
        gtk_grid_attach(GTK_GRID(grid), make_label("Line Numbers:"), 0, row, 1, 1);
        GtkWidget *ln_check = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(ln_check), win->settings.show_line_numbers);
        g_signal_connect(ln_check, "toggled", G_CALLBACK(on_line_numbers_toggled), win);
        gtk_grid_attach(GTK_GRID(grid), ln_check, 1, row++, 1, 1);

        /* Highlight Line */
        gtk_grid_attach(GTK_GRID(grid), make_label("Highlight Line:"), 0, row, 1, 1);
        GtkWidget *hl_check = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(hl_check), win->settings.highlight_current_line);
        g_signal_connect(hl_check, "toggled", G_CALLBACK(on_highlight_line_toggled), win);
        gtk_grid_attach(GTK_GRID(grid), hl_check, 1, row++, 1, 1);

        /* Wrap Lines */
        gtk_grid_attach(GTK_GRID(grid), make_label("Wrap Lines:"), 0, row, 1, 1);
        GtkWidget *wrap_check = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(wrap_check), win->settings.wrap_lines);
        g_signal_connect(wrap_check, "toggled", G_CALLBACK(on_wrap_lines_toggled), win);
        gtk_grid_attach(GTK_GRID(grid), wrap_check, 1, row++, 1, 1);

        gtk_notebook_append_page(GTK_NOTEBOOK(nb), grid, gtk_label_new("Editor"));
    }

    /* ── Tab: Terminal ── */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_widget_set_margin_start(grid, 12);
        gtk_widget_set_margin_end(grid, 12);
        gtk_widget_set_margin_top(grid, 12);
        gtk_widget_set_margin_bottom(grid, 12);
        int row = 0;

        make_font_row(grid, &row, "Font:", win->settings.terminal_font, &win->settings.terminal_font_size, win, allocs);

        gtk_notebook_append_page(GTK_NOTEBOOK(nb), grid, gtk_label_new("Terminal"));
    }

    /* ── Tab: Prompt ── */
    {
        GtkWidget *grid = gtk_grid_new();
        gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
        gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
        gtk_widget_set_margin_start(grid, 12);
        gtk_widget_set_margin_end(grid, 12);
        gtk_widget_set_margin_top(grid, 12);
        gtk_widget_set_margin_bottom(grid, 12);
        int row = 0;

        make_font_row(grid, &row, "Font:", win->settings.prompt_font, &win->settings.prompt_font_size, win, allocs);

        /* Send key */
        gtk_grid_attach(GTK_GRID(grid), make_label("Send with:"), 0, row, 1, 1);
        const char *send_opts[] = {"Ctrl+Enter", "Enter", NULL};
        GtkWidget *send_dd = gtk_drop_down_new_from_strings(send_opts);
        gtk_drop_down_set_selected(GTK_DROP_DOWN(send_dd), win->settings.prompt_send_enter ? 1 : 0);
        g_signal_connect(send_dd, "notify::selected", G_CALLBACK(on_send_key_changed), win);
        gtk_grid_attach(GTK_GRID(grid), send_dd, 1, row++, 1, 1);

        /* Switch to terminal after send */
        gtk_grid_attach(GTK_GRID(grid), make_label("Show Terminal:"), 0, row, 1, 1);
        GtkWidget *sw_check = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(sw_check), win->settings.prompt_switch_terminal);
        g_signal_connect(sw_check, "toggled", G_CALLBACK(on_switch_terminal_toggled), win);
        gtk_grid_attach(GTK_GRID(grid), sw_check, 1, row++, 1, 1);

        gtk_notebook_append_page(GTK_NOTEBOOK(nb), grid, gtk_label_new("Prompt"));
    }

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 4);

    SettingsCtx *ctx = g_new(SettingsCtx, 1);
    ctx->win = win;
    ctx->dialog = GTK_WINDOW(dialog);
    ctx->allocs = allocs;

    g_signal_connect(dialog, "destroy", G_CALLBACK(on_settings_destroy), ctx);

    GtkWidget *cancel_btn = gtk_button_new_with_label("Cancel");
    g_signal_connect(cancel_btn, "clicked", G_CALLBACK(on_settings_cancel), ctx);
    gtk_box_append(GTK_BOX(btn_box), cancel_btn);

    GtkWidget *apply_btn = gtk_button_new_with_label("Apply");
    gtk_widget_add_css_class(apply_btn, "suggested-action");
    g_signal_connect(apply_btn, "clicked", G_CALLBACK(on_settings_apply), ctx);
    gtk_box_append(GTK_BOX(btn_box), apply_btn);

    gtk_box_append(GTK_BOX(vbox), btn_box);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── SFTP/SSH Connection Dialog ── */

typedef struct {
    VibeWindow      *win;
    GtkWindow       *dialog;
    SftpConnections  conns;
    GtkListBox      *conn_list;
    GtkEntry        *name_entry;
    GtkEntry        *host_entry;
    GtkEntry        *port_entry;
    GtkEntry        *user_entry;
    GtkEntry        *path_entry;
    GtkEntry        *password_entry;
    GtkCheckButton  *use_key_check;
    GtkEntry        *key_entry;
    GtkWidget       *key_browse_btn;
    GtkWidget       *password_row;
    GtkWidget       *key_row;
    GtkWidget       *key_btn_row;
    int              selected_idx;
} SftpCtx;

static void sftp_populate_list(SftpCtx *ctx) {
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(GTK_WIDGET(ctx->conn_list))))
        gtk_list_box_remove(ctx->conn_list, child);

    for (int i = 0; i < ctx->conns.count; i++) {
        GtkWidget *lbl = gtk_label_new(ctx->conns.items[i].name);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0);
        gtk_widget_set_margin_start(lbl, 8);
        gtk_widget_set_margin_end(lbl, 8);
        gtk_widget_set_margin_top(lbl, 4);
        gtk_widget_set_margin_bottom(lbl, 4);
        gtk_list_box_append(ctx->conn_list, lbl);
    }
}

static void sftp_update_auth_visibility(SftpCtx *ctx) {
    gboolean use_key = gtk_check_button_get_active(ctx->use_key_check);
    gtk_widget_set_visible(ctx->password_row, !use_key);
    gtk_widget_set_visible(GTK_WIDGET(ctx->password_entry), !use_key);
    gtk_widget_set_visible(ctx->key_row, use_key);
    gtk_widget_set_visible(GTK_WIDGET(ctx->key_entry), use_key);
    gtk_widget_set_visible(ctx->key_btn_row, use_key);
}

static void on_use_key_toggled(GtkCheckButton *btn, gpointer data) {
    (void)btn;
    sftp_update_auth_visibility(data);
}

static void on_key_file_selected(GObject *src, GAsyncResult *res, gpointer data) {
    SftpCtx *ctx = data;
    GtkFileDialog *dialog = GTK_FILE_DIALOG(src);
    GFile *file = gtk_file_dialog_open_finish(dialog, res, NULL);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), path);
            g_free(path);
        }
        g_object_unref(file);
    }
}

static void on_key_browse(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Private Key");

    /* default to ~/.ssh */
    char ssh_dir[1024];
    snprintf(ssh_dir, sizeof(ssh_dir), "%s/.ssh", g_get_home_dir());
    GFile *init = g_file_new_for_path(ssh_dir);
    gtk_file_dialog_set_initial_folder(dialog, init);
    g_object_unref(init);

    gtk_file_dialog_open(dialog, ctx->dialog, NULL, on_key_file_selected, ctx);
}

static void on_conn_selected(GtkListBox *box, GtkListBoxRow *row, gpointer data) {
    (void)box;
    SftpCtx *ctx = data;
    if (!row) { ctx->selected_idx = -1; return; }
    int idx = gtk_list_box_row_get_index(row);
    if (idx < 0 || idx >= ctx->conns.count) return;
    ctx->selected_idx = idx;

    SftpConnection *c = &ctx->conns.items[idx];
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), c->name);
    gtk_editable_set_text(GTK_EDITABLE(ctx->host_entry), c->host);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", c->port);
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), port_str);
    gtk_editable_set_text(GTK_EDITABLE(ctx->user_entry), c->user);
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), c->remote_path);
    gtk_check_button_set_active(ctx->use_key_check, c->use_key);
    gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), c->key_path);
    gtk_editable_set_text(GTK_EDITABLE(ctx->password_entry), "");
    sftp_update_auth_visibility(ctx);
}

static void sftp_save_form_to_conn(SftpCtx *ctx, SftpConnection *c) {
    g_strlcpy(c->name, gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry)), sizeof(c->name));
    g_strlcpy(c->host, gtk_editable_get_text(GTK_EDITABLE(ctx->host_entry)), sizeof(c->host));
    c->port = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->port_entry)));
    if (c->port <= 0) c->port = 22;
    g_strlcpy(c->user, gtk_editable_get_text(GTK_EDITABLE(ctx->user_entry)), sizeof(c->user));
    g_strlcpy(c->remote_path, gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry)), sizeof(c->remote_path));
    c->use_key = gtk_check_button_get_active(ctx->use_key_check);
    g_strlcpy(c->key_path, gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry)), sizeof(c->key_path));
}

static void on_sftp_save(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    const char *name = gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry));
    if (!name[0]) return;

    if (ctx->selected_idx >= 0 && ctx->selected_idx < ctx->conns.count) {
        sftp_save_form_to_conn(ctx, &ctx->conns.items[ctx->selected_idx]);
    } else if (ctx->conns.count < MAX_CONNECTIONS) {
        int idx = ctx->conns.count++;
        memset(&ctx->conns.items[idx], 0, sizeof(SftpConnection));
        sftp_save_form_to_conn(ctx, &ctx->conns.items[idx]);
        ctx->selected_idx = idx;
    }
    connections_save(&ctx->conns);
    sftp_populate_list(ctx);
}

static void on_sftp_delete(GtkButton *btn, gpointer data) {
    (void)btn;
    SftpCtx *ctx = data;
    if (ctx->selected_idx < 0 || ctx->selected_idx >= ctx->conns.count) return;

    for (int i = ctx->selected_idx; i < ctx->conns.count - 1; i++)
        ctx->conns.items[i] = ctx->conns.items[i + 1];
    ctx->conns.count--;
    ctx->selected_idx = -1;
    connections_save(&ctx->conns);
    sftp_populate_list(ctx);

    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->host_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), "22");
    gtk_editable_set_text(GTK_EDITABLE(ctx->user_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), "/");
    gtk_editable_set_text(GTK_EDITABLE(ctx->password_entry), "");
    gtk_editable_set_text(GTK_EDITABLE(ctx->key_entry), "");
}

/* ── Async SSH connect via GTask ── */

typedef struct {
    SftpCtx    *ctx;
    GPtrArray  *argv;
    char        host[256];
    char        user[128];
    int         port;
    char        key[1024];
    char        remote[1024];
    GtkWidget  *connect_btn;
} ConnectTaskData;

static void connect_task_data_free(gpointer p) {
    ConnectTaskData *d = p;
    if (d->argv) g_ptr_array_unref(d->argv);
    g_free(d);
}

/* Runs in worker thread — no GTK calls here */
static void ssh_connect_thread(GTask *task, gpointer src, gpointer data,
                                GCancellable *cancel) {
    (void)src; (void)cancel;
    ConnectTaskData *d = data;

    g_ptr_array_add(d->argv, NULL);

    char *stdout_buf = NULL;
    GError *err = NULL;
    gint status = 0;

    gboolean ok = g_spawn_sync(
        NULL, (char **)d->argv->pdata, NULL,
        G_SPAWN_SEARCH_PATH,
        NULL, NULL, &stdout_buf, NULL, &status, &err);
    g_free(stdout_buf);

    if (!ok) {
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
            "SSH failed: %s\nCheck that ssh is installed.", err ? err->message : "unknown");
        if (err) g_error_free(err);
        return;
    }
    if (!g_spawn_check_wait_status(status, NULL)) {
        int code = WIFEXITED(status) ? WEXITSTATUS(status) : status;
        g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
            "SSH connection failed (exit code %d).\nCheck hostname, credentials, and SSH key.", code);
        return;
    }

    g_task_return_boolean(task, TRUE);
}

/* Called on main thread when test completes */
static void ssh_connect_done(GObject *src, GAsyncResult *res, gpointer data) {
    (void)src;
    ConnectTaskData *d = data;
    SftpCtx *ctx = d->ctx;
    GError *err = NULL;

    if (!g_task_propagate_boolean(G_TASK(res), &err)) {
        /* re-enable button */
        gtk_widget_set_sensitive(d->connect_btn, TRUE);
        gtk_button_set_label(GTK_BUTTON(d->connect_btn), "Connect");

        GtkAlertDialog *alert = gtk_alert_dialog_new("%s", err->message);
        gtk_alert_dialog_show(alert, ctx->dialog);
        g_object_unref(alert);
        g_error_free(err);
        return;
    }

    /* Connection OK — set SSH state on window */
    VibeWindow *win = ctx->win;
    g_strlcpy(win->ssh_host, d->host, sizeof(win->ssh_host));
    g_strlcpy(win->ssh_user, d->user, sizeof(win->ssh_user));
    win->ssh_port = d->port;
    g_strlcpy(win->ssh_key, d->key, sizeof(win->ssh_key));
    g_strlcpy(win->ssh_remote_path, d->remote, sizeof(win->ssh_remote_path));

    /* virtual mount path — no actual FUSE mount, just a marker for path_is_remote() */
    snprintf(win->ssh_mount, sizeof(win->ssh_mount),
             "/tmp/vibe-light-sftp-%d-%s@%s", (int)getpid(), d->user, d->host);

    /* close dialog and open remote directory */
    gtk_window_destroy(GTK_WINDOW(ctx->dialog));
    vibe_window_set_root_directory(win, win->ssh_mount);
}

static void on_sftp_connect(GtkButton *btn, gpointer data) {
    SftpCtx *ctx = data;

    const char *host = gtk_editable_get_text(GTK_EDITABLE(ctx->host_entry));
    const char *user = gtk_editable_get_text(GTK_EDITABLE(ctx->user_entry));
    const char *remote = gtk_editable_get_text(GTK_EDITABLE(ctx->path_entry));
    const char *port_str = gtk_editable_get_text(GTK_EDITABLE(ctx->port_entry));
    gboolean use_key = gtk_check_button_get_active(ctx->use_key_check);

    if (!host[0] || !user[0]) return;
    if (!remote[0]) remote = "/";

    int port = atoi(port_str[0] ? port_str : "22");
    if (port <= 0) port = 22;

    /* disable button while connecting */
    gtk_widget_set_sensitive(GTK_WIDGET(btn), FALSE);
    gtk_button_set_label(btn, "Connecting...");

    /* build argv — no shell, immune to injection */
    GPtrArray *av = g_ptr_array_new_with_free_func(g_free);
    g_ptr_array_add(av, g_strdup("ssh"));
    g_ptr_array_add(av, g_strdup("-p"));
    g_ptr_array_add(av, g_strdup_printf("%d", port));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("StrictHostKeyChecking=accept-new"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("BatchMode=yes"));
    g_ptr_array_add(av, g_strdup("-o"));
    g_ptr_array_add(av, g_strdup("ConnectTimeout=10"));
    if (use_key) {
        const char *key = gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry));
        if (key[0]) {
            g_ptr_array_add(av, g_strdup("-i"));
            g_ptr_array_add(av, g_strdup(key));
        }
    }
    g_ptr_array_add(av, g_strdup_printf("%s@%s", user, host));
    g_ptr_array_add(av, g_strdup("--"));
    g_ptr_array_add(av, g_strdup("echo"));
    g_ptr_array_add(av, g_strdup("ok"));

    ConnectTaskData *td = g_new0(ConnectTaskData, 1);
    td->ctx = ctx;
    td->argv = av;
    td->connect_btn = GTK_WIDGET(btn);
    g_strlcpy(td->host, host, sizeof(td->host));
    g_strlcpy(td->user, user, sizeof(td->user));
    td->port = port;
    if (use_key) {
        const char *key = gtk_editable_get_text(GTK_EDITABLE(ctx->key_entry));
        g_strlcpy(td->key, key, sizeof(td->key));
    }
    g_strlcpy(td->remote, remote, sizeof(td->remote));

    GTask *task = g_task_new(NULL, NULL, ssh_connect_done, td);
    g_task_set_task_data(task, td, connect_task_data_free);
    g_task_run_in_thread(task, ssh_connect_thread);
    g_object_unref(task);
}

static void on_sftp_dialog_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    g_free(data);
}

static void on_sftp_dialog(GSimpleAction *action, GVariant *param, gpointer data) {
    (void)action; (void)param;
    VibeWindow *win = data;

    SftpCtx *ctx = g_new0(SftpCtx, 1);
    ctx->win = win;
    ctx->selected_idx = -1;
    connections_load(&ctx->conns);

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "SFTP/SSH Connection");
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(win->window));
    gtk_window_set_default_size(GTK_WINDOW(dialog), 520, 460);
    ctx->dialog = GTK_WINDOW(dialog);

    g_signal_connect(dialog, "destroy", G_CALLBACK(on_sftp_dialog_destroy), ctx);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(hbox, 12);
    gtk_widget_set_margin_end(hbox, 12);
    gtk_widget_set_margin_top(hbox, 12);
    gtk_widget_set_margin_bottom(hbox, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), hbox);

    /* Left: saved connections list */
    GtkWidget *left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_size_request(left_box, 160, -1);

    GtkWidget *list_label = gtk_label_new("Connections");
    gtk_label_set_xalign(GTK_LABEL(list_label), 0);
    gtk_box_append(GTK_BOX(left_box), list_label);

    GtkWidget *list_scroll = gtk_scrolled_window_new();
    gtk_widget_set_vexpand(list_scroll, TRUE);
    ctx->conn_list = GTK_LIST_BOX(gtk_list_box_new());
    gtk_list_box_set_selection_mode(ctx->conn_list, GTK_SELECTION_SINGLE);
    g_signal_connect(ctx->conn_list, "row-activated", G_CALLBACK(on_conn_selected), ctx);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(list_scroll), GTK_WIDGET(ctx->conn_list));
    gtk_box_append(GTK_BOX(left_box), list_scroll);

    gtk_box_append(GTK_BOX(hbox), left_box);
    gtk_box_append(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL));

    /* Right: form */
    GtkWidget *right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_hexpand(right_box, TRUE);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);
    int row = 0;

    gtk_grid_attach(GTK_GRID(grid), make_label("Name:"), 0, row, 1, 1);
    ctx->name_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->name_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->name_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Host:"), 0, row, 1, 1);
    ctx->host_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->host_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->host_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Port:"), 0, row, 1, 1);
    ctx->port_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), "22");
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->port_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("User:"), 0, row, 1, 1);
    ctx->user_entry = GTK_ENTRY(gtk_entry_new());
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->user_entry), 1, row++, 2, 1);

    gtk_grid_attach(GTK_GRID(grid), make_label("Remote Path:"), 0, row, 1, 1);
    ctx->path_entry = GTK_ENTRY(gtk_entry_new());
    gtk_editable_set_text(GTK_EDITABLE(ctx->path_entry), "/");
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->path_entry), 1, row++, 2, 1);

    /* Auth type */
    gtk_grid_attach(GTK_GRID(grid), make_label("Use Private Key:"), 0, row, 1, 1);
    ctx->use_key_check = GTK_CHECK_BUTTON(gtk_check_button_new());
    g_signal_connect(ctx->use_key_check, "toggled", G_CALLBACK(on_use_key_toggled), ctx);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->use_key_check), 1, row++, 2, 1);

    /* Password row */
    GtkWidget *pass_lbl = make_label("Password:");
    gtk_grid_attach(GTK_GRID(grid), pass_lbl, 0, row, 1, 1);
    ctx->password_entry = GTK_ENTRY(gtk_entry_new());
    gtk_entry_set_visibility(ctx->password_entry, FALSE);
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->password_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->password_entry), 1, row++, 2, 1);
    ctx->password_row = pass_lbl;

    /* Key path row */
    GtkWidget *key_lbl = make_label("Key File:");
    gtk_grid_attach(GTK_GRID(grid), key_lbl, 0, row, 1, 1);
    ctx->key_entry = GTK_ENTRY(gtk_entry_new());
    gtk_widget_set_hexpand(GTK_WIDGET(ctx->key_entry), TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(ctx->key_entry), 1, row, 1, 1);
    ctx->key_row = key_lbl;

    ctx->key_browse_btn = gtk_button_new_with_label("...");
    g_signal_connect(ctx->key_browse_btn, "clicked", G_CALLBACK(on_key_browse), ctx);
    gtk_grid_attach(GTK_GRID(grid), ctx->key_browse_btn, 2, row++, 1, 1);
    ctx->key_btn_row = ctx->key_browse_btn;

    sftp_update_auth_visibility(ctx);

    gtk_box_append(GTK_BOX(right_box), grid);

    /* Buttons */
    GtkWidget *btn_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(btn_box, 12);
    gtk_widget_set_valign(btn_box, GTK_ALIGN_END);
    gtk_widget_set_vexpand(btn_box, TRUE);

    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    gtk_widget_add_css_class(del_btn, "destructive-action");
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_sftp_delete), ctx);
    gtk_box_append(GTK_BOX(btn_box), del_btn);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_sftp_save), ctx);
    gtk_box_append(GTK_BOX(btn_box), save_btn);

    GtkWidget *connect_btn = gtk_button_new_with_label("Connect");
    gtk_widget_add_css_class(connect_btn, "suggested-action");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_sftp_connect), ctx);
    gtk_box_append(GTK_BOX(btn_box), connect_btn);

    gtk_box_append(GTK_BOX(right_box), btn_box);
    gtk_box_append(GTK_BOX(hbox), right_box);

    sftp_populate_list(ctx);
    gtk_window_present(GTK_WINDOW(dialog));
}

/* ── Setup ── */

static const GActionEntry win_actions[] = {
    {"open-folder", on_open_folder, NULL, NULL, NULL, {0}},
    {"settings",    on_settings,    NULL, NULL, NULL, {0}},
    {"sftp",        on_sftp_dialog, NULL, NULL, NULL, {0}},
    {"zoom-in",     on_zoom_in,     NULL, NULL, NULL, {0}},
    {"zoom-out",    on_zoom_out,    NULL, NULL, NULL, {0}},
};

void actions_setup(VibeWindow *win, GtkApplication *app) {
    (void)app;
    g_action_map_add_action_entries(
        G_ACTION_MAP(win->window),
        win_actions,
        G_N_ELEMENTS(win_actions),
        win);

    const char *open_accels[] = {"<Control>o", NULL};
    const char *zoomin_accels[] = {"<Control>plus", "<Control>equal", NULL};
    const char *zoomout_accels[] = {"<Control>minus", NULL};
    gtk_application_set_accels_for_action(app, "win.open-folder", open_accels);
    gtk_application_set_accels_for_action(app, "win.zoom-in", zoomin_accels);
    gtk_application_set_accels_for_action(app, "win.zoom-out", zoomout_accels);
}
