#ifndef AI_H
#define AI_H

#include "window.h"

char *latex_to_unicode(const char *latex);
void ai_refresh_output(VibeWindow *win);
void ai_parse_and_display(VibeWindow *win);
void send_prompt_to_ai(VibeWindow *win);
void on_ai_communicate_done(GObject *src, GAsyncResult *res, gpointer data);
gboolean on_prompt_key(GtkEventControllerKey *ctrl, guint keyval,
                       guint keycode, GdkModifierType state, gpointer data);
gboolean ai_timer_tick(gpointer data);

#endif
