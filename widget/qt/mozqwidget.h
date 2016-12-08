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
    virtual void dropReceiver() { mReceiver = nullptr; };
    virtual void SetCursor(nsCursor aCursor);

public Q_SLOTS:
    void renderLater();
    void renderNow();

protected:
    virtual bool event(QEvent *e) override;
    virtual void exposeEvent(QExposeEvent *e) override;
    virtual void focusInEvent(QFocusEvent *e) override;
    virtual void focusOutEvent(QFocusEvent *e) override;
    virtual void hideEvent(QHideEvent *e) override;
    virtual void keyPressEvent(QKeyEvent *e) override;
    virtual void keyReleaseEvent(QKeyEvent *e) override;
    virtual void mouseDoubleClickEvent(QMouseEvent *e) override;
    virtual void mouseMoveEvent(QMouseEvent *e) override;
    virtual void mousePressEvent(QMouseEvent *e) override;
    virtual void mouseReleaseEvent(QMouseEvent *e) override;
    virtual void moveEvent(QMoveEvent *e) override;
    virtual void resizeEvent(QResizeEvent *e) override;
    virtual void showEvent(QShowEvent *e) override;
    virtual void tabletEvent(QTabletEvent *e) override;
    virtual void touchEvent(QTouchEvent *e) override;
    virtual void wheelEvent(QWheelEvent *e) override;

private:
    nsWindow* mReceiver;
    bool mUpdatePending;
    nsWindowType mWindowType;
};

} // namespace widget
} // namespace mozilla

#endif // MOZQWIDGET_H

