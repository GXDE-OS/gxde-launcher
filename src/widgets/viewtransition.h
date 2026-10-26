#ifndef VIEWTRANSITION_H
#define VIEWTRANSITION_H

#include <functional>

class QWidget;

namespace ViewTransition {

enum Direction {
    Forward,    // 进入下一级：旧内容向左滑出，新内容从右侧滑入
    Backward,   // 返回上一级：方向相反
    Fade,       // 原地切换：淡出旧内容，新内容轻微上移淡入
    ZoomIn,     // 入场：内容从略小的尺寸放大并淡入
    ZoomOut,    // 退场：内容缩小并淡出，结束后保持隐藏
};

// 退场动画时长，调用方在这之后再真正隐藏窗口
constexpr int LeaveDuration = 200;

// 在 change() 前后各截一张图，用截图播放过渡动画，过渡期间隐藏真实控件。
// settle() 在布局刷新之后、截取新内容之前调用，用于把仍在进行的滚动等状态直接定到终点。
void run(QWidget *target, Direction direction, const std::function<void()> &change,
         const std::function<void()> &settle = {});

// 对已经显示出来的控件播放入场动画
void enter(QWidget *target);

// 播放退场动画；结束后控件保持透明，直到它被隐藏或调用 cancel()
void leave(QWidget *target);

// 立即结束控件上正在进行的过渡，恢复真实控件
void cancel(QWidget *target);

}  // namespace ViewTransition

#endif  // VIEWTRANSITION_H
