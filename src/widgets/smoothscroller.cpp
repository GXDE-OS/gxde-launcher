#include "smoothscroller.h"

#include <QAbstractAnimation>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QScrollBar>
#include <QTimer>
#include <QVariantAnimation>
#include <QWheelEvent>
#include <QtMath>

namespace {

const int WheelDuration = 300;
// 临界阻尼弹簧的角频率，越大回弹越快
const qreal SpringOmega = 15;
// 惯性滚动速度衰减的时间常数（秒）
const qreal MomentumTau = 0.325;
const qreal MinMomentum = 30;
const qreal MinFlingVelocity = 200;
// 惯性滚动撞到边缘时，剩余速度转为回弹速度的比例
const qreal EdgeTransfer = 0.6;
// 松手前停顿超过这个时间（毫秒）就不再触发惯性
const qint64 FlingTimeout = 80;
// 没有滚动阶段信息（如 X11）时，多久没有新事件视为手指离开（毫秒）
const int FingerReleaseTimeout = 120;

// 每帧回调一次的动画，时间步长跟随 Qt 的动画时钟
class FrameAnimation : public QAbstractAnimation
{
public:
    FrameAnimation(std::function<void(qreal)> tick, QObject *parent)
        : QAbstractAnimation(parent)
        , m_tick(std::move(tick))
    {
    }

    int duration() const override { return -1; }

protected:
    void updateCurrentTime(int msecs) override
    {
        // 限制单步时长，避免卡顿后一次积分过大导致数值发散
        const int elapsed = qMin(msecs - m_last, 32);
        m_last = msecs;
        if (elapsed > 0)
            m_tick(elapsed / 1000.0);
    }

    void updateState(State newState, State oldState) override
    {
        Q_UNUSED(oldState);

        if (newState == Running)
            m_last = 0;
    }

private:
    std::function<void(qreal)> m_tick;
    int m_last = 0;
};

} // namespace

SmoothScroller::SmoothScroller(QAbstractScrollArea *area, const OffsetSetter &setOffset)
    : QObject(area)
    , m_area(area)
    , m_setOffset(setOffset)
    , m_wheelAni(new QVariantAnimation(this))
    , m_fingerReleaseTimer(new QTimer(this))
    , m_physics(new FrameAnimation([this](qreal dt) { tick(dt); }, this))
{
    m_wheelAni->setEasingCurve(QEasingCurve::OutCubic);
    m_wheelAni->setDuration(WheelDuration);

    connect(m_wheelAni, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        bar()->setValue(value.toInt());
        emit scrolled();
    });
    connect(m_wheelAni, &QVariantAnimation::finished, this, [this] {
        // 滚到边缘时还有没用完的距离，转为一次回弹
        if (m_pendingExcess) {
            kick(m_pendingExcess);
            m_pendingExcess = 0;
        }
        maybeFinish();
    });

    m_fingerReleaseTimer->setSingleShot(true);
    m_fingerReleaseTimer->setInterval(FingerReleaseTimeout);
    connect(m_fingerReleaseTimer, &QTimer::timeout, this, &SmoothScroller::releaseFinger);
}

void SmoothScroller::setWheelStep(qreal step)
{
    m_wheelStep = step;
}

void SmoothScroller::handleWheel(QWheelEvent *e)
{
    e->accept();

    QScrollBar *vbar = bar();
    if (vbar->maximum() <= vbar->minimum())
        return;

    emit scrollStarted();

    // 触控板等精确滚动设备：跟手滚动，松手后惯性滑动
    if (!e->pixelDelta().isNull() || e->phase() != Qt::NoScrollPhase) {
        handleTouchpad(e);
        return;
    }

    m_momentum = 0;

    const qreal step = m_wheelStep > 0
            ? m_wheelStep
            : QApplication::wheelScrollLines() * vbar->singleStep() / 120.0;
    const int delta = -qRound(e->angleDelta().y() * step);
    if (delta == 0)
        return;

    const bool running = m_wheelAni->state() == QAbstractAnimation::Running;
    // 连续滚动时在上一次的目标上累加，避免快速滚动时丢距离
    const int base = running ? m_wheelTarget : vbar->value();
    const int raw = base + delta;
    const int target = qBound(vbar->minimum(), raw, vbar->maximum());
    const int excess = raw - target;

    if (target != base) {
        m_wheelTarget = target;
        m_pendingExcess = excess;
        m_wheelAni->stop();
        m_wheelAni->setStartValue(vbar->value());
        m_wheelAni->setEndValue(target);
        m_wheelAni->start();
    } else if (running) {
        m_pendingExcess += excess;
    } else if (excess) {
        kick(excess);
    }
}

