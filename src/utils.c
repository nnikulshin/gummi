/**
 * @file    utils.c
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


#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>
#include <unistd.h>

#ifdef WIN32
    #include <windows.h>
#else
    #include <sys/types.h>
    #include <sys/wait.h>
#endif

#ifndef WEXITSTATUS
#   define WEXITSTATUS(stat_val) ((unsigned int) (stat_val) >> 8)
#endif

#include "completion.h"
#include "constants.h"
#include "environment.h"
#include "utils.h"

#ifdef WIN32
    static gchar *slogmsg_info = "[Info] ";
    static gchar *slogmsg_thread = "[Thread]";
    static gchar *slogmsg_debug = "[Debug] ";
    static gchar *slogmsg_fatal = "[Fatal] ";
    static gchar *slogmsg_error = "[Error] ";
    static gchar *slogmsg_warning = "[Warning] ";
#else
    static gchar *slogmsg_info = "\e[1;34m[Info]\e[0m ";
    static gchar *slogmsg_thread = "\e[1;31m[Thread]\e[0m";
    static gchar *slogmsg_debug = "\e[1;32m[Debug]\e[0m ";
    static gchar *slogmsg_fatal = "\e[1;37;41m[Fatal]\e[0m ";
    static gchar *slogmsg_error = "\e[1;31m[Error]\e[0m ";
    static gchar *slogmsg_warning = "\e[1;33m[Warning]\e[0m ";
#endif


static gint slog_debug = 0;
static GtkWindow* parent = 0;
GThread* main_thread = 0;
extern pid_t typesetter_pid;


void slog_init (gint debug) {
    slog_debug = debug;
    main_thread = g_thread_self ();
}

gboolean in_debug_mode() {
    return slog_debug;
}

void slog_set_gui_parent (GtkWindow* p) {
    parent = p;
}

void slog (gint level, const gchar *fmt, ...) {
    gchar message[BUFSIZ];
    va_list vap;

    if (L_IS_TYPE (level, L_DEBUG) && !slog_debug) return;

    if (g_thread_self () != main_thread)
        g_fprintf (stderr, "%s", slogmsg_thread);

    if (L_IS_TYPE (level, L_DEBUG))
        g_fprintf (stderr, "%s", slogmsg_debug);
    else if (L_IS_TYPE (level, L_FATAL) || L_IS_TYPE (level, L_G_FATAL))
        g_fprintf (stderr, "%s", slogmsg_fatal);
    else if (L_IS_TYPE (level, L_ERROR) || L_IS_TYPE (level, L_G_ERROR))
        g_fprintf (stderr, "%s", slogmsg_error);
    else if (L_IS_TYPE (level, L_WARNING))
        g_fprintf (stderr, "%s", slogmsg_warning);
    else
        g_fprintf (stderr, "%s", slogmsg_info);

    va_start (vap, fmt);
    vsnprintf (message, BUFSIZ, fmt, vap);
    va_end (vap);
    fprintf (stderr, "%s", message);

    if (L_IS_GUI (level)) {
        GtkWidget* dialog;

        dialog = gtk_message_dialog_new (parent,
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                L_IS_TYPE (level,L_G_INFO)? GTK_MESSAGE_INFO: GTK_MESSAGE_ERROR,
                GTK_BUTTONS_OK,
                "%s", message);

        if (L_IS_TYPE (level, L_G_ERROR))
            gtk_window_set_title (GTK_WINDOW (dialog), "Error!");
        else if (L_IS_TYPE (level, L_G_FATAL))
            gtk_window_set_title (GTK_WINDOW (dialog), "Fatal Error!");
        else if (L_IS_TYPE (level, L_G_INFO))
            gtk_window_set_title (GTK_WINDOW (dialog), "Info");

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
    }

    if (!L_IS_TYPE (level, L_INFO) &&
        !L_IS_TYPE (level, L_DEBUG) &&
        !L_IS_TYPE (level, L_ERROR) &&
        !L_IS_TYPE (level, L_G_INFO) &&
        !L_IS_TYPE (level, L_G_ERROR))
        exit (1);
}

gint utils_save_reload_dialog (const gchar* message) {
    GtkWidget* dialog;
    gint ret = 0;

    g_return_val_if_fail (message != NULL, 0);

    dialog = gtk_message_dialog_new (parent,
                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                 GTK_MESSAGE_QUESTION,
                 GTK_BUTTONS_NONE,
                 "%s", message);
    gtk_dialog_add_buttons(GTK_DIALOG (dialog), "Reload", GTK_RESPONSE_YES, "Save", GTK_RESPONSE_NO, NULL);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Confirmation"));
    ret = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    return ret;
}

static gchar* css_add (gchar* base, gchar* property, const gchar* value) {
    // helper to create css pairs for utils_pango_font_desc_to_css()
    return (g_strconcat (base, property, ": ", value, "; ", NULL));
}

gchar* utils_pango_font_desc_to_css (PangoFontDescription* font_desc) {
PangoFontMask font_mask;
    gchar* result = NULL;
    gchar* val = NULL;

    // Generate css by analysing PangoFontDescription structure:
    //
    //     selector {
    //         property1: value1;
    //         property2: value2;
    //         propertyN: valueN;
    //     }

    font_mask = pango_font_description_get_set_fields (font_desc);

	// add selector:
    result = "* { ";

    // add font family:
	if (font_mask & PANGO_FONT_MASK_FAMILY) {
        result = css_add (result, "font-family",
                          pango_font_description_get_family (font_desc));
    }

    // add font slant styling:
    if (font_mask & PANGO_FONT_MASK_STYLE) {
        switch (pango_font_description_get_style (font_desc)) {
            case PANGO_STYLE_NORMAL:  val = "normal";  break;
            case PANGO_STYLE_OBLIQUE: val = "oblique"; break;
            case PANGO_STYLE_ITALIC:  val = "italic";  break;
        }
        result = css_add (result, "font-style", val);
    }

    // add font capitalization variant:
    if (font_mask & PANGO_FONT_MASK_VARIANT) {
        switch (pango_font_description_get_variant (font_desc)) {
            case PANGO_VARIANT_NORMAL:     val = "normal";     break;
            case PANGO_VARIANT_SMALL_CAPS: val = "small-caps"; break;
        }
        result = css_add (result, "font-variant", val);
    }

    // add font boldness / weight:
    if (font_mask & PANGO_FONT_MASK_WEIGHT) {
        gint weight = (gint) pango_font_description_get_weight (font_desc);
        result = css_add (result, "font-weight", g_strdup_printf ("%d", weight));
    }

    // add font stretch:
    if (font_mask & PANGO_FONT_MASK_STRETCH) {
        switch (pango_font_description_get_stretch (font_desc)) {
            case PANGO_STRETCH_ULTRA_CONDENSED: val = "ultra-condensed"; break;
            case PANGO_STRETCH_EXTRA_CONDENSED: val = "extra-condensed"; break;
            case PANGO_STRETCH_CONDENSED:       val = "condensed";       break;
            case PANGO_STRETCH_SEMI_CONDENSED:  val = "semi-condensed";  break;
            case PANGO_STRETCH_NORMAL:          val = "normal";          break;
            case PANGO_STRETCH_SEMI_EXPANDED:   val = "semi-expanded";   break;
            case PANGO_STRETCH_EXPANDED:        val = "expanded";        break;
            case PANGO_STRETCH_EXTRA_EXPANDED:  val = "extra-expanded";  break;
            case PANGO_STRETCH_ULTRA_EXPANDED:  val = "ultra-expanded";  break;
        }
        result = css_add (result, "font-stretch", val);
    }

    // add font size:
    if (font_mask & PANGO_FONT_MASK_SIZE) {
        gint size = pango_font_description_get_size (font_desc);

        if (!pango_font_description_get_size_is_absolute (font_desc)) {
            size = size / PANGO_SCALE;
        }
        result = css_add (result, "font-size", g_strdup_printf("%dpx", size));
    }

    // add closing bracket
    result = g_strconcat (result, "}", NULL);

	return result;
}

gint utils_yes_no_dialog (const gchar* message) {
    GtkWidget* dialog;
    gint ret = 0;

    g_return_val_if_fail (message != NULL, 0);

    dialog = gtk_message_dialog_new (parent,
                 GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                 GTK_MESSAGE_QUESTION,
                 GTK_BUTTONS_YES_NO,
                 "%s", message);

    gtk_window_set_title (GTK_WINDOW (dialog), _("Confirmation"));
    ret = gtk_dialog_run (GTK_DIALOG (dialog));
    gtk_widget_destroy (dialog);

    return ret;
}

gboolean utils_path_exists (const gchar* path) {
    if (NULL == path) return FALSE;
    gboolean result = FALSE;
    GFile* file = g_file_new_for_path (path);
    result = g_file_query_exists (file, NULL);
    g_object_unref (file);
    return result;
}

gboolean utils_uri_path_exists (const gchar* uri) {
    gboolean result = FALSE;
    
    gchar *filepath = g_filename_from_uri(uri, NULL, NULL);
    result = utils_path_exists(filepath);
    g_free(filepath);
    return(result);
}

gboolean utils_set_file_contents (const gchar *filename, const gchar *text,
                                  gssize length) {
    /* g_file_set_contents may not work correctly on Windows. See the
     * API documentation of this function for details. Should Gummi
     * be affected, we might have to implement an alternative */
        GError* error = NULL;
        if (!g_file_set_contents(filename, text, length, &error)) {
            slog (L_ERROR, "%s\n", error->message);
            g_error_free(error);
            return FALSE;
        }
        return TRUE;
}

