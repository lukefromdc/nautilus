/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus - Floating status bar.
 *
 * Copyright (C) 2011 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include <config.h>

#include <string.h>

#include "nautilus-floating-bar.h"

struct _NautilusFloatingBarDetails {
	gchar *primary_label;
	gchar *details_label;

	GtkWidget *primary_label_widget;
	GtkWidget *details_label_widget;
	GtkWidget *spinner;
	gboolean show_spinner;
	gboolean is_interactive;
};

enum {
	PROP_PRIMARY_LABEL = 1,
	PROP_DETAILS_LABEL,
	PROP_SHOW_SPINNER,
	NUM_PROPERTIES
};

enum {
	ACTION,
	NUM_SIGNALS
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };
static guint signals[NUM_SIGNALS] = { 0, };

G_DEFINE_TYPE (NautilusFloatingBar, nautilus_floating_bar,
               GTK_TYPE_BOX);

static void
action_button_clicked_cb (GtkButton *button,
			  NautilusFloatingBar *self)
{
	gint action_id;

	action_id = GPOINTER_TO_INT
		(g_object_get_data (G_OBJECT (button), "action-id"));
	
	g_signal_emit (self, signals[ACTION], 0, action_id);
}

static void
nautilus_floating_bar_finalize (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);

	g_free (self->priv->primary_label);
	g_free (self->priv->details_label);

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->finalize (obj);
}

static void
nautilus_floating_bar_get_property (GObject *object,
				    guint property_id,
				    GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_PRIMARY_LABEL:
		g_value_set_string (value, self->priv->primary_label);
		break;
	case PROP_DETAILS_LABEL:
		g_value_set_string (value, self->priv->details_label);
		break;
	case PROP_SHOW_SPINNER:
		g_value_set_boolean (value, self->priv->show_spinner);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
nautilus_floating_bar_set_property (GObject *object,
				    guint property_id,
				    const GValue *value,
				    GParamSpec *pspec)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (object);

	switch (property_id) {
	case PROP_PRIMARY_LABEL:
		nautilus_floating_bar_set_primary_label (self, g_value_get_string (value));
		break;
	case PROP_DETAILS_LABEL:
		nautilus_floating_bar_set_details_label (self, g_value_get_string (value));
		break;
	case PROP_SHOW_SPINNER:
		nautilus_floating_bar_set_show_spinner (self, g_value_get_boolean (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
update_labels (NautilusFloatingBar *self)
{
	gboolean primary_visible, details_visible;

	primary_visible = (self->priv->primary_label != NULL) &&
		(strlen (self->priv->primary_label) > 0);
	details_visible = (self->priv->details_label != NULL) &&
		(strlen (self->priv->details_label) > 0);

	gtk_label_set_text (GTK_LABEL (self->priv->primary_label_widget),
			    self->priv->primary_label);
	gtk_widget_set_visible (self->priv->primary_label_widget, primary_visible);

	gtk_label_set_text (GTK_LABEL (self->priv->details_label_widget),
			    self->priv->details_label);
	gtk_widget_set_visible (self->priv->details_label_widget, details_visible);
}

static gboolean
overlay_enter_notify_cb (GtkWidget        *parent,
			 GdkEventCrossing *event,
			 gpointer          user_data)
{
	GtkWidget *widget = user_data;

	if (event->window != gtk_widget_get_window (widget)) {
		return FALSE;
	}

	if (NAUTILUS_FLOATING_BAR (widget)->priv->is_interactive) {
		return FALSE;
	}

	if (gtk_widget_get_halign (widget) == GTK_ALIGN_START) {
		gtk_widget_set_halign (widget, GTK_ALIGN_END);
	} else {
		gtk_widget_set_halign (widget, GTK_ALIGN_START);
	}

	gtk_widget_queue_resize (widget);

	return FALSE;
}

static void
nautilus_floating_bar_parent_set (GtkWidget *widget,
				  GtkWidget *old_parent)
{
	GtkWidget *parent;

	parent = gtk_widget_get_parent (widget);

	if (old_parent != NULL) {
		g_signal_handlers_disconnect_by_func (old_parent,
						      overlay_enter_notify_cb, widget);
	}

	if (parent != NULL) {
		g_signal_connect (parent, "enter-notify-event",
				  G_CALLBACK (overlay_enter_notify_cb), widget);
	}
}

static void
nautilus_floating_bar_show (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->show (widget);

	if (self->priv->show_spinner) {
		gtk_spinner_start (GTK_SPINNER (self->priv->spinner));
	}
}

static void
nautilus_floating_bar_hide (GtkWidget *widget)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (widget);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->hide (widget);

	gtk_spinner_stop (GTK_SPINNER (self->priv->spinner));
}

static void
get_padding_and_border (GtkWidget *widget,
                        GtkBorder *border)
{
  GtkStyleContext *context;
  GtkStateFlags state;
  GtkBorder tmp;

  context = gtk_widget_get_style_context (widget);
  state = gtk_widget_get_state_flags (widget);

  gtk_style_context_get_padding (context, state, border);
  gtk_style_context_get_border (context, state, &tmp);
  border->top += tmp.top;
  border->right += tmp.right;
  border->bottom += tmp.bottom;
  border->left += tmp.left;
}

static void
nautilus_floating_bar_get_preferred_width (GtkWidget *widget,
					   gint      *minimum_size,
					   gint      *natural_size)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_width (widget,
										    minimum_size,
										    natural_size);

	*minimum_size += border.left + border.right;
	*natural_size += border.left + border.right;
}

static void
nautilus_floating_bar_get_preferred_width_for_height (GtkWidget *widget,
						      gint       height,
						      gint      *minimum_size,
						      gint      *natural_size)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_width_for_height (widget,
											       height,
											       minimum_size,
											       natural_size);

	*minimum_size += border.left + border.right;
	*natural_size += border.left + border.right;
}

static void
nautilus_floating_bar_get_preferred_height (GtkWidget *widget,
					    gint      *minimum_size,
					    gint      *natural_size)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_height (widget,
										     minimum_size,
										     natural_size);

	*minimum_size += border.top + border.bottom;
	*natural_size += border.top + border.bottom;
}

