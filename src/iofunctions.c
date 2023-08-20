/**
 * @file    iofunctions.c
 * @brief
 *
 * Copyright (C) 2009 Gummi Developers
 * All Rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


#include "iofunctions.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

#include "constants.h"
#include "configfile.h"
#include "editor.h"
#include "environment.h"
#include "gui/gui-main.h"
#include "utils.h"

extern Gummi* gummi;

static guint sid = 0;

/* private functions */
void iofunctions_real_load_file (GObject* hook, const gchar* filename);
void iofunctions_real_save_file (GObject* hook, GObject* savecontext);
gchar* iofunctions_decode_text (gchar* text);
gchar* iofunctions_encode_text (gchar* text);

GuIOFunc* iofunctions_init (void) {
    GuIOFunc* io = g_new0(GuIOFunc, 1);

    io->sig_hook = g_object_new(G_TYPE_OBJECT, NULL);

    /* Connect signals */
    g_signal_connect (io->sig_hook, "document-load",
        G_CALLBACK(iofunctions_real_load_file), NULL);
    g_signal_connect (io->sig_hook, "document-write",
        G_CALLBACK(iofunctions_real_save_file), NULL);

    return io;
}

void iofunctions_load_default_text (gboolean loopedonce) {
    GError* readerr = NULL;
    GError* copyerr = NULL;
    gchar* text = NULL;

    GuEditor* ec = gummi_get_active_editor();

    if (!g_file_get_contents (C_WELCOMETEXT, &text, NULL, &readerr)) {
        slog (L_WARNING, "Could not find default welcome text, resetting..\n");
        utils_copy_file (C_DEFAULTTEXT, C_WELCOMETEXT, &copyerr);
        if (!loopedonce) return iofunctions_load_default_text (TRUE);
    }

    if (text) editor_fill_buffer (ec, text);

    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (ec->buffer), FALSE);
    g_free (text);
}

void iofunctions_load_file (GuIOFunc* io, const gchar* filename) {
    gchar* status;

    slog (L_INFO, "loading %s ...\n", filename);

    /* add Loading message to status bar and ensure GUI is current */
    status = g_strdup_printf ("Loading %s...", filename);
    statusbar_set_message (status);
    g_free (status);

    g_signal_emit_by_name (io->sig_hook, "document-load", filename);
}

void iofunctions_real_load_file (GObject* hook, const gchar* filename) {
    GError* err = NULL;
    gchar* text = NULL;
    gchar* decoded = NULL;
    gchar* dirname = NULL;
    gboolean result;
    GuEditor* ec = NULL;

    if (FALSE == (g_file_get_contents (filename, &text, NULL, &err))) {
        slog (L_G_ERROR, "g_file_get_contents (): %s\n", err->message);
        g_error_free (err);
        iofunctions_load_default_text (FALSE);
        return;
    }

    if (NULL == (decoded = iofunctions_decode_text (text)))
        goto cleanup;

    ec = gummi_get_active_editor();
    editor_fill_buffer (gummi_get_active_editor(), decoded);
    gtk_text_buffer_set_modified (GTK_TEXT_BUFFER(ec->buffer), FALSE);
    
    // Check for custom packages in file directory
    dirname = g_path_get_dirname (filename);
    scan_directory (dirname);
    g_free (dirname);
    // Scan the current file for bibitems, newenvironments and newcommands
    scan_for_labels (decoded);
    scan_for_bibitems (decoded);
    scan_for_new_envs (decoded, NULL);
    scan_for_new_cmds (decoded, NULL);

cleanup:
    g_free (decoded);
    g_free (text);
}

void iofunctions_save_file (GuIOFunc* io, gchar* filename, gchar *text) {
    gchar* status = NULL;

    status = g_strdup_printf (_("Saving %s..."), filename);
    statusbar_set_message (status);
    g_free (status);

    GObject *savecontext = g_object_new(G_TYPE_OBJECT, NULL);

    g_object_set_data (savecontext, "filename", filename);
    g_object_set_data (savecontext, "text", text);

    g_signal_emit_by_name (io->sig_hook, "document-write", savecontext);

    gtk_text_buffer_set_modified
                (GTK_TEXT_BUFFER(gummi_get_active_editor()->buffer), FALSE);
}

void iofunctions_real_save_file (GObject* hook, GObject* savecontext) {

    gboolean result = FALSE;
    gchar* filename = NULL;
    gchar* encoded = NULL;
    gchar* text = NULL;
    GError* err = NULL;

    filename = g_object_get_data (savecontext, "filename");
    text = g_object_get_data (savecontext, "text");

    encoded = iofunctions_encode_text (text);

    // set the contents of the file to the text from the buffer
    if (filename != NULL) {
        if (! (result = g_file_set_contents (filename, encoded, -1, &err))) {
            slog (L_ERROR, "g_file_set_contents (): %s\n", err->message);
            g_error_free (err);
        }
    }

    if (result == FALSE) {
        slog (L_G_ERROR, _("%s\nPlease try again later."), err->message);
        g_error_free (err);
    }
    
    // Update completion information for bibitems, newenvironments and newcommands
    scan_for_labels (text);
    scan_for_bibitems (text);
    scan_for_new_envs (text, NULL);
    scan_for_new_cmds (text, NULL);

    g_free (encoded);
    g_free (text);
    g_object_unref (savecontext);
}

