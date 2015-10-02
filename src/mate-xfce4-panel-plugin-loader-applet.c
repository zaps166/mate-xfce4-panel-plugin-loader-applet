/* mate-xfce4-panel-plugin-loader-applet.c:
 *
 * Copyright (C) 2015 Błażej Szczygieł <spaz16@wp.pl>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

/* Commom includes */
#include <config.h>
#include <glib/gi18n.h>
#include <sys/stat.h>
#include <dlfcn.h>

/* Xfce4 include */
#include <libxfce4panel/libxfce4panel.h>

/* Mate include */
#include <mate-panel-applet-gsettings.h>
#include <mate-panel-applet.h>

#define PANEL_SETTINGS_SCHEMA "org.mate.panel.applet.Xfce4PanelPluginLoader"
#define PANEL_SETTINGS_KEY_FILENAME_SCHEMA "filename"
#define XFCE4_ICON "xfce4-logo"

/* Xfce4 plugin private struct from "xfce4-panel" source code */
struct _XfcePanelPluginPrivate
{
	/* plugin information */
	gchar               *name;
	gchar               *display_name;
	gchar               *comment;
	gint                 unique_id;
	gchar               *property_base;
	gchar              **arguments;
	gint                 size; /* single row size */
	guint                expand : 1;
	guint                shrink : 1;
	guint                nrows;
	XfcePanelPluginMode  mode;
	guint                small : 1;
	XfceScreenPosition   screen_position;
	guint                locked : 1;
	GSList              *menu_items;

	/* flags for rembering states */
	gint          flags; /* PluginFlags */

	/* plugin right-click menu */
	GtkMenu             *menu;

	/* menu block counter (configure insensitive) */
	gint                 menu_blocked;

	/* autohide block counter */
	gint                 panel_lock;
};

/* Mate panel private struct from "mate-panel" source code */
struct _MatePanelAppletPrivate
{
	GtkWidget         *plug;
	GtkWidget         *applet;
	GDBusConnection   *connection;

	char              *id;
	GClosure          *closure;
	char              *object_path;
	guint              object_id;
	char              *prefs_path;

	GtkUIManager      *ui_manager;
	GtkActionGroup    *applet_action_group;
	GtkActionGroup    *panel_action_group;

	MatePanelAppletFlags   flags;
	MatePanelAppletOrient  orient;
	guint              size;
	char              *background;
	GtkWidget         *background_widget;

	int                previous_width;
	int                previous_height;

	int               *size_hints;
	int                size_hints_len;

	gboolean           moving_focus_out;

	gboolean           locked;
	gboolean           locked_down;
};

/* Global array */
static int num_applets = 0;
static gchar **xfce4_panel_plugin_filenames = NULL;

/* Structure */
typedef struct
{
	GSettings *settings;
	GtkWidget *button;
	int lib_name_pos;
	MatePanelApplet *mate_applet;
	XfcePanelPlugin *xfce_panel_plugin;
} Xfce4PanelPluginLoader;

