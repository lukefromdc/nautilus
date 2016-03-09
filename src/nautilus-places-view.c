/* nautilus-places-view.c
 *
 * Copyright (C) 2015 Georges Basile Stavracas Neto <georges.stavracas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-mime-actions.h"
#include "nautilus-places-view.h"
#include "nautilus-window-slot.h"
#include "nautilus-application.h"
#include "gtk/gtkplacesviewprivate.h"

typedef struct
{
        GFile                  *location;
        GIcon                  *icon;
        NautilusQuery          *search_query;

        GtkWidget              *places_view;
} NautilusPlacesViewPrivate;

struct _NautilusPlacesView
{
        GtkFrameClass parent;
};

static void          nautilus_places_view_iface_init             (NautilusViewInterface *iface);

G_DEFINE_TYPE_WITH_CODE (NautilusPlacesView, nautilus_places_view, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (NautilusPlacesView)
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_VIEW, nautilus_places_view_iface_init));

enum {
        PROP_0,
        PROP_ICON,
        PROP_LOCATION,
        PROP_SEARCH_QUERY,
        PROP_VIEW_WIDGET,
        PROP_IS_LOADING,
        PROP_IS_SEARCHING,
        LAST_PROP
};

static void
open_location_cb (NautilusPlacesView *view,
                  GFile              *location,
                  GtkPlacesOpenFlags  open_flags)
{
        NautilusWindowOpenFlags flags;
        GtkWidget *slot;

        slot = gtk_widget_get_ancestor (GTK_WIDGET (view), NAUTILUS_TYPE_WINDOW_SLOT);

        switch (open_flags) {
        case GTK_PLACES_OPEN_NEW_TAB:
                flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_TAB;
                break;

        case GTK_PLACES_OPEN_NEW_WINDOW:
                flags = NAUTILUS_WINDOW_OPEN_FLAG_NEW_WINDOW;
                break;

        case GTK_PLACES_OPEN_NORMAL: /* fall-through */
        default:
                flags = 0;
                break;
        }

        if (slot) {
                NautilusFile *file;
                GtkWidget *window;
                char *path;

                path = "other-locations:///";
                file = nautilus_file_get (location);
                window = gtk_widget_get_toplevel (GTK_WIDGET (view));

                nautilus_mime_activate_file (GTK_WINDOW (window),
                                             NAUTILUS_WINDOW_SLOT (slot),
                                             file,
                                             path,
                                             flags);
                nautilus_file_unref (file);
        }
}

static void
loading_cb (NautilusView *view)
{
        g_object_notify (G_OBJECT (view), "is-loading");
}

static void
nautilus_places_view_finalize (GObject *object)
{
        NautilusPlacesView *self = (NautilusPlacesView *)object;
        NautilusPlacesViewPrivate *priv = nautilus_places_view_get_instance_private (self);

        g_clear_object (&priv->icon);
        g_clear_object (&priv->location);
        g_clear_object (&priv->search_query);

        G_OBJECT_CLASS (nautilus_places_view_parent_class)->finalize (object);
}

static void
nautilus_places_view_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
        NautilusView *view = NAUTILUS_VIEW (object);

        switch (prop_id) {
        case PROP_ICON:
                g_value_set_object (value, nautilus_view_get_icon (view));
                break;

        case PROP_LOCATION:
                g_value_set_object (value, nautilus_view_get_location (view));
                break;

        case PROP_SEARCH_QUERY:
                g_value_set_object (value, nautilus_view_get_search_query (view));
                break;

        case PROP_VIEW_WIDGET:
                g_value_set_object (value, nautilus_view_get_view_widget (view));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static void
nautilus_places_view_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
        NautilusView *view = NAUTILUS_VIEW (object);

        switch (prop_id) {
        case PROP_LOCATION:
                nautilus_view_set_location (view, g_value_get_object (value));
                break;

        case PROP_SEARCH_QUERY:
                nautilus_view_set_search_query (view, g_value_get_object (value));
                break;

        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
}

static GIcon*
nautilus_places_view_get_icon (NautilusView *view)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        return priv->icon;
}

static GFile*
nautilus_places_view_get_location (NautilusView *view)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        return priv->location;
}

static void
nautilus_places_view_set_location (NautilusView *view,
                                   GFile        *location)
{
        if (location) {
                NautilusPlacesViewPrivate *priv;
                gchar *uri;

                priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));
                uri = g_file_get_uri (location);

                /*
                 * If it's not trying to open the places view itself, simply
                 * delegates the location to application, which takes care of
                 * selecting the appropriate view.
                 */
                if (!g_strcmp0 (uri, "other-locations:///") == 0) {
                        nautilus_application_open_location_full (NAUTILUS_APPLICATION (g_application_get_default ()),
                                                                 location, 0, NULL, NULL, NULL);
                } else {
                        g_set_object (&priv->location, location);
                }

                g_free (uri);
        }
}

