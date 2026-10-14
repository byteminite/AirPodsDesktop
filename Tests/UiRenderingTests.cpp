#include <algorithm>
#include <atomic>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDesktopServices>
#include <QDir>
#include <QScreen>
#include <QFontDatabase>
#include <QLabel>
#include <QListWidget>
#include <QRawFont>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QTest>
#include <QTranslator>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QStyleOptionComboBox>
#include <QThread>
#include <QUrl>
#include <QWidgetAction>
#include <QTimer>
#include <QPainter>

#include "Source/Core/QuickConnect.h"
#include "Source/Gui/FrostedBackdrop.h"
#include "Source/Gui/MainWindow.h"
#include "Source/Gui/QuickConnectBanner.h"
#include "Source/Gui/SettingsWindow.h"
#include "Source/Gui/Theme.h"
#include "Source/Gui/TrayActivation.h"
#include "Source/Gui/TrayIcon.h"
#include "Source/Gui/UpdateWindow.h"
#include "Source/Gui/Widget/BatteryRow.h"
#include "Source/Gui/Widget/DeviceCard.h"
#include "Source/Gui/Widget/ToggleSwitch.h"
#include "Source/Gui/Widget/TrayBatteryHeader.h"

class UpdateUrlRecorder : public QObject
{
    Q_OBJECT
public:
    QList<QUrl> urls;
    UpdateUrlRecorder()
    {
        QDesktopServices::setUrlHandler("https", this, "Open");
    }
    ~UpdateUrlRecorder() override
    {
        QDesktopServices::unsetUrlHandler("https");
    }
public Q_SLOTS:
    void Open(const QUrl &url)
    {
        urls.push_back(url);
    }
};

namespace {
void VerifySmoothPopupCorners(QWidget *popup, const QString &fileName)
{
    const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation/");
    QVERIFY(QDir{}.mkpath(outputDir));
    const auto image = popup->grab().toImage();
    QVERIFY(image.save(outputDir + fileName));
    QVERIFY(image.hasAlphaChannel());
    const int cornerSize = qRound(10 * image.devicePixelRatio());
    // Every corner must have a transparent exterior and intermediate alpha along the curve.
    // An opaque native background or a binary region mask fails these pixel checks.
    for (const bool right : {false, true}) {
        for (const bool bottom : {false, true}) {
            const auto pixelAt = [&](int x, int y) {
                return image.pixel(
                    right ? image.width() - 1 - x : x, bottom ? image.height() - 1 - y : y);
            };
            QCOMPARE(qAlpha(pixelAt(0, 0)), 0);
            QCOMPARE(qAlpha(pixelAt(cornerSize, cornerSize)), 255);
            int blendedPixels = 0;
            for (int y = 0; y < cornerSize; ++y) {
                for (int x = 0; x < cornerSize; ++x) {
                    const int alpha = qAlpha(pixelAt(x, y));
                    blendedPixels += alpha > 0 && alpha < 255;
                }
            }
            QVERIFY(blendedPixels > 0);
        }
    }
}

Core::Update::ReleaseInfo UpdateFixture()
{
    Core::Update::ReleaseInfo info;
    info.version = QVersionNumber{2, 12, 345};
    info.url = "https://example.invalid/release";
    info.isPreRelease = true;
    info.fileName = "simulated.exe";
    info.downloadUrl = "https://example.invalid/simulated.exe";
    info.fileSize = 4000000000u;
    info.sha256 = QString(64, QChar{'0'});
    info.changeLog =
        "Improved device connections.\nFixed battery status updates.\n<b>Plain text, not HTML</b>";
    return info;
}

struct SimulatedDownload {
    std::atomic<int> stage{0};
    std::atomic<int> calls{0};

    bool Run(const Core::Update::ReleaseInfo &, const Core::Update::FnProgress &progress)
    {
        ++calls;
        for (;;) {
            const auto current = stage.load();
            if (current >= 4) {
                return current == 4;
            }
            const size_t total = current == 0 ? 0 : 4000000000u;
            const size_t downloaded = current == 0 ? 0 : current == 1 ? 3000000000u : total;
            if (!progress(downloaded, total)) {
                return false;
            }
            QThread::msleep(10);
        }
    }

    Gui::UpdateWindow::DownloadFunction Function()
    {
        return [this](const auto &info, const auto &progress) { return Run(info, progress); };
    }
};
} // namespace

