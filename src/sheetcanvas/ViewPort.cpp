/*
Copyright (C) 2005-2006 Remon Sijrier 

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

#include <QMouseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QRect>
#include <QPainter>
#include <QPixmap>
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QEvent>
#include <QStyleOptionGraphicsItem>
#include <QApplication>

#include <Utils.h>
#include "InputEngine.h"
#include "Themer.h"

#include "SheetView.h"
#include "Sheet.h"
#include "ViewPort.h"
#include "ViewItem.h"
#include "ContextPointer.h"

#include "Import.h"

// Always put me below _all_ includes, this is needed
// in case we run with memory leak detection enabled!
#include "Debugger.h"


/**
 * \class ViewPort
 * \brief An Interface class to create Contextual, or so called 'Soft Selection' enabled Widgets.

	The ViewPort class inherits QGraphicsView, and thus is a true Canvas type of Widget.<br />
	Reimplement ViewPort to create a 'Soft Selection' enabled widget. You have to create <br />
	a QGraphicsScene object yourself, and set it as the scene the ViewPort visualizes.

	ViewPort should be used to visualize 'core' data objects. This is done by creating a <br />
	ViewItem object for each core class that has to be visualized. The naming convention <br />
	for classes that inherit ViewItem is: core class name + View.<br />
	E.g. the ViewItem class that represents an AudioClip should be named AudioClipView.

	All keyboard and mouse events by default are propagated to the InputEngine, which in <br />
	turn will parse the events. In case the event sequence was recognized by the InputEngine <br />
	it will ask a list of (pointed) ContextItem's from ContextPointer, which in turns <br />
	call's the pure virtual function get_pointed_context_items(), which you have to reimplement.<br />
	In the reimplemented function, you have to fill the supplied list with ViewItems that are <br />
	under the mouse cursor, and if needed, ViewItem's that _always_ have to be taken into account. <br />
	One can use the convenience functions of QGraphicsView for getting ViewItem's under the mouse cursor!

	Since there can be a certain delay before a key sequence has been verified, the ContextPointer <br />
	stores the position of the first event of a new key fact. This improves the pointed ViewItem <br />
	detection a lot in case the mouse is moved during a key sequence.<br />
	You should use these x/y coordinates in the get_pointed_context_items() function, see:<br />
	ContextPointer::on_first_input_event_x(), ContextPointer::on_first_input_event_y()


 *	\sa ContextPointer, InputEngine
 */



ViewPort::ViewPort(QWidget* parent)
	: QGraphicsView(parent)
        , AbstractViewPort()
        , m_sv(0)
        , m_mode(0)
{
	PENTERCONS;
	setFrameStyle(QFrame::NoFrame);
	setAlignment(Qt::AlignLeft | Qt::AlignTop);
}

ViewPort::ViewPort(QGraphicsScene* scene, QWidget* parent)
	: QGraphicsView(scene, parent)
        , AbstractViewPort()
        , m_sv(0)
        , m_mode(0)
{
	PENTERCONS;
	setFrameStyle(QFrame::NoFrame);
	setAlignment(Qt::AlignLeft | Qt::AlignTop);
	
        // FIXME This flag causes clips to disappear after mouse leave event,
        // at least when using Qt < 4.6.0 ??
	setOptimizationFlag(DontAdjustForAntialiasing);
        setOptimizationFlag(DontSavePainterState);

	m_holdcursor = new HoldCursor(this);
	scene->addItem(m_holdcursor);
	m_holdcursor->hide();
	// m_holdCursorActive is a replacement for m_holdcursor->isVisible()
	// in mouseMoveEvents, which crashes when a hold action in one viewport
	// ends with the mouse upon a different viewport.
	// Should get a proper fix ?
	m_holdCursorActive = false;
}

ViewPort::~ViewPort()
{
	PENTERDES;
	
	cpointer().set_current_viewport((ViewPort*) 0);
}

bool ViewPort::event(QEvent * event)
{
	// We want Tab events also send to the InputEngine
	// so treat them as 'normal' key events.
	if (event->type() == QEvent::KeyPress) {
		QKeyEvent *ke = static_cast<QKeyEvent *>(event);
		if (ke->key() == Qt::Key_Tab) {
			keyPressEvent(ke);
			return true;
		}
	}
	if (event->type() == QEvent::KeyRelease) {
		QKeyEvent *ke = static_cast<QKeyEvent *>(event);
		if (ke->key() == Qt::Key_Tab) {
			keyReleaseEvent(ke);
			return true;
		}
        }
	return QGraphicsView::event(event);
}

