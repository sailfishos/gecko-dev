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
  , m_update_pending(false)
{
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
    if (!m_update_pending) {
        m_update_pending = true;
        QCoreApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
    }
}

void MozQWidget::renderNow()
{
    if (!isExposed())
        return;

    mReceiver->OnQRender();
}

bool MozQWidget::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::UpdateRequest:
        m_update_pending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(event);
    }
}

void MozQWidget::exposeEvent(QExposeEvent* event)
{
    Q_UNUSED(event);
    if (!isExposed())
        return;

    renderNow();
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
}

void MozQWidget::focusInEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::focusInEvent(event);
}

void MozQWidget::focusOutEvent(QFocusEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    MozQWidget::focusOutEvent(event);
}

void MozQWidget::hideEvent(QHideEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::hideEvent(event);
}

void MozQWidget::keyPressEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::keyPressEvent(event);
}

void MozQWidget::keyReleaseEvent(QKeyEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::keyReleaseEvent(event);
}

void MozQWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::mouseDoubleClickEvent(event);
}

void MozQWidget::mouseMoveEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::mouseMoveEvent(event);
}

void MozQWidget::mousePressEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::mousePressEvent(event);
}

void MozQWidget::mouseReleaseEvent(QMouseEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::mouseReleaseEvent(event);
}

void MozQWidget::moveEvent(QMoveEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::moveEvent(event);
}

void MozQWidget::resizeEvent(QResizeEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::resizeEvent(event);
}

void MozQWidget::showEvent(QShowEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::showEvent(event);
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

void MozQWidget::wheelEvent(QWheelEvent* event)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::wheelEvent(event);
}
