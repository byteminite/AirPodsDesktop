//
// AirPodsDesktop - AirPods Desktop User Experience Enhancement Program.
// Copyright (C) 2021-2022 SpriteOvO
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include "SettingsWindow.h"

#include <algorithm>
#include <cmath>

#include <QAbstractItemView>
#include <QAbstractTextDocumentLayout>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QSvgRenderer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

#include <Config.h>

#include "../Core/Debug.h"
#include "Theme.h"
#include "TintedSvg.h"
#include "Widget/SettingsCard.h"
#include "Widget/ToggleSwitch.h"

using namespace std::chrono_literals;

namespace Gui {

namespace {

constexpr auto kIconRole = Qt::UserRole + 1;
constexpr int kNavItemHeight = 36;
constexpr int kNavIconSize = 16;
constexpr int kContentMargin = 32;

enum Page { AirPodsPage, AppearancePage, GeneralPage, AboutPage, DebugPage };

QString MenuIcon(const char *name)
{
    return QString{":/Resource/Image/Menu/%1.svg"}.arg(name);
}

class ComboPopupPaintFilter final : public QObject
{
public:
    explicit ComboPopupPaintFilter(QObject *parent) : QObject{parent} {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) {
            auto *popup = qobject_cast<QWidget *>(watched);
            const auto &colors = Theme::Manager::Instance().Colors();
            QPainter painter{popup};
            painter.setCompositionMode(QPainter::CompositionMode_Source);
            painter.fillRect(popup->rect(), Qt::transparent);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(colors.popupBorder);
            painter.setBrush(colors.popupSurface);
            painter.drawRoundedRect(QRectF{popup->rect()}.adjusted(3, 3, -3, -3), 8, 8);
            // Consume the private QFrame paint event to avoid a second, native rectangular border.
            return true;
        }
        return QObject::eventFilter(watched, event);
    }
};

void ConfigureComboBox(QComboBox *comboBox)
{
    comboBox->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    comboBox->setMinimumHeight(32);
    // Windows 11 Settings gives its dropdowns one common minimum width.
    comboBox->setMinimumWidth(160);
    // The default combo popup delegate ignores stylesheet item padding; the styled one honours it.
    comboBox->setItemDelegate(new QStyledItemDelegate{comboBox});

    auto *view = comboBox->view();
    view->setFrameShape(QFrame::NoFrame);

    // Qt wraps combo views in a private QFrame. Paint one antialiased surface on a translucent
    // window; a QRegion mask would discard the partially transparent pixels along its corners.
    auto *popup = view->window();
    popup->setObjectName("apdComboPopup");
    Theme::ConfigurePopupSurface(popup);
    popup->installEventFilter(new ComboPopupPaintFilter{popup});
    view->viewport()->setAutoFillBackground(false);
}

// Windows 11 Settings navigation item: a line icon, the label, a subtle fill on hover and
// selection, and a short accent pill on the selected one.
class NavDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        Q_UNUSED(index)
        return QSize{option.rect.width(), kNavItemHeight + 4};
    }

    void paint(
        QPainter *painter, const QStyleOptionViewItem &option,
        const QModelIndex &index) const override
    {
        const auto &colors = Theme::Manager::Instance().Colors();
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        const bool hovered = option.state.testFlag(QStyle::State_MouseOver);
        const QRectF item = QRectF{option.rect}.adjusted(4, 2, -4, -2);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(Qt::NoPen);
        if (selected || hovered) {
            const int alpha = selected ? 15 : 10;
            painter->setBrush(colors.dark ? QColor{255, 255, 255, alpha} : QColor{0, 0, 0, alpha - 4});
            painter->drawRoundedRect(item, 4, 4);
        }
        if (selected) {
            painter->setBrush(colors.accent);
            painter->drawRoundedRect(
                QRectF{item.left(), item.center().y() - 8, 3, 16}, 1.5, 1.5);
        }
        if (option.state.testFlag(QStyle::State_HasFocus) &&
            option.state.testFlag(QStyle::State_KeyboardFocusChange))
        {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen{colors.text, 1.5});
            painter->drawRoundedRect(item.adjusted(1, 1, -1, -1), 4, 4);
        }

        const auto iconResource = index.data(kIconRole).toString();
        const auto icon = TintedSvgImage(
            iconResource, colors.text, kNavIconSize, painter->device()->devicePixelRatioF());
        const QPointF iconPos{item.left() + 14, item.center().y() - kNavIconSize / 2.0};
        painter->drawImage(QRectF{iconPos, QSizeF{kNavIconSize, kNavIconSize}}, icon);

        painter->setPen(colors.text);
        painter->setFont(option.font);
        const QRectF textRect = item.adjusted(14 + kNavIconSize + 14, 0, -8, 0);
        painter->drawText(
            textRect, Qt::AlignVCenter | Qt::AlignLeft,
            option.fontMetrics.elidedText(
                index.data(Qt::DisplayRole).toString(), Qt::ElideRight,
                static_cast<int>(textRect.width())));
        painter->restore();
    }
};

QLabel *SectionHeading(QWidget *parent)
{
    auto *label = new QLabel{parent};
    auto font = label->font();
    font.setPixelSize(14);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setContentsMargins(2, 18, 0, 6);
    return label;
}

QVBoxLayout *PageLayout(QWidget *page)
{
    auto *layout = new QVBoxLayout{page};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    return layout;
}

Widget::SettingsCard *AddCard(QVBoxLayout *layout, QWidget *page, const char *icon)
{
    auto *card = new Widget::SettingsCard{icon == nullptr ? QString{} : MenuIcon(icon), page};
    layout->addWidget(card);
    return card;
}

