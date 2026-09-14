/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
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

#include "menuworker.h"

#include <QMenu>
#include <QSignalMapper>
#include <QActionGroup>
#include <QProcess>
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <LayerShellQt/Window>

#include "../wayland/treeland_shell.h"

static QString ChainsProxy_path = QStandardPaths::standardLocations(QStandardPaths::ConfigLocation).first()
        + "/deepin/proxychains.conf";

static bool anchorWaylandPopupAt(QMenu *menu, const QPoint &anchorPos) {
    QWindow *win = menu->windowHandle();
    if (!win)
        return false;
    LayerShellQt::Window *lsWin = LayerShellQt::Window::get(win);
    if (!lsWin)
        return false;

    const QSize sz = menu->sizeHint();
    QScreen *scr = QGuiApplication::screenAt(anchorPos);
    if (!scr)
        scr = QGuiApplication::primaryScreen();
    const QRect sg = scr ? scr->geometry() : QRect();
    if (sg.isNull())
        return false;

    int x = anchorPos.x();
    int y = anchorPos.y();
    const int w = sz.width();
    const int h = sz.height();
    if (x + w > sg.right() + 1) x = anchorPos.x() - w;
    if (y + h > sg.bottom() + 1) y = anchorPos.y() - h;
    if (x < sg.left()) x = sg.left();
    if (y < sg.top()) y = sg.top();

    lsWin->setLayer(LayerShellQt::Window::LayerTop);
    LayerShellQt::Window::Anchors anchors(LayerShellQt::Window::AnchorTop);
    anchors |= LayerShellQt::Window::AnchorLeft;
    lsWin->setAnchors(anchors);
    lsWin->setExclusiveZone(0);
    lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    lsWin->setMargins(QMargins(x - sg.left(), y - sg.top(), 0, 0));
    return true;
}

MenuWorker::MenuWorker(QObject *parent) : QObject(parent)
{
    m_xsettings = new QGSettings("com.deepin.xsettings", QByteArray(), this);
    m_dockAppManagerInterface = new DBusDock(this);
    m_startManagerInterface = new DBusStartManager(this);
    m_launcherInterface = new DBusLauncher(this);
    m_appManager = AppsManager::instance();

    initConnect();
}


void MenuWorker::initConnect(){

}

MenuWorker::~MenuWorker()
{
}