static void
nautilus_floating_bar_get_preferred_height_for_width (GtkWidget *widget,
						      gint       width,
						      gint      *minimum_size,
						      gint      *natural_size)
{
	GtkBorder border;

	get_padding_and_border (widget, &border);

	GTK_WIDGET_CLASS (nautilus_floating_bar_parent_class)->get_preferred_height_for_width (widget,
											       width,
											       minimum_size,
											       natural_size);

	*minimum_size += border.top + border.bottom;
	*natural_size += border.top + border.bottom;
}

static void
nautilus_floating_bar_constructed (GObject *obj)
{
	NautilusFloatingBar *self = NAUTILUS_FLOATING_BAR (obj);
	GtkWidget *w, *box, *labels_box;

	G_OBJECT_CLASS (nautilus_floating_bar_parent_class)->constructed (obj);

	box = GTK_WIDGET (obj);

	w = gtk_spinner_new ();
	gtk_box_pack_start (GTK_BOX (box), w, FALSE, FALSE, 0);
	gtk_widget_set_visible (w, self->priv->show_spinner);
	self->priv->spinner = w;

	gtk_widget_set_size_request (w, 16, 16);
	gtk_widget_set_margin_start (w, 8);

	labels_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	gtk_box_pack_start (GTK_BOX (box), labels_box, TRUE, TRUE, 0);
	g_object_set (labels_box,
		      "margin-top", 2,
		      "margin-bottom", 2,
		      "margin-start", 12,
		      "margin-end", 12,
		      NULL);
	gtk_widget_show (labels_box);

	w = gtk_label_new (NULL);
	gtk_label_set_ellipsize (GTK_LABEL (w), PANGO_ELLIPSIZE_MIDDLE);
	gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
	gtk_container_add (GTK_CONTAINER (labels_box), w);
	self->priv->primary_label_widget = w;
	gtk_widget_show (w);

	w = gtk_label_new (NULL);
	gtk_label_set_single_line_mode (GTK_LABEL (w), TRUE);
	gtk_container_add (GTK_CONTAINER (labels_box), w);
	self->priv->details_label_widget = w;
	gtk_widget_show (w);
}

static void
nautilus_floating_bar_init (NautilusFloatingBar *self)
{
	GtkStyleContext *context;

	self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_FLOATING_BAR,
						  NautilusFloatingBarDetails);

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, "floating-bar");
}