void SmoothScroller::stop()
{
    const bool active = m_fingerDown
            || m_wheelAni->state() == QAbstractAnimation::Running
            || m_physics->state() == QAbstractAnimation::Running;

    m_wheelAni->stop();
    m_physics->stop();
    m_fingerReleaseTimer->stop();
    m_lastFingerMove.invalidate();
    m_fingerDown = false;
    m_pendingExcess = 0;
    m_momentum = 0;
    m_stretch = 0;
    m_offset = 0;
    m_offsetVelocity = 0;
    applyOffset();

    if (active)
        emit scrollFinished();
}

void SmoothScroller::handleTouchpad(QWheelEvent *e)
{
    m_wheelAni->stop();
    m_pendingExcess = 0;
    m_momentum = 0;

    if (!m_fingerDown) {
        m_fingerDown = true;
        m_fingerVelocity = 0;
        // 回弹途中再次滑动时，从当前的偏移继续拉
        m_stretch = inverseRubberBand(m_offset);
        m_offsetVelocity = 0;
    }

    const int delta = -e->pixelDelta().y();
    if (delta != 0) {
        const qint64 elapsed = m_lastFingerMove.isValid() ? m_lastFingerMove.elapsed() : -1;
        m_lastFingerMove.start();

        if (elapsed >= 1 && elapsed < FlingTimeout)
            m_fingerVelocity = m_fingerVelocity * 0.2 + delta * 1000.0 / elapsed * 0.8;
        else if (elapsed >= FlingTimeout)
            m_fingerVelocity = 0;

        applyFingerDelta(delta);
    }

    if (e->phase() == Qt::ScrollEnd)
        releaseFinger();
    else if (e->phase() == Qt::NoScrollPhase)
        m_fingerReleaseTimer->start();
}

void SmoothScroller::applyFingerDelta(int delta)
{
    QScrollBar *vbar = bar();
    qreal remaining = delta;

    // 往回推时先收回越界的部分
    if (m_stretch != 0 && (m_stretch > 0) != (remaining > 0)) {
        if (qAbs(remaining) <= qAbs(m_stretch)) {
            m_stretch += remaining;
            remaining = 0;
        } else {
            remaining += m_stretch;
            m_stretch = 0;
        }
    }

    if (remaining != 0) {
        if (m_stretch != 0) {
            m_stretch += remaining;
        } else {
            const int raw = vbar->value() + qRound(remaining);
            const int value = qBound(vbar->minimum(), raw, vbar->maximum());
            vbar->setValue(value);
            m_stretch = raw - value;
        }
    }

    m_offset = -rubberBand(m_stretch);
    applyOffset();
    emit scrolled();
}

void SmoothScroller::releaseFinger()
{
    if (!m_fingerDown)
        return;

    m_fingerDown = false;
    m_fingerReleaseTimer->stop();
    m_stretch = 0;

    const bool fling = m_lastFingerMove.isValid()
            && m_lastFingerMove.elapsed() < FlingTimeout
            && qAbs(m_fingerVelocity) > MinFlingVelocity
            && qFuzzyIsNull(m_offset);
    m_lastFingerMove.invalidate();

    if (fling) {
        m_pos = bar()->value();
        m_momentum = m_fingerVelocity;
    }

    // 越界时由弹簧拉回；否则按松手时的速度继续惯性滚动
    if (fling || !qFuzzyIsNull(m_offset))
        startPhysics();
    else
        maybeFinish();
}

