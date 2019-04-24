/*
Copyright (C) 2005-2007 Remon Sijrier 

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

#ifndef CONTEXTITEM_H
#define CONTEXTITEM_H

#include <QObject>
#include <QPointer>
#include "ContextPointer.h"

class TCommand;
class QUndoStack;
class QUndoGroup;

/**
 * \class ContextItem
 * \brief Interface class for objects (both core and gui) that operate in a 'Soft Selection'
 *
    Each core object that has/can have a visual representation should inherit from this class.

    Only core objects that inherit this class need to set the historystack which <br />
    they need to create/get themselves.
 */

class ContextItem : public QObject
{
    Q_OBJECT
public:
    ContextItem(QObject* parent)
        : QObject(parent)
        , m_hs(nullptr)
        , m_ignoreContext(false)
        , m_contextItem(nullptr)
        , m_hasActiveContext(false)
    {}

    ContextItem()
        : ContextItem(nullptr)
    {}

    virtual ~ContextItem() {
        if (m_hasActiveContext) {
            cpointer().about_to_delete(this);
        }
    }

    ContextItem* get_context() const {return m_contextItem;}

    QUndoStack* get_history_stack() const {return m_hs;}
    qint64 get_id() const {return m_id;}

    void set_history_stack(QUndoStack* hs) {m_hs = hs;}
    void set_context_item(ContextItem* item) {m_contextItem = item;}
    void set_has_active_context(bool context) {
        if (context == m_hasActiveContext) {
            return;
        }

        m_hasActiveContext = context;

        if (m_contextItem) {
            m_contextItem->set_has_active_context(context);
        }

        emit activeContextChanged();
    }
    bool has_active_context() const {return m_hasActiveContext;}
    bool ignore_context() const {return m_ignoreContext;}


protected:
    QUndoStack*     m_hs;
    qint64          m_id;
    bool            m_ignoreContext;

private:
    QPointer<ContextItem>    m_contextItem;
    bool                     m_hasActiveContext;

signals:
    void activeContextChanged();
};

#endif

//eof