gboolean utils_copy_file (const gchar* source, const gchar* dest, GError** err) {
    gchar* contents;
    gsize length;

    g_return_val_if_fail (source != NULL, FALSE);
    g_return_val_if_fail (dest != NULL, FALSE);
    g_return_val_if_fail (err == NULL || *err == NULL, FALSE);

    if (!g_file_get_contents (source, &contents, &length, err))
        return FALSE;

    if (!g_file_set_contents (dest, contents, length, err))
        return FALSE;

    g_free (contents);

    return TRUE;
}

Tuple2 utils_popen_r (const gchar* cmd, const gchar* chdir) {
    gchar buf[BUFSIZ];
    int pout = 0;
    gchar* ret = NULL;
    gchar* rot = NULL;
    glong len = 0;
    gint status = 0;
    int n_args = 0;
    gchar** args = NULL;
    GError* error = NULL;

    g_assert (cmd != NULL);

    /* XXX: Set process pid, ugly... */
    if (!g_shell_parse_argv(cmd, &n_args, &args, &error)) {
        slog(L_G_FATAL, "%s", error->message);
        /* Not reached */
    }

    if (!g_spawn_async_with_pipes (chdir, args, NULL,
                G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                NULL, NULL, &typesetter_pid, NULL, &pout, NULL, &error)) {
        slog(L_G_FATAL, "%s", error->message);
        /* Not reached */
    }

    // TODO: replace with GIOChannel implementation:
    while ((len = read (pout, buf, BUFSIZ)) > 0) {
        buf[len - (len == BUFSIZ)] = 0;
        rot = g_strdup (ret);
        g_free (ret);
        if (ret)
            ret = g_strconcat (rot, buf, NULL);
        else
            ret = g_strdup (buf);
        g_free (rot);
    }

    // close the file descriptor:
    close(pout);

    #ifdef WIN32 // TODO: check this
        status = WaitForSingleObject(typesetter_pid, INFINITE);
    #else
        waitpid(typesetter_pid, &status, 0);
    #endif

    // See bug 446:
    if (ret) {
        if (!g_utf8_validate (ret, -1, NULL)) {
            ret = g_convert_with_fallback (ret, -1, "UTF-8",
                    "ISO-8859-1", NULL, NULL, NULL, NULL);
        }
    }

    return (Tuple2){NULL, (gpointer)(glong)status, (gpointer)ret};
}

