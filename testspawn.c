#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h> /* O_CREAT O_WRONLY */
#include <unistd.h> /* read() write() */

#define USE_IBUS_FD 1

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#else
#define HAVE_O_CLOEXEC 1
#endif

#ifdef USE_IBUS_FD
static int
unix_open_file (const char *filename,
                int         mode)
{
    int my_fd = g_open (filename, mode | O_BINARY | O_CLOEXEC, 0666);
    if (my_fd < 0) {
        int saved_errno = errno;
        char *display_name = g_filename_display_name (filename);
        g_warning ("Error opening file %s: %s",
                   display_name, g_strerror (saved_errno));
    }
#ifndef HAVE_O_CLOEXEC
    else {
        fcntl (my_fd, F_SETFD, FD_CLOEXEC);
    }
#endif
    return my_fd;
}
#endif

static void
bus_component_child_cb (GPid          pid,
                        gint          status,
                        GMainLoop   **loop)
{
    g_spawn_close_pid (pid);

    g_main_loop_quit (*loop);
    *loop = NULL;
}

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
    GSpawnFlags flags = G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_LEAVE_DESCRIPTORS_OPEN;
#ifdef USE_IBUS_FD
    int stdin_fd = unix_open_file ("/dev/null", O_CREAT | O_WRONLY);
    int stdout_fd = unix_open_file ("./stdout.log", O_CREAT | O_WRONLY);
    int stderr_fd = unix_open_file ("./stderr.log", O_CREAT | O_WRONLY);
    retval = g_spawn_async_with_pipes_and_fds (NULL,
                                               (const gchar * const *)argv,
                                               NULL,
                                               flags,
                                               NULL,
                                               NULL,
                                               stdin_fd,
                                               stdout_fd,
                                               stderr_fd,
                                               NULL,
                                               NULL,
                                               0,
                                               &pid,
                                               NULL, NULL, NULL,
                                               &error);
#else
    retval = g_spawn_async (NULL, argv, NULL,
                            flags,
                            NULL, NULL,
                            &pid, &error);
#endif
    g_strfreev (argv);
    if (!retval) {
        g_warning ("Can not execute component: %s",
                   error->message);
        g_error_free (error);
        return FALSE;
    }

    GMainLoop *loop = g_main_loop_new (NULL, FALSE);
    g_child_watch_add (pid,
                       (GChildWatchFunc) bus_component_child_cb,
                       &loop);
    g_main_loop_run (loop);

#ifdef USE_IBUS_FD
    close (stdin_fd);
    close (stdout_fd);
    close (stderr_fd);
#define SIZE 512
    int n;
    char buf[SIZE];
    if ((stdout_fd = unix_open_file ("./stdout.log", O_RDONLY)) > 0) {
        while ((n = read (stdout_fd, buf, sizeof (buf))) > 0)
            write (1, buf, n);
        close (stdout_fd);
        unlink ("./stdout.log");
    }
    if ((stderr_fd = unix_open_file ("./stderr.log", O_RDONLY)) > 0) {
        while ((n = read (stderr_fd, buf, sizeof (buf))) > 0)
            write (2, buf, n);
        close (stderr_fd);
        unlink ("./stderr.log");
    }
#undef SIZE
#endif
    return TRUE;
}

int
main (int argc, char *argv[])
{
    return !bus_component_start ();
}

