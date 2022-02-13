/*
 * cpuspeedpanel.h
 *
 * Copyright 2003-2004 Richard Drummond
 */

#ifndef __CPUSPEEDPANEL_H__
#define __CPUSPEEDPANEL_H__

#include <gdk/gdk.h>
#include <gtk/gtkframe.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define CPUSPEEDPANEL(obj)          GTK_CHECK_CAST (obj, cpuspeedpanel_get_type (), CpuSpeedPanel)
#define CPUSPEEDPANEL_CLASS(klass)  GTK_CHECK_CLASS_CAST (klass, cpuspeedpanel_get_type (), CpuSpeedPanelClass)
#define IS_CPUSPEEDPANEL(obj)       GTK_CHECK_TYPE (obj, cpuspeedpanel_get_type ())

typedef struct _CpuSpeedPanel      CpuSpeedPanel;
typedef struct _CpuSpeedPanelClass CpuSpeedPanelClass;

struct _CpuSpeedPanel
{
    /* super class */
    GtkFrame   frame;

    /* private */
    GtkWidget *speed_widget;
    GtkWidget *adjust_widget;
    GtkWidget *dontbusywait_widget;
    GtkWidget *idleenabled_widget;
    GtkWidget *idlerate_widget;
    gboolean   idleenabled;
    guint      cpulevel;

    /* properties */
    guint      cpuspeed;
    gboolean   dontbusywait;
    guint      cpuidle;
};

struct _CpuSpeedPanelClass
{
    GtkFrameClass parent_class;
    void (* cpuspeedpanel) (CpuSpeedPanel *cpuspeedpanel );
};

guint		cpuspeedpanel_get_type		(void);
GtkWidget*	cpuspeedpanel_new		(void);
void		cpuspeedpanel_set_cpuspeed	(CpuSpeedPanel *cspanel, gint cpuspeed);
void		cpuspeedpanel_set_cpulevel	(CpuSpeedPanel *cspanel, guint cpulevel);
void		cpuspeedpanel_set_dontbusywait	(CpuSpeedPanel *cspanel, gboolean dontbusywait);
//void		cpuspeedpanel_set_ideenabled	(CpuSpeedPanel *cspanel, gboolean idleenabled);
//void		cpuspeedpanel_set_idlerate	(CpuSpeedPanel *cspanel, guint idlerate);
#ifdef JIT
void cpuspeedpanel_set_cpuidle (CpuSpeedPanel *cspanel, guint cpuidle);
#endif
     
# ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __CPUSPEEDPANEL_H__ */