gchar* utils_path_to_relative (const gchar* root, const gchar* target) {
    gchar* tstr = NULL;
    if ( (root != NULL) && (0 == strncmp (target, root, strlen (root))))
        tstr = g_strdup (target + strlen (root) + 1);
    else
        tstr = g_strdup (target);
    return tstr;
}

gchar* utils_get_tmp_tmp_dir (void) {
	/* brb, gonna go punch a wall */
    gchar *tmp_tmp = g_build_path
						(C_DIRSEP, g_get_home_dir(), "gtmp", NULL);
    g_mkdir_with_parents (tmp_tmp, DIR_PERMS);

    return tmp_tmp;
}


gboolean utils_glist_is_member (GList* list, gchar* item) {
    int nrofitems = g_list_length (list);
    int i;

    for (i=0;i<nrofitems;i++) {
        if (STR_EQU (item, g_list_nth_data (list,i))) {
            return TRUE;
        }
    }
    return FALSE;
}

gboolean utils_subinstr (const gchar* substr, const gchar* target,
        gboolean case_insens) {
    if (target != NULL && substr != NULL) {
        if (case_insens) {
            gchar* ntarget = g_utf8_strup(target, -1);
            gchar* nsubstr = g_utf8_strup(substr, -1);
            gboolean result = g_strstr_len(ntarget, -1, nsubstr) != NULL;
            g_free(ntarget);
            g_free(nsubstr);
            return result;
        }
        else {
            return g_strstr_len(target, -1, substr) != NULL;
        }
    }
    return FALSE;
}

gchar* g_substr(gchar* src, gint start, gint end) {
    gint len = end - start + 1;
    char* dst = g_malloc(len * sizeof(gchar));
    memset(dst, 0, len);
    return strncpy(dst, &src[start], end - start);
}