void MenuWorker::showMenuByAppItem(QPoint pos, const QModelIndex &index) {
    setCurrentModelIndex(index);

    m_appKey = m_currentModelIndex.data(AppsListModel::AppKeyRole).toString();
    m_appDesktop = m_currentModelIndex.data(AppsListModel::AppDesktopRole).toString();
    m_isItemOnDesktop = m_currentModelIndex.data(AppsListModel::AppIsOnDesktopRole).toBool();
    m_isItemOnDock = m_currentModelIndex.data(AppsListModel::AppIsOnDockRole).toBool();
    m_isItemStartup = m_currentModelIndex.data(AppsListModel::AppAutoStartRole).toBool();
    m_isRemovable = m_currentModelIndex.data(AppsListModel::AppIsRemovableRole).toBool();
    m_isItemProxy = m_currentModelIndex.data(AppsListModel::AppIsProxyRole).toBool();
    m_isItemNoSandbox = m_currentModelIndex.data(AppsListModel::AppIsNoSandbox).toBool();
    m_isItemPrimeNvidia = m_currentModelIndex.data(AppsListModel::AppIsPrimeNvidia).toBool();
    m_isItemEnableScaling = m_currentModelIndex.data(AppsListModel::AppEnableScalingRole).toBool();
    m_isMarkLaunched = !m_currentModelIndex.data(AppsListModel::AppNewInstallRole).toBool();

    qDebug() << "appKey" << m_appKey;

    const bool isWayland = QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive);

    QMenu *menu = new QMenu;

    QSignalMapper *signalMapper = new QSignalMapper(menu);

    QAction *open;
    QAction *noSandboxOption;
    QAction *primeNvidiaOption;
    QAction *desktop;
    QAction *dock;
    QAction *startup;
    QAction *proxy;
    QAction *scale;
    QAction *uninstall;
    QAction *markLaunched;

    open = new QAction(tr("Open"), menu);

    primeNvidiaOption = new QAction(tr("Use Nvidia Only"), menu);

    noSandboxOption = new QAction(tr("Disable App Sandbox"), menu);

    desktop = new QAction(m_isItemOnDesktop ?
                              tr("Remove from desktop") :
                              tr("Send to desktop"),
                          menu);

    dock = new QAction(m_isItemOnDock ?
                           tr("Remove from dock") :
                           tr("Send to dock"),
                       menu);

    startup = new QAction(m_isItemStartup ?
                              tr("Remove from startup") :
                              tr("Add to startup"),
                          menu);


    uninstall = new QAction(tr("Uninstall"), menu);
    // 存在卸载器才启用卸载项
    uninstall->setEnabled(QFile::exists("/usr/bin/gxde-app-uninstaller"));

    markLaunched = new QAction(tr("Mark Launched"), menu);
    //markLaunched->setVisible(!m_isMarkLaunched);
    markLaunched->setVisible(false);

    menu->addAction(open);
    menu->addSeparator();
    menu->addSeparator();
    menu->addAction(desktop);
    menu->addAction(dock);
    menu->addSeparator();
    menu->addAction(noSandboxOption);
    menu->addAction(primeNvidiaOption);
    menu->addAction(startup);
    menu->addAction(markLaunched);

    if (QFile::exists(ChainsProxy_path)) {
        proxy = new QAction(tr("Use a proxy"), menu);
        proxy->setCheckable(true);
        proxy->setChecked(m_isItemProxy);
        menu->addAction(proxy);
        signalMapper->setMapping(proxy, Proxy);
        connect(proxy, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    }

    const double scale_ratio = m_xsettings->get("scale-factor").toDouble();
    if (!qFuzzyCompare(1.0, scale_ratio)) {
        scale = new QAction(tr("Disable display scaling"), menu);
        scale->setCheckable(true);
        scale->setChecked(!m_isItemEnableScaling);
        menu->addAction(scale);
        signalMapper->setMapping(scale, SwitchScale);
        connect(scale, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    }

    noSandboxOption->setCheckable(true);
    noSandboxOption->setChecked(m_isItemNoSandbox);
    signalMapper->setMapping(noSandboxOption, NoSandbox);
    connect(noSandboxOption, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));

    primeNvidiaOption->setCheckable(true);
    primeNvidiaOption->setChecked(m_isItemPrimeNvidia);
    // 不存在闭源 N 卡驱动或 prime-run，则不启用该选项
    if (!QFile::exists("/usr/bin/nvidia-smi")) {
        primeNvidiaOption->setVisible(false);
    }
    signalMapper->setMapping(primeNvidiaOption, PrimeNvidia);
    connect(primeNvidiaOption, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));

    uninstall->setEnabled(m_isRemovable);

#ifndef WITHOUT_UNINSTALL_APP
    menu->addAction(uninstall);
