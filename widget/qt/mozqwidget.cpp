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
    mWindowType = mReceiver->WindowType();
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

bool MozQWidget::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::UpdateRequest:
        mUpdatePending = false;
        renderNow();
        return true;
    default:
        return QWindow::event(e);
    }
}

void MozQWidget::exposeEvent(QExposeEvent* e)
{
    Q_UNUSED(e);
    if (!isExposed() || eWindowType_child != mWindowType || !isVisible()) {
        return;
    }
    LOG(("MozQWidget::%s [%p] flags:%x\n", __FUNCTION__, (void *)this, flags()));
    renderNow();

}

void MozQWidget::resizeEvent(QResizeEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->resizeEvent(e);
    QWindow::resizeEvent(e);
}

void MozQWidget::focusInEvent(QFocusEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusInEvent(e);
    QWindow::focusInEvent(e);
}

void MozQWidget::focusOutEvent(QFocusEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->focusOutEvent(e);
    QWindow::focusOutEvent(e);
}

void MozQWidget::hideEvent(QHideEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->hideEvent(e);
    QWindow::hideEvent(e);
}

void MozQWidget::keyPressEvent(QKeyEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyPressEvent(e);
    QWindow::keyPressEvent(e);
}

void MozQWidget::keyReleaseEvent(QKeyEvent *e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->keyReleaseEvent(e);
    QWindow::keyReleaseEvent(e);
}

void MozQWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseDoubleClickEvent(e);
    QWindow::mouseDoubleClickEvent(e);
}

void MozQWidget::mouseMoveEvent(QMouseEvent* e)
{
    mReceiver->mouseMoveEvent(e);
    QWindow::mouseMoveEvent(e);
}

void MozQWidget::mousePressEvent(QMouseEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mousePressEvent(e);
    QWindow::mousePressEvent(e);
}

void MozQWidget::mouseReleaseEvent(QMouseEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->mouseReleaseEvent(e);
    QWindow::mouseReleaseEvent(e);
}

void MozQWidget::moveEvent(QMoveEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->moveEvent(e);
    QWindow::moveEvent(e);
}

void MozQWidget::showEvent(QShowEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->showEvent(e);
    QWindow::showEvent(e);
}

void MozQWidget::wheelEvent(QWheelEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    mReceiver->wheelEvent(e);
    QWindow::wheelEvent(e);
}

void MozQWidget::tabletEvent(QTabletEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::tabletEvent(e);
}

void MozQWidget::touchEvent(QTouchEvent* e)
{
    LOG(("MozQWidget::%s [%p]\n", __FUNCTION__, (void *)this));
    QWindow::touchEvent(e);
}

void MozQWidget::SetCursor(nsCursor aCursor)
{
    Qt::CursorShape c = Qt::ArrowCursor;
    switch(aCursor) {
    case eCursor_standard:
        c = Qt::ArrowCursor;
        break;
    case eCursor_wait:
        c = Qt::WaitCursor;
        break;
    case eCursor_select:
        c = Qt::IBeamCursor;
        break;
    case eCursor_hyperlink:
        c = Qt::PointingHandCursor;
        break;
    case eCursor_ew_resize:
        c = Qt::SplitHCursor;
        break;
    case eCursor_ns_resize:
        c = Qt::SplitVCursor;
        break;
    case eCursor_nw_resize:
    case eCursor_se_resize:
        c = Qt::SizeBDiagCursor;
        break;
    case eCursor_ne_resize:
    case eCursor_sw_resize:
        c = Qt::SizeFDiagCursor;
        break;
    case eCursor_crosshair:
    case eCursor_move:
        c = Qt::SizeAllCursor;
        break;
    case eCursor_help:
        c = Qt::WhatsThisCursor;
        break;
    case eCursor_copy:
    case eCursor_alias:
        break;
    case eCursor_context_menu:
    case eCursor_cell:
    case eCursor_grab:
    case eCursor_grabbing:
    case eCursor_spinning:
    case eCursor_zoom_in:
    case eCursor_zoom_out:

    default:
        break;
    }

    setCursor(c);
}
