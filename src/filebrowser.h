#ifndef FILEBROWSER_H
#define FILEBROWSER_H

#include "window.h"

/* Context menu (Zed-style) — called from right-click handler */
void filebrowser_show_context_menu(VibeWindow *win, GtkWidget *widget,
                                   const char *path, gboolean is_dir,
                                   double x, double y);

/* Inline rename — replaces the row label with an editable entry */
void filebrowser_inline_rename(VibeWindow *win, GtkListBoxRow *row);

/* Set up drag & drop for file tree reordering/moving */
void filebrowser_setup_dnd(VibeWindow *win);

/* DnD prepare callback — used by create_tree_row in window.c */
GdkContentProvider *filebrowser_dnd_prepare(GtkDragSource *source, double x, double y,
                                             gpointer data);

#endif
