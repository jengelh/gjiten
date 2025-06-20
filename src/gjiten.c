/* -*- Mode: C; tab-width: 2;   indent-tabs-mode: space; c-basic-offset: 2 -*- */
/* vi: set ts=2 sw=2: */
/* gjiten.c

   GJITEN : A GTK+/GNOME BASED JAPANESE DICTIONARY

   Copyright (C) 1999 - 2005 Botond Botyanszki <boti@rocketmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published  by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "error.h"
#include "constants.h"
#include "conf.h"
#include "dicfile.h"
#include "worddic.h"
#include "kanjidic.h"
#include "pref.h"
#include "gjiten.h"
#include "dicutil.h"
#include "utils.h"
#include "resources.h"

GjitenApp *gjitenApp = NULL;


/***************** VARIABLES ***********************/

void init_old ();
gchar *clipboard_text = NULL;


enum {
  KANJIDIC_KEY        = -1,
  WORD_LOOKUP_KEY     = -2,
  KANJI_LOOKUP_KEY    = -3,
  CLIP_KANJI_KEY      = -4,
  CLIP_WORD_KEY       = -5,
  QUICK_LOOKUP_KEY    = -6
};



void
gjiten_init_cmd_params(GApplication *app, GjitenConfig *conf)
{
  const GOptionEntry cmd_params[] =
  {
    {
      .long_name = "version",
      .short_name = 'v',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &(conf->cli_option_show_version),
      .description = N_("Show version information."),
      .arg_description = NULL,
    },
    {
      .long_name = "kanjidic",
      .short_name = 'k',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &(conf->cli_option_startkanjidic),
      .description = N_("Start up Kanjidic instead of Word dictionary"),
      .arg_description = NULL,
    },
    {
      .long_name = "word-lookup",
      .short_name = 'w',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_STRING,
      .arg_data = &(conf->cli_option_word_to_lookup),
      .description = N_("Look up WORD in first dictionary"),
      .arg_description = N_("WORD")
    },
    {
      .long_name = "kanji-lookup",
      .short_name = 'l',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_STRING,
      .arg_data = &(conf->cli_option_kanji_to_lookup),
      .description = N_("Look up KANJI in kanji dictionary"),
      .arg_description = N_("KANJI")
    },
    {
      .long_name = "clip-kanji",
      .short_name = 'c',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &(conf->cli_option_clip_kanji_lookup),
      .description = N_("Look up kanji from clipboard"),
      .arg_description = NULL,
    },
    {
      .long_name = "clip-word",
      .short_name = 'v',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &(conf->cli_option_clip_word_lookup),
      .description = N_("Look up word from clipboard"),
      .arg_description = NULL,
    },
    {
      .long_name = "quick-lookup",
      .short_name = '\0',
      .flags = G_OPTION_FLAG_NONE,
      .arg = G_OPTION_ARG_NONE,
      .arg_data = &(conf->cli_option_quick_lookup_mode),
      .description = N_("Start in quick-lookup-mode: Terminate on Escape or clicking somewhere else."),
      .arg_description = NULL,
    },
    {NULL}
  };

  g_application_add_main_option_entries (G_APPLICATION (app), cmd_params);
}



/**
 * Cleanly close gjiten from anywhere in the code
**/
void
gjiten_quit_if_all_windows_closed()
{
  if ((gjitenApp->worddic == NULL) && (gjitenApp->kanjidic == NULL))
  {
    GJITEN_DEBUG ("gjiten_quit_if_all_windows_closed ()\n");
    gjitenconfig_save_options (gjitenApp->conf);
    dicutil_unload_dic ();
    gjitenconfig_free (gjitenApp->conf);

    GApplication * app = g_application_get_default ();
    g_application_quit (app);
  }
}



/**
 * Cleanly close gjiten from anywhere in the code
 **/