static void
nautilus_floating_bar_class_init (NautilusFloatingBarClass *klass)
{
	GObjectClass *oclass = G_OBJECT_CLASS (klass);
	GtkWidgetClass *wclass = GTK_WIDGET_CLASS (klass);

	oclass->constructed = nautilus_floating_bar_constructed;
	oclass->set_property = nautilus_floating_bar_set_property;
	oclass->get_property = nautilus_floating_bar_get_property;
	oclass->finalize = nautilus_floating_bar_finalize;

	wclass->get_preferred_width = nautilus_floating_bar_get_preferred_width;
	wclass->get_preferred_width_for_height = nautilus_floating_bar_get_preferred_width_for_height;
	wclass->get_preferred_height = nautilus_floating_bar_get_preferred_height;
	wclass->get_preferred_height_for_width = nautilus_floating_bar_get_preferred_height_for_width;
	wclass->show = nautilus_floating_bar_show;
	wclass->hide = nautilus_floating_bar_hide;
	wclass->parent_set = nautilus_floating_bar_parent_set;

	properties[PROP_PRIMARY_LABEL] =
		g_param_spec_string ("primary-label",
				     "Bar's primary label",
				     "Primary label displayed by the bar",
				     NULL,
				     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_DETAILS_LABEL] =
		g_param_spec_string ("details-label",
				     "Bar's details label",
				     "Details label displayed by the bar",
				     NULL,
				     G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS);
	properties[PROP_SHOW_SPINNER] =
		g_param_spec_boolean ("show-spinner",
				      "Show spinner",
				      "Whether a spinner should be shown in the floating bar",
				      FALSE,
				      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

	signals[ACTION] =
		g_signal_new ("action",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL,
			      g_cclosure_marshal_VOID__INT,
			      G_TYPE_NONE, 1,
			      G_TYPE_INT);

	g_type_class_add_private (klass, sizeof (NautilusFloatingBarDetails));
	g_object_class_install_properties (oclass, NUM_PROPERTIES, properties);
}

void
nautilus_floating_bar_set_primary_label (NautilusFloatingBar *self,
					 const gchar *label)
{
	if (g_strcmp0 (self->priv->primary_label, label) != 0) {
		g_free (self->priv->primary_label);
		self->priv->primary_label = g_strdup (label);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PRIMARY_LABEL]);

		update_labels (self);
	}
}

void
nautilus_floating_bar_set_details_label (NautilusFloatingBar *self,
					 const gchar *label)
{
	if (g_strcmp0 (self->priv->details_label, label) != 0) {
		g_free (self->priv->details_label);
		self->priv->details_label = g_strdup (label);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DETAILS_LABEL]);

		update_labels (self);
	}
}

void
nautilus_floating_bar_set_labels (NautilusFloatingBar *self,
				  const gchar *primary_label,
				  const gchar *details_label)
{
	nautilus_floating_bar_set_primary_label (self, primary_label);
	nautilus_floating_bar_set_details_label (self, details_label);
}

void
nautilus_floating_bar_set_show_spinner (NautilusFloatingBar *self,
					gboolean show_spinner)
{
	if (self->priv->show_spinner != show_spinner) {
		self->priv->show_spinner = show_spinner;
		gtk_widget_set_visible (self->priv->spinner,
					show_spinner);

		g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_SHOW_SPINNER]);
	}
}

GtkWidget *
nautilus_floating_bar_new (const gchar *primary_label,
			   const gchar *details_label,
			   gboolean show_spinner)
{
	return g_object_new (NAUTILUS_TYPE_FLOATING_BAR,
			     "primary-label", primary_label,
			     "details-label", details_label,
			     "show-spinner", show_spinner,
			     "orientation", GTK_ORIENTATION_HORIZONTAL,
			     "spacing", 8,
			     NULL);
}

void
nautilus_floating_bar_add_action (NautilusFloatingBar *self,
				  const gchar *icon_name,
				  gint action_id)
{
	GtkWidget *button;
	GtkStyleContext *context;

	button = gtk_button_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
	context = gtk_widget_get_style_context (button);
	gtk_button_set_relief (button, GTK_RELIEF_NONE);
	gtk_style_context_add_class (context, "circular");
	gtk_style_context_add_class (context, "flat");
	gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
	gtk_box_pack_end (GTK_BOX (self), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	g_object_set_data (G_OBJECT (button), "action-id",
			   GINT_TO_POINTER (action_id));

	g_signal_connect (button, "clicked",
			  G_CALLBACK (action_button_clicked_cb), self);

	self->priv->is_interactive = TRUE;
}

void
nautilus_floating_bar_cleanup_actions (NautilusFloatingBar *self)
{
	GtkWidget *widget;
	GList *children, *l;
	gpointer data;

	children = gtk_container_get_children (GTK_CONTAINER (self));
	l = children;

	while (l != NULL) {
		widget = l->data;
		data = g_object_get_data (G_OBJECT (widget), "action-id");
		l = l->next;

		if (data != NULL) {
			/* destroy this */
			gtk_widget_destroy (widget);
		}
	}

	g_list_free (children);

	self->priv->is_interactive = FALSE;
}