QToolButton *ExternalLinkButton(QWidget *parent, const QString &url)
{
    auto *button = new QToolButton{parent};
    button->setAutoRaise(true);
    button->setFixedSize(32, 32);
    button->setCursor(Qt::PointingHandCursor);
    button->setStyleSheet("QToolButton { border: none; background: transparent; }");
    const auto applyIcon = [button] {
        button->setIcon(
            TintedSvgIcon(MenuIcon("ExternalLink"), Theme::Manager::Instance().Colors().text, 16));
    };
    applyIcon();
    QObject::connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, button, applyIcon);
    QObject::connect(button, &QToolButton::clicked, button, [url] {
        QDesktopServices::openUrl(QUrl{url});
    });
    return button;
}

} // namespace

SettingsWindow::SettingsWindow(
    std::function<int()> getCurrentLocaleIndex, Core::QuickConnect::Controller &quickConnect,
    QWidget *parent)
    : QDialog{parent}, _getCurrentLocaleIndex{std::move(getCurrentLocaleIndex)},
      _quickConnect{quickConnect}
{
    setObjectName("SettingsWindow");
    // Mica shows through every transparent pixel; without it `paintEvent` fills a solid colour.
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowIcon(QIcon{":/Resource/Image/Icon.svg"});
    setMinimumSize(640, 420);
    resize(900, 620);

    BuildUi();

    connect(_ui.navList, &QListWidget::currentRowChanged, this, &SettingsWindow::OnPageChanged);
    _ui.navList->setCurrentRow(0);

    for (const auto &[card, checkBox] : {
             std::pair{_ui.cardLowAudioLatency, _ui.cbLowAudioLatency},
             std::pair{_ui.cardAutoEarDetection, _ui.cbAutoEarDetection},
             std::pair{_ui.cardTrayQuickConnect, _ui.cbTrayQuickConnectEnabled},
         })
    {
        card->Title()->setBuddy(checkBox);
    }

#if !defined APD_DEBUG
    _ui.navList->item(DebugPage)->setHidden(true);
#else
    connect(
        _ui.cbAdvOverride, &QCheckBox::toggled, this, &SettingsWindow::On_cbAdvOverride_toggled);
    connect(
        _ui.teAdvOverride, &QTextEdit::textChanged, this,
        &SettingsWindow::On_teAdvOverride_textChanged);
#endif

    auto versionText =
        QString{"<a href=\"%1\">v%2</a>"}.arg(Config::UrlCurrentRelease).arg(CONFIG_VERSION_STRING);
#if defined APD_BUILD_GIT_HASH
    versionText +=
        QString{" (<a href=\"%1\">%2</a>)"}
            .arg(QString{"%1/commit/%2"}.arg(Config::UrlRepository).arg(APD_BUILD_GIT_HASH))
            .arg(QString{APD_BUILD_GIT_HASH}.left(7));
#endif
    _ui.lbVersion->setText(versionText);

    _ui.hsMaxReceivingRange->setMinimum(50);
    _ui.hsMaxReceivingRange->setMaximum(100);

    for (const auto &locale : Utils::AvailableLocales()) {
        const auto displayName = locale.language() == QLocale::English
                                     ? QStringLiteral("English")
                                     : locale.nativeLanguageName();
        _ui.cbLanguages->addItem(displayName);
    }
    _ui.cbLanguages->addItem("...");

    Retranslate();
    connect(&Theme::Manager::Instance(), &Theme::Manager::Changed, this, [this] {
        UpdateDescriptions();
        update();
    });

    connect(&_quickConnect, &Core::QuickConnect::Controller::DevicesChanged, this, [this] {
        UpdateQuickConnectDevices(GetCurrent());
    });

    Update(GetCurrent(), false);

    connect(_ui.pbRestoreDefaults, &QPushButton::clicked, this, &SettingsWindow::RestoreDefaults);

    connect(
        _ui.cbLanguages, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int index) {
            if (_trigger) {
                On_cbLanguages_currentIndexChanged(index);
            }
        });

    connect(_ui.cbAutoRun, &QCheckBox::toggled, this, [this](bool checked) {
        if (_trigger) {
            On_cbAutoRun_toggled(checked);
        }
    });

    connect(
        _ui.cbAppearanceMode, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (_trigger) {
                On_cbAppearanceMode_currentIndexChanged(index);
            }
        });

    connect(_ui.cbTrayQuickConnectEnabled, &QCheckBox::toggled, this, [this](bool checked) {
        if (_trigger) {
            On_cbTrayQuickConnectEnabled_toggled(checked);
        }
    });

    connect(
        _ui.cbTrayQuickConnectDevice, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (_trigger) {
                On_cbTrayQuickConnectDevice_currentIndexChanged(index);
            }
        });

    connect(_ui.cbLowAudioLatency, &QCheckBox::toggled, this, [this](bool checked) {
        if (_trigger) {
            On_cbLowAudioLatency_toggled(checked);
        }
    });

    connect(_ui.cbAutoEarDetection, &QCheckBox::toggled, this, [this](bool checked) {
        if (_trigger) {
            On_cbAutoEarDetection_toggled(checked);
        }
    });

    connect(_ui.hsMaxReceivingRange, &QSlider::valueChanged, this, [this](int value) {
        if (_trigger) {
            On_hsMaxReceivingRange_valueChanged(value);
        }
    });

    connect(
        _ui.cbTrayIconBattery, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (_trigger && index >= 0) {
                On_cbDisplayBatteryOnTrayIcon_toggled(static_cast<TrayIconBatteryBehavior>(index));
            }
        });

    connect(
        _ui.cbTaskbarBattery, qOverload<int>(&QComboBox::currentIndexChanged), this,
        [this](int index) {
            if (_trigger && index >= 0) {
                On_cbDisplayBatteryOnTaskbar_toggled(static_cast<TaskbarStatusBehavior>(index));
            }
        });

    connect(_ui.deviceCard, &Widget::DeviceCard::UnbindRequested, this, [this] {
        if (_trigger) {
            On_pbUnbind_clicked();
        }
    });
    connect(_ui.deviceCard, &Widget::DeviceCard::BindRequested, this, [this] {
        emit BindRequested();
    });

    connect(_ui.pbOpenLogsDirectory, &QPushButton::clicked, this, [this]() {
        if (_trigger) {
            On_pbOpenLogsDirectory_clicked();
        }
    });
}