/* Signal handlers from Mate panel */
static void xfce4_panel_plugin_loader_handle_orient_change(gpointer mate_applet, MatePanelAppletOrient orient, XfcePanelPluginProvider *xfce_panel_plugin)
{
	(void)mate_applet;
	static XfcePanelPluginMode mate_orient_to_xfce_position[] =
	{
		XFCE_PANEL_PLUGIN_MODE_HORIZONTAL,
		XFCE_PANEL_PLUGIN_MODE_HORIZONTAL,
		XFCE_PANEL_PLUGIN_MODE_VERTICAL,
		XFCE_PANEL_PLUGIN_MODE_VERTICAL
	};
	xfce_panel_plugin_provider_set_mode(xfce_panel_plugin, mate_orient_to_xfce_position[orient]);

}
static void xfce4_panel_plugin_loader_handle_size_change(gpointer mate_applet, gint size, Xfce4PanelPluginLoader *xfce4_panel_plugin_loader)
{
	(void)mate_applet;
	if (xfce4_panel_plugin_loader->xfce_panel_plugin)
		xfce_panel_plugin_provider_set_size((XfcePanelPluginProvider *)xfce4_panel_plugin_loader->xfce_panel_plugin, size);
	else if (xfce4_panel_plugin_loader->button)
	{
		GtkWidget *image = gtk_image_new();
		GdkPixbuf *icon = xfce_panel_pixbuf_from_source_at_size(XFCE4_ICON, gtk_icon_theme_get_default(), size, size);
		gtk_image_set_from_pixbuf(GTK_IMAGE(image), icon);
		g_object_unref(G_OBJECT(icon));
		gtk_button_set_image(GTK_BUTTON(xfce4_panel_plugin_loader->button), image);
	}
}
static void xfce4_panel_plugin_loader_handle_dispose(MatePanelApplet *mate_applet, Xfce4PanelPluginLoader *xfce4_panel_plugin_loader)
{
	(void)mate_applet;
	if (xfce4_panel_plugin_loader->xfce_panel_plugin)
	{
		/* Free information about the loaded library */
		g_free(xfce4_panel_plugin_filenames[xfce4_panel_plugin_loader->lib_name_pos]);
		xfce4_panel_plugin_filenames[xfce4_panel_plugin_loader->lib_name_pos] = NULL;

		/* "xfce4_panel_plugin_loader->xfce_panel_plugin" - automatically destroys itself with "mate_applet" */
		xfce4_panel_plugin_loader->xfce_panel_plugin = NULL;
	}
	/* "lib_handle" - unloading libraries is dangerous and causes crashes */
	if (xfce4_panel_plugin_loader->button)
		gtk_widget_destroy(GTK_WIDGET(xfce4_panel_plugin_loader->button));
	if (xfce4_panel_plugin_loader->settings)
		g_object_unref(xfce4_panel_plugin_loader->settings);
	g_free(xfce4_panel_plugin_loader);

	/* Free global array if can */
	gboolean can_free = TRUE;
	int i;
	for (i = 0; i < num_applets; ++i)
	{
		if (xfce4_panel_plugin_filenames[i] != NULL)
		{
			can_free = FALSE;
			break;
		}
	}
	if (can_free)
	{
		g_free(xfce4_panel_plugin_filenames);
		xfce4_panel_plugin_filenames = NULL;
	}
}

/* Helper function for creating menu separator */
static GtkWidget *create_separator()
{
	GtkWidget *separator = gtk_separator_menu_item_new();
	gtk_widget_show(separator);
	return separator;
}

/* Helper function to obtain library file name */
static gchar *get_lib_file_name()
{
	const gchar *base_dir = NULL;
	gchar *file_name = NULL;

	/* Find Xfce4 panel plugins directory */
	static const gchar base_dirs[][37] =
	{
#if defined(__x86_64__) || defined(_M_X64) //What about AArch64 and PPC64?
		"/usr/lib64/xfce4/panel/plugins",
		"/usr/local/lib64/xfce4/panel/plugins",
#endif
		"/usr/lib/xfce4/panel/plugins",
		"/usr/local/lib/xfce4/panel/plugins",
	};
	struct stat sb;
	if (!stat(base_dirs[0], &sb) && S_ISDIR(sb.st_mode))
		base_dir = base_dirs[0];
	else if (!stat(base_dirs[1], &sb) && S_ISDIR(sb.st_mode))
		base_dir = base_dirs[1];
#if defined(__x86_64__) || defined(_M_X64) //Some distros (like Arch Linux) have 64-bit libs in "lib" instead of "lib64" and sometimes symlink to "lib->lib64" may not exists.
	else if (!stat(base_dirs[2], &sb) && S_ISDIR(sb.st_mode))
		base_dir = base_dirs[2];
	else if (!stat(base_dirs[3], &sb) && S_ISDIR(sb.st_mode))
		base_dir = base_dirs[3];
#endif

	if (!base_dir)
		base_dir = "/usr";

	/* Show file dialog to choose the library */
	GtkWidget *file_dialog = gtk_file_chooser_dialog_new(_("Open Xfce4 panel plugin library"), NULL, GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_dialog), base_dir);

	GtkFileFilter *filter = gtk_file_filter_new();
	gtk_file_filter_add_pattern(filter, "lib*.so");
	gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_dialog), filter);

	gint res = gtk_dialog_run(GTK_DIALOG(file_dialog));
	if (res == GTK_RESPONSE_ACCEPT)
		file_name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(file_dialog));
	gtk_widget_destroy(file_dialog);

	return file_name;
}

/* Helper function to show message dialogs */
static void show_message(const gchar *lib_name, const gchar *info_msg)
{
	gchar *msg = (gchar *)g_malloc(strlen(lib_name) + 2 + strlen(info_msg) + 1);
	strcpy(msg, lib_name);
	strcat(msg, ": ");
	strcat(msg, info_msg);

	GtkWidget *dlg = gtk_message_dialog_new(NULL, 0, GTK_MESSAGE_INFO, GTK_BUTTONS_OK, msg);
	gtk_dialog_run(GTK_DIALOG(dlg));
	gtk_widget_destroy(dlg);

	g_free(msg);
}

