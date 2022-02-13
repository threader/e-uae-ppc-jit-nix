/*
 * led.c
 *
 * Copyright 2004 Martin Garton
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "led.h"

#define LED_W 20
#define LED_H 10

static void led_init (Led *theled);
static void led_class_init (LedClass *class);
static gint led_expose (GtkWidget *w, GdkEventExpose *event);
static void led_destroy (GtkObject *object);

guint led_get_type ()
{
    static guint led_type = 0;

    if (!led_type) {
	GtkTypeInfo led_info = {
	    "Led",
	    sizeof (Led),
	    sizeof (LedClass),
	    (GtkClassInitFunc) led_class_init,
	    (GtkObjectInitFunc) led_init,
	    NULL,
	    NULL,
	    (GtkClassInitFunc) NULL
	};
	led_type = gtk_type_unique (gtk_misc_get_type (), &led_info);
    }
    return led_type;
}

static GtkObjectClass *parent_class;

static void led_class_init (LedClass *class)
{
    GtkObjectClass *object_class = (GtkObjectClass *) class;
    GtkWidgetClass *widget_class = (GtkWidgetClass *) class;
    parent_class = gtk_type_class (gtk_object_get_type ());

    object_class->destroy = led_destroy;
    widget_class->expose_event = led_expose;
}

static void led_init (Led *theled)
{
    theled->color = LED_OFF;

    GTK_WIDGET (theled)->requisition.width = LED_W + GTK_MISC (theled)->xpad * 2;
    GTK_WIDGET (theled)->requisition.height = LED_H + GTK_MISC (theled)->ypad * 2;
}


GtkWidget *led_new (void)
{
    return gtk_type_new (led_get_type ());
}


static gint led_expose (GtkWidget *w, GdkEventExpose *event)
{
    if (w && GTK_WIDGET_DRAWABLE (w)) {
	GtkStyle *style = gtk_style_copy (w->style);
	style->bg[GTK_STATE_NORMAL] = LED (w)->color;
	gtk_style_attach (style, w->window);
	gtk_draw_flat_box (style, w->window, GTK_STATE_NORMAL, GTK_SHADOW_NONE,
			   0, 0, LED_W, LED_H);
    }
    return 0;
}

static void led_destroy (GtkObject *o)
{
    g_return_if_fail (o != NULL);
    g_return_if_fail (IS_LED (o));

    // TODO: ?? free any stuff here.

    GTK_OBJECT_CLASS (parent_class)->destroy (o);
}

void led_set_color (Led *l, GdkColor col)
{
    l->color = col;

    if (GTK_WIDGET_DRAWABLE (l))
	led_expose (GTK_WIDGET (l), NULL);
}
