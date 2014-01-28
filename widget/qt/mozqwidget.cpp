/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 et sw=4 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <QtCore/QCoreApplication>
#include <QtGui/QResizeEvent>

#include "mozqwidget.h"
#include "nsWindow.h"

using namespace mozilla::widget;

MozQWidget::MozQWidget(nsWindow* aReceiver, QWindow* aParent)
  : QWindow(aParent)
  , mReceiver(aReceiver)
  , mUpdatePending(false)
{
    mReceiver->GetWindowType(mWindowType);
}

MozQWidget::~MozQWidget()
{
}

void MozQWidget::render(QPainter* painter)
{
    Q_UNUSED(painter);
}

void MozQWidget::renderLater()
{
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }

    if (!mUpdatePending) {
        mUpdatePending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void MozQWidget::renderNow()
{
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }

    mReceiver->OnPaint();
}

bool MozQWidget::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        mUpdatePending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

void MozQWidget::exposeEvent(QExposeEvent* event)
{
    Q_UNUSED(event);
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }
    LOG(("MozQWidget::%s [%p] flags:%x\n", __FUNCTION__, (void *)this, flags()));
    renderNow();

}

void MozQWidget::resizeEvent(QResizeEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->resizeEvent(event);
    QWindow::resizeEvent(event);
}

void MozQWidget::focusInEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusInEvent(event);
    QWindow::focusInEvent(event);
}

void MozQWidget::focusOutEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusOutEvent(event);
    QWindow::focusOutEvent(event);
}

void MozQWidget::hideEvent(QHideEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->hideEvent(event);
    QWindow::hideEvent(event);
}

void MozQWidget::keyPressEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyPressEvent(event);
    QWindow::keyPressEvent(event);
}

void MozQWidget::keyReleaseEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyReleaseEvent(event);
    QWindow::keyReleaseEvent(event);
}

void MozQWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseDoubleClickEvent(event);
    QWindow::mouseDoubleClickEvent(event);
}

void MozQWidget::mouseMoveEvent(QMouseEvent* event)
{
    mReceiver->mouseMoveEvent(event);
    QWindow::mouseMoveEvent(event);
}

void MozQWidget::mousePressEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mousePressEvent(event);
    QWindow::mousePressEvent(event);
}

void MozQWidget::mouseReleaseEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseReleaseEvent(event);
    QWindow::mouseReleaseEvent(event);
}

void MozQWidget::moveEvent(QMoveEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->moveEvent(event);
    QWindow::moveEvent(event);
}

void MozQWidget::showEvent(QShowEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->showEvent(event);
    QWindow::showEvent(event);
}

void MozQWidget::wheelEvent(QWheelEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->wheelEvent(event);
    QWindow::wheelEvent(event);
}

void MozQWidget::tabletEvent(QTabletEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::tabletEvent(event);
}

void MozQWidget::touchEvent(QTouchEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::touchEvent(event);
}