/* Load Xfce4 applet */
static XfcePanelPlugin *get_xfce_panel_plugin(const gchar *file_name)
{
	void *handle = dlopen(file_name, RTLD_LOCAL | RTLD_LAZY);
	if (!handle)
		show_message(file_name, _("cannot load the library!"));
	else
	{
		PluginConstructFunc construct_func = dlsym(handle, "xfce_panel_module_construct");
		if (!construct_func)
		{
			if (dlsym(handle, "xfce_panel_module_init"))
				show_message(file_name, _("this plugin is not supported yet!"));
			else
				show_message(file_name, _("is not Xfce4 panel plugin library!"));
		}
		else
		{
			const gchar *lib_name = strrchr(file_name, '/');
			size_t lib_name_len = strlen(lib_name);
			if (lib_name_len > 7)
			{
				lib_name += 4; //"/lib"
				lib_name_len -= 4;
				gchar *name = g_malloc(lib_name_len-3+1); //Discard ".so"
				memcpy(name, lib_name, lib_name_len-3);
				name[lib_name_len-3] = '\0';
				XfcePanelPlugin *xfce_panel_plugin = (XfcePanelPlugin *)construct_func(name, 0, NULL, NULL, NULL, gdk_screen_get_default());
				g_free(name);
				return xfce_panel_plugin;
			}
		}
		dlclose(handle);
	}
	return NULL;
}
static gboolean load_xfce4_panel_plugin(Xfce4PanelPluginLoader *xfce4_panel_plugin_loader, gchar *lib_file_name)
{
	/* Extract library file name from path */
	const gchar *lib_name = strrchr(lib_file_name, '/');
	if (lib_name)
		++lib_name;
	else
		lib_name = lib_file_name;

	/* Check whether Xfce4 panel plugin is already loaded */
	int i;
	for (i = 0; i < num_applets; ++i)
	{
		if (xfce4_panel_plugin_filenames[i] == NULL)
			continue;
		if (!strcmp(xfce4_panel_plugin_filenames[i], lib_name))
		{
			show_message(lib_name, _("the library is already loaded!"));
			return FALSE;
		}
	}

	/* Load Xfce4 panel plugin library */
	XfcePanelPlugin *xfce_panel_plugin = get_xfce_panel_plugin(lib_file_name);
	if (!xfce_panel_plugin)
	{
		g_settings_set_string(xfce4_panel_plugin_loader->settings, PANEL_SETTINGS_KEY_FILENAME_SCHEMA, "");
		return FALSE;
	}

	/* Save data */
	xfce4_panel_plugin_loader->xfce_panel_plugin = xfce_panel_plugin;
	g_settings_set_string(xfce4_panel_plugin_loader->settings, PANEL_SETTINGS_KEY_FILENAME_SCHEMA, lib_file_name);

	/* Delete button */
	if (xfce4_panel_plugin_loader->button)
	{
		gtk_widget_destroy(GTK_WIDGET(xfce4_panel_plugin_loader->button));
		xfce4_panel_plugin_loader->button = NULL;
	}

	/* Show */
	gtk_widget_set_can_focus(GTK_WIDGET(xfce_panel_plugin), TRUE); //If this property is "FALSE", there is 1px border
	gtk_container_add(GTK_CONTAINER(xfce4_panel_plugin_loader->mate_applet), GTK_WIDGET(xfce_panel_plugin));
	gtk_widget_show(GTK_WIDGET(xfce_panel_plugin));
	gtk_widget_show(GTK_WIDGET(xfce4_panel_plugin_loader->mate_applet));

	/*
	 * Manually create menu object, because I must manage all items by my-self.
	 * I must merge Xfce4 and Mate menus here.
	*/

	/* Simulate an unlocked panel (for "show_configure") */
	xfce_panel_plugin_provider_set_locked((XfcePanelPluginProvider *)xfce_panel_plugin, FALSE);

	/* Create a new menu */
	xfce_panel_plugin->priv->menu = GTK_MENU(gtk_menu_new());
	gtk_menu_attach_to_widget(xfce_panel_plugin->priv->menu, GTK_WIDGET(xfce_panel_plugin), NULL);

	/* Add "configure" item to the menu */
	if (xfce_panel_plugin_provider_get_show_configure((XfcePanelPluginProvider *)xfce_panel_plugin))
	{
		GtkWidget *item = gtk_image_menu_item_new_from_stock(GTK_STOCK_PROPERTIES, NULL);
		g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(xfce_panel_plugin_provider_show_configure), xfce_panel_plugin);
		gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), item);
		gtk_widget_show(item);
	}

	/*
	 * Don't add "about" item to the menu, because it is not wotking properly when more then one Xfce4 panel plugin is loaded
	*/

	/* Add other plugin items to the menu */
	GSList *menu_items = xfce_panel_plugin->priv->menu_items;
	while (menu_items)
	{
		gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), menu_items->data);
		menu_items = menu_items->next;
	}

	/* Add menu items from Mate panel */
	gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), create_separator());
	gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), gtk_action_create_menu_item(gtk_action_group_get_action(xfce4_panel_plugin_loader->mate_applet->priv->panel_action_group, "Remove")));
	gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), gtk_action_create_menu_item(gtk_action_group_get_action(xfce4_panel_plugin_loader->mate_applet->priv->panel_action_group, "Move")));
	gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), create_separator());
	gtk_menu_shell_append(GTK_MENU_SHELL(xfce_panel_plugin->priv->menu), gtk_action_create_menu_item(gtk_action_group_get_action(xfce4_panel_plugin_loader->mate_applet->priv->panel_action_group, "Lock")));


	/* Handle change orientation */
	xfce4_panel_plugin_loader_handle_orient_change(NULL, mate_panel_applet_get_orient(xfce4_panel_plugin_loader->mate_applet), (XfcePanelPluginProvider *)xfce_panel_plugin);
	g_signal_connect(G_OBJECT(xfce4_panel_plugin_loader->mate_applet), "change_orient", G_CALLBACK(xfce4_panel_plugin_loader_handle_orient_change), xfce_panel_plugin);

	/* Set size */
	xfce4_panel_plugin_loader_handle_size_change(NULL, mate_panel_applet_get_size(xfce4_panel_plugin_loader->mate_applet), xfce4_panel_plugin_loader);

	/* Reset translations (Xfce4 panel plugin probably set it to its own package) */
	textdomain(GETTEXT_PACKAGE);

	/* Add to global array that the library is loaded */
	xfce4_panel_plugin_filenames = (gchar **)g_realloc(xfce4_panel_plugin_filenames, ++num_applets * sizeof(void *) );
	xfce4_panel_plugin_filenames[num_applets-1] = (gchar *)g_malloc(strlen(lib_name)+1);
	strcpy(xfce4_panel_plugin_filenames[num_applets-1], lib_name);
	xfce4_panel_plugin_loader->lib_name_pos = num_applets-1;

	return TRUE;
}

