#ifndef SMOOTHSCROLLER_H
#define SMOOTHSCROLLER_H

#include <QElapsedTimer>
#include <QObject>

#include <functional>

class QAbstractAnimation;
class QAbstractScrollArea;
class QScrollBar;
class QTimer;
class QVariantAnimation;
class QWheelEvent;

// 为滚动区域提供平滑滚轮滚动、触控板惯性滚动和边缘回弹。
// 越界偏移通过 setViewportMargins 实现，它是 protected 的，所以由调用方在回调里应用。
class SmoothScroller : public QObject
{
    Q_OBJECT

public:
    // offset > 0 表示内容被向下拉（顶部越界），< 0 表示内容被向上推（底部越界）
    using OffsetSetter = std::function<void(int offset)>;

    SmoothScroller(QAbstractScrollArea *area, const OffsetSetter &setOffset);

    void handleWheel(QWheelEvent *e);
    void stop();

    // 每 1/8 度滚轮角度滚动的像素数；不设置时沿用 Qt 默认的 wheelScrollLines * singleStep
    void setWheelStep(qreal step);

signals:
    void scrollStarted();
    void scrolled();
    void scrollFinished();

private:
    void handleTouchpad(QWheelEvent *e);
    void applyFingerDelta(int delta);
    void releaseFinger();
    void kick(int excess);
    void tick(qreal dt);
    void startPhysics();
    void applyOffset();
    void maybeFinish();
    qreal maxOffset() const;
    qreal rubberBand(qreal stretch) const;
    qreal inverseRubberBand(qreal offset) const;
    QScrollBar *bar() const;

private:
    QAbstractScrollArea *m_area;
    OffsetSetter m_setOffset;
    qreal m_wheelStep = 0;

    QVariantAnimation *m_wheelAni;
    int m_wheelTarget = 0;
    int m_pendingExcess = 0;

    QTimer *m_fingerReleaseTimer;
    QElapsedTimer m_lastFingerMove;
    bool m_fingerDown = false;
    qreal m_stretch = 0;
    qreal m_fingerVelocity = 0;

    QAbstractAnimation *m_physics;
    qreal m_pos = 0;
    qreal m_momentum = 0;
    qreal m_offset = 0;
    qreal m_offsetVelocity = 0;
    int m_appliedOffset = 0;
};

#endif // SMOOTHSCROLLER_H
