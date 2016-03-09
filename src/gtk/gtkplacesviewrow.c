/* gtkplacesviewrow.c
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

#include "config.h"

#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "gtkplacesviewrowprivate.h"

struct _GtkNautilusPlacesViewRow
{
  GtkListBoxRow  parent_instance;

  GtkLabel      *available_space_label;
  GtkSpinner    *busy_spinner;
  GtkButton     *eject_button;
  GtkImage      *eject_icon;
  GtkEventBox   *event_box;
  GtkImage      *icon_image;
  GtkLabel      *name_label;
  GtkLabel      *path_label;

  GVolume       *volume;
  GMount        *mount;
  GFile         *file;

  GCancellable  *cancellable;

  gint           is_network : 1;
};

G_DEFINE_TYPE (GtkNautilusPlacesViewRow, gtk_nautilus_places_view_row, GTK_TYPE_LIST_BOX_ROW)

enum {
  PROP_0,
  PROP_ICON,
  PROP_NAME,
  PROP_PATH,
  PROP_VOLUME,
  PROP_MOUNT,
  PROP_FILE,
  PROP_IS_NETWORK,
  LAST_PROP
};

static GParamSpec *properties [LAST_PROP];

static void
measure_available_space_finished (GObject      *object,
                                  GAsyncResult *res,
                                  gpointer      user_data)
{
  GtkNautilusPlacesViewRow *row = user_data;
  GFileInfo *info;
  GError *error;
  guint64 free_space;
  guint64 total_space;
  gchar *formatted_free_size;
  gchar *formatted_total_size;
  gchar *label;

  error = NULL;

  info = g_file_query_filesystem_info_finish (G_FILE (object),
                                              res,
                                              &error);

  if (error)
    {
      if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED) &&
          !g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
        {
          g_warning ("Failed to measure available space: %s", error->message);
        }

      g_clear_error (&error);
      goto out;
    }

  if (!g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE) ||
      !g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE))
    {
      g_object_unref (info);
      goto out;
    }

  free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
  total_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);

  formatted_free_size = g_format_size (free_space);
  formatted_total_size = g_format_size (total_space);
  /* Translators: respectively, free and total space of the drive */
  label = g_strdup_printf (_("%s / %s available"), formatted_free_size, formatted_total_size);

  gtk_label_set_label (row->available_space_label, label);

  g_object_unref (info);
  g_free (formatted_total_size);
  g_free (formatted_free_size);
  g_free (label);
out:
  g_object_unref (object);
}

static void
measure_available_space (GtkNautilusPlacesViewRow *row)
{
  gboolean should_measure;

  should_measure = (!row->is_network && (row->volume || row->mount || row->file));

  gtk_label_set_label (row->available_space_label, "");
  gtk_widget_set_visible (GTK_WIDGET (row->available_space_label), should_measure);

  if (should_measure)
    {
      GFile *file = NULL;

      if (row->file)
        {
          file = g_object_ref (row->file);
        }
      else if (row->mount)
        {
          file = g_mount_get_root (row->mount);
        }
      else if (row->volume)
        {
          GMount *mount;

          mount = g_volume_get_mount (row->volume);

          if (mount)
            file = g_mount_get_root (row->mount);

          g_clear_object (&mount);
        }

      if (file)
        {
          g_cancellable_cancel (row->cancellable);
          g_clear_object (&row->cancellable);
          row->cancellable = g_cancellable_new ();

          g_file_query_filesystem_info_async (file,
                                              G_FILE_ATTRIBUTE_FILESYSTEM_FREE "," G_FILE_ATTRIBUTE_FILESYSTEM_SIZE,
                                              G_PRIORITY_DEFAULT,
                                              row->cancellable,
                                              measure_available_space_finished,
                                              row);
        }
    }
}

static void
gtk_nautilus_places_view_row_finalize (GObject *object)
{
  GtkNautilusPlacesViewRow *self = GTK_NAUTILUS_PLACES_VIEW_ROW (object);

  g_cancellable_cancel (self->cancellable);

  g_clear_object (&self->volume);
  g_clear_object (&self->mount);
  g_clear_object (&self->file);
  g_clear_object (&self->cancellable);

  G_OBJECT_CLASS (gtk_nautilus_places_view_row_parent_class)->finalize (object);
}