#endif

    if (isWayland) {
        addForcedDisplaySubMenus(menu);
    }

    connect(open, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    connect(desktop, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    connect(dock, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    connect(startup, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));

    connect(uninstall, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));
    connect(markLaunched, &QAction::triggered, signalMapper, static_cast<void (QSignalMapper::*)()>(&QSignalMapper::map));

    signalMapper->setMapping(open, Open);
    signalMapper->setMapping(desktop, Desktop);
    signalMapper->setMapping(dock, Dock);
    signalMapper->setMapping(startup, Startup);
    signalMapper->setMapping(uninstall, Uninstall);
    signalMapper->setMapping(markLaunched, MarkLaunched);

    connect(signalMapper, &QSignalMapper::mappedInt, this, &MenuWorker::handleMenuAction);
    connect(menu, &QMenu::aboutToHide, this, &MenuWorker::handleMenuClosed);
    connect(menu, &QMenu::aboutToHide, menu, &QMenu::deleteLater);

    if (isWayland) {
        menu->adjustSize();
        menu->setFixedSize(menu->sizeHint());
        // 在 Wayland 下，菜单的 wayland surface 会关联到 launcher（WindowedFrame）
        // 的 layer-shell 父 surface，而它本身没有尺寸锚定，compositor 会将其撑满
        // 全屏。这里对其 surface 应用 layer-shell 锚定 + margins，把它约束在鼠标
        // 右键点击位置、由内容决定大小的小矩形内，与 X11 下表现一致。
        auto applyLayer = [this, menu, pos]() -> bool {
            QWindow *win = menu->windowHandle();
            if (!win)
                return false;
            LayerShellQt::Window *lsWin = LayerShellQt::Window::get(win);
            if (!lsWin)
                return false;
            const QSize sz = menu->sizeHint();
            QScreen *scr = QGuiApplication::screenAt(pos);
            if (!scr)
                scr = QGuiApplication::primaryScreen();
            const QRect sg = scr ? scr->geometry() : QRect();
            if (sg.isNull())
                return false;
            // 菜单默认出现在鼠标右下方（左上角对齐 pos），超出屏幕则翻转到
            // 鼠标另一侧，与 X11 下 QMenu::exec(pos) 的 flip 行为一致。
            int x = pos.x();
            int y = pos.y();
            const int w = sz.width();
            const int h = sz.height();
            if (x + w > sg.right() + 1) x = pos.x() - w;
            if (y + h > sg.bottom() + 1) y = pos.y() - h;
            if (x < sg.left()) x = sg.left();
            if (y < sg.top()) y = sg.top();
            lsWin->setLayer(LayerShellQt::Window::LayerTop);
            // 仅锚定左上角，surface 保持 fixedSize 的内容尺寸，左上角精确定位
            // 在 (x, y)，避免四边锚定被 compositor 拉伸/居中导致位置偏移。
            LayerShellQt::Window::Anchors anchors(LayerShellQt::Window::AnchorTop);
            anchors |= LayerShellQt::Window::AnchorLeft;
            lsWin->setAnchors(anchors);
            lsWin->setExclusiveZone(0);
            lsWin->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
            lsWin->setMargins(QMargins(x - sg.left(), y - sg.top(), 0, 0));
            m_menuGeometry = menu->geometry();
            return true;
        };
        menu->winId();
        {
            QScreen *scr = QGuiApplication::screenAt(pos);
            if (!scr)
                scr = QGuiApplication::primaryScreen();
            if (scr)
                menu->setScreen(scr);
        }

        menu->move(pos);

        Wayland::TreelandDdeShell::setAutoPlacement(menu->windowHandle(), 0);
        
        applyLayer();
        QTimer::singleShot(0, this, [applyLayer, menu]() {
            Wayland::TreelandDdeShell::setAutoPlacement(menu->windowHandle(), 0);
            applyLayer();
        });
        m_menuIsShown = true;
        menu->exec();
    } else {
        menu->move(pos);
        m_menuIsShown = true;
        m_menuGeometry = menu->geometry();
        menu->exec();
    }
}

void MenuWorker::handleOpen()
{
    m_appManager->launchApp(m_currentModelIndex);

    emit appLaunched();
}

void MenuWorker::handleMenuClosed()
{
    emit menuAccepted();
    m_menuIsShown = false;
}

void MenuWorker::setCurrentModelIndex(const QModelIndex &index)
{
    m_currentModelIndex = index;
}

const QModelIndex MenuWorker::getCurrentModelIndex()
{
    return m_currentModelIndex;
}

void MenuWorker::handleMenuAction(int index)
{
    switch (index) {
    case Open:
        handleOpen();
        break;
    case Desktop:
        handleToDesktop();
        break;
    case Dock:
        handleToDock();
        break;
    case Startup:
        handleToStartup();
        break;
    case Proxy:
        handleToProxy();
        break;
    case SwitchScale:
        handleSwitchScaling();
        break;
    case Uninstall:
        emit unInstallApp(m_currentModelIndex);
        break;
    case NoSandbox:
        handleToNoSandbox();
        break;
    case PrimeNvidia:
        handleToPrimeNvidia();
        break;
    case MarkLaunched:
        handleToMarkLaunched();
        break;
    default:
        break;
    }
}