gchar* iofunctions_get_swapfile (const gchar* filename) {
    gchar* basename = NULL;
    gchar* dirname = NULL;
    gchar* swapfile = NULL;

    basename = g_path_get_basename (filename);
    dirname = g_path_get_dirname (filename);
    swapfile = g_strdup_printf ("%s%c.%s.swp", dirname,
            G_DIR_SEPARATOR, basename);

    g_free (dirname);
    g_free (basename);
    return swapfile;
}

gboolean iofunctions_has_swapfile (const gchar* filename) {
    if (filename == NULL) return FALSE;

    gchar* swapfile = iofunctions_get_swapfile (filename);
    if (utils_path_exists (swapfile)) {
        return TRUE;
    }
    return FALSE;
}

void iofunctions_start_autosave (void) {
    sid = g_timeout_add_seconds (config_get_integer ("File", "autosave_timer") * 60,
                                 iofunctions_autosave_cb,
                                 NULL);
    slog (L_DEBUG, "Autosaving function started..\n");
}

void iofunctions_stop_autosave (void) {
    if (sid > 0) {
        g_source_remove (sid);
        slog (L_DEBUG, "Autosaving function stopped..\n");
        return;
    }
    else {
        slog (L_ERROR, "Error occurred stopping autosaving..\n");
    }
}

void iofunctions_reset_autosave (const gchar* name) {
    iofunctions_stop_autosave ();
    if (config_get_boolean ("File", "autosaving")) {
        iofunctions_start_autosave ();
    }
}

char* iofunctions_decode_text (gchar* text) {
    GError* err = NULL;
    gchar* result = 0;
    gsize read = 0, written = 0;

    if (! (result = g_locale_to_utf8 (text, -1, &read, &written, &err))) {
        g_error_free (err);
        slog (L_ERROR, "Failed to convert text from default locale, trying "
                "ISO-8859-1\n");
        gsize in_size = strlen (text), out_size = in_size * 2;
        gchar* out = (gchar*)g_malloc (out_size);
        gchar* process = out;
        /* TODO: replace these calls to the non-raw glib functions */
        GIConv cd = g_iconv_open ("UTF-8//IGNORE", "ISO−8859-1");

        if (-1 == g_iconv (cd, &text, &in_size, &process, &out_size)) {
            slog (L_G_ERROR, _("Can not convert text to UTF-8!\n"));
            g_free (out);
            out = NULL;
        }
        result = out;
    }
    return result;
}

gchar* iofunctions_encode_text (gchar* text) {
    GError* err = NULL;
    gchar* result = 0;
    gsize read = 0, written = 0;

    if (! (result = g_locale_from_utf8 (text, -1, &read, &written, &err))) {
        g_error_free (err);
        slog (L_ERROR, "failed to convert text to default locale, text will "
                "be saved in UTF-8\n");
        result = g_strdup (text);
    }
    return result;
}

gboolean iofunctions_autosave_cb (void *user) {
    gint tabtotal, i;
    GtkWidget *focus;
    GuTabContext* tab;
    GuEditor *ec;
    gchar *text;

    GList *tabs = gummi_get_all_tabs();
    tabtotal = g_list_length(tabs);

    /* skip the autosave procedure when there are no tabs open */
    if (tabtotal == 0) return TRUE;

    for (i=0; i < tabtotal; i++) {
        tab = g_list_nth_data (tabs, i);
        ec = tab->editor;

        if ((ec->filename) && editor_buffer_changed (ec)) {
            focus = gtk_window_get_focus (gummi_get_gui ()->mainwindow);
            text = editor_grab_buffer (ec);
            gtk_widget_grab_focus (focus);
            iofunctions_save_file (gummi->io, ec->filename, text);
            gtk_text_buffer_set_modified (GTK_TEXT_BUFFER (ec->buffer), FALSE);
            slog (L_DEBUG, "Autosaving document: %s\n", ec->filename);
            gui_set_filename_display (tab, TRUE, TRUE);
       }
    }
    return TRUE;
}

void scan_directory (const gchar* dirname) {
    GFile* directory = g_file_new_for_path (dirname);
    GFileEnumerator* enumerator = g_file_enumerate_children (directory, "standard::*", G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
    GFileInfo* info;
    
    while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
        GFile* child = g_file_enumerator_get_child (enumerator, info);
        gchar* path = g_file_get_path (child);
        gchar* name = g_file_get_basename (child);
        if (g_str_has_suffix (name, ".sty")) {
            gchar* text;
            gchar* decoded;
            gchar* package = (gchar*)g_malloc ((strlen(name) - 3)*sizeof(gchar));
            
            if (!g_file_get_contents (path, &text, NULL, NULL)) continue;
            if (NULL == (decoded = iofunctions_decode_text (text))) {
                g_free (text);
                continue;
            }
            
            g_strlcpy (package, name, strlen(name) - 3);
            scan_for_new_envs (decoded, package);
            scan_for_new_cmds (decoded, package);
            
            g_free (text);
            g_free (decoded);
            g_free (package);
        }
        g_object_unref (child);
        g_free (path);
        g_free (name);
        g_object_unref (info);
    }
    
    g_object_unref (directory);
    g_object_unref (enumerator);
}
