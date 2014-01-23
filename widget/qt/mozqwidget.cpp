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

void MozQWidget::render(QPainter *painter)
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

bool MozQWidget::event(QEvent *event)
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

void MozQWidget::resizeEvent(QResizeEvent* event)
{
}

void MozQWidget::exposeEvent(QExposeEvent *event)
{
    Q_UNUSED(event);
    if (isExposed()) {
        renderNow();
    }
}

void MozQWidget::renderNow()
{
    if (!isExposed())
        return;

    mReceiver->OnQRender();
}
