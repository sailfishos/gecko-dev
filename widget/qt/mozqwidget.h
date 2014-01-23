/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZQWIDGET_H
#define MOZQWIDGET_H

#include "nsIWidget.h"

#include <QtGui/QWindow>

QT_BEGIN_NAMESPACE
class QPainter;
class QExposeEvent;
class QResizeEvent;
QT_END_NAMESPACE

namespace mozilla {
namespace widget {

class nsWindow;

class MozQWidget : public QWindow
{
    Q_OBJECT
public:
    explicit MozQWidget(nsWindow* aReceiver, QWindow* aParent = 0);
    ~MozQWidget();

    virtual void render(QPainter* painter);

    virtual nsWindow* getReceiver() { return mReceiver; };

public Q_SLOTS:
    void renderLater();
    void renderNow();

protected:
    bool event(QEvent* event);

    void resizeEvent(QResizeEvent* event);
    void exposeEvent(QExposeEvent* event);

private:
    nsWindow* mReceiver;
    bool m_update_pending;
};

} // namespace widget
} // namespace mozilla

#endif // MOZQWIDGET_H