void SettingsWindow::BuildUi()
{
    // Nav pane: the AirPods at the top, then the page list.
    auto *navPane = new QWidget{this};
    navPane->setObjectName("settingsNavPane");
    auto *navLayout = new QVBoxLayout{navPane};
    navLayout->setContentsMargins(8, 4, 4, 8);
    navLayout->setSpacing(4);

    _ui.deviceBadge = new Widget::DeviceBadge{navPane};
    navLayout->addWidget(_ui.deviceBadge);

    _ui.navList = new QListWidget{navPane};
    _ui.navList->setObjectName("navList");
    _ui.navList->setFrameShape(QFrame::NoFrame);
    _ui.navList->setFocusPolicy(Qt::StrongFocus);
    _ui.navList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui.navList->setMouseTracking(true);
    _ui.navList->setItemDelegate(new NavDelegate{_ui.navList});
    _ui.navList->setStyleSheet(
        "QListWidget#navList { background: transparent; border: none; padding: 0; }"
        "QListWidget#navList::item { background: transparent; border: none; margin: 0; }");
    _ui.navList->viewport()->setAutoFillBackground(false);
    for (const auto *icon : {"Headphones", "Palette", "Sliders", "Info", "Bug"}) {
        auto *item = new QListWidgetItem{_ui.navList};
        item->setData(kIconRole, MenuIcon(icon));
    }
    navLayout->addWidget(_ui.navList, 1);

    // Content: a fixed page title over a scrolling column of cards.
    auto *content = new QWidget{this};
    auto *contentLayout = new QVBoxLayout{content};
    contentLayout->setContentsMargins(0, 8, 0, 0);
    contentLayout->setSpacing(0);

    _ui.pageTitle = new QLabel{content};
    _ui.pageTitle->setObjectName("pageTitle");
    _ui.pageTitle->setProperty("cssClass", "pageTitle");
    _ui.pageTitle->setContentsMargins(kContentMargin / 2, 4, kContentMargin, 16);
    contentLayout->addWidget(_ui.pageTitle);

    _ui.pagesScrollArea = new QScrollArea{content};
    _ui.pagesScrollArea->setObjectName("pagesScrollArea");
    _ui.pagesScrollArea->setWidgetResizable(true);
    _ui.pagesScrollArea->setFrameShape(QFrame::NoFrame);
    _ui.pagesScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui.pagesScrollArea->setStyleSheet(
        "QScrollArea#pagesScrollArea, QWidget#pagesViewport, QWidget#pagesContainer, "
        "QWidget#pagesContainer > QWidget { background: transparent; }");
    _ui.pagesScrollArea->viewport()->setObjectName("pagesViewport");
    _ui.pagesScrollArea->viewport()->setAutoFillBackground(false);

    auto *container = new QWidget;
    container->setObjectName("pagesContainer");
    auto *containerLayout = new QVBoxLayout{container};
    containerLayout->setContentsMargins(kContentMargin / 2, 0, kContentMargin, 24);
    containerLayout->setSpacing(0);
    _ui.pages = {
        BuildAirPodsPage(), BuildAppearancePage(), BuildGeneralPage(), BuildAboutPage(),
        BuildDebugPage()};
    for (auto *page : _ui.pages) {
        page->setParent(container);
        page->hide();
        containerLayout->addWidget(page);
    }
    containerLayout->addStretch();
    _ui.pagesScrollArea->setWidget(container);
    contentLayout->addWidget(_ui.pagesScrollArea, 1);
    APD_ASSERT(static_cast<int>(_ui.pages.size()) == _ui.navList->count());

    auto *layout = new QHBoxLayout{this};
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(navPane);
    layout->addWidget(content, 1);
}

QWidget *SettingsWindow::BuildAirPodsPage()
{
    auto *page = new QWidget;
    auto *layout = PageLayout(page);

    _ui.deviceCard = new Widget::DeviceCard{page};
    layout->addWidget(_ui.deviceCard);

    _ui.lbFeatures = SectionHeading(page);
    layout->addWidget(_ui.lbFeatures);

    _ui.cardAutoEarDetection = AddCard(layout, page, "Ear");
    _ui.cardAutoEarDetection->Title()->setObjectName("lbAutoEarDetection");
    _ui.cardAutoEarDetection->Description()->setObjectName("lbDescAutoEarDetection");
    _ui.cbAutoEarDetection = new Widget::ToggleSwitch{_ui.cardAutoEarDetection};
    _ui.cbAutoEarDetection->setObjectName("cbAutoEarDetection");
    _ui.cardAutoEarDetection->SetControl(_ui.cbAutoEarDetection);

    _ui.cardLowAudioLatency = AddCard(layout, page, "AudioLines");
    _ui.cardLowAudioLatency->Title()->setObjectName("lbLowAudioLatency");
    _ui.cardLowAudioLatency->Description()->setObjectName("lbDescLowAudioLatency");
    _ui.cbLowAudioLatency = new Widget::ToggleSwitch{_ui.cardLowAudioLatency};
    _ui.cbLowAudioLatency->setObjectName("cbLowAudioLatency");
    _ui.cardLowAudioLatency->SetControl(_ui.cbLowAudioLatency);

    _ui.cardTrayQuickConnect = AddCard(layout, page, "MousePointerClick");
    _ui.cardTrayQuickConnect->Title()->setObjectName("lbTrayQuickConnect");
    _ui.cardTrayQuickConnect->Description()->setObjectName("lbDescTrayQuickConnect");
    _ui.cbTrayQuickConnectEnabled = new Widget::ToggleSwitch{_ui.cardTrayQuickConnect};
    _ui.cbTrayQuickConnectEnabled->setObjectName("cbTrayQuickConnectEnabled");
    _ui.cardTrayQuickConnect->SetControl(_ui.cbTrayQuickConnectEnabled);

    _ui.cardTrayQuickConnectDevice = AddCard(layout, page, nullptr);
    _ui.cardTrayQuickConnectDevice->SetSubCard(true);
    _ui.cbTrayQuickConnectDevice = new QComboBox{_ui.cardTrayQuickConnectDevice};
    _ui.cbTrayQuickConnectDevice->setObjectName("cbTrayQuickConnectDevice");
    ConfigureComboBox(_ui.cbTrayQuickConnectDevice);
    _ui.cardTrayQuickConnectDevice->SetControl(_ui.cbTrayQuickConnectDevice);
    _ui.cardTrayQuickConnectDevice->Title()->setBuddy(_ui.cbTrayQuickConnectDevice);

    return page;
}

