#ifndef WINDOW_H
#define WINDOW_H

#include <gtk/gtk.h>
#include <vte/vte.h>
#include "settings.h"

typedef struct {
    GtkApplicationWindow *window;
    GtkNotebook          *notebook;

    /* Tab 1: File Browser (left) + File Viewer (right) */
    GtkListBox           *file_list;
    GtkLabel             *path_label;
    GtkTextView          *file_view;
    GtkTextBuffer        *file_buffer;
    GtkTextTag           *intensity_tag;
    GtkTextView          *line_numbers;
    GtkWidget            *ln_scrolled;
    int                   highlight_line;
    int                   cached_line_count;
    guint                 intensity_idle_id;
    GdkRGBA               highlight_rgba;
    char                  current_dir[2048];
    char                  root_dir[2048];

    /* Tab 2: Terminal */
    VteTerminal          *terminal;

    /* Tab 3: Prompt */
    GtkTextView          *prompt_view;
    GtkTextBuffer        *prompt_buffer;
    GtkTextTag           *prompt_intensity_tag;

    /* Status bar */
    GtkLabel             *status_label;

    VibeSettings          settings;
    GtkCssProvider       *css_provider;
} VibeWindow;

VibeWindow *vibe_window_new(GtkApplication *app);
void vibe_window_apply_settings(VibeWindow *win);
void vibe_window_open_directory(VibeWindow *win, const char *path);
void vibe_window_set_root_directory(VibeWindow *win, const char *path);

#endif