static void
gtk_nautilus_places_view_row_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GtkNautilusPlacesViewRow *self;
  GIcon *icon;

  self = GTK_NAUTILUS_PLACES_VIEW_ROW (object);
  icon = NULL;

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_image_get_gicon (self->icon_image, &icon, NULL);
      g_value_set_object (value, icon);
      break;

    case PROP_NAME:
      g_value_set_string (value, gtk_label_get_label (self->name_label));
      break;

    case PROP_PATH:
      g_value_set_string (value, gtk_label_get_label (self->path_label));
      break;

    case PROP_VOLUME:
      g_value_set_object (value, self->volume);
      break;

    case PROP_MOUNT:
      g_value_set_object (value, self->mount);
      break;

    case PROP_FILE:
      g_value_set_object (value, self->file);
      break;

    case PROP_IS_NETWORK:
      g_value_set_boolean (value, self->is_network);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_nautilus_places_view_row_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GtkNautilusPlacesViewRow *self = GTK_NAUTILUS_PLACES_VIEW_ROW (object);

  switch (prop_id)
    {
    case PROP_ICON:
      gtk_image_set_from_gicon (self->icon_image,
                                g_value_get_object (value),
                                GTK_ICON_SIZE_LARGE_TOOLBAR);
      break;

    case PROP_NAME:
      gtk_label_set_label (self->name_label, g_value_get_string (value));
      break;

    case PROP_PATH:
      gtk_label_set_label (self->path_label, g_value_get_string (value));
      break;

    case PROP_VOLUME:
      g_set_object (&self->volume, g_value_get_object (value));
      break;

    case PROP_MOUNT:
      g_set_object (&self->mount, g_value_get_object (value));

      /*
       * When we hide the eject button, no size is allocated for it. Since
       * we want to have alignment between rows, it needs an empty space
       * when the eject button is not available. So, call then
       * gtk_widget_set_child_visible(), which makes the button allocate the
       * size but it stays hidden when needed.
       */
      gtk_widget_set_child_visible (GTK_WIDGET (self->eject_button), self->mount != NULL);
      measure_available_space (self);
      break;

    case PROP_FILE:
      g_set_object (&self->file, g_value_get_object (value));
      measure_available_space (self);
      break;

    case PROP_IS_NETWORK:
      gtk_nautilus_places_view_row_set_is_network (self, g_value_get_boolean (value));
      measure_available_space (self);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_nautilus_places_view_row_class_init (GtkNautilusPlacesViewRowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->finalize = gtk_nautilus_places_view_row_finalize;
  object_class->get_property = gtk_nautilus_places_view_row_get_property;
  object_class->set_property = gtk_nautilus_places_view_row_set_property;

  properties[PROP_ICON] =
          g_param_spec_object ("icon",
                               "Icon of the row",
                               "The icon representing the volume",
                               G_TYPE_ICON,
                               G_PARAM_READWRITE);

  properties[PROP_NAME] =
          g_param_spec_string ("name",
                               "Name of the volume",
                               "The name of the volume",
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_PATH] =
          g_param_spec_string ("path",
                               "Path of the volume",
                               "The path of the volume",
                               "",
                               G_PARAM_READWRITE);

  properties[PROP_VOLUME] =
          g_param_spec_object ("volume",
                               "Volume represented by the row",
                               "The volume represented by the row",
                               G_TYPE_VOLUME,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_MOUNT] =
          g_param_spec_object ("mount",
                               "Mount represented by the row",
                               "The mount point represented by the row, if any",
                               G_TYPE_MOUNT,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_FILE] =
          g_param_spec_object ("file",
                               "File represented by the row",
                               "The file represented by the row, if any",
                               G_TYPE_FILE,
                               G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  properties[PROP_IS_NETWORK] =
          g_param_spec_boolean ("is-network",
                                "Whether the row represents a network location",
                                "Whether the row represents a network location",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, LAST_PROP, properties);

  gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/nautilus/gtk/gtkplacesviewrow.ui");

  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, available_space_label);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, busy_spinner);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, eject_button);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, eject_icon);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, event_box);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, icon_image);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, name_label);
  gtk_widget_class_bind_template_child (widget_class, GtkNautilusPlacesViewRow, path_label);
}

static void
gtk_nautilus_places_view_row_init (GtkNautilusPlacesViewRow *self)
{
  gtk_widget_init_template (GTK_WIDGET (self));
}

GtkWidget*
gtk_nautilus_places_view_row_new (GVolume *volume,
                         GMount  *mount)
{
  return g_object_new (GTK_TYPE_NAUTILUS_PLACES_VIEW_ROW,
                       "volume", volume,
                       "mount", mount,
                       NULL);
}

GMount*
gtk_nautilus_places_view_row_get_mount (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), NULL);

  return row->mount;
}

GVolume*
gtk_nautilus_places_view_row_get_volume (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), NULL);

  return row->volume;
}

GFile*
gtk_nautilus_places_view_row_get_file (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), NULL);

  return row->file;
}

GtkWidget*
gtk_nautilus_places_view_row_get_eject_button (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), NULL);

  return GTK_WIDGET (row->eject_button);
}

GtkWidget*
gtk_nautilus_places_view_row_get_event_box (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), NULL);

  return GTK_WIDGET (row->event_box);
}

void
gtk_nautilus_places_view_row_set_busy (GtkNautilusPlacesViewRow *row,
                              gboolean          is_busy)
{
  g_return_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row));

  gtk_widget_set_visible (GTK_WIDGET (row->busy_spinner), is_busy);
}

gboolean
gtk_nautilus_places_view_row_get_is_network (GtkNautilusPlacesViewRow *row)
{
  g_return_val_if_fail (GTK_IS_NAUTILUS_PLACES_VIEW_ROW (row), FALSE);

  return row->is_network;
}

void
gtk_nautilus_places_view_row_set_is_network (GtkNautilusPlacesViewRow *row,
                                    gboolean          is_network)
{
  if (row->is_network != is_network)
    {
      row->is_network = is_network;

      gtk_image_set_from_icon_name (row->eject_icon,
                                    is_network ? "network-offline-symbolic" : "media-eject-symbolic",
                                    GTK_ICON_SIZE_BUTTON);
      gtk_widget_set_tooltip_text (GTK_WIDGET (row->eject_button), is_network ? _("Disconnect") : _("Unmount"));
    }
}

void
gtk_nautilus_places_view_row_set_path_size_group (GtkNautilusPlacesViewRow *row,
                                         GtkSizeGroup     *group)
{
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (row->path_label));
}

void
gtk_nautilus_places_view_row_set_space_size_group (GtkNautilusPlacesViewRow *row,
                                          GtkSizeGroup     *group)
{
  if (group)
    gtk_size_group_add_widget (group, GTK_WIDGET (row->available_space_label));
}