QWidget *SettingsWindow::BuildAppearancePage()
{
    auto *page = new QWidget;
    auto *layout = PageLayout(page);

    _ui.cardAppearance = AddCard(layout, page, "Sun");
    _ui.cardAppearance->Title()->setObjectName("lbAppearance");
    _ui.cbAppearanceMode = new QComboBox{_ui.cardAppearance};
    _ui.cbAppearanceMode->setObjectName("cbAppearanceMode");
    _ui.cbAppearanceMode->addItems({{}, {}, {}});
    ConfigureComboBox(_ui.cbAppearanceMode);
    _ui.cardAppearance->SetControl(_ui.cbAppearanceMode);

    _ui.cardTrayIconBattery = AddCard(layout, page, "BatteryMedium");
    _ui.cbTrayIconBattery = new QComboBox{_ui.cardTrayIconBattery};
    _ui.cbTrayIconBattery->setObjectName("cbTrayIconBattery");
    _ui.cbTrayIconBattery->addItems({{}, {}, {}});
    ConfigureComboBox(_ui.cbTrayIconBattery);
    _ui.cardTrayIconBattery->SetControl(_ui.cbTrayIconBattery);

    _ui.cardTaskbarBattery = AddCard(layout, page, "PanelBottom");
    _ui.cbTaskbarBattery = new QComboBox{_ui.cardTaskbarBattery};
    _ui.cbTaskbarBattery->setObjectName("cbTaskbarBattery");
    _ui.cbTaskbarBattery->addItems({{}, {}, {}});
    ConfigureComboBox(_ui.cbTaskbarBattery);
    _ui.cardTaskbarBattery->SetControl(_ui.cbTaskbarBattery);

    for (const auto &[card, combo] : {
             std::pair{_ui.cardAppearance, _ui.cbAppearanceMode},
             std::pair{_ui.cardTrayIconBattery, _ui.cbTrayIconBattery},
             std::pair{_ui.cardTaskbarBattery, _ui.cbTaskbarBattery},
         })
    {
        card->Title()->setBuddy(combo);
    }

    return page;
}

QWidget *SettingsWindow::BuildGeneralPage()
{
    auto *page = new QWidget;
    auto *layout = PageLayout(page);

    _ui.cardLanguage = AddCard(layout, page, "Languages");
    _ui.cbLanguages = new QComboBox{_ui.cardLanguage};
    _ui.cbLanguages->setObjectName("cbLanguages");
    ConfigureComboBox(_ui.cbLanguages);
    _ui.cardLanguage->SetControl(_ui.cbLanguages);
    _ui.cardLanguage->Title()->setBuddy(_ui.cbLanguages);

    _ui.cardAutoRun = AddCard(layout, page, "Power");
    _ui.cbAutoRun = new Widget::ToggleSwitch{_ui.cardAutoRun};
    _ui.cbAutoRun->setObjectName("cbAutoRun");
    _ui.cardAutoRun->SetControl(_ui.cbAutoRun);
    _ui.cardAutoRun->Title()->setBuddy(_ui.cbAutoRun);

    _ui.cardMaxReceivingRange = AddCard(layout, page, "Bluetooth");
    _ui.hsMaxReceivingRange = new QSlider{Qt::Horizontal, _ui.cardMaxReceivingRange};
    _ui.hsMaxReceivingRange->setObjectName("hsMaxReceivingRange");
    _ui.hsMaxReceivingRange->setFixedWidth(160);
    _ui.cardMaxReceivingRange->SetControl(_ui.hsMaxReceivingRange);
    _ui.cardMaxReceivingRange->Title()->setBuddy(_ui.hsMaxReceivingRange);

    _ui.cardDiagnostics = AddCard(layout, page, "FileText");
    _ui.pbOpenLogsDirectory = new QPushButton{_ui.cardDiagnostics};
    _ui.pbOpenLogsDirectory->setObjectName("pbOpenLogsDirectory");
    _ui.pbOpenLogsDirectory->setAutoDefault(false);
    _ui.cardDiagnostics->SetControl(_ui.pbOpenLogsDirectory);

    _ui.cardRestoreDefaults = AddCard(layout, page, "RotateCcw");
    _ui.pbRestoreDefaults = new QPushButton{_ui.cardRestoreDefaults};
    _ui.pbRestoreDefaults->setObjectName("pbRestoreDefaults");
    _ui.pbRestoreDefaults->setAutoDefault(false);
    _ui.cardRestoreDefaults->SetControl(_ui.pbRestoreDefaults);

    return page;
}

