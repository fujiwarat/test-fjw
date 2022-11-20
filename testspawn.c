#include <gtk/gtk.h>

gboolean
bus_component_start (void)
{
    gint argc;
    gchar **argv;
    gboolean retval;
    GPid pid;

    GError *error = NULL;
    if (!g_shell_parse_argv ("./a.sh",
                             &argc,
                             &argv,
                             &error)) {
        g_warning ("Can not parse component exec: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    error = NULL;
    GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD;
    retval = g_spawn_async (NULL, argv, NULL,
                            flags,
                            NULL, NULL,
                            &pid, &error);
    g_strfreev (argv);
    if (!retval) {
        g_warning ("Can not execute component: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    return TRUE;
}

int
main (int argc, char *argv[])
{
    return !bus_component_start ();
}

