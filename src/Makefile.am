AUTOMAKE_OPTIONS = subdir-objects

bin_PROGRAMS = gummi
AM_CFLAGS = $(GUI_CFLAGS) \
	    -Wl,-export-dynamic -Wall -O2 \
	    -DGUMMI_LIBS=\"$(libdir)/$(PACKAGE)\" \
	    -DGUMMI_DATA=\"$(datadir)/$(PACKAGE)\" \
	    -DGUMMI_LOCALES=\"$(datadir)/locale\"

AM_VALAFLAGS = -H completion.h --pkg gee-0.8 --pkg gtk+-3.0 --pkg gtksourceview-3.0

gummi_LDADD = $(GUI_LIBS) \
	      $(LIBINTL) -lgthread-2.0

gummi_SOURCES = completion.vala \
		biblio.c  biblio.h \
		configfile.c configfile.h \
		editor.c editor.h \
		environment.c environment.h \
		compile/texlive.c compile/texlive.h \
		compile/latexmk.c compile/latexmk.h \
		compile/rubber.c compile/rubber.h \
		gui/gui-menu.c gui/gui-menu.h \
		gui/gui-tabmanager.c gui/gui-tabmanager.h \
		gui/gui-import.c gui/gui-import.h \
		gui/gui-main.c gui/gui-main.h \
		gui/gui-prefs.c gui/gui-prefs.h \
		gui/gui-preview.c gui/gui-preview.h \
		gui/gui-search.c gui/gui-search.h \
		gui/gui-snippets.c gui/gui-snippets.h \
		gui/gui-infoscreen.c gui/gui-infoscreen.h \
		gui/gui-project.c gui/gui-project.h \
		importer.c importer.h \
		iofunctions.c iofunctions.h \
		external.c external.h \
		project.c project.h \
		latex.c latex.h \
		motion.c motion.h \
		signals.c signals.h \
		snippets.c snippets.h \
		template.c template.h \
		utils.c utils.h \
		tabmanager.c tabmanager.h \
		constants.h \
		main.c
