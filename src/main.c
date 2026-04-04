#include <adwaita.h>
#include "window.h"

static void on_activate(GtkApplication *app, gpointer data) {
    (void)data;
    vibe_window_new(app);
}

int main(int argc, char *argv[]) {
    AdwApplication *app = adw_application_new("com.vibe.light", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