class UiRenderingTests : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void FrostBackdropBlursAndKeepsSize()
    {
        QVERIFY(Gui::FrostBackdrop(QImage{}).isNull());

        // Hard-edged 4px red/blue stripes: the worst case for a blur to smooth out.
        QImage stripes{320, 316, QImage::Format_RGB32};
        for (int y = 0; y < stripes.height(); ++y) {
            for (int x = 0; x < stripes.width(); ++x) {
                stripes.setPixel(x, y, (x / 4) % 2 == 0 ? qRgb(255, 0, 0) : qRgb(0, 0, 255));
            }
        }

        const auto frosted = Gui::FrostBackdrop(stripes);
        QCOMPARE(frosted.size(), stripes.size());
        QVERIFY(!frosted.hasAlphaChannel());

        int largestStep = 0;
        const int row = frosted.height() / 2;
        for (int x = 1; x < frosted.width(); ++x) {
            const QColor left = frosted.pixelColor(x - 1, row);
            const QColor right = frosted.pixelColor(x, row);
            largestStep = std::max(
                {largestStep, std::abs(left.red() - right.red()),
                 std::abs(left.blue() - right.blue())});
        }
        // The source jumps by 255 every 4px; once frosted, neighbours differ only slightly.
        QVERIFY2(largestStep < 16, qPrintable(QString::number(largestStep)));
    }

    void MainWindowRendersAirPods5()
    {
        Core::AirPods::State state;
        state.model = Core::AirPods::Model::AirPods_5;
        state.displayName = QStringLiteral("AirPods 5");
        state.pods.left.battery = 80;
        state.pods.right.battery = 70;
        state.caseBox.battery = 50;

        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QVERIFY(QDir{}.mkpath(outputDir));
        const auto scale = qEnvironmentVariable("QT_SCALE_FACTOR", "system");

        for (const auto mode : {Gui::Theme::Mode::Light, Gui::Theme::Mode::Dark}) {
            theme.SetMode(mode);
            Gui::MainWindow window;
            window.UpdateState(state);
            window.Show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));

            auto *label = window.findChild<QLabel *>("deviceLabel");
            auto *animation = window.findChild<Gui::Widget::AnimationView *>();
            QVERIFY(label != nullptr);
            QVERIFY(animation != nullptr);
            QCOMPARE(label->text(), state.displayName);
            QTRY_VERIFY(animation->isVisible());
            QCOMPARE(animation->width(), animation->height() * 2);

            const auto animationImage = animation->grab().toImage();
            int visiblePixels = 0;
            for (int y = 0; y < animationImage.height(); y += 2) {
                for (int x = 0; x < animationImage.width(); x += 2) {
                    visiblePixels += qAlpha(animationImage.pixel(x, y)) != 0;
                }
            }
            QVERIFY(visiblePixels > 100);

            auto *leftRow = window.findChild<Gui::Widget::BatteryRow *>("leftBatteryRow");
            auto *rightRow = window.findChild<Gui::Widget::BatteryRow *>("rightBatteryRow");
            auto *caseRow = window.findChild<Gui::Widget::BatteryRow *>("caseBatteryRow");
            QVERIFY(leftRow != nullptr);
            QVERIFY(rightRow != nullptr);
            QVERIFY(caseRow != nullptr);
            QCOMPARE(leftRow->Label(), QStringLiteral("Left"));
            QCOMPARE(rightRow->Label(), QStringLiteral("Right"));
            QCOMPARE(caseRow->Label(), QStringLiteral("Case"));
            QCOMPARE(leftRow->Value(), std::optional<quint32>{80});
            QCOMPARE(rightRow->Value(), std::optional<quint32>{70});
            QCOMPARE(caseRow->Value(), std::optional<quint32>{50});
            QVERIFY(!leftRow->IsCharging());
            QVERIFY(!rightRow->IsCharging());
            QVERIFY(!caseRow->IsCharging());

            const auto suffix =
                QString{"airpods-5-%1-scale%2.png"}.arg(theme.IsDark() ? "dark" : "light", scale);
            QVERIFY(window.grab().save(outputDir + '/' + suffix));
            window.close();
        }
        theme.SetMode(originalMode);
    }

    void MainWindowBatteryRowsRenderEveryState()
    {
        Core::AirPods::State state;
        state.model = Core::AirPods::Model::AirPods_Pro_2;
        state.displayName = QStringLiteral("Battery States Test");
        state.pods.left.battery = 15;        // low (<=20%), not charging
        state.pods.left.isCharging = false;
        state.pods.right.battery = 60;       // normal, charging
        state.pods.right.isCharging = true;
        // state.caseBox.battery left default-constructed: unavailable

        Gui::MainWindow window;
        window.UpdateState(state);
        window.Show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *leftRow = window.findChild<Gui::Widget::BatteryRow *>("leftBatteryRow");
        auto *rightRow = window.findChild<Gui::Widget::BatteryRow *>("rightBatteryRow");
        auto *caseRow = window.findChild<Gui::Widget::BatteryRow *>("caseBatteryRow");
        QVERIFY(leftRow != nullptr);
        QVERIFY(rightRow != nullptr);
        QVERIFY(caseRow != nullptr);

        QCOMPARE(leftRow->Value(), std::optional<quint32>{15});
        QVERIFY(!leftRow->IsCharging());
        QCOMPARE(rightRow->Value(), std::optional<quint32>{60});
        QVERIFY(rightRow->IsCharging());

        // An unavailable side stays in the row list, dimmed, rather than disappearing and
        // shifting the other two - the whole point of switching away from hide()/show().
        QVERIFY(!caseRow->Value().has_value());
        QVERIFY(caseRow->isVisible());
        QVERIFY(window.findChild<QWidget *>("batteryRowsContainer")->isVisible());

        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QVERIFY(QDir{}.mkpath(outputDir));
        QVERIFY(window.grab().save(outputDir + "/main-window-battery-states.png"));
        window.close();

        // Unbind: the rows hide and the "choose your AirPods" hint + button take their place.
        window.Unbind();
        auto *bindButton = window.findChild<QPushButton *>("pushButton");
        auto *bindHint = window.findChild<QLabel *>("bindHintLabel");
        QVERIFY(bindButton != nullptr);
        QVERIFY(bindHint != nullptr);
        QVERIFY(bindButton->isVisible());
        QVERIFY(bindHint->isVisible());
        QVERIFY(!window.findChild<QWidget *>("batteryRowsContainer")->isVisible());
    }

    void MainWindowElidesLongDeviceNames()
    {
        Gui::MainWindow window;
        auto *label = window.findChild<QLabel *>("deviceLabel");
        QVERIFY(label != nullptr);
        QCOMPARE(label->textFormat(), Qt::PlainText);
        QVERIFY(!label->wordWrap());

        Core::AirPods::State state;
        state.model = Core::AirPods::Model::AirPods_4;
        state.displayName = QStringLiteral("AirPods 4");
        window.UpdateState(state);
        QCOMPARE(label->text(), state.displayName);
        QCOMPARE(label->font().pointSize(), 18);
        QCOMPARE(label->toolTip(), QString{});
        QCOMPARE(label->accessibleName(), state.displayName);

        state.displayName =
            QStringLiteral("結城さくな專用的超級無敵特別限定版 AirPods Pro 第二世代");
        window.UpdateState(state);
        QCOMPARE(label->font().pointSize(), 12);
        QVERIFY(label->text() != state.displayName);
        QVERIFY(label->text().contains(QChar{0x2026}));
        QVERIFY(!label->text().startsWith(QChar{0x2026}));
        QVERIFY(!label->text().endsWith(QChar{0x2026}));
        QVERIFY(
            label->fontMetrics().horizontalAdvance(label->text()) <= label->contentsRect().width());
        QCOMPARE(label->toolTip(), state.displayName);
        QCOMPARE(label->accessibleName(), state.displayName);

        const auto scale = qEnvironmentVariable("QT_SCALE_FACTOR", "system");
        VerifySmoothPopupCorners(
            &window, QString{"main-window-long-device-name-scale%1.png"}.arg(scale));
    }

    void UpdateTextHasAntialiasedEdges_data()
    {
        QTest::addColumn<QString>("localeName");
        QTest::newRow("English") << "en_US";
        QTest::newRow("Traditional Chinese") << "zh_TW";
    }

    void UpdateTextHasAntialiasedEdges()
    {
        QFETCH(QString, localeName);
        Gui::Theme::ApplyApplicationTypography(QLocale{localeName});
        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        Gui::UpdateWindow window{UpdateFixture()};
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (const auto mode : {Gui::Theme::Mode::Light, Gui::Theme::Mode::Dark}) {
            theme.SetMode(mode);
            for (const auto *name : {"promptTitle", "rowSubtitle", "updateButton", "releaseNotes"})
            {
                const auto *widget = window.findChild<QWidget *>(name);
                QVERIFY(widget);
                const auto ratio = window.devicePixelRatioF();
                QPixmap textImage{QSize{400, 60} * ratio};
                textImage.setDevicePixelRatio(ratio);
                const bool dark = mode == Gui::Theme::Mode::Dark;
                textImage.fill(dark ? Qt::black : Qt::white);
                {
                    QPainter painter{&textImage};
                    painter.setFont(widget->font());
                    painter.setPen(dark ? Qt::white : Qt::black);
                    painter.drawText(QPoint{8, 30}, QStringLiteral("AirPodsDesktop 更新版本 2.12"));
                }
                const auto pixels = textImage.toImage();
                int intermediatePixels = 0;
                for (int y = 0; y < pixels.height(); ++y) {
                    for (int x = 0; x < pixels.width(); ++x) {
                        const auto pixel = pixels.pixel(x, y);
                        if (qRed(pixel) > 0 && qRed(pixel) < 255) {
                            ++intermediatePixels;
                        }
                    }
                }
                // Check smooth raster edges while allowing the system's ClearType rendering.
                QVERIFY2(intermediatePixels > 0, name);
            }
        }
        Gui::Theme::ApplyApplicationTypography(QLocale{"en_US"});
        theme.SetMode(originalMode);
    }

    void UpdatePromptActions()
    {
        UpdateUrlRecorder recorder;
        for (const auto *name : {"skipButton", "laterButton", "escape", "close"}) {
            Gui::UpdateWindow window{UpdateFixture()};
            window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&window));
            auto *notes = window.findChild<QPlainTextEdit *>("releaseNotes");
            QVERIFY(notes->isReadOnly());
            QCOMPARE(notes->toPlainText(), UpdateFixture().changeLog);
            QTest::mouseClick(window.findChild<QPushButton *>("viewButton"), Qt::LeftButton);
            QVERIFY(window.isVisible());
            QCOMPARE(recorder.urls.last(), QUrl{UpdateFixture().url});
            if (QString{name} == "escape") {
                QTest::keyClick(&window, Qt::Key_Escape);
            }
            else if (QString{name} == "close") {
                window.close();
            }
            else {
                QTest::mouseClick(window.findChild<QPushButton *>(name), Qt::LeftButton);
            }
            const auto expected =
                QString{name} == "updateButton" ? Gui::UpdateWindow::Action::Update
                : QString{name} == "skipButton" ? Gui::UpdateWindow::Action::Skip
                                                : Gui::UpdateWindow::Action::Later;
            QCOMPARE(window.SelectedAction(), expected);
            QVERIFY(!window.isVisible());
        }
    }

    void UnsupportedUpdateOpensReleaseWithoutDownloading()
    {
        UpdateUrlRecorder recorder;
        auto info = UpdateFixture();
        info.sha256.clear();
        SimulatedDownload download;
        Gui::UpdateWindow window{info, nullptr, download.Function()};
        window.show();
        window.findChild<QPushButton *>("updateButton")->click();
        QCOMPARE(download.calls.load(), 0);
        QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::ManualDownload);
        QCOMPARE(window.SelectedAction(), Gui::UpdateWindow::Action::Update);
        QCOMPARE(recorder.urls, QList<QUrl>{QUrl{info.url}});
        QVERIFY(!window.isVisible());
    }

    void DownloadProgressAndCompletion()
    {
        SimulatedDownload download;
        Gui::UpdateWindow window{UpdateFixture(), nullptr, download.Function()};
        QCOMPARE(download.calls.load(), 0);
        QSignalSpy finished{&window, &QDialog::finished};
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        const auto nativeWindow = window.winId();
        auto *notes = window.findChild<QPlainTextEdit *>("releaseNotes");
        const auto notesPosition = notes->mapTo(&window, QPoint{});
        const auto windowPosition = window.pos();
        QTest::mouseClick(window.findChild<QPushButton *>("updateButton"), Qt::LeftButton);
        window.StartDownload();
        QCOMPARE(window.winId(), nativeWindow);
        QCOMPARE(window.SelectedAction(), Gui::UpdateWindow::Action::Update);
        QCOMPARE(notes->mapTo(&window, QPoint{}), notesPosition);
        QCOMPARE(window.pos(), windowPosition);
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *bar = window.findChild<QProgressBar *>("progressBar");
        QTRY_COMPARE(download.calls.load(), 1);
        QCOMPARE(bar->maximum(), 0);
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!window.close());
        QVERIFY(window.isVisible());
        download.stage = 1;
        QTRY_COMPARE(bar->value(), 750);
        QVERIFY(window.findChild<QLabel *>("progressDetails")->text().contains("75%"));
        window.UpdateProgressSafely(quint64{1} << 32, quint64{1} << 33);
        QCOMPARE(bar->value(), 500);
        download.stage = 0;
        QTRY_COMPARE(bar->maximum(), 0);
        const auto downloadingHeight = window.height();
        download.stage = 2;
        QTRY_COMPARE(bar->value(), 1000);
        QVERIFY(!window.findChild<QPushButton *>("pushButtonDownloadManually")->isEnabled());
        QVERIFY(window.findChild<QLabel *>("hint")->isHidden());
        QVERIFY(window.height() < downloadingHeight);
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!window.close());
        QCOMPARE(bar->accessibleName(), QString{"Preparing to install..."});
        QCOMPARE(finished.count(), 0);
        QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::KeepRunning);
        download.stage = 4;
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::InstallerStarted);
        QVERIFY(!window.isVisible());
    }

    void DownloadFailureAndManualActions()
    {
        UpdateUrlRecorder recorder;
        for (const auto *action : {"closeButton", "escape", "close"}) {
            Gui::UpdateWindow window{
                UpdateFixture(), nullptr, [](const auto &, const auto &) { return false; }};
            QSignalSpy finished{&window, &QDialog::finished};
            window.StartDownload();
            window.show();
            auto *close = window.findChild<QPushButton *>("closeButton");
            QTRY_VERIFY(close->isVisible());
            QVERIFY(window.windowFlags().testFlag(Qt::WindowCloseButtonHint));
            window.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&window));
            QCOMPARE(QApplication::focusWidget(), close);
            const auto focusedSize = close->size();
            window.findChild<QPushButton *>("viewButton")->setFocus();
            QCOMPARE(close->size(), focusedSize);
            close->setFocus();
            QCOMPARE(finished.count(), 0);
            QVERIFY(!window.findChild<QProgressBar *>("progressBar")->isVisible());
            const auto failureText = window.findChild<QLabel *>("failureDescription")->text();
            window.UpdateProgressSafely(100, 100);
            QCOMPARE(window.findChild<QLabel *>("failureDescription")->text(), failureText);
            const auto urlCount = recorder.urls.size();
            QTest::mouseClick(
                window.findChild<QPushButton *>("failedManualButton"), Qt::LeftButton);
            QCOMPARE(recorder.urls.size(), urlCount + 1);
            QVERIFY(window.isVisible());
            QCOMPARE(finished.count(), 0);
            QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::KeepRunning);
            if (QString{action} == "escape") {
                QTest::keyClick(&window, Qt::Key_Escape);
            }
            else if (QString{action} == "close") {
                window.close();
            }
            else {
                QTest::mouseClick(close, Qt::LeftButton);
            }
            QCOMPARE(finished.count(), 1);
            QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::KeepRunning);
            QCOMPARE(window.SelectedAction(), Gui::UpdateWindow::Action::Update);
        }
        SimulatedDownload download;
        Gui::UpdateWindow active{UpdateFixture(), nullptr, download.Function()};
        active.StartDownload();
        active.show();
        QVERIFY(QTest::qWaitForWindowExposed(&active));
        QTest::mouseClick(
            active.findChild<QPushButton *>("pushButtonDownloadManually"), Qt::LeftButton);
        QCOMPARE(active.Result(), Gui::UpdateWindow::Outcome::ManualDownload);
        QVERIFY(!active.isVisible());
    }

    void FailedDownloadStaysInModalLoop()
    {
        Gui::UpdateWindow window{
            UpdateFixture(), nullptr, [](const auto &, const auto &) { return false; }};
        bool failureWasVisible = false;
        bool userClosed = false;
        QTimer closeWhenFailed;
        closeWhenFailed.setInterval(10);
        connect(&closeWhenFailed, &QTimer::timeout, &window, [&] {
            auto *button = window.findChild<QPushButton *>("closeButton");
            if (button->isVisible()) {
                failureWasVisible = window.isVisible();
                userClosed = true;
                button->click();
            }
        });
        QTimer::singleShot(3000, &window, &QDialog::accept);
        closeWhenFailed.start();
        window.StartDownload();
        window.exec();
        QVERIFY(failureWasVisible);
        QVERIFY(userClosed);
        QCOMPARE(window.Result(), Gui::UpdateWindow::Outcome::KeepRunning);
    }

    void UpdateWindowsFollowLanguageAndThemeChanges()
    {
        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        Gui::UpdateWindow prompt{UpdateFixture()};
        Gui::UpdateWindow failure{
            UpdateFixture(), nullptr, [](const auto &, const auto &) { return false; }};
        failure.StartDownload();
        failure.show();
        QTRY_VERIFY(failure.findChild<QPushButton *>("closeButton")->isVisible());
        const auto englishTitle = failure.findChild<QLabel *>("title")->text();
        QTranslator translator;
        QVERIFY(translator.load(
            QLocale{"zh_TW"}, "apd", "_",
            QCoreApplication::applicationDirPath() + "/translations"));
        QVERIFY(QCoreApplication::installTranslator(&translator));
        QCoreApplication::processEvents();
        QCOMPARE(
            failure.findChild<QLabel *>("title")->text(),
            translator.translate("Gui::UpdateWindow", "Unable to update"));
        QCOMPARE(
            prompt.findChild<QPushButton *>("updateButton")->text(),
            translator.translate("UpdateWindow", "Update now"));
        QCOMPARE(
            prompt.findChild<QPlainTextEdit *>("releaseNotes")->toPlainText(),
            UpdateFixture().changeLog);
        theme.SetMode(Gui::Theme::Mode::Dark);
        QCOMPARE(failure.palette().color(QPalette::Window), theme.Colors().windowBackground);
        theme.SetMode(Gui::Theme::Mode::Light);
        QCOMPARE(failure.palette().color(QPalette::Window), theme.Colors().windowBackground);
        QCoreApplication::removeTranslator(&translator);
        QCoreApplication::processEvents();
        QCOMPARE(failure.findChild<QLabel *>("title")->text(), englishTitle);
        QVERIFY(failure.isVisible());
        failure.close();
        theme.SetMode(originalMode);
    }

    void UpdateWindowsRendering_data()
    {
        QTest::addColumn<bool>("dark");
        QTest::addColumn<QString>("localeName");
        for (const auto *locale : {"en_US", "zh_TW"}) {
            QTest::newRow(qPrintable(QString{locale} + "-light")) << false << locale;
            QTest::newRow(qPrintable(QString{locale} + "-dark")) << true << locale;
        }
    }

    void UpdateWindowsRendering()
    {
        QFETCH(bool, dark);
        QFETCH(QString, localeName);
        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        QTranslator translator;
        if (localeName == "zh_TW") {
            QVERIFY(translator.load(
                QLocale{localeName}, "apd", "_",
                QCoreApplication::applicationDirPath() + "/translations"));
            QVERIFY(QCoreApplication::installTranslator(&translator));
        }
        Gui::Theme::ApplyApplicationTypography(QLocale{localeName});
        theme.SetMode(dark ? Gui::Theme::Mode::Dark : Gui::Theme::Mode::Light);
        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QDir{}.mkpath(outputDir);
        const auto suffix = QString{"-%1-%2-scale%3.png"}.arg(
            localeName, dark ? "dark" : "light", qEnvironmentVariable("QT_SCALE_FACTOR", "1"));
        const auto verifyLayout = [](QWidget &window) {
            if (window.width() != 560) {
                return false;
            }
            for (auto *button : window.findChildren<QPushButton *>()) {
                if (button->isVisible() &&
                    (!window.rect().contains(
                         QRect{button->mapTo(&window, QPoint{}), button->size()}) ||
                     button->height() != 28 ||
                     button->width() < button->fontMetrics().horizontalAdvance(button->text()) +
                                           (button->isFlat() ? 14 : 32)))
                {
                    qWarning() << button->objectName() << button->size() << button->text()
                               << button->fontMetrics().horizontalAdvance(button->text());
                    return false;
                }
            }
            auto *status = window.findChild<QLabel *>("status");
            auto *details = window.findChild<QLabel *>("progressDetails");
            if (status->isVisible() &&
                (status->fontMetrics().horizontalAdvance(status->text()) > status->width() ||
                 details->fontMetrics().horizontalAdvance(details->text()) > details->width() ||
                 status->geometry().intersects(details->geometry())))
            {
                qWarning() << status->geometry() << details->geometry();
                return false;
            }
            return true;
        };
        auto stableInfo = UpdateFixture();
        stableInfo.isPreRelease = false;
        Gui::UpdateWindow stable{stableInfo};
        stable.show();
        QVERIFY(QTest::qWaitForWindowExposed(&stable));
        QVERIFY(stable.grab().save(outputDir + "/update-stable" + suffix));
        QVERIFY(verifyLayout(stable));
        stable.close();
        Gui::UpdateWindow prompt{UpdateFixture()};
        prompt.show();
        QVERIFY(QTest::qWaitForWindowExposed(&prompt));
        QVERIFY(verifyLayout(prompt));
        QVERIFY(prompt.grab().save(outputDir + "/update-prompt" + suffix));
        auto *notes = prompt.findChild<QPlainTextEdit *>("releaseNotes");
        prompt.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&prompt));
        notes->setFocus();
        QTest::keyClick(notes, Qt::Key_Tab);
        QCOMPARE(QApplication::focusWidget(), prompt.findChild<QPushButton *>("viewButton"));
        prompt.close();
        auto longInfo = UpdateFixture();
        longInfo.version = QVersionNumber{20260904, 12345678, 2147483647};
        longInfo.changeLog = QString{"A long line without spaces: "} + QString(600, QChar{'X'}) +
                             QString{"\nRelease note\n"}.repeated(100);
        Gui::UpdateWindow longPrompt{longInfo};
        longPrompt.show();
        QVERIFY(QTest::qWaitForWindowExposed(&longPrompt));
        QVERIFY(verifyLayout(longPrompt));
        QTRY_VERIFY(
            longPrompt.findChild<QPlainTextEdit *>("releaseNotes")->verticalScrollBar()->maximum() >
            0);
        QVERIFY(longPrompt.grab().save(outputDir + "/update-long" + suffix));
        longPrompt.close();
        auto emptyInfo = UpdateFixture();
        emptyInfo.changeLog.clear();
        emptyInfo.isPreRelease = false;
        Gui::UpdateWindow emptyPrompt{emptyInfo};
        emptyPrompt.show();
        QVERIFY(QTest::qWaitForWindowExposed(&emptyPrompt));
        QVERIFY(
            emptyPrompt.findChild<QPlainTextEdit *>("releaseNotes")->isHidden() ||
            !emptyPrompt.findChild<QPlainTextEdit *>("releaseNotes")->isVisible());
        QVERIFY(emptyPrompt.findChild<QWidget *>("noChangeLogRow")->isVisible());
        QVERIFY(verifyLayout(emptyPrompt));
        QVERIFY(emptyPrompt.grab().save(outputDir + "/update-empty" + suffix));
        emptyPrompt.close();
        SimulatedDownload download;
        download.stage = 0;
        Gui::UpdateWindow progress{UpdateFixture(), nullptr, download.Function()};
        progress.show();
        QVERIFY(QTest::qWaitForWindowExposed(&progress));
        const auto notesPosition =
            progress.findChild<QPlainTextEdit *>("releaseNotes")->mapTo(&progress, QPoint{});
        const auto nativeWindow = progress.winId();
        progress.findChild<QPushButton *>("updateButton")->click();
        QCOMPARE(progress.winId(), nativeWindow);
        QCOMPARE(
            progress.findChild<QPlainTextEdit *>("releaseNotes")->mapTo(&progress, QPoint{}),
            notesPosition);
        QTRY_COMPARE(progress.findChild<QProgressBar *>("progressBar")->maximum(), 0);
        QVERIFY(verifyLayout(progress));
        QVERIFY(progress.grab().save(outputDir + "/update-waiting" + suffix));
        download.stage = 1;
        QTRY_COMPARE(progress.findChild<QProgressBar *>("progressBar")->value(), 750);
        QVERIFY(verifyLayout(progress));
        QVERIFY(progress.grab().save(outputDir + "/update-download" + suffix));
        download.stage = 2;
        QTRY_COMPARE(progress.findChild<QProgressBar *>("progressBar")->value(), 1000);
        QVERIFY(verifyLayout(progress));
        QVERIFY(progress.grab().save(outputDir + "/update-preparing" + suffix));
        download.stage = 5;
        QTRY_VERIFY(progress.findChild<QPushButton *>("closeButton")->isVisible());
        QVERIFY(verifyLayout(progress));
        QVERIFY(progress.grab().save(outputDir + "/update-failure" + suffix));
        progress.close();
        theme.SetMode(originalMode);
        QCoreApplication::removeTranslator(&translator);
        Gui::Theme::ApplyApplicationTypography(QLocale{"en_US"});
    }

    void BundledTypographyLoadsInterFamilies()
    {
        QVERIFY(Gui::Theme::LoadBundledFonts());

        const auto families = QFontDatabase{}.families();
        QVERIFY(families.contains("Inter"));
        QVERIFY(families.contains("Inter Display"));
        QVERIFY(families.contains("Noto Sans TC"));

        QFont traditionalChineseFont{"Noto Sans TC"};
        const auto traditionalChineseSample = QString::fromUtf8(
            "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87\xE8\xA8\xAD\xE5\xAE\x9A");
        const auto glyphs = QRawFont::fromFont(traditionalChineseFont)
                                .glyphIndexesForString(traditionalChineseSample);
        QVERIFY(!glyphs.isEmpty());
        QVERIFY(
            std::all_of(glyphs.cbegin(), glyphs.cend(), [](quint32 glyph) { return glyph != 0; }));

        const auto applicationFont = Gui::Theme::ApplicationFont(QLocale{"zh_TW"});
        QCOMPARE(applicationFont.families().first(), QStringLiteral("Inter"));
        QCOMPARE(applicationFont.families().at(1), QStringLiteral("Noto Sans TC"));
        QCOMPARE(
            Gui::Theme::DisplayFontFamilies(QLocale{"zh_TW"}).first(),
            QStringLiteral("Inter Display"));

        struct LocaleFontCase {
            const char *locale;
            const char *bodyFallback;
            const char *displayFallback;
        };
        constexpr LocaleFontCase cases[]{
            {"zh_CN", "Microsoft YaHei UI", "Microsoft YaHei UI"},
            {"zh_TW", "Noto Sans TC", "Noto Sans TC"},
            {"ja_JP", "Yu Gothic UI", "Yu Gothic UI"},
            {"ko_KR", "Malgun Gothic", "Malgun Gothic"},
            {"de_DE", "Segoe UI Variable Text", "Inter"},
        };
        for (const auto &fontCase : cases) {
            QCOMPARE(
                Gui::Theme::ApplicationFont(QLocale{fontCase.locale}).families().at(1),
                QString::fromLatin1(fontCase.bodyFallback));
            QCOMPARE(
                Gui::Theme::DisplayFontFamilies(QLocale{fontCase.locale}).at(1),
                QString::fromLatin1(fontCase.displayFallback));
        }
    }

    void SettingsWindowSupportsKeyboardAndCompactLayout()
    {
        auto backend = std::make_shared<Core::QuickConnect::NullBackend>();
        Core::QuickConnect::Controller quickConnect{backend};

        Gui::Theme::ApplyApplicationTypography(QLocale{"zh_TW"});
        Gui::SettingsWindow window{[] { return 0; }, quickConnect};
        window.resize(640, 420);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *languages = window.findChild<QComboBox *>("cbLanguages");
        QVERIFY(languages != nullptr);
        QCOMPARE(languages->itemText(0), QStringLiteral("English"));

        auto *navigation = window.findChild<QListWidget *>("navList");
        QVERIFY(navigation != nullptr);
        QCOMPARE(navigation->frameShape(), QFrame::NoFrame);
        QCOMPARE(navigation->focusPolicy(), Qt::StrongFocus);
        QVERIFY(!qApp->styleSheet().contains(QStringLiteral("border-left")));
        navigation->setFocus();
        QCOMPARE(QApplication::focusWidget(), navigation);
        QCOMPARE(navigation->currentRow(), 0);
        QTest::keyClick(navigation, Qt::Key_Down);
        QCOMPARE(navigation->currentRow(), 1);

        auto *appearanceMode = window.findChild<QComboBox *>("cbAppearanceMode");
        QVERIFY(appearanceMode != nullptr);
        QCOMPARE(appearanceMode->count(), 3);
        QCOMPARE(appearanceMode->sizeAdjustPolicy(), QComboBox::AdjustToContents);
        QVERIFY(!appearanceMode->accessibleName().isEmpty());

        const auto appearanceTextFits = [appearanceMode] {
            QStyleOptionComboBox option;
            option.initFrom(appearanceMode);
            option.currentText = appearanceMode->currentText();
            const auto textRect = appearanceMode->style()->subControlRect(
                QStyle::CC_ComboBox, &option, QStyle::SC_ComboBoxEditField, appearanceMode);
            const QFontMetrics metrics{appearanceMode->font()};
            for (int i = 0; i < appearanceMode->count(); ++i) {
                if (metrics.horizontalAdvance(appearanceMode->itemText(i)) > textRect.width()) {
                    return false;
                }
            }
            return true;
        };
        QVERIFY(appearanceTextFits());

        for (const auto *name : {
                 "cbLowAudioLatency",
                 "cbAutoEarDetection",
                 "cbTrayQuickConnectEnabled",
             })
        {
            auto *checkBox = window.findChild<QCheckBox *>(name);
            QVERIFY(checkBox != nullptr);
            QVERIFY(!checkBox->accessibleName().isEmpty());
            QVERIFY(checkBox->minimumWidth() >= 32);
            QVERIFY(checkBox->minimumHeight() >= 32);
        }

        auto *scrollArea = window.findChild<QScrollArea *>("pagesScrollArea");
        QVERIFY(scrollArea != nullptr);
        QVERIFY(scrollArea->widgetResizable());
        QVERIFY(!scrollArea->horizontalScrollBar()->isVisible());

        // At the compact size every control on every page still fits beside its card text.
        for (int page = 0; page < 4; ++page) {
            navigation->setCurrentRow(page);
            QCoreApplication::processEvents();
            for (auto *control : window.findChildren<QWidget *>()) {
                if (!control->isVisible() || (qobject_cast<QComboBox *>(control) == nullptr &&
                                              qobject_cast<QAbstractButton *>(control) == nullptr &&
                                              qobject_cast<QSlider *>(control) == nullptr))
                {
                    continue;
                }
                if (!scrollArea->viewport()->isAncestorOf(control)) {
                    continue;
                }
                const auto rightEdge =
                    control->mapTo(scrollArea->viewport(), control->rect().topRight()).x();
                QVERIFY2(
                    rightEdge < scrollArea->viewport()->width(),
                    qPrintable(QString{"%1 ends at %2, viewport %3 (content %4)"}
                                   .arg(control->objectName())
                                   .arg(rightEdge)
                                   .arg(scrollArea->viewport()->width())
                                   .arg(scrollArea->widget()->width())));
            }
        }
        navigation->setCurrentRow(1);

        auto *restoreDefaults = window.findChild<QPushButton *>("pbRestoreDefaults");
        QVERIFY(restoreDefaults != nullptr);
        QVERIFY(window.findChild<QDialogButtonBox *>() == nullptr);

        QTranslator traditionalChinese;
        const auto translationsDir = QCoreApplication::applicationDirPath() + "/translations";
        QVERIFY(traditionalChinese.load(QLocale{"zh_TW"}, "apd", "_", translationsDir));
        QVERIFY(QCoreApplication::installTranslator(&traditionalChinese));
        QCoreApplication::processEvents();
        // Pages: AirPods (brand name, untranslated), Appearance, General, About.
        QCOMPARE(navigation->item(0)->text(), QStringLiteral("AirPods"));
        QCOMPARE(navigation->item(1)->text(), QString::fromUtf8("\xE5\xA4\x96\xE8\xA7\x80"));
        QCOMPARE(navigation->item(2)->text(), QString::fromUtf8("\xE4\xB8\x80\xE8\x88\xAC"));
        QCOMPARE(window.findChild<QLabel *>("pageTitle")->text(), navigation->item(1)->text());
        QTRY_COMPARE(
            restoreDefaults->text(),
            QString::fromUtf8("\xE9\x82\x84\xE5\x8E\x9F\xE9\xA0\x90\xE8\xA8\xAD\xE5\x80\xBC"));
        QVERIFY(appearanceTextFits());

        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QDir{}.mkpath(outputDir);
        QVERIFY(window.grab().save(outputDir + "/settings-current-theme.png"));

        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();

        theme.SetMode(Gui::Theme::Mode::Dark);
        QVERIFY(theme.IsDark());
        QCOMPARE(theme.Colors().windowBackground, QColor{"#1C1C1E"});
        QCOMPARE(theme.Colors().surface, QColor{"#2C2C2E"});
        QCoreApplication::processEvents();
        QVERIFY(window.grab().save(outputDir + "/settings-dark.png"));
        appearanceMode->showPopup();
        QTRY_VERIFY(QApplication::activePopupWidget() != nullptr);
        auto *comboPopup = QApplication::activePopupWidget();
        QCOMPARE(comboPopup->objectName(), QStringLiteral("apdComboPopup"));
        QVERIFY(appearanceMode->view()->viewport()->mapTo(comboPopup, QPoint{}).x() >= 4);
        QVERIFY(!appearanceMode->view()->verticalScrollBar()->isVisible());
        appearanceMode->view()->setCurrentIndex(appearanceMode->model()->index(0, 0));
        QVERIFY(appearanceMode->view()->selectionModel()->isSelected(
            appearanceMode->model()->index(0, 0)));
        QTest::qWait(100);
        VerifySmoothPopupCorners(comboPopup, "combo-popup-dark.png");
        appearanceMode->hidePopup();

        theme.SetMode(Gui::Theme::Mode::Light);
        QVERIFY(!theme.IsDark());
        QCOMPARE(theme.Colors().windowBackground, QColor{"#F5F5F7"});
        QCOMPARE(theme.Colors().surface, QColor{"#FFFFFF"});
        QCoreApplication::processEvents();
        QVERIFY(window.grab().save(outputDir + "/settings-light.png"));
        appearanceMode->showPopup();
        QTRY_VERIFY(QApplication::activePopupWidget() != nullptr);
        comboPopup = QApplication::activePopupWidget();
        QCOMPARE(comboPopup->objectName(), QStringLiteral("apdComboPopup"));
        QVERIFY(appearanceMode->view()->viewport()->mapTo(comboPopup, QPoint{}).x() >= 4);
        QVERIFY(!appearanceMode->view()->verticalScrollBar()->isVisible());
        appearanceMode->view()->setCurrentIndex(appearanceMode->model()->index(2, 0));
        QVERIFY(appearanceMode->view()->selectionModel()->isSelected(
            appearanceMode->model()->index(2, 0)));
        QTest::qWait(100);
        VerifySmoothPopupCorners(comboPopup, "combo-popup-light.png");
        appearanceMode->hidePopup();

        theme.SetMode(originalMode);
        QCoreApplication::removeTranslator(&traditionalChinese);
        Gui::Theme::ApplyApplicationTypography(QLocale{"en_US"});
    }

    void TrayMenuShowsBatteryHeader()
    {
        auto backend = std::make_shared<Core::QuickConnect::NullBackend>();
        Core::QuickConnect::Controller quickConnect{backend};
        Gui::TrayIcon tray{[] { return 0; }, quickConnect};

        auto *header = tray.findChild<Gui::Widget::TrayBatteryHeader *>("trayBatteryHeader");
        QVERIFY(header != nullptr);
        auto *status = header->findChild<QLabel *>("trayBatteryHeaderStatus");
        QVERIFY(status != nullptr);

        tray.Disconnect();
        QVERIFY(header->IsShowingStatus());
        QVERIFY(!status->text().isEmpty());

        Core::AirPods::State state;
        state.model = Core::AirPods::Model::AirPods_Pro_2;
        state.displayName = QStringLiteral("Miha's AirPods Pro");
        state.pods.left.battery = 15;
        state.pods.right.battery = 60;
        state.pods.right.isCharging = true;
        tray.UpdateState(state);

        QVERIFY(!header->IsShowingStatus());
        QCOMPARE(
            header->findChild<QLabel *>("trayBatteryHeaderName")->text(), state.displayName);
        auto *left = header->findChild<Gui::Widget::BatteryRow *>("trayLeftBatteryRow");
        auto *right = header->findChild<Gui::Widget::BatteryRow *>("trayRightBatteryRow");
        auto *caseRow = header->findChild<Gui::Widget::BatteryRow *>("trayCaseBatteryRow");
        QVERIFY(left != nullptr && right != nullptr && caseRow != nullptr);
        QCOMPARE(left->Value(), std::optional<quint32>{15});
        QVERIFY(!left->IsCharging());
        QCOMPARE(right->Value(), std::optional<quint32>{60});
        QVERIFY(right->IsCharging());
        QVERIFY(!caseRow->Value().has_value());

        // Every action keeps an icon, and the toggle stays checkable.
        auto *menu = tray.findChild<QMenu *>();
        for (auto *action : menu->actions()) {
            if (action->isSeparator() || qobject_cast<QWidgetAction *>(action) != nullptr) {
                continue;
            }
            QVERIFY2(!action->icon().isNull(), qPrintable(action->text()));
        }

        tray.ShowContextMenu(QPoint{100, 100});
        QVERIFY(QTest::qWaitForWindowExposed(menu));
        QVERIFY(menu->grab().save(QStringLiteral(APD_BINARY_DIR "/UiValidation/tray-menu-battery.png")));
        QTest::keyClick(menu, Qt::Key_Escape);
        QTRY_VERIFY(!menu->isVisible());

        // Opened at the taskbar edge, as from the tray, the menu keeps a gap above it.
        const auto available = QGuiApplication::primaryScreen()->availableGeometry();
        tray.ShowContextMenu(available.bottomRight());
        QVERIFY(QTest::qWaitForWindowExposed(menu));
        constexpr int kVisibleGap = 12;
        const auto surfaceBottom = menu->geometry().bottom() - 3; // painted surface inset
        const auto surfaceRight = menu->geometry().right() - 3;
        QVERIFY2(
            available.bottom() - surfaceBottom >= kVisibleGap,
            qPrintable(QString{"gap %1"}.arg(available.bottom() - surfaceBottom)));
        QVERIFY(available.right() - surfaceRight >= kVisibleGap);
        QTest::keyClick(menu, Qt::Key_Escape);
        QTRY_VERIFY(!menu->isVisible());
    }

    void TrayMenuHasSmoothCorners()
    {
        auto backend = std::make_shared<Core::QuickConnect::NullBackend>();
        Core::QuickConnect::Controller quickConnect{backend};
        Gui::TrayIcon tray{[] { return 0; }, quickConnect};
        auto *menu = tray.findChild<QMenu *>();
        QVERIFY(menu != nullptr);
        const auto actions = menu->actions();
        const auto checkableAction =
            std::find_if(actions.cbegin(), actions.cend(), [](const QAction *action) {
                return action->isCheckable();
            });
        QVERIFY(checkableAction != actions.cend());
        (*checkableAction)->setChecked(true);
        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        QDir{}.mkpath(QStringLiteral(APD_BINARY_DIR "/UiValidation"));
        for (const auto mode : {Gui::Theme::Mode::Light, Gui::Theme::Mode::Dark}) {
            theme.SetMode(mode);
            tray.ShowContextMenu(QPoint{100, 100});
            QVERIFY(QTest::qWaitForWindowExposed(menu));
            for (const bool last : {false, true}) {
                if (last) {
                    menu->setActiveAction(menu->actions().last());
                }
                else {
                    QTest::keyClick(menu, Qt::Key_Down);
                }
                QVERIFY(menu->activeAction() != nullptr);
                VerifySmoothPopupCorners(
                    menu, QString{"tray-menu-%1-%2.png"}
                              .arg(theme.IsDark() ? "dark" : "light")
                              .arg(last ? "last" : "first"));
            }
            QTest::keyClick(menu, Qt::Key_Escape);
            QTRY_VERIFY(!menu->isVisible());
        }
        theme.SetMode(originalMode);
    }

    void SettingsDeviceCardFollowsAirPodsState()
    {
        auto backend = std::make_shared<Core::QuickConnect::NullBackend>();
        Core::QuickConnect::Controller quickConnect{backend};
        Gui::SettingsWindow window{[] { return 0; }, quickConnect};
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        auto *card = window.findChild<Gui::Widget::DeviceCard *>("deviceCard");
        QVERIFY(card != nullptr);
        auto *name = card->findChild<QLabel *>("deviceCardName");
        auto *status = card->findChild<QLabel *>("deviceCardStatus");
        auto *button = card->findChild<QPushButton *>("pbUnbind");
        auto *left = card->findChild<Gui::Widget::BatteryRow *>("deviceLeftBatteryRow");
        auto *right = card->findChild<Gui::Widget::BatteryRow *>("deviceRightBatteryRow");
        auto *caseRow = card->findChild<Gui::Widget::BatteryRow *>("deviceCaseBatteryRow");
        QVERIFY(name && status && button && left && right && caseRow);

        Core::AirPods::State state;
        state.model = Core::AirPods::Model::AirPods_Pro_2;
        state.displayName = QStringLiteral("Miha's AirPods Pro");
        state.pods.left.battery = 80;
        state.pods.right.battery = 15;
        state.caseBox.battery = 50;
        state.caseBox.isCharging = true;

        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QDir{}.mkpath(outputDir);

        // Connected, case charging
        window.SetDevice({Gui::Status::Updating, state});
        QCOMPARE(name->text(), state.displayName);
        QCOMPARE(left->Value(), std::optional<quint32>{80});
        QCOMPARE(right->Value(), std::optional<quint32>{15});
        QCOMPARE(caseRow->Value(), std::optional<quint32>{50});
        QVERIFY(caseRow->IsCharging());
        QVERIFY(!left->IsCharging());
        QVERIFY(left->isVisible());
        QVERIFY(button->text().contains("Unbind"));
        QVERIFY(window.grab().save(outputDir + "/settings-airpods-connected.png"));

        // Disconnected: no stale battery
        window.SetDevice({Gui::Status::Disconnected, std::nullopt});
        QCOMPARE(status->text(), Gui::DisplayableStatus(Gui::Status::Disconnected));
        QVERIFY(!left->Value().has_value());
        QVERIFY(!caseRow->IsCharging());

        // Unbound: the button binds instead and asks for the popup
        QSignalSpy bindSpy{&window, &Gui::SettingsWindow::BindRequested};
        window.SetDevice({Gui::Status::Unbind, std::nullopt});
        QVERIFY(!left->isVisible());
        QVERIFY(button->text().contains("Bind to AirPods"));
        QTest::mouseClick(button, Qt::LeftButton);
        QCOMPARE(bindSpy.count(), 1);
        QVERIFY(window.grab().save(outputDir + "/settings-airpods-unbound.png"));

        // Every page renders; save them for review.
        auto *navigation = window.findChild<QListWidget *>("navList");
        for (const auto &[row, file] : {
                 std::pair{1, "settings-appearance.png"},
                 std::pair{2, "settings-general.png"},
                 std::pair{3, "settings-about.png"},
             })
        {
            navigation->setCurrentRow(row);
            QCoreApplication::processEvents();
            QVERIFY(window.grab().save(outputDir + "/" + file));
        }
    }

    void ToggleSwitchBehavesLikeACheckBox()
    {
        Gui::Widget::ToggleSwitch toggle;
        toggle.setAccessibleName(QStringLiteral("Test"));
        toggle.show();
        QVERIFY(QTest::qWaitForWindowExposed(&toggle));
        QVERIFY(toggle.minimumSizeHint().height() >= 32);

        QSignalSpy spy{&toggle, &QCheckBox::toggled};
        QTest::mouseClick(&toggle, Qt::LeftButton, {}, toggle.rect().center());
        QVERIFY(toggle.isChecked());
        // The whole control is the hit target, including its "On"/"Off" text.
        QTest::mouseClick(&toggle, Qt::LeftButton, {}, QPoint{2, toggle.height() / 2});
        QVERIFY(!toggle.isChecked());
        toggle.setFocus();
        QTest::keyClick(&toggle, Qt::Key_Space);
        QVERIFY(toggle.isChecked());
        QCOMPARE(spy.count(), 3);
    }

    void QuickConnectBannerShowsOnlyResultsAndProblems()
    {
        using Core::QuickConnect::Outcome;
        using Kind = Gui::QuickConnectBannerContent::Kind;

        Core::AirPods::State airPods;
        airPods.model = Core::AirPods::Model::AirPods_Pro_2;
        airPods.displayName = QStringLiteral("Miha's AirPods Pro");
        airPods.pods.left.battery = 80;

        const auto name = airPods.displayName;
        for (const auto silent : {Outcome::Disabled, Outcome::RequestStarted}) {
            QVERIFY(!Gui::QuickConnectBannerContentFor(silent, name, airPods).has_value());
        }
        // Double-clicking an already connected device shows its battery, like a fresh connection.
        QCOMPARE(
            Gui::QuickConnectBannerContentFor(Outcome::AlreadyConnected, name, airPods),
            Gui::QuickConnectBannerContentFor(Outcome::Connected, name, airPods));

        // Connected AirPods carry their state; another device, or no state yet, does not.
        auto connected = Gui::QuickConnectBannerContentFor(Outcome::Connected, name, airPods);
        QVERIFY(connected.has_value());
        QCOMPARE(connected->kind, Kind::AirPods);
        QCOMPARE(connected->title, name);
        QCOMPARE(connected->airPods, std::optional{airPods});
        QVERIFY(!connected->opensSettings);

        auto other = Gui::QuickConnectBannerContentFor(
            Outcome::Connected, QStringLiteral("WH-1000XM5"), airPods);
        QCOMPARE(other->kind, Kind::Device);
        QVERIFY(!other->airPods.has_value());
        QCOMPARE(
            Gui::QuickConnectBannerContentFor(Outcome::Connected, name, std::nullopt)->kind,
            Kind::Device);

        for (const auto problem :
             {Outcome::NoDevice, Outcome::DeviceUnavailable, Outcome::TimedOut, Outcome::Failed})
        {
            const auto content = Gui::QuickConnectBannerContentFor(problem, name, airPods);
            QVERIFY(content.has_value());
            QCOMPARE(content->kind, Kind::Problem);
            QVERIFY(!content->text.isEmpty());
            // Only "choose a device" leads to Settings.
            QCOMPARE(content->opensSettings, problem == Outcome::NoDevice);
        }
        QVERIFY(Gui::QuickConnectBannerContentFor(Outcome::TimedOut, name, std::nullopt)
                    ->text.contains(name));
    }

    void QuickConnectBannerRendersEveryKind()
    {
        using Core::QuickConnect::Outcome;

        Core::AirPods::State airPods;
        airPods.model = Core::AirPods::Model::AirPods_Pro_2;
        airPods.displayName = QStringLiteral("Miha's AirPods Pro");
        airPods.pods.left.battery = 80;
        airPods.pods.right.battery = 15;
        airPods.caseBox.battery = 50;
        airPods.caseBox.isCharging = true;

        Gui::QuickConnectBanner banner;
        QCOMPARE(banner.focusPolicy(), Qt::NoFocus);
        QVERIFY(banner.windowFlags().testFlag(Qt::WindowDoesNotAcceptFocus));

        auto *title = banner.findChild<QLabel *>("quickConnectBannerTitle");
        auto *left = banner.findChild<Gui::Widget::BatteryRow *>("bannerLeftBatteryRow");
        auto *caseRow = banner.findChild<Gui::Widget::BatteryRow *>("bannerCaseBatteryRow");
        QVERIFY(title && left && caseRow);

        auto &theme = Gui::Theme::Manager::Instance();
        const auto originalMode = theme.CurrentMode();
        const auto outputDir = QStringLiteral(APD_BINARY_DIR "/UiValidation");
        QDir{}.mkpath(outputDir);

        // The battery arrives after the connection; the AirPods banner fills in live.
        auto withoutBattery = airPods;
        withoutBattery.pods = {};
        withoutBattery.caseBox = {};
        banner.Show(*Gui::QuickConnectBannerContentFor(Outcome::Connected, airPods.displayName, withoutBattery));
        QVERIFY(QTest::qWaitForWindowExposed(&banner));
        QVERIFY(QApplication::focusWidget() == nullptr || !banner.isAncestorOf(QApplication::focusWidget()));
        QVERIFY(!left->Value().has_value());
        const auto heightBefore = banner.height();
        banner.UpdateAirPods(airPods);
        QCOMPARE(left->Value(), std::optional<quint32>{80});
        QVERIFY(caseRow->IsCharging());
        QCOMPARE(banner.height(), heightBefore);

        const struct {
            const char *name;
            std::optional<Gui::QuickConnectBannerContent> content;
        } kinds[] = {
            {"airpods", Gui::QuickConnectBannerContentFor(Outcome::Connected, airPods.displayName, airPods)},
            {"device", Gui::QuickConnectBannerContentFor(Outcome::Connected, QStringLiteral("WH-1000XM5"), airPods)},
            {"problem", Gui::QuickConnectBannerContentFor(Outcome::TimedOut, airPods.displayName, airPods)},
        };
        for (const auto mode : {Gui::Theme::Mode::Light, Gui::Theme::Mode::Dark}) {
            theme.SetMode(mode);
            for (const auto &kind : kinds) {
                banner.Show(*kind.content);
                QVERIFY(QTest::qWaitForWindowExposed(&banner));
                QTest::qWait(320); // let the slide-in settle
                QCOMPARE(banner.width(), 340);
                // Single-line kinds stay compact even right after a taller AirPods banner.
                if (kind.content->kind != Gui::QuickConnectBannerContent::Kind::AirPods) {
                    QVERIFY2(banner.height() <= 80, qPrintable(QString::number(banner.height())));
                }
                QVERIFY(banner.grab().save(
                    QString{"%1/banner-%2-%3.png"}
                        .arg(outputDir)
                        .arg(kind.name)
                        .arg(theme.IsDark() ? "dark" : "light")));
            }
        }

        // A device name of any length stays on one line.
        auto longName = *kinds[1].content;
        longName.title = QString{"Extremely Long Bluetooth Headset Name "}.repeated(3);
        banner.Show(longName);
        QVERIFY(title->text().endsWith(QChar{0x2026}));
        QVERIFY(title->fontMetrics().horizontalAdvance(title->text()) <= title->width());

        // A click hides it and says where to go.
        QSignalSpy clicked{&banner, &Gui::QuickConnectBanner::Clicked};
        banner.Show(*Gui::QuickConnectBannerContentFor(Outcome::NoDevice, {}, std::nullopt));
        QVERIFY(QTest::qWaitForWindowExposed(&banner));
        QTest::mouseClick(&banner, Qt::LeftButton, {}, banner.rect().center());
        QCOMPARE(clicked.count(), 1);
        QCOMPARE(clicked.at(0).at(0).toBool(), true);
        QVERIFY(!banner.isVisible());

        theme.SetMode(originalMode);
    }

    // Tray clicks (moved from QuickConnectTests: TrayActivation.h belongs to apd_gui). These drive
    // the same TrayActivationState as TrayIcon, including the timer decision.
    void singleClickOpensTrayMenuAfterTheDoubleClickInterval()
    {
        for (const bool quickConnectEnabled : {true, false}) {
            Gui::TrayActivationState state;

            const auto result = state.OnActivation(QSystemTrayIcon::Trigger, quickConnectEnabled);
            QCOMPARE(result.action, Gui::TrayActivationAction::None);
            QCOMPARE(result.timer, Gui::SingleClickTimer::Start);
            QCOMPARE(state.OnSingleClickTimeout(), Gui::TrayActivationAction::ShowMenu);

            // Only the first timeout counts.
            QCOMPARE(state.OnSingleClickTimeout(), Gui::TrayActivationAction::None);
        }
    }

    void doubleClickConnectsAndCancelsPendingMenu()
    {
        Gui::TrayActivationState state;

        QCOMPARE(
            state.OnActivation(QSystemTrayIcon::Trigger, true).action, Gui::TrayActivationAction::None);

        const auto result = state.OnActivation(QSystemTrayIcon::DoubleClick, true);
        QCOMPARE(result.action, Gui::TrayActivationAction::QuickConnect);
        QCOMPARE(result.timer, Gui::SingleClickTimer::Stop);
        QCOMPARE(state.OnSingleClickTimeout(), Gui::TrayActivationAction::None);
    }

    void doubleClickOpensMainWindowWhenQuickConnectIsDisabled()
    {
        Gui::TrayActivationState state;

        state.OnActivation(QSystemTrayIcon::Trigger, false);
        const auto result = state.OnActivation(QSystemTrayIcon::DoubleClick, false);
        QCOMPARE(result.action, Gui::TrayActivationAction::ShowMainWindow);
        QCOMPARE(result.timer, Gui::SingleClickTimer::Stop);
        QCOMPARE(state.OnSingleClickTimeout(), Gui::TrayActivationAction::None);
    }

    // Right-clicking opens the menu itself; the pending single click must not open it again.
    void contextMenuCancelsPendingQuickConnect()
    {
        Gui::TrayActivationState state;

        QCOMPARE(
            state.OnActivation(QSystemTrayIcon::Trigger, true).timer, Gui::SingleClickTimer::Start);

        const auto result = state.OnActivation(QSystemTrayIcon::Context, true);
        QCOMPARE(result.action, Gui::TrayActivationAction::None);
        QCOMPARE(result.timer, Gui::SingleClickTimer::Stop);
        QCOMPARE(state.OnSingleClickTimeout(), Gui::TrayActivationAction::None);
    }

    void unknownActivationDoesNothing()
    {
        Gui::TrayActivationState state;

        const auto result = state.OnActivation(QSystemTrayIcon::Unknown, true);
        QCOMPARE(result.action, Gui::TrayActivationAction::None);
        QCOMPARE(result.timer, Gui::SingleClickTimer::Unchanged);
    }

    void SettingsDescriptionsFollowConsecutiveLanguageChanges()
    {
        auto backend = std::make_shared<Core::QuickConnect::NullBackend>();
        Core::QuickConnect::Controller quickConnect{backend};
        Gui::SettingsWindow window{[] { return 0; }, quickConnect};

        auto *description = window.findChild<QLabel *>("lbDescLowAudioLatency");
        QVERIFY(description != nullptr);

        constexpr auto sourceText =
            "Keeps the audio device awake while your AirPods are connected so short sounds start "
            "immediately. This may produce audible hiss and use more battery.";
        const auto translationsDir = QCoreApplication::applicationDirPath() + "/translations";

        QTranslator first;
        QVERIFY(first.load(QLocale{"zh_TW"}, "apd", "_", translationsDir));
        const auto firstDescription = first.translate("QObject", sourceText);
        QVERIFY(!firstDescription.isEmpty());
        Gui::Theme::ApplyApplicationTypography(QLocale{"zh_TW"});
        QVERIFY(QCoreApplication::installTranslator(&first));
        QCoreApplication::processEvents();
        QCOMPARE(description->text(), firstDescription);
        QCOMPARE(qApp->font().families().at(1), QStringLiteral("Noto Sans TC"));

        QTranslator second;
        QVERIFY(second.load(QLocale{"de_DE"}, "apd", "_", translationsDir));
        const auto secondDescription = second.translate("QObject", sourceText);
        QVERIFY(!secondDescription.isEmpty());
        QCoreApplication::removeTranslator(&first);
        Gui::Theme::ApplyApplicationTypography(QLocale{"de_DE"});
        QVERIFY(QCoreApplication::installTranslator(&second));
        QCoreApplication::processEvents();
        QCOMPARE(description->text(), secondDescription);
        QCOMPARE(qApp->font().families().at(1), QStringLiteral("Segoe UI Variable Text"));

        QCoreApplication::removeTranslator(&second);
        Gui::Theme::ApplyApplicationTypography(QLocale{"en_US"});
    }
};

int main(int argc, char **argv)
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app{argc, argv};
    // Production logging installs a Qt message handler during static initialization. Restore the
    // default handler so QTest assertion details remain visible in CTest and CI output.
    qInstallMessageHandler(nullptr);
    Gui::Theme::ApplyApplicationTypography(QLocale{"en_US"});
    UiRenderingTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "UiRenderingTests.moc"
