/*
 * Copyright (C) 2017 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "applistview.h"
#include "src/global_util/constants.h"
#include "src/delegate/applistdelegate.h"
#include "src/model/appslistmodel.h"
#include "src/widgets/smoothscroller.h"

#include <QStyleOptionViewItem>
#include <QPropertyAnimation>
#include <QApplication>
#include <QMouseEvent>
#include <QScrollBar>
#include <QPainter>
#include <QTimer>
#include <QLabel>
#include <QDebug>
#include <QDrag>
#include <QScroller>
#include <QVariantAnimation>
#include <private/qguiapplication_p.h>
#include <qpa/qplatformtheme.h>

AppListView::AppListView(QWidget *parent)
    : QListView(parent)
    , m_dropThresholdTimer(new QTimer(this))
    , m_scroller(new SmoothScroller(this, [this](int offset) { setViewportMargins(0, offset, 0, -offset); }))
    , m_highlightMoveAni(new QVariantAnimation(this))
    , m_highlightFadeAni(new QVariantAnimation(this))
    , m_opacityEffect(new QGraphicsOpacityEffect(this))
    , m_wmHelper(DWindowManagerHelper::instance())
    , m_updateEnableSelectionByMouseTimer(nullptr)
{
    m_scroller->setWheelStep(m_speedTime);

    m_highlightMoveAni->setDuration(160);
    m_highlightMoveAni->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_highlightMoveAni, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_highlightRect = value.toRectF();
        viewport()->update();
    });

    m_highlightFadeAni->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_highlightFadeAni, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        m_highlightOpacity = value.toReal();
        viewport()->update();
    });

    horizontalScrollBar()->setEnabled(false);
    setFocusPolicy(Qt::NoFocus);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameStyle(NoFrame);
    setVerticalScrollMode(ScrollPerPixel);
    setSelectionMode(SingleSelection);
    setSpacing(0);
    setContentsMargins(0, 0, 0, 0);
    setMouseTracking(true);
    setFixedWidth(300);
    setStyleSheet("AppListView, #qt_scrollarea_viewport { background-color: rgba(255, 255, 255, 0); }");
    verticalScrollBar()->setContextMenuPolicy(Qt::NoContextMenu);

    // support drag and drop.
    setDragDropMode(QAbstractItemView::DragDrop);
    setMovement(QListView::Free);
    setDragEnabled(true);

    // init opacity effect.
    m_opacityEffect->setOpacity(1);
    setGraphicsEffect(m_opacityEffect);

    // init drop threshold timer.
    m_dropThresholdTimer->setInterval(DLauncher::APP_DRAG_SWAP_THRESHOLD);
    m_dropThresholdTimer->setSingleShot(true);

    setVerticalScrollMode(QListView::ScrollPerPixel);
    QScroller::grabGesture(this, QScroller::LeftMouseButtonGesture);

#ifndef DISABLE_DRAG_ANIMATION
    connect(m_dropThresholdTimer, &QTimer::timeout, this, &AppListView::prepareDropSwap, Qt::QueuedConnection);
#else
    connect(m_dropThresholdTimer, &QTimer::timeout, this, &AppListView::dropSwap);
#endif

#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    touchTapDistance = 15;
#else
    touchTapDistance = QGuiApplicationPrivate::platformTheme()->themeHint(QPlatformTheme::TouchDoubleTapDistance).toInt();
#endif

    connect(m_scroller, &SmoothScroller::scrolled, this, &AppListView::handleScrollValueChanged);
    connect(m_scroller, &SmoothScroller::scrollFinished, this, &AppListView::handleScrollFinished);
}

const QModelIndex AppListView::indexAt(const int index) const
{
    return model()->index(index, 0, QModelIndex());
}

void AppListView::currentChanged(const QModelIndex &current, const QModelIndex &previous)
{
    QListView::currentChanged(current, previous);

    syncHighlight();
}

void AppListView::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles)
{
    QListView::dataChanged(topLeft, bottomRight, roles);

    // 鼠标移入移出时模型会切换 DrawBackgroundRole，高亮随之淡入淡出
    syncHighlight();
}

void AppListView::reset()
{
    QListView::reset();

    // 模型重置后条目全部换掉，直接隐藏高亮，不从旧位置滑过去
    m_highlightMoveAni->stop();
    m_highlightFadeAni->stop();
    m_highlightOpacity = 0;
}

void AppListView::paintEvent(QPaintEvent *e)
{
    const QModelIndex current = currentIndex();

    // 没有在滑动时，让高亮贴合当前条目，避免布局变化后错位
    if (m_highlightMoveAni->state() != QAbstractAnimation::Running && current.isValid())
        m_highlightRect = highlightRect(current);

    if (m_highlightOpacity > 0) {
        QPainter painter(viewport());
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, qRound(255 * .1 * m_highlightOpacity)));
        painter.drawRoundedRect(m_highlightRect.translated(0, -verticalOffset()), 4, 4);
    }

    QListView::paintEvent(e);
}

void AppListView::syncHighlight()
{
    const QModelIndex current = currentIndex();
    const bool visible = current.isValid() && current.data(AppsListModel::DrawBackgroundRole).toBool();

    if (visible) {
        const QRectF target = highlightRect(current);

        if (qFuzzyIsNull(m_highlightOpacity)) {
            // 从无到有时直接出现在目标位置再淡入，不从上一次的位置滑过来
            m_highlightMoveAni->stop();
            m_highlightRect = target;
        } else if (target != m_highlightRect
                   && !(m_highlightMoveAni->state() == QAbstractAnimation::Running
                        && m_highlightMoveAni->endValue().toRectF() == target)) {
            m_highlightMoveAni->stop();
            m_highlightMoveAni->setStartValue(m_highlightRect);
            m_highlightMoveAni->setEndValue(target);
            m_highlightMoveAni->start();
        }
    }

    fadeHighlight(visible ? 1 : 0);
}

void AppListView::fadeHighlight(qreal opacity)
{
    const bool running = m_highlightFadeAni->state() == QAbstractAnimation::Running;
    if (running ? qFuzzyCompare(m_highlightFadeAni->endValue().toReal(), opacity)
                : qFuzzyCompare(m_highlightOpacity + 1, opacity + 1))
        return;

    m_highlightFadeAni->stop();
    m_highlightFadeAni->setDuration(opacity > m_highlightOpacity ? 120 : 200);
    m_highlightFadeAni->setStartValue(m_highlightOpacity);
    m_highlightFadeAni->setEndValue(opacity);
    m_highlightFadeAni->start();
}

QRectF AppListView::highlightRect(const QModelIndex &index) const
{
    return QRectF(visualRect(index).marginsRemoved(QMargins(1, 1, 1, 1))).translated(0, verticalOffset());
}

void AppListView::stopScrolling()
{
    m_scroller->stop();
}

void AppListView::wheelEvent(QWheelEvent *e)
{
    m_scroller->handleWheel(e);
}

void AppListView::mouseMoveEvent(QMouseEvent *e)
{
    if (e->source() == Qt::MouseEventSynthesizedByQt) {
        if (QScroller::hasScroller(this)) {
            return;
        }

        if (m_updateEnableSelectionByMouseTimer && m_updateEnableSelectionByMouseTimer->isActive()) {
            const QPoint difference_pos = e->pos() - m_lastTouchBeginPos;
            if (qAbs(difference_pos.x()) > touchTapDistance || qAbs(difference_pos.y()) > touchTapDistance) {
                QScroller::grabGesture(this);
                QScroller *scroller = QScroller::scroller(this);
                QScrollerProperties sp;
                sp.setScrollMetric(QScrollerProperties::VerticalOvershootPolicy, QScrollerProperties::OvershootWhenScrollable);
                scroller->setScrollerProperties(sp);

                scroller->handleInput(QScroller::InputPress, e->localPos(), e->timestamp());
                scroller->handleInput(QScroller::InputMove, e->localPos(), e->timestamp());

                connect(scroller, &QScroller::stateChanged, this, [=](QScroller::State newstate) {
                    if (newstate == QScroller::Inactive) {
                        QScroller::scroller(this)->deleteLater();
                    }
                });
            }
            return;
        }
    }

    setState(NoState);
    blockSignals(false);

    const QModelIndex &index = indexAt(e->pos());
    const QPoint pos = e->pos();

    if (index.isValid() && !m_enableDropInside)
        Q_EMIT entered(index);
    else
        Q_EMIT entered(QModelIndex());

    if (e->button() != Qt::LeftButton)
        return;

    if (qAbs(pos.x() - m_dragStartPos.x()) > DLauncher::DRAG_THRESHOLD ||
        qAbs(pos.y() - m_dragStartPos.y()) > DLauncher::DRAG_THRESHOLD) {
        m_dragStartPos = e->pos();
        m_dragStartRow = index.row();
        return startDrag(index);
    }

    QListView::mouseMoveEvent(e);
}

void AppListView::mousePressEvent(QMouseEvent *e)
{
    // 拖动滚动由 QScroller 接管，先停掉滚轮滚动并清掉越界偏移，避免两者同时移动 viewport
    m_scroller->stop();

    if (e->source() == Qt::MouseEventSynthesizedByQt) {
        m_lastTouchBeginPos = e->pos();

        if (m_updateEnableSelectionByMouseTimer) {
            m_updateEnableSelectionByMouseTimer->stop();
        }
        else {
            m_updateEnableSelectionByMouseTimer = new QTimer(this);
            m_updateEnableSelectionByMouseTimer->setSingleShot(true);
            m_updateEnableSelectionByMouseTimer->setInterval(500);

            connect(m_updateEnableSelectionByMouseTimer, &QTimer::timeout, this, [=] {
                m_updateEnableSelectionByMouseTimer->deleteLater();
                m_updateEnableSelectionByMouseTimer = nullptr;
            });
        }
        m_updateEnableSelectionByMouseTimer->start();
        return;
    }

    const QModelIndex &index = indexAt(e->pos());
    if (!index.isValid())
        e->ignore();

    const bool isCategoryList = qobject_cast<AppsListModel*>(model())->category() == AppsListModel::Category;

    if (e->button() == Qt::RightButton && !isCategoryList) {
        const QPoint rightClickPoint = QCursor::pos();
        const QPoint surfacePoint = mapTo(window(), e->pos());
        const QModelIndex &clickedIndex = QListView::indexAt(e->pos());

        if (clickedIndex.isValid())
            emit popupMenuRequested(rightClickPoint, surfacePoint, clickedIndex);
    }

    if (e->button() == Qt::LeftButton) {
        if (isCategoryList) {
            return;
        } else {
            m_dragStartPos = e->pos();
            m_dragStartRow = indexAt(e->pos()).row();
        }
    }

    QListView::mousePressEvent(e);
}

void AppListView::mouseReleaseEvent(QMouseEvent *e)
{
    if (QScroller::hasScroller(this)) {
        QScroller::scroller(this)->deleteLater();
        return;
    }

    const QModelIndex &index = indexAt(e->pos());
    if (!index.isValid())
        e->ignore();

    if (qobject_cast<AppsListModel*>(model())->category() == AppsListModel::Category && e->button() == Qt::LeftButton) {
        emit requestSwitchToCategory(index);
        return;
    }

    if (e->source() == Qt::MouseEventSynthesizedByQt) {
        // reissue event
        emit clicked(index);
    }

    QListView::mouseReleaseEvent(e);
}

void AppListView::dragEnterEvent(QDragEnterEvent *e)
{
    const QModelIndex index = indexAt(e->pos());

    if (model()->canDropMimeData(e->mimeData(), e->dropAction(), index.row(), index.column(), QModelIndex())) {
        // to enable transparent.
        m_opacityEffect->setOpacity(0.5);

        return e->accept();
    }
}

void AppListView::dragMoveEvent(QDragMoveEvent *e)
{
    if (m_lastFakeAni)
        return;

    const QModelIndex dropIndex = QListView::indexAt(e->pos());
    if (dropIndex.isValid())
        m_dropToRow = dropIndex.row();

    m_dropThresholdTimer->stop();

    const QPoint pos = e->pos();
    const QRect rect = this->rect();

    if (pos.y() < DRAG_SCROLL_THRESHOLD) {
        Q_EMIT requestScrollUp();
    } else if (pos.y() > rect.height() - DRAG_SCROLL_THRESHOLD) {
        Q_EMIT requestScrollDown();
    } else {
        Q_EMIT requestScrollStop();

        // rolling without swapping.
        m_dropThresholdTimer->start();
    }

    // drag move does not allow to have selected effect.
    Q_EMIT entered(QModelIndex());
}

void AppListView::dragLeaveEvent(QDragLeaveEvent *e)
{
    e->accept();

    // drag leave will also restore opacity.
    m_opacityEffect->setOpacity(1);
    m_dropThresholdTimer->stop();

    Q_EMIT requestScrollStop();
}

void AppListView::dropEvent(QDropEvent *e)
{
    e->accept();

    // restore opacity.
    m_opacityEffect->setOpacity(1);
    m_enableDropInside = true;
}

void AppListView::enterEvent(QEnterEvent *event)
{
    QListView::enterEvent(event);

    emit requestEnter(true);
}

void AppListView::leaveEvent(QEvent *event)
{
    QListView::leaveEvent(event);

    emit requestEnter(false);
}

void AppListView::startDrag(const QModelIndex &index)
{
    if (!index.isValid())
        return;

    AppsListModel *listModel = qobject_cast<AppsListModel *>(model());
    if (!listModel || listModel->category() != AppsListModel::All)
        return;

    const QModelIndex &dragIndex = index;
    const QRect index_rect = visualRect(index);
    const auto ratio = devicePixelRatioF();
    const QSize rectSize = index_rect.size();

    // QPoint(50, 42) is shadow radius
    const QPoint hotSpot = m_dragStartPos - index_rect.topLeft() + QPoint(50, 42)  / ratio;

    QPixmap dropPixmap;
    QImage sourcePixmap(rectSize * ratio, QImage::Format_ARGB32_Premultiplied);
    sourcePixmap.fill(Qt::transparent);
    sourcePixmap.setDevicePixelRatio(ratio);

    QPainter painter(&sourcePixmap);
    QStyleOptionViewItem item;
    item.rect = QRect(QPoint(0, 0), rectSize);
    item.features |= QStyleOptionViewItem::Alternate;

    itemDelegate()->paint(&painter, item, index);
    dropPixmap = QPixmap::fromImage(sourcePixmap);

    // wm support transparent to draw a shadow effect.
    if (m_wmHelper->hasComposite()) {
        QColor shadowColor("#2CA7F8");
        shadowColor.setAlpha(180);
        dropPixmap = AppListDelegate::dropShadow(dropPixmap, 50, shadowColor, QPoint(0, 8));
        dropPixmap.setDevicePixelRatio(ratio);
    }

    QDrag *drag = new QDrag(this);
    drag->setMimeData(model()->mimeData(QModelIndexList() << dragIndex));
    drag->setPixmap(dropPixmap);
    drag->setHotSpot(hotSpot);

    // request remove current item.
    if (listModel->category() == AppsListModel::All) {
        m_dropToRow = index.row();
        listModel->setDraggingIndex(index);
    }

    setState(DraggingState);
    drag->exec(Qt::MoveAction);

    // disable animation when finally dropped
    m_dropThresholdTimer->stop();

    // disable auto scroll
    Q_EMIT requestScrollStop();

    if (listModel->category() != AppsListModel::All)
        return;

    if (!m_lastFakeAni) {
        if (m_enableDropInside)
            listModel->dropSwap(m_dropToRow);
        else
            listModel->dropSwap(m_dragStartRow);

        listModel->clearDraggingIndex();
    } else {
        connect(m_lastFakeAni, &QPropertyAnimation::finished, listModel, &AppsListModel::clearDraggingIndex);
    }

    m_enableDropInside = false;
}

void AppListView::handleScrollValueChanged()
{
    QScrollBar *vscroll = verticalScrollBar();

    if (vscroll->value() == vscroll->maximum() ||
        vscroll->value() == vscroll->minimum()) {
        blockSignals(false);
    } else {
        blockSignals(true);
    }
}

void AppListView::handleScrollFinished()
{
    blockSignals(false);

    QPoint pos = mapFromGlobal(QCursor::pos());
    emit entered(indexAt(pos));
}

void AppListView::prepareDropSwap()
{
    if (m_lastFakeAni || m_dropThresholdTimer->isActive())
        return;

    const QModelIndex dropIndex = indexAt(m_dropToRow);
    if (!dropIndex.isValid())
        return;

    const QModelIndex dragStartIndex = indexAt(m_dragStartRow);
    if (dropIndex == dragStartIndex)
        return;

    if (!dragStartIndex.isValid()) {
        m_dragStartRow = dropIndex.row();
        return;
    }

    AppsListModel *listModel = qobject_cast<AppsListModel *>(model());
    if (!listModel)
        return;

    listModel->clearDraggingIndex();
    listModel->setDraggingIndex(dragStartIndex);
    listModel->setDragDropIndex(dropIndex);

    const int startIndex = dragStartIndex.row();
    const bool moveToNext = startIndex <= m_dropToRow;
    const int start = moveToNext ? startIndex : m_dropToRow;
    const int end = !moveToNext ? startIndex : m_dropToRow;

    if (start == end)
        return;

    for (int i = start + moveToNext; i != end - !moveToNext; ++i) {
        createFakeAnimation(i, moveToNext);
    }

    // last animation.
    createFakeAnimation(end - !moveToNext, moveToNext, true);

    m_dragStartRow = dropIndex.row();
}

void AppListView::createFakeAnimation(const int pos, const bool moveNext, const bool isLastAni)
{
    const QModelIndex index(indexAt(pos));

    QLabel *floatLabel = new QLabel(this);
    QPropertyAnimation *animation = new QPropertyAnimation(floatLabel, "pos", floatLabel);

    const auto ratio = devicePixelRatioF();
    const QSize rectSize(300, 36);

    QPixmap pixmap(rectSize * ratio);
    pixmap.fill(Qt::transparent);
    pixmap.setDevicePixelRatio(ratio);

    QStyleOptionViewItem item;
    item.rect = QRect(QPoint(0, 0), rectSize);
    item.features |= QStyleOptionViewItem::HasDisplay;

    QPainter painter(&pixmap);
    itemDelegate()->paint(&painter, item, index);

    floatLabel->setFixedSize(rectSize);
    floatLabel->setPixmap(pixmap);
    floatLabel->show();

    animation->setStartValue(visualRect(index).topLeft());
    animation->setEndValue(visualRect(indexAt(moveNext ? pos - 1 : pos + 1)).topLeft());
    animation->setEasingCurve(QEasingCurve::OutCubic);
    animation->setDuration(250);

    connect(animation, &QPropertyAnimation::finished, floatLabel, &QLabel::deleteLater);

    if (isLastAni) {
        m_lastFakeAni = animation;
        connect(animation, &QPropertyAnimation::finished, this, &AppListView::dropSwap, Qt::QueuedConnection);
        connect(animation, &QPropertyAnimation::valueChanged, m_dropThresholdTimer, &QTimer::stop);
    }

    animation->start();
}

void AppListView::dropSwap()
{
    AppsListModel *listModel = qobject_cast<AppsListModel *>(model());

    if (!listModel)
        return;

    listModel->dropSwap(m_dropToRow);

    m_lastFakeAni = nullptr;
    m_dragStartRow = m_dropToRow;

    setState(NoState);
}