void SmoothScroller::kick(int excess)
{
    // 峰值位移与没用完的距离相关；临界阻尼弹簧从 0 出发时，初速度 v 的最大位移为 v / (omega * e)
    const qreal peak = qBound<qreal>(8, qAbs(excess) * 0.1, maxOffset() * 0.5);
    m_offsetVelocity += (excess > 0 ? -1 : 1) * peak * SpringOmega * M_E;
    startPhysics();
}

void SmoothScroller::tick(qreal dt)
{
    QScrollBar *vbar = bar();

    if (m_momentum != 0) {
        m_pos += m_momentum * dt;
        m_momentum *= qExp(-dt / MomentumTau);

        const qreal bounded = qBound<qreal>(vbar->minimum(), m_pos, vbar->maximum());
        if (bounded != m_pos) {
            // 惯性滚动撞到边缘，剩余速度转为回弹
            m_offsetVelocity -= m_momentum * EdgeTransfer;
            m_momentum = 0;
            m_pos = bounded;
        } else if (qAbs(m_momentum) < MinMomentum) {
            m_momentum = 0;
        }

        vbar->setValue(qRound(m_pos));
        emit scrolled();
    }

    if (!m_fingerDown && (m_offset != 0 || m_offsetVelocity != 0)) {
        // 临界阻尼弹簧：把越界偏移拉回 0，且不会来回振荡。
        // 用解析解 x(t) = (x0 + (v0 + w * x0) * t) * e^(-w * t) 推进，步长再大也不会失真
        const qreal decay = qExp(-SpringOmega * dt);
        const qreal b = m_offsetVelocity + SpringOmega * m_offset;
        m_offset = (m_offset + b * dt) * decay;
        m_offsetVelocity = (m_offsetVelocity - SpringOmega * b * dt) * decay;

        const qreal limit = maxOffset();
        if (qAbs(m_offset) > limit) {
            m_offset = m_offset > 0 ? limit : -limit;
            m_offsetVelocity = 0;
        }
        if (qAbs(m_offset) < 0.5 && qAbs(m_offsetVelocity) < 10) {
            m_offset = 0;
            m_offsetVelocity = 0;
        }
        applyOffset();
    }

    const bool springIdle = m_fingerDown || (m_offset == 0 && m_offsetVelocity == 0);
    if (m_momentum == 0 && springIdle) {
        m_physics->stop();
        maybeFinish();
    }
}

void SmoothScroller::startPhysics()
{
    if (m_physics->state() != QAbstractAnimation::Running)
        m_physics->start();
}

void SmoothScroller::applyOffset()
{
    const int offset = qRound(m_offset);
    if (offset == m_appliedOffset)
        return;

    m_appliedOffset = offset;
    m_setOffset(offset);
}

void SmoothScroller::maybeFinish()
{
    if (m_fingerDown
            || m_wheelAni->state() == QAbstractAnimation::Running
            || m_physics->state() == QAbstractAnimation::Running)
        return;

    emit scrollFinished();
}

qreal SmoothScroller::maxOffset() const
{
    return qMin<qreal>(m_area->viewport()->height() * 0.2, 96);
}

qreal SmoothScroller::rubberBand(qreal stretch) const
{
    // 越往外拉越费劲，位移逐渐逼近 maxOffset
    const qreal limit = maxOffset();
    if (limit <= 0)
        return 0;

    const qreal offset = limit * (1 - 1 / (1 + 0.55 * qAbs(stretch) / limit));
    return stretch < 0 ? -offset : offset;
}

qreal SmoothScroller::inverseRubberBand(qreal offset) const
{
    // rubberBand 的反函数，注意 m_offset = -rubberBand(m_stretch)
    const qreal limit = maxOffset();
    if (limit <= 0 || offset == 0)
        return 0;

    const qreal ratio = qMin<qreal>(qAbs(offset) / limit, 0.95);
    const qreal stretch = limit / 0.55 * (1 / (1 - ratio) - 1);
    return offset < 0 ? stretch : -stretch;
}

QScrollBar *SmoothScroller::bar() const
{
    return m_area->verticalScrollBar();
}