void
gjiten_quit(GSimpleAction *xa, GVariant *xb, void *xc)
{
  // Close all windows
  // (gjiten_quit_if_all_windows_closed () will be called inside there)
  kanjidic_close ();
  worddic_close ();
}



void
gjiten_start_kanjipad(GSimpleAction *xa, GVariant *xb, void *xc)
{
  FILE *kanjipad_binary;
  char *kpad_cmd;
  int32_t len;

  kanjipad_binary = fopen (gjitenApp->conf->kanjipad, "r");
  if (kanjipad_binary == NULL) {
    error_show (NULL,_("Couldn't find the KanjiPad executable!\n"
                         "Please make sure you have it installed on your system \n"
                         "and set the correct path to it in the Preferences.\n"
                         "See the Documentation for more details about KanjiPad."));
  }
  else {
    kpad_cmd = g_strconcat (gjitenApp->conf->kanjipad, "&", NULL);
    int unused = system (kpad_cmd);
    g_free (kpad_cmd);
  }
}



gboolean
gnome_help_display(const char *file_name,
                   const char *link_id,
                   GError **error)
{
  return FALSE;
}



void
gjiten_display_manual(GtkWidget *parent_window_nullable,
                      void      *data)
{
  GError *err = NULL;

  gboolean retval = FALSE;
  retval = gtk_show_uri_on_window ( GTK_WINDOW (parent_window_nullable),
                                    "ghelp:gjiten",
                                    GDK_CURRENT_TIME,
                                    &err);

  if (retval == FALSE)
  {
    char * message = _("(unknown)");
    if (err)
      message = err->message;

    error_show (GTK_WINDOW (parent_window_nullable),
                       _("Could not display help: %s"), message);

    if (err)
      g_error_free (err);
  }
}




static void
gjiten_show_whatsnew(GSimpleAction *xa, GVariant *xb, void *xc)
{
  GtkTextBuffer *textbuffer = gtk_text_buffer_new (NULL);

  GtkTextIter iter;
  gtk_text_buffer_get_start_iter (textbuffer,&iter);
  gtk_text_buffer_insert_markup (textbuffer,&iter,
  _("\n<span size='x-large'><b>What's New?</b></span>"
  "\n"
  "\n<span size='large'><u><i>Version 3.1</i></u></span>"
  "\n- New command line option `quick-lookup`. "
  "\n  With this option GJiten will close as soon as ESC is pressed"
  "\n  or the window focus is lost. It's designed for popup-like "
  "\n   dictionary lookups."
  "\n- New Icons"
  "\n- bug fixes"
  "\n"
  "\n<span size='large'><u><i>Version 3.0</i></u></span>"
  "\n- Technical: Settings are now stored under ~/.config/gjiten/gjiten.conf"
  "\n- Technical: Migration to GTK 3"
  "\n"
  "\n")
  ,-1);

  GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gj_window_set_icon_default (GTK_WINDOW (window));
  gtk_window_set_title (GTK_WINDOW (window), APPLICATION_NAME " - What's New?");
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 400);

  GtkWidget * box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add (GTK_CONTAINER (window), box);

  GtkWidget * scrolled = gtk_scrolled_window_new (NULL, NULL);
  gtk_box_pack_start (GTK_BOX (box), scrolled, TRUE, TRUE, 0);

  GtkWidget * view = gtk_text_view_new_with_buffer (textbuffer);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (view), FALSE);
  gtk_container_add (GTK_CONTAINER (scrolled), view);

  /*// close button
  {
    GtkWidget * hbox = gtk_button_box_new (GTK_ORIENTATION_HORIZONTAL);
    gtk_button_box_set_layout (GTK_BUTTON_BOX (hbox), GTK_BUTTONBOX_END);

    gtk_box_pack_end (GTK_BOX (box), hbox, FALSE, FALSE, 0);

    GtkWidget * btnClose = gtk_button_new_with_label ("Close");
    g_signal_connect_swapped (btnClose, "clicked", G_CALLBACK (gtk_widget_destroy), window);
    gtk_box_pack_end (GTK_BOX (hbox), btnClose, FALSE, FALSE, 0);
  }*/


  gtk_widget_show_all (window);
}




