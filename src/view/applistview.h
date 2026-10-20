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

#ifndef APPLISTVIEW_H
#define APPLISTVIEW_H

#include <QListView>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <DWindowManagerHelper>

#define DRAG_SCROLL_THRESHOLD 25

class SmoothScroller;
class QVariantAnimation;

DWIDGET_USE_NAMESPACE
//DGUI_USE_NAMESPACE
DCORE_USE_NAMESPACE

class AppListView : public QListView
{
    Q_OBJECT

public:
    explicit AppListView(QWidget *parent = 0);

    using QListView::indexAt;
    const QModelIndex indexAt(const int index) const;
    void stopScrolling();

signals:
    void popupMenuRequested(const QPoint &globalPos, const QPoint &surfacePos,
                            const QModelIndex &index) const;
    void requestScrollUp() const;
    void requestScrollDown() const;
    void requestScrollStop() const;
    void requestSwitchToCategory(const QModelIndex &index) const;
    void requestEnter(bool enter) const;

protected:
    void currentChanged(const QModelIndex &current, const QModelIndex &previous) override;
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight,
                     const QList<int> &roles = QList<int>()) override;
    void reset() override;
    void paintEvent(QPaintEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dragLeaveEvent(QDragLeaveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void startDrag(const QModelIndex &index);

private:
    void syncHighlight();
    void fadeHighlight(qreal opacity);
    QRectF highlightRect(const QModelIndex &index) const;
    void handleScrollValueChanged();
    void handleScrollFinished();
    void prepareDropSwap();
    void createFakeAnimation(const int pos, const bool moveNext, const bool isLastAni = false);
    void dropSwap();

private:
    int m_dropToRow;
    int m_dragStartRow;
    QPoint m_dragStartPos;
    QTimer *m_dropThresholdTimer;
    bool m_enableDropInside = false;

    QPropertyAnimation *m_lastFakeAni = nullptr;
    SmoothScroller *m_scroller;
    // 悬停高亮由视图绘制，在条目之间滑动；位置使用内容坐标，滚动时跟随内容
    QVariantAnimation *m_highlightMoveAni;
    QVariantAnimation *m_highlightFadeAni;
    QRectF m_highlightRect;
    qreal m_highlightOpacity = 0;
    QGraphicsOpacityEffect *m_opacityEffect;
    DWindowManagerHelper *m_wmHelper;
    double m_speedTime = 2.0;

    QTimer *m_updateEnableSelectionByMouseTimer;
    QPoint m_lastTouchBeginPos;
    int touchTapDistance = -1;
};

#endif // APPLISTVIEW_H
