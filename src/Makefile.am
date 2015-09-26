AM_CFLAGS = $(APPLET_CFLAGS) -DMATELOCALEDIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

APPLET_SOURCES = mate-xfce4-panel-plugin-loader-applet.c
APPLET_LDADD = $(APPLET_LIBS) $(DL_LIBS)
APPLET_LOCATION = $(libexecdir)/mate-xfce4-panel-plugin-loader-applet

libexec_PROGRAMS = mate-xfce4-panel-plugin-loader-applet
mate_xfce4_panel_plugin_loader_applet_SOURCES = $(APPLET_SOURCES)
mate_xfce4_panel_plugin_loader_applet_LDADD = $(APPLET_LDADD)
mate_xfce4_panel_plugin_loader_applet_CFLAGS = $(AM_CFLAGS)

appletsdir       = $(datadir)/mate-panel/applets
applets_in_files = org.mate.Xfce4PanelPluginLoader.Applet.mate-panel-applet.in
applets_DATA     = $(applets_in_files:.mate-panel-applet.in=.mate-panel-applet)

$(applets_in_files): $(applets_in_files).in
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@

servicedir       = $(datadir)/dbus-1/services
service_in_files = org.mate.panel.applet.Xfce4PanelPluginLoaderFactory.service.in
service_DATA     = $(service_in_files:.service.in=.service)

org.mate.panel.applet.Xfce4PanelPluginLoaderFactory.service: $(service_in_files)
	$(AM_V_GEN)sed \
		-e "s|\@LOCATION\@|$(APPLET_LOCATION)|" \
		$< > $@
%.mate-panel-applet: %.mate-panel-applet.in $(INTLTOOL_MERGE) $(wildcard $(top_srcdir)/po/*po) ; $(INTLTOOL_MERGE) $(top_srcdir)/po $< $@ -d -u -c $(top_builddir)/po/.intltool-merge-cache

gschemas_in_files = org.mate.panel.applet.Xfce4PanelPluginLoader.gschema.xml.in
gsettings_SCHEMAS = $(gschemas_in_files:.xml.in=.xml)
@INTLTOOL_XML_NOMERGE_RULE@
@GSETTINGS_RULES@

EXTRA_DIST = \
	$(applets_in_files) \
	$(service_in_files) \
	$(gschemas_in_files)

CLEANFILES = $(applets_DATA) $(applets_DATA).in $(service_DATA) $(gsettings_SCHEMAS)