static void
gjiten_create_about(GSimpleAction *, GVariant *, void *)
{
  const gchar *authors[] = { "Botond Botyanszki <boti@rocketmail.com>, DarkTrick", NULL };
  const gchar *documenters[] = { NULL };
  const gchar *translator = _("TRANSLATORS! PUT YOUR NAME HERE");
  GdkPixbuf *pixbuf = NULL;

  pixbuf =  gdk_pixbuf_new_from_resource (RESOURCE_PATH "images/gjiten-logo.png",NULL);

  if (strncmp (translator, "translated_by", 13) == 0) translator = NULL;

  /*
    _("Released under the terms of the GNU GPL.\n"
  */
  {

    GtkAboutDialog * about = GTK_ABOUT_DIALOG (gtk_about_dialog_new ());
    gtk_about_dialog_set_program_name       (about, "gjiten");
    gtk_about_dialog_set_version            (about, VERSION);
    gtk_about_dialog_set_copyright          (about, "Copyright \xc2\xa9 1999-2005 Botond Botyanszki\nCopyright \xc2\xa9 2019-2021 DarkTrick");
    gtk_about_dialog_set_comments           (about, _("Gjiten is a Japanese dictionary."));
    gtk_about_dialog_set_authors            (about, (const char **)authors);
    gtk_about_dialog_set_documenters        (about, (const char **)documenters);
    gtk_about_dialog_set_translator_credits (about, (const char *)translator);
    gtk_about_dialog_set_logo               (about, pixbuf);

    gtk_window_set_destroy_with_parent (GTK_WINDOW (about), TRUE);
    if (pixbuf != NULL)  g_object_unref (pixbuf);

    g_signal_connect (G_OBJECT (about), "destroy", G_CALLBACK (gtk_widget_destroyed), &about);
    gtk_widget_show (GTK_WIDGET (about));
  }

}



static void
_create_submenu(const gchar *name,
                      GMenu *content,
                      GMenu *parent)
{
  GMenuItem * button = g_menu_item_new (name, "unused");
  g_menu_item_set_submenu (button, G_MENU_MODEL (content));
  g_menu_append_item (parent, button);
}



static void
_action_start_kanjipad(GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       gtk_application)
{
  gjiten_start_kanjidic (GTK_APPLICATION (gtk_application));
}



static void
_action_start_worddic(GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       gtk_application)
{
  gjiten_start_worddic (GTK_APPLICATION (gtk_application));
}




static void
_action_display_manual(GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       gtk_application)
{
  gjiten_display_manual (NULL, NULL);
}