QWidget *SettingsWindow::BuildAboutPage()
{
    auto *page = new QWidget;
    auto *layout = PageLayout(page);

    // Header: app icon, name and version, painted as a Fluent card.
    class HeaderCard final : public QFrame
    {
    public:
        using QFrame::QFrame;

    protected:
        void paintEvent(QPaintEvent *) override
        {
            Widget::PaintFluentCard(this, 8.0);
        }
    };
    auto *header = new HeaderCard{page};
    header->setObjectName("aboutHeaderCard");
    auto *icon = new QLabel{header};
    icon->setFixedSize(48, 48);
    {
        const qreal ratio = devicePixelRatioF();
        QImage image{QSize{48, 48} * ratio, QImage::Format_ARGB32_Premultiplied};
        image.setDevicePixelRatio(ratio);
        image.fill(Qt::transparent);
        QPainter painter{&image};
        QSvgRenderer{QStringLiteral(":/Resource/Image/Icon.svg")}.render(
            &painter, QRectF{0, 0, 48, 48});
        painter.end();
        icon->setPixmap(QPixmap::fromImage(image));
    }
    _ui.lbAppName = new QLabel{QStringLiteral("AirPodsDesktop"), header};
    _ui.lbAppName->setProperty("cssClass", "appTitle");
    _ui.lbVersion = new QLabel{header};
    _ui.lbVersion->setObjectName("lbVersion");
    _ui.lbVersion->setOpenExternalLinks(true);
    _ui.lbVersion->setTextFormat(Qt::RichText);

    auto *headerText = new QVBoxLayout;
    headerText->setSpacing(2);
    headerText->addWidget(_ui.lbAppName);
    headerText->addWidget(_ui.lbVersion);
    auto *headerLayout = new QHBoxLayout{header};
    headerLayout->setContentsMargins(20, 16, 20, 16);
    headerLayout->setSpacing(16);
    headerLayout->addWidget(icon);
    headerLayout->addLayout(headerText, 1);
    layout->addWidget(header);

    _ui.lbRelatedLinks = SectionHeading(page);
    layout->addWidget(_ui.lbRelatedLinks);

    _ui.cardSourceCode = AddCard(layout, page, "Code");
    _ui.cardSourceCode->SetControl(ExternalLinkButton(_ui.cardSourceCode, Config::UrlRepository));
    _ui.cardIssues = AddCard(layout, page, "Bug");
    _ui.cardIssues->SetControl(ExternalLinkButton(_ui.cardIssues, Config::UrlIssues));
    _ui.cardLicense = AddCard(layout, page, "Scale");
    _ui.cardLicense->SetControl(ExternalLinkButton(_ui.cardLicense, Config::UrlLicense));

    _ui.tbCredits = new QTextBrowser{page};
    _ui.tbCredits->setObjectName("tbCredits");
    _ui.tbCredits->setOpenExternalLinks(true);
    _ui.tbCredits->setFrameShape(QFrame::NoFrame);
    _ui.tbCredits->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui.tbCredits->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    _ui.tbCredits->viewport()->setAutoFillBackground(false);
    // Grow with the text so the page, not the browser, scrolls.
    connect(
        _ui.tbCredits->document()->documentLayout(),
        &QAbstractTextDocumentLayout::documentSizeChanged, _ui.tbCredits,
        [browser = _ui.tbCredits](const QSizeF &size) {
            browser->setFixedHeight(static_cast<int>(std::ceil(size.height())) + 4);
        });
    layout->addSpacing(12);
    layout->addWidget(_ui.tbCredits);

    _ui.lbCopyright = new QLabel{page};
    _ui.lbCopyright->setObjectName("lbCopyright");
    _ui.lbCopyright->setContentsMargins(4, 8, 0, 0);
    layout->addWidget(_ui.lbCopyright);

    return page;
}

QWidget *SettingsWindow::BuildDebugPage()
{
    auto *page = new QWidget;
    auto *layout = PageLayout(page);
    _ui.cbAdvOverride = new QCheckBox{page};
    _ui.cbAdvOverride->setObjectName("cbAdvOverride");
    _ui.teAdvOverride = new QTextEdit{page};
    _ui.teAdvOverride->setObjectName("teAdvOverride");
    _ui.teAdvOverride->setMinimumHeight(200);
    layout->addWidget(_ui.cbAdvOverride);
    layout->addWidget(_ui.teAdvOverride);
    return page;
}

void SettingsWindow::OnPageChanged(int index)
{
    if (index < 0 || index >= static_cast<int>(_ui.pages.size())) {
        return;
    }
    if (_ui.currentPage >= 0) {
        _ui.pages.at(_ui.currentPage)->hide();
    }
    _ui.currentPage = index;
    _ui.pages.at(index)->show();
    _ui.pagesScrollArea->verticalScrollBar()->setValue(0);
    if (auto *item = _ui.navList->item(index); item != nullptr) {
        _ui.pageTitle->setText(item->text());
    }
}