/* Helper function to create file dialog button */
static void file_dialog_button_clicked(GtkWidget *button, Xfce4PanelPluginLoader *xfce4_panel_plugin_loader)
{
	(void)button;
	gtk_widget_set_sensitive(GTK_WIDGET(xfce4_panel_plugin_loader->button), FALSE);
	gchar *lib_file_name = get_lib_file_name();
	if (lib_file_name && *lib_file_name)
		load_xfce4_panel_plugin(xfce4_panel_plugin_loader, lib_file_name);
	gtk_widget_set_sensitive(GTK_WIDGET(xfce4_panel_plugin_loader->button), TRUE);
}
static gboolean do_not_eat_button_press(GtkWidget *widget, GdkEventButton *event)
{
	if (event->button != 1)
		g_signal_stop_emission_by_name(widget, "button_press_event");
	return FALSE;
}
static void create_file_dialog_button(Xfce4PanelPluginLoader *xfce4_panel_plugin_loader)
{
	xfce4_panel_plugin_loader->button = gtk_button_new();
	gtk_button_set_relief(GTK_BUTTON(xfce4_panel_plugin_loader->button), GTK_RELIEF_NONE);
	xfce4_panel_plugin_loader_handle_size_change(NULL, mate_panel_applet_get_size(xfce4_panel_plugin_loader->mate_applet), xfce4_panel_plugin_loader);
	gtk_widget_set_tooltip_text(GTK_WIDGET(xfce4_panel_plugin_loader->button), _("Click here to choose Xfce4 panel plugin"));
	g_signal_connect(xfce4_panel_plugin_loader->button, "button_press_event", G_CALLBACK(do_not_eat_button_press), NULL);
	g_signal_connect(G_OBJECT(xfce4_panel_plugin_loader->button), "clicked", G_CALLBACK(file_dialog_button_clicked), xfce4_panel_plugin_loader);
	gtk_container_add(GTK_CONTAINER(xfce4_panel_plugin_loader->mate_applet), GTK_WIDGET(xfce4_panel_plugin_loader->button));
	gtk_widget_show_all(GTK_WIDGET(xfce4_panel_plugin_loader->mate_applet));
}

