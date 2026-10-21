#include "viewtransition.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QPainter>
#include <QPixmap>
#include <QPointer>
#include <QVariantAnimation>
#include <QWidget>

namespace ViewTransition {
namespace {

const char *const OverlayProperty = "_launcher_view_transition";

QPixmap snapshot(QWidget *widget) {
    const qreal ratio = widget->devicePixelRatioF();
    QPixmap pixmap(widget->size() * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    widget->render(&pixmap, QPoint(), QRegion(), QWidget::DrawChildren);
    return pixmap;
}

class TransitionOverlay : public QWidget
{
public:
    TransitionOverlay(QWidget *target, const QPixmap &from, const QPixmap &to, Direction direction)
        : QWidget(target->parentWidget())
        , m_target(target)
        , m_from(from)
        , m_to(to)
        , m_direction(direction)
        , m_animation(new QVariantAnimation(this))
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setGeometry(target->geometry());

        // 用透明度效果隐藏真实控件；控件已有透明度效果时复用它，避免把它替换掉
        m_effect = qobject_cast<QGraphicsOpacityEffect *>(target->graphicsEffect());
        if (m_effect) {
            m_savedOpacity = m_effect->opacity();
        } else {
            m_effect = new QGraphicsOpacityEffect(target);
            m_ownsEffect = true;
            target->setGraphicsEffect(m_effect);
        }
        m_effect->setOpacity(0);

        target->setProperty(OverlayProperty, QVariant::fromValue<QObject *>(this));
        target->installEventFilter(this);

        m_animation->setStartValue(0.0);
        m_animation->setEndValue(1.0);
        m_animation->setDuration(direction == Fade ? 240 : direction == ZoomIn ? 300 : 280);
        m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_progress = value.toReal();
            update();
        });
        connect(m_animation, &QVariantAnimation::finished, this, [this] { finish(); });

        // 紧贴在目标控件之上，不盖住原本就在它上面的兄弟控件
        stackUnder(target);
        target->stackUnder(this);
        show();
        m_animation->start();
    }

    void finish()
    {
        if (m_finished)
            return;
        m_finished = true;

        m_animation->stop();
        if (m_target) {
            m_target->removeEventFilter(this);
            m_target->setProperty(OverlayProperty, QVariant());
            if (m_ownsEffect)
                m_target->setGraphicsEffect(nullptr);
            else if (m_effect)
                m_effect->setOpacity(m_savedOpacity);
        }

        hide();
        deleteLater();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_target) {
            switch (event->type()) {
            case QEvent::Move:
            case QEvent::Resize:
                setGeometry(m_target->geometry());
                break;
            case QEvent::Hide:
                finish();
                break;
            default:
                break;
            }
        }

        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        const qreal p = m_progress;

        if (m_direction == ZoomIn) {
            const qreal scale = 0.94 + 0.06 * p;
            painter.setRenderHint(QPainter::SmoothPixmapTransform);
            painter.setOpacity(p);
            painter.translate(rect().center());
            painter.scale(scale, scale);
            painter.translate(-rect().center());
            painter.drawPixmap(0, 0, m_to);
            return;
        }

        // 旧内容先淡出、新内容稍后淡入，减少两层叠在一起时的重影
        const qreal fromOpacity = 1.0 - qMin<qreal>(1.0, p / 0.6);
        const qreal toOpacity = qBound<qreal>(0.0, (p - 0.2) / 0.8, 1.0);

        QPointF fromOffset;
        QPointF toOffset;
        if (m_direction == Fade) {
            toOffset = QPointF(0, (1.0 - p) * 16);
        } else {
            const qreal shift = width() * 0.25 * (m_direction == Forward ? 1 : -1);
            fromOffset = QPointF(-p * shift, 0);
            toOffset = QPointF((1.0 - p) * shift, 0);
        }

        painter.setOpacity(fromOpacity);
        painter.drawPixmap(fromOffset, m_from);
        painter.setOpacity(toOpacity);
        painter.drawPixmap(toOffset, m_to);
    }

private:
    QPointer<QWidget> m_target;
    QPointer<QGraphicsOpacityEffect> m_effect;
    QPixmap m_from;
    QPixmap m_to;
    Direction m_direction;
    QVariantAnimation *m_animation;
    qreal m_progress = 0;
    qreal m_savedOpacity = 1;
    bool m_ownsEffect = false;
    bool m_finished = false;
};

}  // namespace

void run(QWidget *target, Direction direction, const std::function<void()> &change,
         const std::function<void()> &settle)
{
    if (!target || !target->isVisible() || !target->parentWidget()) {
        change();
        if (settle)
            settle();
        return;
    }

    // 上一次过渡还没结束时先收尾，保证截到的是真实内容
    QObject *previous = target->property(OverlayProperty).value<QObject *>();
    if (TransitionOverlay *overlay = dynamic_cast<TransitionOverlay *>(previous))
        overlay->finish();

    const QPixmap from = snapshot(target);

    change();

    // 让新内容完成布局后再截图
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    if (QAbstractItemView *view = qobject_cast<QAbstractItemView *>(target))
        view->doItemsLayout();
    if (settle)
        settle();

    const QPixmap to = snapshot(target);

    new TransitionOverlay(target, from, to, direction);
}

void enter(QWidget *target)
{
    if (!target || !target->isVisible() || !target->parentWidget())
        return;

    QObject *previous = target->property(OverlayProperty).value<QObject *>();
    if (TransitionOverlay *overlay = dynamic_cast<TransitionOverlay *>(previous))
        overlay->finish();

    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    new TransitionOverlay(target, QPixmap(), snapshot(target), ZoomIn);
}

}  // namespace ViewTransition