void SettingsWindow::Retranslate()
{
    // Strings from the former `.ui` form keep their "SettingsWindow" translations. Each call is
    // spelled out so `lupdate` still sees them.
    setWindowTitle(QCoreApplication::translate("SettingsWindow", "AirPodsDesktop Settings"));

    _ui.navList->item(AirPodsPage)->setText(QStringLiteral("AirPods"));
    _ui.navList->item(AppearancePage)
        ->setText(QCoreApplication::translate("SettingsWindow", "Appearance"));
    _ui.navList->item(GeneralPage)->setText(QCoreApplication::translate("SettingsWindow", "General"));
    _ui.navList->item(AboutPage)->setText(QCoreApplication::translate("SettingsWindow", "About"));
    _ui.navList->item(DebugPage)->setText(QStringLiteral("Debug"));
    for (int i = 0; i < _ui.navList->count(); ++i) {
        _ui.navList->item(i)->setToolTip(_ui.navList->item(i)->text());
    }
    if (auto *item = _ui.navList->currentItem(); item != nullptr) {
        _ui.pageTitle->setText(item->text());
    }

    // AirPods
    _ui.lbFeatures->setText(QCoreApplication::translate("SettingsWindow", "Features"));
    _ui.cardAutoEarDetection->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Automatic ear detection"));
    _ui.cardLowAudioLatency->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Low audio latency"));
    _ui.cardTrayQuickConnect->Title()->setText(QCoreApplication::translate(
        "SettingsWindow", "Connect the selected device on a tray icon click"));
    _ui.cardTrayQuickConnectDevice->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Device"));
    _ui.cardTrayQuickConnectDevice->Description()->setText(QCoreApplication::translate(
        "SettingsWindow", "The paired Bluetooth audio device a tray icon click connects."));
    _ui.cardTrayQuickConnectDevice->Description()->show();
    for (const auto &[card, checkBox] : {
             std::pair{_ui.cardLowAudioLatency, _ui.cbLowAudioLatency},
             std::pair{_ui.cardAutoEarDetection, _ui.cbAutoEarDetection},
             std::pair{_ui.cardTrayQuickConnect, _ui.cbTrayQuickConnectEnabled},
         })
    {
        checkBox->setAccessibleName(card->Title()->text());
    }
    _ui.cbTrayQuickConnectDevice->setAccessibleName(
        _ui.cardTrayQuickConnectDevice->Title()->text());

    // Appearance
    _ui.cardAppearance->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Appearance"));
    _ui.cbAppearanceMode->setItemText(
        0, QCoreApplication::translate("SettingsWindow", "Use system setting"));
    _ui.cbAppearanceMode->setItemText(1, QCoreApplication::translate("SettingsWindow", "Light"));
    _ui.cbAppearanceMode->setItemText(2, QCoreApplication::translate("SettingsWindow", "Dark"));

    _ui.cardTrayIconBattery->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Display battery on tray icon"));
    _ui.cbTrayIconBattery->setItemText(0, QCoreApplication::translate("SettingsWindow", "Disable"));
    _ui.cbTrayIconBattery->setItemText(
        1, QCoreApplication::translate("SettingsWindow", "When low battery"));
    _ui.cbTrayIconBattery->setItemText(2, QCoreApplication::translate("SettingsWindow", "Always"));

    _ui.cardTaskbarBattery->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Display battery on taskbar"));
    _ui.cbTaskbarBattery->setItemText(0, QCoreApplication::translate("SettingsWindow", "Disable"));
    _ui.cbTaskbarBattery->setItemText(1, QCoreApplication::translate("SettingsWindow", "As text"));
    _ui.cbTaskbarBattery->setItemText(2, QCoreApplication::translate("SettingsWindow", "As icon"));

    for (const auto &[card, combo] : {
             std::pair{_ui.cardAppearance, _ui.cbAppearanceMode},
             std::pair{_ui.cardTrayIconBattery, _ui.cbTrayIconBattery},
             std::pair{_ui.cardTaskbarBattery, _ui.cbTaskbarBattery},
         })
    {
        combo->setAccessibleName(card->Title()->text());
        // `AdjustToContents` only re-measures when the items change.
        combo->adjustSize();
    }

    // General
    _ui.cardLanguage->Title()->setText(QCoreApplication::translate("SettingsWindow", "Language"));
    _ui.cardAutoRun->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Launch when system starts"));
    _ui.cardMaxReceivingRange->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Bluetooth maximum receiving range"));
    _ui.cardDiagnostics->Title()->setText(
        QCoreApplication::translate("SettingsWindow", "Diagnostics"));
    _ui.cardDiagnostics->Description()->setText(
        QCoreApplication::translate("SettingsWindow", "Log files help when reporting an issue."));
    _ui.cardDiagnostics->Description()->show();
    _ui.pbOpenLogsDirectory->setText(
        QCoreApplication::translate("SettingsWindow", "Open logs directory"));
    _ui.cardRestoreDefaults->Title()->setText(tr("Reset all settings"));
    _ui.cardRestoreDefaults->Description()->setText(
        tr("Put every setting back to how it was when the app was first installed."));
    _ui.cardRestoreDefaults->Description()->show();
    _ui.pbRestoreDefaults->setText(tr("Restore Defaults"));
    for (const auto &[card, control] : {
             std::pair<Widget::SettingsCard *, QWidget *>{_ui.cardLanguage, _ui.cbLanguages},
             std::pair<Widget::SettingsCard *, QWidget *>{_ui.cardAutoRun, _ui.cbAutoRun},
             std::pair<Widget::SettingsCard *, QWidget *>{
                 _ui.cardMaxReceivingRange, _ui.hsMaxReceivingRange},
         })
    {
        control->setAccessibleName(card->Title()->text());
    }

    // About
    _ui.lbRelatedLinks->setText(tr("Related links"));
    _ui.cardSourceCode->Title()->setText(tr("Source code"));
    _ui.cardSourceCode->Description()->setText(QString{Config::UrlRepository});
    _ui.cardSourceCode->Description()->show();
    _ui.cardIssues->Title()->setText(tr("Report an issue"));
    _ui.cardLicense->Title()->setText(tr("License"));
    _ui.cardLicense->Description()->setText(QStringLiteral("GNU General Public License v3.0"));
    _ui.cardLicense->Description()->show();
    _ui.lbCopyright->setText(QCoreApplication::translate(
        "SettingsWindow", "Copyright \xC2\xA9 2021-2022 SpriteOvO. All rights reserved."));

    // Debug
    _ui.cbAdvOverride->setText(QCoreApplication::translate("SettingsWindow", "Enabled"));

    InitCreditsText();
    UpdateDescriptions();
}

int SettingsWindow::GetTabCount() const
{
    return static_cast<int>(_ui.pages.size());
}

void SettingsWindow::SetLowAudioLatencyChecked(bool checked)
{
    const auto oldTrigger = _trigger;
    _trigger = false;
    _ui.cbLowAudioLatency->setChecked(checked);
    _trigger = oldTrigger;
}

void SettingsWindow::SetDevice(const Widget::DeviceSnapshot &snapshot)
{
    _ui.deviceCard->SetSnapshot(snapshot);
    _ui.deviceBadge->SetSnapshot(snapshot);
}

int SettingsWindow::GetTabCurrentIndex() const
{
    return _ui.currentPage;
}

int SettingsWindow::GetTabLastVisibleIndex() const
{
    return GetTabCount() - 2 /* Skip the Debug tab */;
}