void ViewPort::mouseMoveEvent(QMouseEvent* event)
{
        PENTER4;

        // tells the context pointer where we are, so command object can 'get' the
        // scene position in their jog function from cpointer, or view items that
        // accept mouse hover move 'events'
        cpointer().set_mouse_cursor_position(event->x(), event->y());

        if (cpointer().keyboard_only_input()) {
                event->accept();
                return;
        }

	// Qt generates mouse move events when the scrollbars move
	// since a mouse move event generates a jog() call for the 
	// active holding command, this has a number of nasty side effects :-(
	// For now, we ignore such events....
	if (event->pos() == m_oldMousePos) {
		return;
        }

        m_oldMousePos = event->pos();

        QList<ViewItem*> mouseTrackingItems;
	
	if (!ie().is_holding()) {
		QList<QGraphicsItem *> itemsUnderCursor = scene()->items(mapToScene(event->pos()));
                QList<ContextItem*> activeContextItems;

		if (itemsUnderCursor.size()) {
                        itemsUnderCursor.first()->setCursor(itemsUnderCursor.first()->cursor());


                        foreach(QGraphicsItem* item, itemsUnderCursor) {
				if (ViewItem::is_viewitem(item)) {
					ViewItem* viewItem = (ViewItem*)item;
					if (!viewItem->ignore_context()) {
						activeContextItems.append(viewItem);
						if (viewItem->has_mouse_tracking()) {
							mouseTrackingItems.append(viewItem);
						}
					}
                                }
                        }
                } else {
			// If no item is below the mouse, default to default cursor
			viewport()->setCursor(themer()->get_cursor("Default"));
		}

                // since sheetview has no bounding rect, and should always have 'active context'
                // add it if it's available
                if (m_sv) {
                        activeContextItems.append(m_sv);
                }

                cpointer().set_active_context_items_by_mouse_movement(activeContextItems);


	} else {
		// It can happen that a cursor is set for a newly created viewitem
		// but we don't want that when the holdcursor is set!
		// So force it back to be a blankcursor.
		if (m_holdCursorActive /* was m_holdcursor->isVisible() */ && viewport()->cursor().shape() != Qt::BlankCursor) {
			viewport()->setCursor(Qt::BlankCursor);
		}
	}

        foreach(ViewItem* item, mouseTrackingItems) {
                item->mouse_hover_move_event();
        }

//        EditPointLocation editPoint;
//        editPoint.sceneY = mapToScene(event->pos()).y();
//        if (m_sv) {
//                editPoint.location = m_sv->get_sheet()->get_work_location();
//                m_sv->get_sheet()->set_edit_point_location(editPoint);
//        }
	event->accept();
}


void ViewPort::tabletEvent(QTabletEvent * event)
{
	PMESG("ViewPort tablet event:: x, y: %d, %d", (int)event->x(), (int)event->y());
	PMESG("ViewPort tablet event:: high resolution x, y: %f, %f",
	      event->hiResGlobalX(), event->hiResGlobalY());
        cpointer().set_mouse_cursor_position((int)event->x(), (int)event->y());
	
	QGraphicsView::tabletEvent(event);
}

void ViewPort::enterEvent(QEvent* e)
{
	QGraphicsView::enterEvent(e);
	cpointer().set_current_viewport(this);
	setFocus();
}

void ViewPort::leaveEvent(QEvent *)
{
	cpointer().set_current_viewport(0);
	// Force the next mouse move event to do something
        // even if the mouse didn't move, so switching viewports
        // does update the current context!
        m_oldMousePos = QPoint();
}

void ViewPort::keyPressEvent( QKeyEvent * e)
{
	ie().catch_key_press(e);
	e->accept();
}

void ViewPort::keyReleaseEvent( QKeyEvent * e)
{
	ie().catch_key_release(e);
	e->accept();
}

void ViewPort::mousePressEvent( QMouseEvent * e )
{
	ie().catch_mousebutton_press(e);
	e->accept();
}

void ViewPort::mouseReleaseEvent( QMouseEvent * e )
{
	ie().catch_mousebutton_release(e);
	e->accept();
}

void ViewPort::mouseDoubleClickEvent( QMouseEvent * e )
{
	ie().catch_mousebutton_doubleclick(e);
	e->accept();
}

void ViewPort::wheelEvent( QWheelEvent * e )
{
	ie().catch_scroll(e);
	e->accept();
}

void ViewPort::paintEvent( QPaintEvent* e )
{
// 	PWARN("ViewPort::paintEvent()");
	QGraphicsView::paintEvent(e);
}

void ViewPort::reset_cursor( )
{
	viewport()->unsetCursor();
	m_holdcursor->hide();
	m_holdcursor->reset();
        m_holdCursorActive = false;

        QPoint pos = mapToGlobal(mapFromScene(m_holdcursor->get_scene_pos()));
        QCursor::setPos(pos);
}

void ViewPort::set_cursor_shape(const QString &cursor)
{
        viewport()->setCursor(themer()->get_cursor(cursor));
}

void ViewPort::set_holdcursor( const QString & cursorName )
{
	viewport()->setCursor(Qt::BlankCursor);
	
	if (!m_holdCursorActive) {
                m_holdcursor->set_pos(cpointer().scene_pos());
		m_holdcursor->show();
	}
	m_holdcursor->set_type(cursorName);
	m_holdCursorActive = true;
}

