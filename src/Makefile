############################################################################
# NOTE: Releases use autotools, this makefile is for development purposes. #
############################################################################

TARGET=gummi

OBJS = main.o gui/gui-main.o gui/gui-prefs.o gui/gui-menu.o gui/gui-search.o gui/gui-import.o gui/gui-preview.o gui/gui-tabmanager.o gui/gui-project.o gui/gui-snippets.o gui/gui-infoscreen.o compile/texlive.o compile/rubber.o compile/latexmk.o motion.o external.o latex.o editor.o utils.o configfile.o iofunctions.o environment.o project.o importer.o tabmanager.o template.o biblio.o snippets.o signals.o


CFLAGS=-g -Wall -Wno-deprecated-declarations -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGSEAL_ENABLE -export-dynamic -I. `pkg-config --cflags --libs gtk+-3.0 gthread-2.0 gtksourceview-3.0 cairo poppler-glib gtkspell3-3.0 synctex zlib` -lm -DUSE_SYNCTEX2 -DGUMMI_LOCALES="\"/usr/share/locale\"" -DGUMMI_DATA="\"$$PWD/../data\"" -DGUMMI_LIBS="\"$$PWD/../lib\""

VALAFLAGS=-g -X -O0 -X -DGUMMI_DATA="\"$$PWD/../data\"" --pkg gee-0.8 --pkg gtk+-3.0 --pkg gtksourceview-3.0

gummi: completion $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) completion.o $(CFLAGS)

completion:
	valac completion.vala -H completion.h -c $(VALAFLAGS)
	@mv completion.vala.o completion.o

clean:
	rm -f $(TARGET) $(OBJS)
