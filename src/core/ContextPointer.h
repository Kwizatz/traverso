/*
    Copyright (C) 2005-2010 Remon Sijrier

    This file is part of Traverso

    Traverso is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA.

*/

#ifndef CONTEXTPOINTER_H
#define CONTEXTPOINTER_H

#include <QObject>
#include <QTimer>

#include "AbstractViewPort.h"

class ContextPointer : public QObject
{
    Q_OBJECT

public:
    /**
     * 	Returns the current ViewPort's mouse x coordinate

     * @return The current ViewPort's mouse x coordinate
     */
    inline int x() const {return m_mousePos.x();}

    /**
     * 	Returns the current ViewPort's mouse y coordinate

     * @return The current ViewPort's mouse y coordinate
     */
    inline int y() const {return m_mousePos.y();}

    /**
     *        Convenience function, equals QPoint(cpointer().x(), cpointer().y())

    * @return The current ViewPorts mouse position;
    */

    inline QPoint pos() {return m_mousePos;}

    /**
     * 	Convenience function that maps the ViewPort's mouse x <br />
        coordinate to the scene x coordinate

     * @return The current scene x coordinate, mapped from the ViewPort's mouse x coordinate
     */
    inline qreal scene_x() const {
        if (!m_viewPort) {
            qDebug("scene_x() called, but no ViewPort was set!");
            return 0;
        }
        return m_viewPort->map_to_scene(m_mousePos).x();
    }

    /**
     * 	Convenience function that maps the ViewPort's mouse y <br />
        coordinate to the ViewPort's scene y coordinate

     * @return The current ViewPort's scene y coordinate, mapped from the ViewPort's mouse y coordinate
     */
    inline qreal scene_y() const {
        if (!m_viewPort) {
            qDebug("scene_y() called, but no ViewPort was set!");
            return 0;
        }
        return m_viewPort->map_to_scene(m_mousePos).y();
    }

    /**
     * 	Returns the current's ViewPort's mouse position in the ViewPort's scene position.
     * @return The current's ViewPort's mouse position in the ViewPort's scene position.
     */
    inline QPointF scene_pos() const {
        if (!m_viewPort) {
            qDebug("scene_pos() called, but no ViewPort was set!");
            return QPointF(0,0);
        }
        return m_viewPort->map_to_scene(m_mousePos);
    }


    /**
     *     	Used by ViewPort to update the internal state of ContextPointer
        Not intended to be used somewhere else.
     * @param x The ViewPort's mouse x coordinate
     * @param y The ViewPort's mouse y coordinate
     */
    void store_canvas_cursor_position(const QPoint& pos);

    /**
     *        Returns the ViewPort x coordinate on first input event.
     * @return The ViewPort x coordinate on first input event.
     */
    inline int on_first_input_event_x() const {return m_onFirstInputEventPos.x(); }

    /**
     *        Returns the ViewPort y coordinate on first input event.
     * @return The ViewPort y coordinate on first input event.
     */
    inline int on_first_input_event_y() const {return m_onFirstInputEventPos.y(); }

    /**
     *        Returns the scene x coordinate on first input event.
         * @return The scene x coordinate on first input event, -1 if no Port was set
     */
    inline qreal on_first_input_event_scene_x() const {
        if (!m_viewPort) {
            // what else to do?
            return -1;
        }
        return m_viewPort->map_to_scene(m_onFirstInputEventPos).x();
    }

    inline QPointF on_first_input_event_scene_pos() const {
        if (!m_viewPort) {
            // what else to do?
            return QPointF(-1, -1);
        }
        return m_viewPort->map_to_scene(m_onFirstInputEventPos);
    }

    /**
     *        Returns the scene y coordinate on first input event.
     * @return The scene y coordinate on first input event.
     */
    inline qreal on_first_input_event_scene_y() const {
        if (!m_viewPort) {
            // what else to do?
            return -1;
        }
        return m_viewPort->map_to_scene(m_onFirstInputEventPos).y();
    }

    inline int get_current_mode() const {
        if (m_viewPort) {
            return m_viewPort->get_current_mode();
        }
        return -1;
    }

    void set_jog_bypass_distance(int distance);
    void set_left_mouse_click_bypasses_jog(bool bypassOnLeftMouseClick);
    void mouse_button_left_pressed();

    inline AbstractViewPort* get_viewport() const {
        return m_viewPort;
    }

    void set_current_viewport(AbstractViewPort* vp);
    void setCursorShape(const QString& cursor, int alignment=Qt::AlignCenter);
    void setCursorText(const QString& text, int mseconds=-1);
    void setCursorPos(QPointF pos);

    QList<QObject* > get_context_items();
    QList<ContextItem*> get_active_context_items() const {return m_activeContextItems;}

    void add_contextitem(QObject* item);
    void remove_contextitem(QObject* item);
    void remove_from_active_context_list(ContextItem* item);
    void about_to_delete(ContextItem* item);

    QList<QObject* > get_contextmenu_items() const;
    void set_contextmenu_items(QList<QObject* > list);
    void set_active_context_items_by_mouse_movement(const QList<ContextItem*>& items);
    void set_active_context_items_by_keyboard_input(const QList<ContextItem*>& items);

    bool keyboard_only_input() const {return m_keyboardOnlyInput;}
    bool left_mouse_click_bypasses_jog() const {return m_mouseLeftClickBypassesJog;}

    QPoint get_global_mouse_pos() const {return m_globalMousePos;}

    void update_mouse_positions(const QPoint &pos, const QPoint &globalPos);


private:
    ContextPointer();
    ContextPointer(const ContextPointer&);

    // allow this function to create one instance
    friend ContextPointer& cpointer();
    friend class TInputEventDispatcher;
    friend class Traverso;

    int m_jogBypassDistance;

    QTimer  m_jogTimer;

    QPoint  m_onFirstInputEventPos;
    QPoint  m_jogStartGlobalMousePos;   // global Mouse Screen position at jog start
    QPoint  m_mousePos;
    QPoint  m_globalMousePos;           // global Mouse Screen position while holding
    QPoint  m_mouseCursorPosDuringHold;  // global Mouse Screen pos while holding centered in ViewPort

    bool    m_keyboardOnlyInput;
    bool    m_jogEvent;
    bool    m_mouseLeftClickBypassesJog;



    AbstractViewPort* m_viewPort;
    ContextItem*     m_currentContext;
    QList<QObject* > m_contextItemsList;
    QList<QObject* > m_contextMenuItems;
    QList<ContextItem*> m_activeContextItems;
    QList<ContextItem*> m_onFirstInputEventActiveContextItems;

    void set_active_context_items(const QList<ContextItem*>& items);

    /**
     *        Called ONLY by InputEngine, not to be used anywhere else.
     */
    void prepare_for_shortcut_dispatch( );

    void set_keyboard_only_input(bool keyboardOnly);

signals:
    void contextChanged();

private slots:
    void update_jog();
    void jog_start();
    void jog_finished();
};

// use this function to access the context pointer
ContextPointer& cpointer();

#endif


//eof