void SettingsWindow::SetTabIndex(int index)
{
    _ui.navList->setCurrentRow(index);
}

void SettingsWindow::UpdateDescriptions()
{
    // The setting descriptions live in `Core::Settings` (as `Impl::Desc`) and are translated
    // there, so they are re-read on every language change.
    const auto &constMetaFields = GetConstMetaFields();

    for (const auto &[card, description] : {
             std::pair{_ui.cardLowAudioLatency, constMetaFields.low_audio_latency.Description()},
             std::pair{
                 _ui.cardAutoEarDetection, constMetaFields.automatic_ear_detection.Description()},
             std::pair{
                 _ui.cardTrayQuickConnect, constMetaFields.tray_quick_connect_enabled.Description()},
         })
    {
        card->Description()->setText(description);
        card->Description()->setVisible(!description.isEmpty());
    }

    // Links in rich-text labels take the theme accent so they stay readable in dark mode.
    const auto accent = Theme::Manager::Instance().Accent().name(QColor::HexRgb);
    for (auto *label : {static_cast<QWidget *>(_ui.lbVersion), static_cast<QWidget *>(_ui.tbCredits)}) {
        auto palette = label->palette();
        palette.setColor(QPalette::Link, QColor{accent});
        label->setPalette(palette);
    }
}

void SettingsWindow::InitCreditsText()
{
    //: To credit translators, you can leave your name here if you wish.
    //: (Sorted by time added, separated by "|" character, only single "|" for empty)
    auto l10nContributorsStr = tr("|");
    if (!l10nContributorsStr.isEmpty()) {
        auto l10nContributors = l10nContributorsStr.split('|', Qt::SkipEmptyParts);
        if (l10nContributors.isEmpty()) {
            l10nContributorsStr.clear();
        }
        else {
            l10nContributorsStr = tr("Translation Contributors:");
            for (const auto &contributor : l10nContributors) {
                l10nContributorsStr += "<br> - " + contributor.trimmed();
            }
            l10nContributorsStr += "<br><br>";
        }
    }

    static QString libs = [] {
        struct LibInfo {
            const char *name, *url, *license, *licenseUrl;
        };
        static std::vector<LibInfo> libs{
            // clang-format off
            { "Qt 6", "https://www.qt.io/download-qt-installer", "LGPLv3", "https://doc.qt.io/qt-6/lgpl.html" },
            { "spdlog", "https://github.com/gabime/spdlog", "MIT", "https://github.com/gabime/spdlog/blob/v1.x/LICENSE" },
            { "cxxopts", "https://github.com/jarro2783/cxxopts", "MIT", "https://github.com/jarro2783/cxxopts/blob/master/LICENSE" },
            { "cpr", "https://github.com/whoshuu/cpr", "MIT", "https://github.com/whoshuu/cpr/blob/master/LICENSE" },
            { "json", "https://github.com/nlohmann/json", "MIT", "https://github.com/nlohmann/json/blob/develop/LICENSE.MIT" },
            { "SingleApplication", "https://github.com/itay-grudev/SingleApplication", "MIT", "https://github.com/itay-grudev/SingleApplication/blob/master/LICENSE" },
            { "pfr", "https://github.com/boostorg/pfr", "BSL-1.0", "https://github.com/boostorg/pfr/blob/develop/LICENSE_1_0.txt" },
            { "magic_enum", "https://github.com/Neargye/magic_enum", "MIT", "https://github.com/Neargye/magic_enum/blob/master/LICENSE" },
            { "stacktrace", "https://github.com/boostorg/stacktrace", "BSL-1.0", "https://www.boost.org/LICENSE_1_0.txt" },
            { "Lucide", "https://lucide.dev", "ISC", "https://lucide.dev/license" } // clang-format on
        };

        QString result;
        for (const auto &lib : libs) {
            result += QString{"<br> - <a href=\"%2\">%1</a> (<a href=\"%4\">%3 License</a>)"}
                          .arg(lib.name)
                          .arg(lib.url)
                          .arg(lib.license)
                          .arg(lib.licenseUrl);
        }
        return result;
    }();
    auto libsStr = tr("Third-Party Libraries:") + libs;

    _ui.tbCredits->setHtml(l10nContributorsStr + libsStr);
}

void SettingsWindow::RestoreDefaults()
{
    Save(GetDefault());
    Update(GetCurrent(), false);
}

void SettingsWindow::Update(const Fields &fields, bool trigger)
{
    _trigger = trigger;

    auto currentLangIndex = _getCurrentLocaleIndex();
    _lastLanguageIndex = currentLangIndex;
    _ui.cbLanguages->setCurrentIndex(currentLangIndex);

    _ui.cbAutoRun->setChecked(fields.auto_run);

    _ui.cbAppearanceMode->setCurrentIndex(static_cast<int>(fields.appearance_mode));

    _ui.cbLowAudioLatency->setChecked(fields.low_audio_latency);

    _ui.cbAutoEarDetection->setChecked(fields.automatic_ear_detection);

    _ui.hsMaxReceivingRange->setValue(-fields.rssi_min);

    _ui.cbTrayIconBattery->setCurrentIndex(static_cast<int>(fields.tray_icon_battery));
    _ui.cbTaskbarBattery->setCurrentIndex(static_cast<int>(fields.battery_on_taskbar));

    UpdateQuickConnectDevices(fields);

    _trigger = true;
}