static void
_gjiten_create_menu(GtkApplication *app)
{
  GMenu * menubar = g_menu_new ();

  // ---- create containers for menus ----

  {
    GMenu * content = g_menu_new ();
    g_menu_append (content, _("_Quit"), "app.quit");
    _create_submenu (_("_File"), content, menubar);

    gtk_application_set_accel_for_action (app, "app.quit", "<Ctrl>Q");
  }

  {
    GMenu * content = g_menu_new ();

    GMenu * section1 = g_menu_new ();
    GMenu * section2 = g_menu_new ();

    g_menu_append (section1, _("_Copy"),        "window.copy"     );
    g_menu_append (section1, _("_Paste"),       "window.paste"    );
    g_menu_append (section2, _("_Preferences"), "app.preferences" );

    g_menu_append_section (content, NULL, G_MENU_MODEL (section1));
    g_menu_append_section (content, NULL, G_MENU_MODEL (section2));

    _create_submenu (_("_Edit"), content, menubar);

    // set shortcuts
    gtk_application_set_accel_for_action (app, "window.copy", "<Ctrl>C");
    gtk_application_set_accel_for_action (app, "window.paste", "<Ctrl>V");
  }

  {
    GMenu * content = g_menu_new ();
    g_menu_append (content, _("_Word Dictionary"),  "app.startWorddic" );
    g_menu_append (content, _("_Kanji Dictionary"), "app.startKanjidic" );
    g_menu_append (content, _("Kanji _Pad"),        "app.startKanjipad" );
    _create_submenu (_("_Tools"), content, menubar);
  }

  {
    GMenu * content = g_menu_new ();

    GMenu * section1 = g_menu_new ();
    GMenu * section2 = g_menu_new ();

    g_menu_append (section1, _("_Manual"),     "app.showManual" );
    g_menu_append (section1, _("What's _New?"), "app.showWhatsNew" );
    g_menu_append (section2, _("_About"),      "app.showAbout" );

    g_menu_append_section (content, NULL, G_MENU_MODEL (section1));
    g_menu_append_section (content, NULL, G_MENU_MODEL (section2));

    _create_submenu (_("_Help"), content, menubar);
  }


  gtk_application_set_menubar (GTK_APPLICATION (g_application_get_default ()), G_MENU_MODEL (menubar));

  g_object_unref (menubar);

  // link action names (above) to callback functions
  {
    GActionEntry actions[] = {
        {.name="quit",          .activate=gjiten_quit },
        {.name="preferences",   .activate=create_dialog_preferences },
        {.name="startKanjipad", .activate=gjiten_start_kanjipad },
        {.name="showWhatsNew",     .activate=gjiten_show_whatsnew },
        {.name="showAbout",     .activate=gjiten_create_about },
      };

    g_action_map_add_action_entries (G_ACTION_MAP (app), actions, G_N_ELEMENTS (actions), NULL);
  }
  // separate processing for functions with parameters
  {
    GActionEntry actions[] = {
      {.name="startWorddic",  .activate=_action_start_worddic },
      {.name="startKanjidic", .activate=_action_start_kanjipad },
      {.name="showManual",    .activate=_action_display_manual },
    };

    g_action_map_add_action_entries (G_ACTION_MAP (app), actions, G_N_ELEMENTS (actions), app);
  }
}

void
_start_window (GtkWindow *window)
{
  if (gjitenApp->conf->cli_option_quick_lookup_mode)
    gj_enable_quick_lookup_mode (GTK_WINDOW (window));
}



void
gjiten_start_worddic(GtkApplication *app){

  if (gjitenApp->worddic != NULL) {
    gtk_window_present (GTK_WINDOW (gjitenApp->worddic));
  }

  GjWorddicWindow *window = worddic_create ();
  gjitenApp->worddic = window;
  _start_window (GTK_WINDOW (window));
}



void
gjiten_start_kanjidic(GtkApplication *app)
{
  if (gjitenApp->kanjidic != NULL) {
    gtk_window_present (GTK_WINDOW (gjitenApp->kanjidic));
  }

  GjKanjidicWindow *window = kanjidic_create ();
  gjitenApp->kanjidic = window;
  _start_window (GTK_WINDOW (window));
}



void
gjiten_start_kanjidic_with_search(gunichar kanji)
{
  GError * error = NULL;
  gchar * str_kanji = g_ucs4_to_utf8 (&kanji,1, NULL, NULL, &error);

  if (error)
    return;

  gjiten_start_kanjidic (NULL);
  kanjidic_lookup (str_kanji);
  g_free (str_kanji);
}



void
gjiten_apply_fonts(GjitenApp * gjitenApp)
{
  // apply css styles
  if (gjitenApp->conf->normalfont != NULL &&
      !g_str_equal (gjitenApp->conf->normalfont, ""))
  {
    gchar * css_font = g_pango_font_convert_to_css (gjitenApp->conf->normalfont);
    GString * css = g_string_new ("");
    g_string_printf (css, "font: %s;", css_font);
    set_global_css ("normalfont", css->str);
    g_string_free (css, TRUE);
    g_free (css_font);
  }

  // apply tag styles
  worddic_apply_fonts ();
  kanjidic_apply_fonts ();
}




