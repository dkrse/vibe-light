#include "actions.h"
#include <string.h>

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
        strncpy(win->settings.theme, theme_ids[idx], sizeof(win->settings.theme) - 1);
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
        strncpy(ctx->font_field, family, 255);
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

/* ── Setup ── */

static const GActionEntry win_actions[] = {
    {"open-folder", on_open_folder, NULL, NULL, NULL, {0}},
    {"settings",    on_settings,    NULL, NULL, NULL, {0}},
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
