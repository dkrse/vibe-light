#ifndef EDITOR_H
#define EDITOR_H

#include "window.h"

void save_current_file(VibeWindow *win);
void show_goto_line(VibeWindow *win);
gboolean on_editor_key(GtkEventControllerKey *ctrl, guint keyval,
                       guint keycode, GdkModifierType state, gpointer data);
gboolean on_search_key(GtkEventControllerKey *ctrl, guint keyval,
                       guint keycode, GdkModifierType state, gpointer data);
void on_search_changed(GtkEditable *editable, gpointer data);
void on_search_next(GtkButton *btn, gpointer data);
void on_search_prev(GtkButton *btn, gpointer data);
void on_search_close(GtkButton *btn, gpointer data);

#endif