void
_init_resources()
{
  g_resources_register (resources_get_resource ());

  // enable icons
  GtkIconTheme * icon_theme = gtk_icon_theme_get_default ();
  gtk_icon_theme_add_resource_path (icon_theme, RESOURCE_PATH "icons/scalable/actions");

  // enable css
  GtkCssProvider * css = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css,RESOURCE_PATH "css/styles.css");
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                               GTK_STYLE_PROVIDER(css),
                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}



void
_try_open_kanjidic_and_search (GtkApplication *app,
                               const char *text)
{


  // validate
  if (gx_utf8_validate (text, -1, NULL) == FALSE)
  {
    // TODO: try to convert EUC-JP to UTF8 if it's non-utf8
    error_show (NULL,_("Cannot look up kanji: \n"
                          "Non-UTF8 string received."));
    return;
  }

  if (gchar_isKanjiChar (text) == FALSE)
  {
    error_show (NULL,_("Non-kanji string received:\n \"%s\"\n"), text);
    return;
  }

  gjiten_start_kanjidic (app);
  kanjidic_lookup (text);
}



void
gjiten_activate(GtkApplication *app,
                gpointer        user_data)
{
  if (TRUE == gjitenApp->conf->cli_option_show_version)
  {
    g_print (PACKAGE_STRING "\n");
    return;
  }

  _init_resources();
  _gjiten_create_menu (GTK_APPLICATION (app));
  gjiten_apply_fonts (gjitenApp);

  // the following is for clipboard lookup.
  if (TRUE == gjitenApp->conf->cli_option_clip_word_lookup)
  {
    gjiten_start_worddic (app);
    worddic_paste ();
    on_search_clicked ();
    return;
  }

  if (TRUE == gjitenApp->conf->cli_option_clip_kanji_lookup)
  {
    clipboard_text = gtk_clipboard_wait_for_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY));
    _try_open_kanjidic_and_search (app, clipboard_text);
    return;
  }

  if (gjitenApp->conf->cli_option_startkanjidic)
  {
    gjiten_start_kanjidic (app);
    return;
  }

  if (gjitenApp->conf->cli_option_word_to_lookup)
  {
    gjiten_start_worddic (app);
    worddic_lookup_word (gjitenApp->conf->cli_option_word_to_lookup);
    return;
  }


  if (gjitenApp->conf->cli_option_kanji_to_lookup != NULL)
  {
    const char * kanji = gjitenApp->conf->cli_option_kanji_to_lookup;
    _try_open_kanjidic_and_search (app, kanji);
    return;
  }



  gjiten_start_worddic (app);
  return;
}



GtkApplication *
gjiten_new()
{
  gjitenApp = g_new0(GjitenApp, 1);
  gjitenApp->conf = gjitenconfig_new_and_init ();

  if (gjitenApp->conf->envvar_override == TRUE) {
    if (gjitenApp->conf->gdk_use_xft == TRUE) putenv ("GDK_USE_XFT=1");
    else putenv ("GDK_USE_XFT=0");
    // if (gjitenApp->conf->force_ja_JP == TRUE) putenv ("LC_CTYPE=ja_JP");
    if (gjitenApp->conf->force_ja_JP == TRUE) putenv ("LC_ALL=ja_JP");
    if (gjitenApp->conf->force_language_c == TRUE) putenv ("LANGUAGE=C");
  }



  #ifdef ENABLE_NLS
    bindtextdomain (PACKAGE, PACKAGE_LOCALE_DIR);
    bind_textdomain_codeset (PACKAGE, "UTF-8");
    textdomain (PACKAGE);
  #endif


  GtkApplication * app = gtk_application_new (NULL,G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate", G_CALLBACK (gjiten_activate), NULL);


  gjiten_init_cmd_params (G_APPLICATION (app), gjitenApp->conf);

  return app;
}


