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

#include "searchwidget.h"
#include "src/global_util/util.h"

#include <QHBoxLayout>
#include <QEvent>
#include <QDebug>
#include <QKeyEvent>
#include <dimagebutton.h>
#include <DDBusSender>

DWIDGET_USE_NAMESPACE

SearchWidget::SearchWidget(QWidget *parent) :
    QFrame(parent)
{
    m_leftSpacing = new QFrame(this);
    m_rightSpacing = new QFrame(this);

    m_leftSpacing->setFixedWidth(0);
    m_rightSpacing->setFixedWidth(0);

    m_toggleCategoryBtn = new DImageButton(this);
    m_toggleCategoryBtn->setAccessibleName("mode-toggle-button");
    m_toggleCategoryBtn->setNormalPic(":/icons/skin/icons/category_normal_22px.png");
    m_toggleCategoryBtn->setHoverPic(":/icons/skin/icons/category_hover_22px.png");
    m_toggleCategoryBtn->setPressPic(":/icons/skin/icons/category_active_22px.png");

    m_togglePowerBtn = new DImageButton(this);
    m_togglePowerBtn->setNormalPic(":/icons/skin/icons/poweroff_normal.png");
    m_togglePowerBtn->setHoverPic(":/icons/skin/icons/poweroff_hover.png");
    m_togglePowerBtn->setPressPic(":/icons/skin/icons/poweroff_press@2x.png");

    m_toggleModeBtn = new DImageButton(this);
    m_toggleModeBtn->setNormalPic(":/icons/skin/icons/unfullscreen_normal.png");
    m_toggleModeBtn->setHoverPic(":/icons/skin/icons/unfullscreen_hover.png");
    m_toggleModeBtn->setPressPic(":/icons/skin/icons/unfullscreen_press.png");

    m_toggleSettingBtn = new DImageButton(this);
    m_toggleSettingBtn->setNormalPic(":/icons/skin/icons/settings_normal_24px.svg");
    m_toggleSettingBtn->setHoverPic(":/icons/skin/icons/settings_hover_24px.svg");
    m_toggleSettingBtn->setPressPic(":/icons/skin/icons/settings_press_24px.svg");

    m_searchEdit = new SearchLineEdit(this);
    m_searchEdit->setAccessibleName("search-edit");
    m_searchEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_searchEdit->setFixedWidth(290);

    // 判断是否在 Chroot 下运行，如果是则不显示电源按钮
    QDBusMessage checkChrootDBus = QDBusMessage::createMethodCall(CHROOTCHECKDESTINATION,
                                                                  CHROOTCHECKPATH,
                                                                  CHROOTCHECKINTERFACE,
                                                                  "IsInChroot");
    QDBusMessage res = QDBusConnection::sessionBus().call(checkChrootDBus);
    if (res.arguments().at(0).toBool()) {
        m_togglePowerBtn->setHidden(1);
    }

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);

    mainLayout->addSpacing(30);
    mainLayout->addWidget(m_leftSpacing);
    mainLayout->addWidget(m_toggleCategoryBtn);
    mainLayout->addSpacing(30);
    mainLayout->addSpacing(24);
    mainLayout->addSpacing(30);
    mainLayout->addSpacing(24);

    mainLayout->addStretch();
    mainLayout->addWidget(m_searchEdit);
    mainLayout->addStretch();

    mainLayout->addWidget(m_toggleModeBtn);
    mainLayout->addSpacing(30);
    mainLayout->addWidget(m_toggleSettingBtn);
    mainLayout->addSpacing(30);
    mainLayout->addWidget(m_togglePowerBtn);
    mainLayout->addWidget(m_rightSpacing);
    mainLayout->addSpacing(30);

    setLayout(mainLayout);

    connect(m_searchEdit, &SearchLineEdit::textChanged, [this] {
        emit searchTextChanged(m_searchEdit->text().trimmed());
    });
    connect(m_togglePowerBtn, &DImageButton::clicked, this, [=]{
        QProcess::startDetached("dde-shutdown");
    });
    connect(m_toggleModeBtn, &DImageButton::clicked, this, [=] {
#if (DTK_VERSION >= DTK_VERSION_CHECK(2, 0, 8, 0))
        DDBusSender()
            .service("com.deepin.dde.daemon.Launcher")
            .interface("com.deepin.dde.daemon.Launcher")
            .path("/com/deepin/dde/daemon/Launcher")
            .property("Fullscreen")
            .set(false);
#else
            const QStringList args{
                "--print-reply",
                "--dest=com.deepin.dde.daemon.Launcher",
                "/com/deepin/dde/daemon/Launcher",
                "org.freedesktop.DBus.Properties.Set",
                "string:com.deepin.dde.daemon.Launcher",
                "string:Fullscreen",
                "variant:boolean:false"};

            QProcess::startDetached("dbus-send", args);
#endif
    });
    connect(m_toggleSettingBtn, &DImageButton::clicked, this, [](){
#if (DTK_VERSION >= DTK_VERSION_CHECK(2, 0, 8, 0))
        DDBusSender()
            .service("com.deepin.dde.ControlCenter")
            .interface("com.deepin.dde.ControlCenter")
            .path("/com/deepin/dde/ControlCenter")
            .method(QString("Toggle"))
            .call();
#else
        const QString command("dbus-send "
                              "--type=method_call "
                              "--dest=com.deepin.dde.ControlCenter "
                              "/com/deepin/dde/ControlCenter "
                              "com.deepin.dde.ControlCenter.Toggle");
        QProcess::startDetached(command);
#endif
    });
    connect(m_toggleCategoryBtn, &DImageButton::clicked, this, &SearchWidget::toggleMode);
}

QLineEdit *SearchWidget::edit()
{
    return m_searchEdit;
}

void SearchWidget::clearSearchContent()
{
    m_searchEdit->normalMode();
    m_searchEdit->moveFloatWidget();
}

void SearchWidget::setLeftSpacing(int spacing) {
    m_leftSpacing->setFixedWidth(spacing);
}

void SearchWidget::setRightSpacing(int spacing) {
    m_rightSpacing->setFixedWidth(spacing);
}

void SearchWidget::showToggle()
{
    m_toggleCategoryBtn->show();
    m_toggleModeBtn->show();
}

void SearchWidget::hideToggle()
{
    m_toggleCategoryBtn->hide();
    m_toggleModeBtn->hide();
}
