/*
 * Hedgewars, a free turn based strategy game
 * Copyright (c) 2006-2007 Igor Ulyanov <iulyanov@gmail.com>
 * Copyright (c) 2007-2011 Andrey Korotaev <unC0Rr@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <QStringList>

#include "SmartLineEdit.h"

HistoryLineEdit::HistoryLineEdit(QWidget * parent, int maxHistorySize)
: QLineEdit(parent)
{
    m_curHistEntryIdx = 0;
    m_maxHistorySize = maxHistorySize;
    m_history = new QStringList();
}


void HistoryLineEdit::rememberCurrentText()
{
    m_historyMutex.lock();

    rememberCurrentTextUnsynced();

    m_historyMutex.unlock();
}


void HistoryLineEdit::rememberCurrentTextUnsynced()
{
    QString newEntry = text();

    // don't store whitespace-only/empty text
    if (newEntry.trimmed().isEmpty())
        return;

    m_history->removeOne(newEntry); // no duplicates please
    m_history->append(newEntry);

    // do not keep more entries than allowed
    if (m_history->size() > m_maxHistorySize)
        m_history->removeFirst();

    // we're looking at the latest entry
    m_curHistEntryIdx = m_history->size() - 1;
}


void HistoryLineEdit::clear()
{
    m_historyMutex.lock();

    QLineEdit::clear();
    m_curHistEntryIdx = m_history->size();

    m_historyMutex.unlock();
}


void HistoryLineEdit::reset()
{
    // forget history
    m_historyMutex.lock();

    m_history->clear();
    m_curHistEntryIdx = 0;

    m_historyMutex.unlock();
}


void HistoryLineEdit::navigateHistory(bool isGoingUp)
{
    m_historyMutex.lock();

    // save possible changes to new entry
    if ((m_curHistEntryIdx >= m_history->size() ||
        (text() != m_history->at(m_curHistEntryIdx))))
        {
            rememberCurrentTextUnsynced();
        }

    if (isGoingUp)
        m_curHistEntryIdx--;
    else
        m_curHistEntryIdx++;

    // if Idx higher than valid range
    if (m_curHistEntryIdx >= m_history->size())
    {
        QLineEdit::clear();
        m_curHistEntryIdx = m_history->size();
    }
    // if Idx in valid range
    else if (m_curHistEntryIdx >= 0)
    {
        setText(m_history->at(m_curHistEntryIdx));
    }
    // if Idx below 0
    else
        m_curHistEntryIdx = 0;


    m_historyMutex.unlock();
}


void HistoryLineEdit::keyPressEvent(QKeyEvent * event)
{
    int key = event->key(); // retrieve pressed key

    // navigate history with arrow keys
    if (event->modifiers() == Qt::NoModifier)
        switch (key)
        {
            case Qt::Key_Escape:
                clear();
                event->accept();
                break;

            case Qt::Key_Up:
                navigateHistory(true);
                event->accept();
                break;

            case Qt::Key_Down:
                navigateHistory(false);
                event->accept();
                break;

            default:
                QLineEdit::keyPressEvent(event);
                break;
        }
    // otherwise forward keys to parent
    else
        QLineEdit::keyPressEvent(event);
}