/* Helper function to set environmental variables */
static FILE *open_proc_environ(const char *pname_to_find)
{
	/* Open "/proc" directory */
	DIR *dirp = opendir("/proc");
	if (!dirp)
		return NULL;

	/* Scan "/proc" dir */
	struct dirent *dp;
	while ((dp = readdir(dirp)) != NULL)
	{
		/* Convert dir name to number */
		char *end_ptr;
		pid_t pid = strtol(dp->d_name, &end_ptr, 10);
		/* Directory name is not entirely numeric */
		if (*end_ptr != '\0')
			continue;

		/* Open the "stat" file */
		char pth[32];
		snprintf(pth, sizeof(pth), "/proc/%d/comm", pid);
		FILE *f = fopen(pth, "r");
		if (f)
		{
			/* Get process name from file and check whether is the same as in function argument */
			char pname[256];
			if ((fscanf(f, "%255s", pname)) == 1 && !strcmp(pname, pname_to_find))
			{
				fclose(f);
				closedir(dirp);
				/* Open "environ" file */
				snprintf(pth, sizeof pth, "/proc/%d/environ", pid);
				return fopen(pth, "rb");
			}
			fclose(f);
		}
	}

	closedir(dirp);
	return NULL;
}
static void set_environmental_variables_from_mate_panel()
{
	FILE *f = open_proc_environ("mate-panel");
	if (f)
	{
		int env_buff_size = 0, env_buff_pos = 0;
		char *env_buff = NULL;
		for (;;)
		{
			/* Read one byte */
			int c = fgetc(f);
			/* EOF or error */
			if (c < 0)
				break;
			/* Expand buffer */
			if (env_buff_pos == env_buff_size)
				env_buff = (char *)realloc(env_buff, ++env_buff_size);
			/* Append byte to buffer */
			env_buff[env_buff_pos++] = c;
			/* End of single entry */
			if (c == '\0')
			{
				/* Allocate more memroy than necessary */
				char *var = (char *)malloc(env_buff_pos);
				char *val = (char *)malloc(env_buff_pos);
				/* Get variable name and its value */
				sscanf(env_buff, "%[^=]=%s", var, val);
				/* Set env */
				setenv(var, val, 1);
				/* Free allocated memory */
				free(var);
				free(val);
				/* Reset buffer position */
				env_buff_pos = 0;
			}
		}
		free(env_buff);
		fclose(f);
	}
}

/* Load applet */
static gboolean xfce4_panel_plugin_loader_factory(MatePanelApplet *mate_applet, const gchar *iid, gpointer data)
{
	(void)data;
	if (!g_strcmp0(iid, "Xfce4PanelPluginLoader"))
	{
		Xfce4PanelPluginLoader *xfce4_panel_plugin_loader = (Xfce4PanelPluginLoader *)g_malloc0(sizeof(Xfce4PanelPluginLoader));
		xfce4_panel_plugin_loader->settings = mate_panel_applet_settings_new(mate_applet, PANEL_SETTINGS_SCHEMA);
		xfce4_panel_plugin_loader->mate_applet = mate_applet;

		/* Handle destroy signal */
		g_signal_connect(G_OBJECT(mate_applet), "destroy", G_CALLBACK(xfce4_panel_plugin_loader_handle_dispose), xfce4_panel_plugin_loader);

		/* Handle change size */
		g_signal_connect(G_OBJECT(xfce4_panel_plugin_loader->mate_applet), "change_size", G_CALLBACK(xfce4_panel_plugin_loader_handle_size_change), xfce4_panel_plugin_loader);

		/* Set Mate applet options */
		mate_panel_applet_set_flags(mate_applet, MATE_PANEL_APPLET_EXPAND_MINOR);
		mate_panel_applet_set_background_widget(mate_applet, GTK_WIDGET(mate_applet));

		/* Set environmental variables from "mate-panel" (Linux only), call only once */
		if (num_applets == 0)
			set_environmental_variables_from_mate_panel();

		/* Get library file name from settings */
		gchar *lib_file_name = g_settings_get_string(xfce4_panel_plugin_loader->settings, PANEL_SETTINGS_KEY_FILENAME_SCHEMA);

		/* Create xfce4 applet or button to load it later */
		if (!lib_file_name || !*lib_file_name || !load_xfce4_panel_plugin(xfce4_panel_plugin_loader, lib_file_name))
			create_file_dialog_button(xfce4_panel_plugin_loader);

		if (lib_file_name)
			g_free(lib_file_name);

		gtk_window_set_default_icon_name(XFCE4_ICON);

		return TRUE;
	}
	return FALSE;
}

MATE_PANEL_APPLET_OUT_PROCESS_FACTORY("Xfce4PanelPluginLoaderFactory", PANEL_TYPE_APPLET, "Xfce4 panel plugin loader", xfce4_panel_plugin_loader_factory, NULL)
