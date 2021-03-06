/******************************************************************************
 * $Id$
 *
 * Project:  OpenEV
 * Purpose:  Point of interest editing mode.
 * Author:   OpenEV Team
 * Maintainer: Mario Beauchamp, starged@gmail.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Atlantis Scientific Inc. (www.atlsci.com)
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 ******************************************************************************
 *
 */

#include "gvpoitool.h"
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>

#define DEFAULT_POI_SIZE 6.0

/* Signals */
enum
{
    POI_CHANGED,
    LAST_SIGNAL
};

static void gv_poi_tool_class_init(GvPoiToolClass *klass);
static void gv_poi_tool_init(GvPoiTool *tool);
static gboolean gv_poi_tool_draw(GvTool *tool);
static gboolean gv_poi_tool_button_release(GvTool *tool, GdkEventButton *event);
static void gv_poi_tool_deactivate(GvTool *tool, GvViewArea *view);

static guint poitool_signals[LAST_SIGNAL] = { 0 };

GType
gv_poi_tool_get_type(void)
{
    static GType poi_tool_type = 0;

    if (!poi_tool_type) {
        static const GTypeInfo poi_tool_info =
        {
            sizeof(GvPoiToolClass),
            (GBaseInitFunc) NULL,
            (GBaseFinalizeFunc) NULL,
            (GClassInitFunc) gv_poi_tool_class_init,
            /* reserved_1 */ NULL,
            /* reserved_2 */ NULL,
            sizeof(GvPoiTool),
            0,
            (GInstanceInitFunc) gv_poi_tool_init,
        };
        poi_tool_type = g_type_register_static (GV_TYPE_TOOL,
                                                  "GvPoiTool",
                                                  &poi_tool_info, 0);
        }

    return poi_tool_type;
}

static void
gv_poi_tool_class_init(GvPoiToolClass *klass)
{
    GvToolClass *tool_class = GV_TOOL_CLASS (klass);

    poitool_signals[POI_CHANGED] =
      g_signal_new ("poi_changed",
                    G_TYPE_FROM_CLASS (klass),
                    G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                    G_STRUCT_OFFSET (GvPoiToolClass, poi_changed),
                    NULL, NULL,
                    g_cclosure_marshal_VOID__POINTER, G_TYPE_NONE, 0);

    klass->poi_changed = NULL;

    tool_class->deactivate = gv_poi_tool_deactivate;
    tool_class->draw = gv_poi_tool_draw;
    tool_class->button_release = gv_poi_tool_button_release;
}

static void
gv_poi_tool_init(GvPoiTool *tool)
{
    GV_TOOL(tool)->cursor = gdk_cursor_new(GDK_TCROSS);
    tool->poi_marked = FALSE;
    tool->poi_size = DEFAULT_POI_SIZE;
}

GvTool *
gv_poi_tool_new(void)
{
    GvPoiTool *tool = g_object_new(GV_TYPE_POI_TOOL, NULL);

    return GV_TOOL(tool);
}

gint
gv_poi_tool_get_point(GvPoiTool *tool, GvVertex *point)
{
    if (!tool->poi_marked)
        return FALSE;

    point->x = tool->v_center.x;
    point->y = tool->v_center.y;

    return TRUE;
}

gint
gv_poi_tool_new_point(GvPoiTool *tool, GvVertex *point)
{
    /* Create new POI */
    tool->poi_marked = TRUE;

    tool->v_center.x = point->x;
    tool->v_center.y = point->y;

    gv_tool_clamp_to_bounds( GV_TOOL(tool), 
                             &(tool->v_center.x), &(tool->v_center.y) );

    g_signal_emit(tool, poitool_signals[POI_CHANGED], 0);

    gv_view_area_queue_draw(GV_TOOL(tool)->view);       

    return TRUE;
}

/**************************************************************/

static gboolean
gv_poi_tool_button_release(GvTool *ptool, GdkEventButton *event)
{
    GvPoiTool *tool = GV_POI_TOOL(ptool);

    if ((event->button == 1)  && !(event->state & GDK_CONTROL_MASK)
                              && !(event->state & GDK_SHIFT_MASK) )
    {

        /* Set head and tail vertex to pointer position */
        /* Map pointer position */
        gv_view_area_map_pointer(GV_TOOL(tool)->view, event->x, event->y,
                     &tool->v_center.x, &tool->v_center.y);

        if( gv_tool_check_bounds( GV_TOOL(tool), 
                                  tool->v_center.x, tool->v_center.y ) )
        {
            tool->poi_marked = TRUE;
            gv_view_area_queue_draw(GV_TOOL(tool)->view);       
            g_signal_emit(tool, poitool_signals[POI_CHANGED], 0);
        }
    }
    if ((event->button == 2)  && !(event->state & GDK_CONTROL_MASK)
                              && !(event->state & GDK_SHIFT_MASK) )
    {
        /* If user presses 2nd button within the view window, */
        /* clear the point and redraw view without it */
        gv_view_area_map_pointer(GV_TOOL(tool)->view, event->x, event->y,
                     &tool->v_center.x, &tool->v_center.y);

        if( gv_tool_check_bounds( GV_TOOL(tool), 
                                  tool->v_center.x, tool->v_center.y ) )
        {
            tool->poi_marked = FALSE;
            gv_view_area_queue_draw(GV_TOOL(tool)->view);       

            g_signal_emit(tool, poitool_signals[POI_CHANGED], 0);
        }
    }

    return FALSE;
}

static gboolean
gv_poi_tool_draw(GvTool *ptool)
{
    GvPoiTool *tool = GV_POI_TOOL(ptool);
    gvgeocoord dx, dy, bx, by, x, y;

    if (tool->poi_marked)
    {
        dx = tool->poi_size;
        dy = 0.0;
        gv_view_area_correct_for_transform(GV_TOOL(tool)->view, dx, dy, &dx, &dy);
        bx = by = tool->poi_size + 2;
        gv_view_area_correct_for_transform(GV_TOOL(tool)->view, bx, by, &bx, &by);

        x = tool->v_center.x;
        y = tool->v_center.y;

        glRenderMode(GL_RENDER);
        glColor3f(1.0,0.5,0.0);

        /* Draw crosshairs */
        glBegin(GL_LINES);
        glVertex2(x-dx, y-dy);
        glVertex2(x+dx, y+dy);
        glVertex2(x+dy, y-dx);
        glVertex2(x-dy, y+dx);
        glEnd();

        /* Draw box around crosshairs */
        glBegin(GL_LINE_LOOP);
        glVertex2(x-bx, y-by);
        glVertex2(x+by, y-bx);
        glVertex2(x+bx, y+by);
        glVertex2(x-by, y+bx);
        glEnd();        
    }

    return FALSE;
}

static void
gv_poi_tool_deactivate(GvTool *ptool, GvViewArea *view)
{
    GvPoiTool *tool = GV_POI_TOOL(ptool);

    /* Call the parent class func */
    GV_TOOL_DEACTIVATE(tool, view);

    tool->poi_marked = FALSE;

    gv_view_area_queue_draw(view);
}