void ViewPort::set_holdcursor_text( const QString & text )
{
	m_holdcursor->set_text(text);
}

void ViewPort::set_holdcursor_pos(QPointF pos)
{
        m_holdcursor->set_pos(pos);
}

QPointF ViewPort::get_hold_cursor_pos() const
{
        return m_holdcursor->get_scene_pos();
}

void ViewPort::set_edit_point_position(QPointF pos)
{
        QList<QGraphicsItem *> itemsUnderCursor = scene()->items(pos);
        QList<ContextItem*> activeContextItems;

        foreach(QGraphicsItem* item, itemsUnderCursor) {
                if (ViewItem::is_viewitem(item)) {
                        ViewItem* vItem = (ViewItem*)item;
                        activeContextItems.append(vItem);
                }
        }
        if (m_sv) {
                activeContextItems.append(m_sv);
        }

        cpointer().set_active_context_items_by_keyboard_input(activeContextItems);

        m_holdcursor->show();
        set_holdcursor_pos(pos);
        update_holdcursor_shape();
}

void ViewPort::update_holdcursor_shape()
{
        QList<ContextItem*> items = cpointer().get_active_context_items();

        if (cpointer().keyboard_only_input() && items.size()) {
                if (items.first()->metaObject()->className() == QString("AudioClipView")) {
                        set_holdcursor(":/cursorFloatOverClip");
                }
                if (items.first()->metaObject()->className() == QString("AudioTrackView")) {
                        set_holdcursor(":/cursorFloatOverTrack");
                }
                if (items.first()->metaObject()->className() == QString("CurveView")) {
                        set_holdcursor(":/cursorDragNode");
                }
        }

}

void ViewPort::hide_mouse_cursor()
{
        viewport()->setCursor(Qt::BlankCursor);
}

void ViewPort::set_current_mode(int mode)
{
	m_mode = mode;
}

void ViewPort::grab_mouse()
{
        viewport()->grabMouse();
}


void ViewPort::release_mouse()
{
        viewport()->releaseMouse();
        // This issues a mouse move event, so the cursor
        // will change to the item that's below it....
//        QCursor::setPos(QCursor::pos()-QPoint(1,1));
}


/**********************************************************************/
/*                      HoldCursor                                    */
/**********************************************************************/


HoldCursor::HoldCursor(ViewPort* vp)
	: m_vp(vp)
{
	m_textItem = new QGraphicsTextItem(this);
	m_textItem->setFont(themer()->get_font("ViewPort:fontscale:infocursor"));

	setZValue(200);
}

HoldCursor::~ HoldCursor( )
{
}

void HoldCursor::paint( QPainter * painter, const QStyleOptionGraphicsItem * option, QWidget * widget )
{
	Q_UNUSED(widget);
	Q_UNUSED(option);

	painter->drawPixmap(0, 0, m_pixmap);
}


void HoldCursor::set_text( const QString & text )
{
	m_text = text;
	
	if (!m_text.isEmpty()) {
		QString html = "<html><body bgcolor=ghostwhite>" + m_text + "</body></html>";
		m_textItem->setHtml(html);
		m_textItem->show();
	} else {
		m_textItem->hide();
	}
}

void HoldCursor::set_type( const QString & type )
{
        QPointF origPos = scenePos();
        origPos.setX(origPos.x() + (qreal(m_pixmap.width()) / 2));
        origPos.setY(origPos.y() + (qreal(m_pixmap.height()) / 2));
        m_pixmap = find_pixmap(type);
        set_pos(origPos);
}

QRectF HoldCursor::boundingRect( ) const
{
        return QRectF(0, 0, m_pixmap.width(), m_pixmap.height());
}

void HoldCursor::reset()
{
	m_text = "";
	m_textItem->hide();
}

void HoldCursor::set_pos(QPointF p)
{
	int x = m_vp->mapFromScene(pos()).x();
	int y = m_vp->mapFromScene(pos()).y();
        int yoffset = m_pixmap.height() + 25;
	
	if (y < 0) {
		yoffset = - y;
	} else if (y > m_vp->height() - m_pixmap.height()) {
		yoffset = m_vp->height() - y - m_pixmap.height();
	}
	
	int diff = m_vp->width() - (x + m_pixmap.width() + 8);
	
	if (diff < m_textItem->boundingRect().width()) {
		m_textItem->setPos(diff - m_pixmap.width(), yoffset);
	} else if (x < -m_pixmap.width()) {
		m_textItem->setPos(8 - x, yoffset);
	} else {
		m_textItem->setPos(m_pixmap.width() + 8, yoffset);
	}

        p.setX(p.x() - (qreal(m_pixmap.width()) / 2));
        p.setY(p.y() - (qreal(m_pixmap.height()) / 2));

        setPos(p);
}

QPointF HoldCursor::get_scene_pos()
{
        QPointF holdcursorShapeAdjust(boundingRect().width() / 2, boundingRect().height() / 2);
        return scenePos() + holdcursorShapeAdjust;
}