slist* slist_find (slist* head, const gchar* term, gboolean n, gboolean create) {
    slist* current = head;
    slist* prev = 0;

    while (current) {
        if (n) {
            if (0 == strncmp (current->first, term, strlen (term)))
                return current;
        } else {
            if (STR_EQU (current->first, term))
                return current;
        }
        prev = current;
        current = current->next;
    }
    if (create) {
        slog (L_WARNING, "can't find `%s', creating new field for it...\n",
                term);
        prev->next = g_new0 (slist, 1);
        current = prev->next;
        current->first = g_strdup (term);
        current->second = g_strdup ("");
    } else
        current = NULL;
    return current;
}

slist* slist_append (slist* head, slist* node) {
    slist* current = head;
    slist* prev = NULL;

    while (current) {
        prev = current;
        current = current->next;
    }
    prev->next = node;
    return head;
}

slist* slist_remove (slist* head, slist* node) {
    slist* current = head;
    slist* prev = NULL;

    while (current) {
        if (current == node) break;
        prev = current;
        current = current->next;
    }
    if (current) {
        if (current == head)
            head = head->next;
        else
            prev->next = current->next;
    }
    return head;
}

void scan_for_labels (gchar* content) {
    GRegex* regex = NULL;
    GMatchInfo* match_info;
    
    regex = g_regex_new ("\\\\label{\\s*([^{}\\s]*)\\s*}",
        G_REGEX_MULTILINE, 0, NULL);
    g_regex_match (regex, content, 0, &match_info);
    while (g_match_info_matches (match_info)) {
        gchar** result = g_match_info_fetch_all (match_info);
        if (result[1]) gu_completion_add_ref_choice (gu_completion_get_default (), result[1]);
        g_match_info_next (match_info, NULL);
        g_strfreev (result);
    }
    
    g_match_info_free (match_info);
    g_regex_unref (regex);
}

void scan_for_bibitems (gchar* content) {
    GRegex* regex = NULL;
    GMatchInfo* match_info;
    
    regex = g_regex_new ("\\\\bibitem{\\s*([^{}\\s]*)\\s*}",
        G_REGEX_MULTILINE, 0, NULL);
    g_regex_match (regex, content, 0, &match_info);
    while (g_match_info_matches (match_info)) {
        gchar** result = g_match_info_fetch_all (match_info);
        if (result[1]) gu_completion_add_citation_choice (gu_completion_get_default (), result[1]);
        g_match_info_next (match_info, NULL);
        g_strfreev (result);
    }
    
    g_match_info_free (match_info);
    g_regex_unref (regex);
}

void scan_for_new_envs (gchar* content, gchar* package) {
    GRegex* regex = NULL;
    GMatchInfo* match_info;
    
    regex = g_regex_new ("\\\\newenvironment\\*?{\\s*([^{}\\s]*)\\s*}",
        G_REGEX_MULTILINE, 0, NULL);
    g_regex_match (regex, content, 0, &match_info);
    while (g_match_info_matches (match_info)) {
        gchar** result = g_match_info_fetch_all (match_info);
        if (result[1]) gu_completion_add_environment (gu_completion_get_default (), result[1], package);
        g_match_info_next (match_info, NULL);
        g_strfreev (result);
    }
    
    g_match_info_free (match_info);
    g_regex_unref (regex);
}

void scan_for_new_cmds (gchar* content, gchar* package) {
    GRegex* regex = NULL;
    GMatchInfo* match_info;
    
    regex = g_regex_new ("\\\\(?:re)?newcommand\\*?{\\s*([^{}\\[\\]\\s]*)\\s*}\\s*(?:\\[\\s*(\\d)\\s*\\])?\\s*(?:\\[\\s*([^{}\\[\\]\\s]*)\\s*\\])?",
        G_REGEX_MULTILINE, 0, NULL);
    g_regex_match (regex, content, 0, &match_info);
    while (g_match_info_matches (match_info)) {
        gchar** result = g_match_info_fetch_all (match_info);
        gint n_arg = 0;
        gchar** arg_names = NULL;
        gint i;
        if (!result[1]) continue;
        if (result[2]) n_arg = atoi (result[2]);
        if (n_arg != 0) {
            arg_names = g_malloc ((n_arg+1)*sizeof(gchar*));
            for (i = 0; i < n_arg; i++) arg_names[i] = g_strdup_printf ("arg%i", i);
            arg_names[n_arg] = NULL;
        }
        gu_completion_add_command (gu_completion_get_default (), result[1], arg_names, n_arg, result[2] != NULL && result[3] != NULL, package);
        g_match_info_next (match_info, NULL);
        g_strfreev (result);
        g_strfreev (arg_names);
    }
    
    g_match_info_free (match_info);
    g_regex_unref (regex);
}