void MenuWorker::handleToDesktop(){
    qDebug() << "handleToDesktop" << m_appKey;
    if (m_isItemOnDesktop){
        QDBusPendingReply<bool> reply = m_launcherInterface->RequestRemoveFromDesktop(m_appKey);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "remove from desktop:" << ret;
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }else{
        QDBusPendingReply<bool> reply = m_launcherInterface->RequestSendToDesktop(m_appKey);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "send to desktop:" << ret;
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }
}

void MenuWorker::handleToDock(){
    qDebug() << "handleToDock" << m_appKey;
    if (m_isItemOnDock){
        QDBusPendingReply<bool> reply = m_dockAppManagerInterface->RequestUndock(m_appDesktop);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "remove from dock:" << ret;
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }else{
        QDBusPendingReply<bool> reply =  m_dockAppManagerInterface->RequestDock(m_appDesktop, -1);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "send to dock:" << ret;
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }
}

void MenuWorker::handleToStartup(){
    QString desktopUrl = m_currentModelIndex.data(AppsListModel::AppDesktopRole).toString();
    if (m_isItemStartup){
        QDBusPendingReply<bool> reply = m_startManagerInterface->RemoveAutostart(desktopUrl);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "remove from startup:" << ret;
            if (ret) {
//                emit signalManager->hideAutoStartLabel(appKey);
            }
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }else{
        QDBusPendingReply<bool> reply =  m_startManagerInterface->AddAutostart(desktopUrl);
        reply.waitForFinished();
        if (!reply.isError()) {
            bool ret = reply.argumentAt(0).toBool();
            qDebug() << "add to startup:" << ret;
            if (ret){
//                emit signalManager->showAutoStartLabel(appKey);
            }
        } else {
            qCritical() << reply.error().name() << reply.error().message();
        }
    }
}

void MenuWorker::handleToProxy()
{
    m_launcherInterface->SetUseProxy(m_appKey, !m_isItemProxy);
}

bool MenuWorker::isElectronApp(const QString &desktopPath)
{
    if (!QFile::exists(desktopPath)) {
        return false;
    }

    QFile file(desktopPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.startsWith("Exec=")) {
            QString execValue = line.mid(5);
            // 检查 Exec 字段是否包含 electron 相关关键字
            QStringList electronKeywords = {"electron", "electron-builder", "electron-forge", "electron-packager"};
            for (const QString &keyword : electronKeywords) {
                if (execValue.contains(keyword, Qt::CaseInsensitive)) {
                    return true;
                }
            }
            // 检查是否使用 electron 作为运行时
            if (execValue.contains("/electron", Qt::CaseInsensitive) || 
                execValue.startsWith("electron", Qt::CaseInsensitive)) {
                return true;
            }
        }
    }

    return false;
}

void MenuWorker::setElectronAppScaling(const QString &appKey, bool enableScaling)
{
    QProcess process;
    QString gsettingsCmd;
    
    if (enableScaling) {
        // 启用缩放从 electron 应用列表中移除
        gsettingsCmd = QString("gsettings get com.deepin.dde.launcher apps-disable-scaling-electron");
        process.start("bash", QStringList() << "-c" << gsettingsCmd);
        process.waitForFinished();
        QString currentList = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        
        // 从列表中移除该应用
        if (currentList.contains(appKey)) {
            currentList.replace("'" + appKey + "',", "");
            currentList.replace("'" + appKey + "'", "");
            currentList = currentList.replace(",,", ",").trimmed();
            
            // 确保格式正确
            if (currentList.isEmpty() || currentList == "[]" || currentList == "@as") {
                currentList = "[]";
            }
            
            gsettingsCmd = QString("gsettings set com.deepin.dde.launcher apps-disable-scaling-electron '%1'").arg(currentList);
            QProcess setProcess;
            setProcess.start("bash", QStringList() << "-c" << gsettingsCmd);
            setProcess.waitForFinished();
        }
    } else {
        // 禁用缩放添加到 electron 应用列表
        gsettingsCmd = QString("gsettings get com.deepin.dde.launcher apps-disable-scaling-electron");
        process.start("bash", QStringList() << "-c" << gsettingsCmd);
        process.waitForFinished();
        QString currentList = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
        
        // 解析当前列表并添加新应用
        if (currentList.isEmpty() || currentList == "@as") {
            currentList = "['" + appKey + "']";
        } else if (currentList == "[]") {
            currentList = "['" + appKey + "']";
        } else if (!currentList.contains(appKey)) {
            // 在列表中添加新应用
            if (currentList.endsWith("]")) {
                currentList.chop(1);
                if (currentList.endsWith("'")) {
                    currentList += ", '" + appKey + "']";
                } else {
                    currentList += "'" + appKey + "']";
                }
            }
        }
        
        gsettingsCmd = QString("gsettings set com.deepin.dde.launcher apps-disable-scaling-electron '%1'").arg(currentList);
        QProcess setProcess;
        setProcess.start("bash", QStringList() << "-c" << gsettingsCmd);
        setProcess.waitForFinished();
    }
}