static GList*
nautilus_places_view_get_selection (NautilusView *view)
{
        /* STUB */
        return NULL;
}

static void
nautilus_places_view_set_selection (NautilusView *view,
                                    GList        *selection)
{
        /* STUB */
}

static NautilusQuery*
nautilus_places_view_get_search_query (NautilusView *view)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        return priv->search_query;
}

static void
nautilus_places_view_set_search_query (NautilusView  *view,
                                       NautilusQuery *query)
{
        NautilusPlacesViewPrivate *priv;
        gchar *text;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        g_set_object (&priv->search_query, query);

        text = query ? nautilus_query_get_text (query) : NULL;

        gtk_nautilus_places_view_set_search_query (GTK_NAUTILUS_PLACES_VIEW (priv->places_view), text);

        g_free (text);
}

static GtkWidget*
nautilus_places_view_get_view_widget (NautilusView *view)
{
        /* By returning NULL, the view menu button turns insensitive */
        return NULL;
}

static gboolean
nautilus_places_view_is_loading (NautilusView *view)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        return gtk_nautilus_places_view_get_loading (GTK_NAUTILUS_PLACES_VIEW (priv->places_view));
}

static gboolean
nautilus_places_view_is_searching (NautilusView *view)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (NAUTILUS_PLACES_VIEW (view));

        return priv->search_query != NULL;
}

static void
nautilus_places_view_iface_init (NautilusViewInterface *iface)
{
        iface->get_icon = nautilus_places_view_get_icon;
        iface->get_location = nautilus_places_view_get_location;
        iface->set_location = nautilus_places_view_set_location;
        iface->get_selection = nautilus_places_view_get_selection;
        iface->set_selection = nautilus_places_view_set_selection;
        iface->get_search_query = nautilus_places_view_get_search_query;
        iface->set_search_query = nautilus_places_view_set_search_query;
        iface->get_view_widget = nautilus_places_view_get_view_widget;
        iface->is_loading = nautilus_places_view_is_loading;
        iface->is_searching = nautilus_places_view_is_searching;
}

static void
nautilus_places_view_class_init (NautilusPlacesViewClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = nautilus_places_view_finalize;
        object_class->get_property = nautilus_places_view_get_property;
        object_class->set_property = nautilus_places_view_set_property;

        g_object_class_override_property (object_class, PROP_ICON, "icon");
        g_object_class_override_property (object_class, PROP_IS_LOADING, "is-loading");
        g_object_class_override_property (object_class, PROP_IS_SEARCHING, "is-searching");
        g_object_class_override_property (object_class, PROP_LOCATION, "location");
        g_object_class_override_property (object_class, PROP_SEARCH_QUERY, "search-query");
        g_object_class_override_property (object_class, PROP_VIEW_WIDGET, "view-widget");
}

static void
nautilus_places_view_init (NautilusPlacesView *self)
{
        NautilusPlacesViewPrivate *priv;

        priv = nautilus_places_view_get_instance_private (self);

        /* Icon */
        priv->icon = g_themed_icon_new_with_default_fallbacks ("view-list-symbolic");

        /* Location */
        priv->location = g_file_new_for_uri ("other-locations:///");

        /* Places view */
        priv->places_view = gtk_nautilus_places_view_new ();
        gtk_nautilus_places_view_set_open_flags (GTK_NAUTILUS_PLACES_VIEW (priv->places_view),
                                                 GTK_PLACES_OPEN_NEW_TAB | GTK_PLACES_OPEN_NEW_WINDOW | GTK_PLACES_OPEN_NORMAL);
        gtk_widget_set_hexpand (priv->places_view, TRUE);
        gtk_widget_set_vexpand (priv->places_view, TRUE);
        gtk_widget_show (priv->places_view);
        gtk_container_add (GTK_CONTAINER (self), priv->places_view);

        g_signal_connect_swapped (priv->places_view,
                                  "notify::loading",
                                  G_CALLBACK (loading_cb),
                                  self);

        g_signal_connect_swapped (priv->places_view,
                                  "open-location",
                                  G_CALLBACK (open_location_cb),
                                  self);

}

NautilusPlacesView *
nautilus_places_view_new (void)
{
        NautilusPlacesView *view;

        view = g_object_new (NAUTILUS_TYPE_PLACES_VIEW, NULL);
        if (g_object_is_floating (view)) {
                g_object_ref_sink (view);
        }

        return view;
}
