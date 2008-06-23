/* LIBGIMP - The GIMP Library
 * Copyright (C) 1995-1997 Peter Mattis and Spencer Kimball
 *
 * gimpstock.c
 * Copyright (C) 2001 Michael Natterer <mitch@gimp.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include "gnubgstock.h"
#include "pixmaps/gnubg-stock-pixbufs.h"

static GtkIconFactory *gnubg_stock_factory = NULL;

static void icon_set_from_inline(GtkIconSet * set,
		     const guchar * inline_data, GtkIconSize size, GtkTextDirection direction, gboolean fallback)
{
	GtkIconSource *source;
	GdkPixbuf *pixbuf;

	source = gtk_icon_source_new();

	if (direction != GTK_TEXT_DIR_NONE) {
		gtk_icon_source_set_direction(source, direction);
		gtk_icon_source_set_direction_wildcarded(source, FALSE);
	}

	gtk_icon_source_set_size(source, size);
	gtk_icon_source_set_size_wildcarded(source, FALSE);

	pixbuf = gdk_pixbuf_new_from_inline(-1, inline_data, FALSE, NULL);

	g_assert(pixbuf);

	gtk_icon_source_set_pixbuf(source, pixbuf);

	g_object_unref(pixbuf);

	gtk_icon_set_add_source(set, source);

	if (fallback) {
		gtk_icon_source_set_size_wildcarded(source, TRUE);
		gtk_icon_set_add_source(set, source);
	}

	gtk_icon_source_free(source);
}

static void add_sized_with_same_fallback(GtkIconFactory * factory,
			     const guchar * inline_data,
			     const guchar * inline_data_rtl, GtkIconSize size, const gchar * stock_id)
{
	GtkIconSet *set;
	gboolean fallback = FALSE;

	set = gtk_icon_factory_lookup(factory, stock_id);

	if (!set) {
		set = gtk_icon_set_new();
		gtk_icon_factory_add(factory, stock_id, set);
		gtk_icon_set_unref(set);

		fallback = TRUE;
	}

	icon_set_from_inline(set, inline_data, size, GTK_TEXT_DIR_NONE, fallback);

	if (inline_data_rtl)
		icon_set_from_inline(set, inline_data_rtl, size, GTK_TEXT_DIR_RTL, fallback);
}

static const GtkStockItem gnubg_stock_items[] = {
	{GNUBG_STOCK_ACCEPT, N_("_Accept"), 0, 0, NULL},
	{GNUBG_STOCK_REJECT, N_("_Reject"), 0, 0, NULL},
	{GNUBG_STOCK_HINT, N_("_Hint"), 0, 0, NULL},
	{GNUBG_STOCK_DOUBLE, N_("_Double"), 0, 0, NULL},
};

static const struct {
	const gchar *stock_id;
	gconstpointer ltr;
	gconstpointer rtl;
	GtkIconSize size;
} gnubg_stock_pixbufs[] = {
	{
	GNUBG_STOCK_ACCEPT, ok_16, NULL, GTK_ICON_SIZE_MENU}, {
	GNUBG_STOCK_ACCEPT, ok_24, NULL, GTK_ICON_SIZE_LARGE_TOOLBAR}, {
	GNUBG_STOCK_REJECT, cancel_16, NULL, GTK_ICON_SIZE_MENU}, {
	GNUBG_STOCK_REJECT, cancel_24, NULL, GTK_ICON_SIZE_LARGE_TOOLBAR}, {
	GNUBG_STOCK_HINT, hint_16, NULL, GTK_ICON_SIZE_MENU}, {
	GNUBG_STOCK_HINT, hint_24, NULL, GTK_ICON_SIZE_LARGE_TOOLBAR}, {
	GNUBG_STOCK_DOUBLE, double_16, NULL, GTK_ICON_SIZE_MENU}, {
	GNUBG_STOCK_DOUBLE, double_24, NULL, GTK_ICON_SIZE_LARGE_TOOLBAR}
};

/**
 * gnubg_stock_init:
 *
 * Initializes the GNUBG stock icon factory.
 */
void gnubg_stock_init(void)
{
	guint i;
	gnubg_stock_factory = gtk_icon_factory_new();

	for (i = 0; i < G_N_ELEMENTS(gnubg_stock_pixbufs); i++) {
		add_sized_with_same_fallback(gnubg_stock_factory,
					     gnubg_stock_pixbufs[i].ltr,
					     gnubg_stock_pixbufs[i].rtl,
					     gnubg_stock_pixbufs[i].size,
					     gnubg_stock_pixbufs[i].stock_id);
	}

	gtk_icon_factory_add_default(gnubg_stock_factory);
	gtk_stock_add_static(gnubg_stock_items, G_N_ELEMENTS(gnubg_stock_items));
}