void SettingsWindow::UpdateQuickConnectDevices(const Fields &fields)
{
    const auto previousTrigger = _trigger;
    _trigger = false;

    const auto &selectedId = fields.tray_quick_connect_device_id;

    _ui.cbTrayQuickConnectDevice->clear();
    for (const auto &device : _quickConnect.Devices()) {
        _ui.cbTrayQuickConnectDevice->addItem(device.name, device.id);
    }

    auto selectedIndex = _ui.cbTrayQuickConnectDevice->findData(selectedId);
    if (selectedIndex == -1 && !selectedId.isEmpty()) {
        // The saved device is paired but not reporting an audio endpoint right now - switched off,
        // out of range, or the enumeration has not finished. Show it as chosen but unavailable
        // rather than an empty box that reads like nothing was ever configured.
        _ui.cbTrayQuickConnectDevice->addItem(tr("Saved device (not available)"), selectedId);
        selectedIndex = _ui.cbTrayQuickConnectDevice->count() - 1;
    }
    _ui.cbTrayQuickConnectDevice->setCurrentIndex(selectedIndex);

    _ui.cbTrayQuickConnectEnabled->setChecked(fields.tray_quick_connect_enabled);
    // Always leave the toggle usable: a transient empty enumeration must never trap the user in
    // a state they cannot turn off.
    _ui.cbTrayQuickConnectEnabled->setEnabled(true);
    _ui.cbTrayQuickConnectDevice->setEnabled(_ui.cbTrayQuickConnectDevice->count() > 0);

    _trigger = previousTrigger;
}

void SettingsWindow::UpdateAdvOverride()
{
    auto advsStr = _ui.teAdvOverride->toPlainText();
    auto vAdvsStr = advsStr.split('\n', Qt::SkipEmptyParts);

    std::vector<std::vector<uint8_t>> advs;

    for (const auto &advStr : vAdvsStr) {

        auto advBytesStr = advStr.split(' ', Qt::SkipEmptyParts);

        std::vector<uint8_t> bytes;
        for (const auto advByteStr : advBytesStr) {
            bool success{false};
            auto byte = advByteStr.toUInt(&success, 16);
            APD_ASSERT(success);
            bytes.emplace_back(byte);
        }

        advs.emplace_back(std::move(bytes));
    }

    Core::Debug::DebugConfig::GetInstance().UpdateAdvOverride(
        _ui.cbAdvOverride->isChecked(), std::move(advs));
}

void SettingsWindow::showEvent(QShowEvent *event)
{
    if (!_mica) {
        _mica = Theme::EnableMicaBackdrop(this);
    }

    Update(GetCurrent(), false);

    // Enumerating paired Bluetooth devices and every audio endpoint is not free, so it happens
    // when the dialog is opened rather than on every application start.
    _quickConnect.RefreshDevices();
    QDialog::showEvent(event);
}

void SettingsWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter{this};
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    // With Mica the window stays transparent so the material shows through, as in Windows 11
    // Settings; otherwise (and in off-screen grabs) it is the solid theme background.
    painter.fillRect(
        rect(), _mica ? QColor{Qt::transparent}
                      : Theme::Manager::Instance().Colors().windowBackground);
}

void SettingsWindow::resizeEvent(QResizeEvent *event)
{
    // The nav pane narrows on small windows so the cards keep enough room.
    if (auto *navPane = findChild<QWidget *>("settingsNavPane"); navPane != nullptr) {
        navPane->setFixedWidth(std::clamp(width() * 3 / 10, 200, 280));
    }
    QDialog::resizeEvent(event);
}

void SettingsWindow::On_cbLanguages_currentIndexChanged(int index)
{
    if (_ui.cbLanguages->count() != index + 1) {
        _lastLanguageIndex = index;

        const auto &availableLocales = Utils::AvailableLocales();
        const auto &locale = availableLocales.at(index);

        ModifiableAccess()->language_locale = locale.name();
    }
    else {
        _ui.cbLanguages->setCurrentIndex(_lastLanguageIndex);
        // clang-format off
        QDesktopServices::openUrl(QUrl{
            "https://github.com/SpriteOvO/AirPodsDesktop/blob/main/CONTRIBUTING.md#-translation-guide"
        });
        // clang-format on
    }
}

void SettingsWindow::On_cbAutoRun_toggled(bool checked)
{
    ModifiableAccess()->auto_run = checked;
}

void SettingsWindow::On_cbTrayQuickConnectEnabled_toggled(bool checked)
{
    ModifiableAccess()->tray_quick_connect_enabled = checked;
}

void SettingsWindow::On_cbTrayQuickConnectDevice_currentIndexChanged(int index)
{
    const auto id = _ui.cbTrayQuickConnectDevice->itemData(index).toString();
    ModifiableAccess()->tray_quick_connect_device_id = id;
}

void SettingsWindow::On_pbUnbind_clicked()
{
    ModifiableAccess()->device_address = 0;
}

void SettingsWindow::On_cbDisplayBatteryOnTrayIcon_toggled(TrayIconBatteryBehavior behavior)
{
    ModifiableAccess()->tray_icon_battery = behavior;
}

void SettingsWindow::On_cbAppearanceMode_currentIndexChanged(int index)
{
    if (index < static_cast<int>(AppearanceMode::System) ||
        index > static_cast<int>(AppearanceMode::Dark))
    {
        return;
    }
    ModifiableAccess()->appearance_mode = static_cast<AppearanceMode>(index);
}

void SettingsWindow::On_cbDisplayBatteryOnTaskbar_toggled(TaskbarStatusBehavior behavior)
{
    ModifiableAccess()->battery_on_taskbar = behavior;
}

void SettingsWindow::On_cbLowAudioLatency_toggled(bool checked)
{
    ModifiableAccess()->low_audio_latency = checked;
}

void SettingsWindow::On_cbAutoEarDetection_toggled(bool checked)
{
    ModifiableAccess()->automatic_ear_detection = checked;
}

void SettingsWindow::On_hsMaxReceivingRange_valueChanged(int value)
{
    ModifiableAccess()->rssi_min = -value;
}

void SettingsWindow::On_pbOpenLogsDirectory_clicked()
{
    Utils::File::OpenFileLocation(Logger::GetLogFilePath());
}

void SettingsWindow::On_cbAdvOverride_toggled(bool checked)
{
    UpdateAdvOverride();
}

void SettingsWindow::On_teAdvOverride_textChanged()
{
    UpdateAdvOverride();
}

} // namespace Gui