void MenuWorker::handleSwitchScaling()
{
    if (isElectronApp(m_appDesktop)) {
        setElectronAppScaling(m_appKey, !m_isItemEnableScaling);
    } else {
        m_launcherInterface->SetDisableScaling(m_appKey, m_isItemEnableScaling);
    }
}

void MenuWorker::handleToNoSandbox()
{
    m_launcherInterface->SetNoSandbox(m_appKey, !m_isItemNoSandbox);
}

void MenuWorker::handleToMarkLaunched()
{
    m_launcherInterface->MarkLaunched(m_appKey);
    qDebug() << m_appKey;
}

void MenuWorker::handleToPrimeNvidia()
{
    m_launcherInterface->SetPrimeNvidia(m_appKey, !m_isItemPrimeNvidia);
}

void MenuWorker::addForcedDisplaySubMenus(QMenu *menu) {
    QMenu *waylandMenu = menu->addMenu(tr("Force launch in Wayland mode"));
    QMenu *x11Menu = menu->addMenu(tr("Force launch in X11 mode"));

    waylandMenu->menuAction()->setCheckable(true);
    x11Menu->menuAction()->setCheckable(true);

    if (QGuiApplication::platformName().startsWith("wayland", Qt::CaseInsensitive)) {
        auto setupWaylandSubMenu = [this](QMenu *subMenu) {
            connect(subMenu, &QMenu::aboutToShow, this, [this, subMenu]() {
                subMenu->setFixedSize(subMenu->sizeHint());
                QTimer::singleShot(0, this, [this, subMenu]() {
                    subMenu->winId();
                    Wayland::TreelandDdeShell::setAutoPlacement(subMenu->windowHandle(), 0);
                    anchorWaylandPopupAt(subMenu, subMenu->mapToGlobal(QPoint(0, 0)));
                });
            });
        };
        setupWaylandSubMenu(waylandMenu);
        setupWaylandSubMenu(x11Menu);
    }

    auto isWaylandMode = [](AppsManager::ForcedDisplayMode mode) {
        return mode == AppsManager::DisplayModeWaylandQt
            || mode == AppsManager::DisplayModeWaylandGdk
            || mode == AppsManager::DisplayModeWaylandOzone;
    };
    auto isX11Mode = [](AppsManager::ForcedDisplayMode mode) {
        return mode == AppsManager::DisplayModeX11QtXcb
            || mode == AppsManager::DisplayModeX11QtDxcb
            || mode == AppsManager::DisplayModeX11Gdk
            || mode == AppsManager::DisplayModeX11Ozone;
    };

    // 每个母菜单一组互斥项，组内第一个「Unset preference」作为默认/清除项。
    QActionGroup *waylandGroup = new QActionGroup(menu);
    waylandGroup->setExclusive(true);
    QActionGroup *x11Group = new QActionGroup(menu);
    x11Group->setExclusive(true);

    QAction *waylandUnset = waylandMenu->addAction(tr("Unset preference"));
    waylandUnset->setCheckable(true);
    waylandGroup->addAction(waylandUnset);

    QAction *x11Unset = x11Menu->addAction(tr("Unset preference"));
    x11Unset->setCheckable(true);
    x11Group->addAction(x11Unset);

    // 具体后端项：点击后仅记住选择并打勾，不立即启动应用。选中一侧时把另一侧
    // 复位到「Unset preference」，从而保持两组之间互斥。
    auto makeForceAction = [&](QActionGroup *group, QMenu *subMenu, const QString &text, AppsManager::ForcedDisplayMode mode) {
        QAction *action = subMenu->addAction(text);
        action->setCheckable(true);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, mode, waylandMenu, x11Menu, waylandUnset, x11Unset, isWaylandMode, isX11Mode]() {
            m_appManager->setForcedDisplayMode(m_appKey, mode);
            if (isWaylandMode(mode))
                x11Unset->setChecked(true);
            else
                waylandUnset->setChecked(true);
            waylandMenu->menuAction()->setChecked(isWaylandMode(mode));
            x11Menu->menuAction()->setChecked(isX11Mode(mode));
        });
        return action;
    };

    QAction *waylandQt = makeForceAction(waylandGroup, waylandMenu, tr("Set QT_QPA_PLATFORM"), AppsManager::DisplayModeWaylandQt);
    QAction *waylandGdk = makeForceAction(waylandGroup, waylandMenu, tr("Set GDK_BACKEND"), AppsManager::DisplayModeWaylandGdk);
    QAction *waylandOzone = makeForceAction(waylandGroup, waylandMenu, tr("Set Electron Ozone platform"), AppsManager::DisplayModeWaylandOzone);

    QAction *x11QtXcb = makeForceAction(x11Group, x11Menu, tr("Set QT_QPA_PLATFORM (XCB)"), AppsManager::DisplayModeX11QtXcb);
    QAction *x11QtDxcb = makeForceAction(x11Group, x11Menu, tr("Set QT_QPA_PLATFORM (D-XCB)"), AppsManager::DisplayModeX11QtDxcb);
    QAction *x11Gdk = makeForceAction(x11Group, x11Menu, tr("Set GDK_BACKEND"), AppsManager::DisplayModeX11Gdk);
    QAction *x11Ozone = makeForceAction(x11Group, x11Menu, tr("Set Electron Ozone platform"), AppsManager::DisplayModeX11Ozone);

    // 两个「Unset preference」语义相同：清除整个强制后端设置，回到默认启动。
    auto unsetForcedMode = [this, waylandMenu, x11Menu, waylandUnset, x11Unset]() {
        m_appManager->setForcedDisplayMode(m_appKey, AppsManager::DisplayModeNone);
        waylandUnset->setChecked(true);
        x11Unset->setChecked(true);
        waylandMenu->menuAction()->setChecked(false);
        x11Menu->menuAction()->setChecked(false);
    };
    connect(waylandUnset, &QAction::triggered, this, unsetForcedMode);
    connect(x11Unset, &QAction::triggered, this, unsetForcedMode);

    // 反映当前已保存的选择
    const AppsManager::ForcedDisplayMode mode = m_appManager->forcedDisplayMode(m_appKey);
    QAction *checkedWayland = nullptr;
    QAction *checkedX11 = nullptr;
    switch (mode) {
    case AppsManager::DisplayModeWaylandQt:    checkedWayland = waylandQt;    break;
    case AppsManager::DisplayModeWaylandGdk:   checkedWayland = waylandGdk;   break;
    case AppsManager::DisplayModeWaylandOzone: checkedWayland = waylandOzone; break;
    case AppsManager::DisplayModeX11QtXcb:     checkedX11 = x11QtXcb;         break;
    case AppsManager::DisplayModeX11QtDxcb:    checkedX11 = x11QtDxcb;        break;
    case AppsManager::DisplayModeX11Gdk:       checkedX11 = x11Gdk;           break;
    case AppsManager::DisplayModeX11Ozone:     checkedX11 = x11Ozone;         break;
    default: break;
    }
    if (checkedWayland)
        checkedWayland->setChecked(true);
    else
        waylandUnset->setChecked(true);
    if (checkedX11)
        checkedX11->setChecked(true);
    else
        x11Unset->setChecked(true);

    waylandMenu->menuAction()->setChecked(isWaylandMode(mode));
    x11Menu->menuAction()->setChecked(isX11Mode(mode));
}
