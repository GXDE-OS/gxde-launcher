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

#include "fullscreenframe.h"
#include "src/dbusinterface/dbuslauncherframe.h"
#include "model/appsmanager.h"
#include "dbusservices/dbuslauncherservice.h"

#include <QCommandLineParser>
#include <QTranslator>
#include <QDebug>

#include <unistd.h>

#include <dapplication.h>
#include <QByteArray>
#include <DLog>
#include <QIcon>
#include <QSettings>

#include <LayerShellQt/Shell>

#include "wayland/xsettings.h"
#include "wayland/treeland_shell.h"

DWIDGET_USE_NAMESPACE
DCORE_USE_NAMESPACE

void correctStaleX11Platform() {
    if (qgetenv("XDG_SESSION_TYPE").compare("wayland",
                Qt::CaseInsensitive) != 0 ||
            qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
        return;
    }

    const QByteArray configuredPlatform = qgetenv("QT_QPA_PLATFORM");
    const qsizetype separator = configuredPlatform.indexOf(';');
    QByteArray primaryPlatform = configuredPlatform.left(
        separator >= 0 ? separator : configuredPlatform.size()).trimmed();

    const qsizetype optionSeparator = primaryPlatform.indexOf(':');
    if (optionSeparator >= 0) {
        primaryPlatform.truncate(optionSeparator);
    }

    if (primaryPlatform.compare("dxcb", Qt::CaseInsensitive) != 0 &&
        primaryPlatform.compare("xcb", Qt::CaseInsensitive) != 0) {
        return;
    }

    qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("wayland"));
}

void dump_user_apss_preset_order_list()
{
    AppsManager *appsManager = AppsManager::instance();
    const auto appsList = appsManager->appsInfoList(AppsListModel::All);

    QStringList buf;

    for (const auto &app : appsList)
        buf << QString("'%1'").arg(app.m_key);

    qDebug().noquote() << '[' << buf.join(", ") << ']';
}

int main(int argv, char *args[])
{
    correctStaleX11Platform();
    LayerShellQt::Shell::useLayerShell();

    DApplication app(argv, args);

    if (DApplication::isWayland()) {
        Wayland::TreelandDdeShell::init();
        QString iconTheme = Wayland::xsettingsString(QStringLiteral(
            "Net/IconThemeName"));

        if (iconTheme.isEmpty()) {
            QSettings qtSettings(QSettings::IniFormat, QSettings::UserScope,
                "deepin", "qt-theme");
            qtSettings.beginGroup("Theme");
            iconTheme = qtSettings.value("IconThemeName").toString();
        }

        if (!iconTheme.isEmpty()) {
            QIcon::setThemeName(iconTheme);
        }
    }

    app.setQuitOnLastWindowClosed(false);
    app.setOrganizationName("deepin");
    app.setApplicationName("gxde-launcher");
    app.setApplicationVersion("3.0");
    app.setTheme("dark");
    app.loadTranslator();

    DLogManager::registerConsoleAppender();
#ifndef QT_DEBUG
    DLogManager::registerFileAppender();
#endif

    bool quit = !app.setSingleInstance(QString("gxde-launcher_%1").arg(getuid()));

    QCommandLineOption showOption(QStringList() << "s" << "show", "show launcher(hide for default.)");
    QCommandLineOption toggleOption(QStringList() << "t" << "toggle", "toggle launcher visible.");
    QCommandLineOption dumpPresetOrder(QStringList() << "d" << "dump", "dump user-specificed preset order list and exit.");

    QCommandLineParser cmdParser;
    cmdParser.setApplicationDescription("DDE Launcher");
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();
    cmdParser.addOption(showOption);
    cmdParser.addOption(toggleOption);
    cmdParser.addOption(dumpPresetOrder);
    cmdParser.process(app);

    if (cmdParser.isSet(dumpPresetOrder))
    {
        quit = true;
        dump_user_apss_preset_order_list();
    }

    if (quit)
    {
        DBusLauncherFrame launcherFrame;

        do {
            if (!launcherFrame.isValid())
                break;

            if (cmdParser.isSet(toggleOption))
                launcherFrame.Toggle();
            else if (cmdParser.isSet(showOption))
                launcherFrame.Show();

        } while (false);

        return 0;
    }

    // INFO: what's this?
    setlocale(LC_ALL, "");

    LauncherSys launcher;
    DBusLauncherService service(&launcher);
    Q_UNUSED(service);
    QDBusConnection connection = QDBusConnection::sessionBus();
    if (!connection.registerService("com.deepin.dde.Launcher") ||
        !connection.registerObject("/com/deepin/dde/Launcher", &launcher))
        qWarning() << "register dbus service failed";

#ifndef QT_DEBUG
    if (/*!positionArgs.isEmpty() && */cmdParser.isSet(showOption))
#endif
        QTimer::singleShot(1, &launcher, &LauncherSys::showLauncher);

    return app.exec();
}
