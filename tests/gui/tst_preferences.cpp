// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/language.h"
#include "app/mainwindow.h"
#include "app/settings.h"
#include "app/theme.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QApplication>
#include <QDialog>
#include <QDockWidget>
#include <QFile>
#include <QGraphicsView>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QSettings>
#include <QStatusBar>
#include <QStyle>
#include <QTest>
#include <QTimer>

#include <array>
#include <functional>

Q_DECLARE_METATYPE(Language)

namespace {
void clickMenuBarItem(QMainWindow &window, QMenu *menu)
{
    QMenuBar *bar = window.menuBar();
    QTest::mouseClick(bar, Qt::LeftButton, Qt::NoModifier,
                      bar->actionGeometry(menu->menuAction()).center());
    QTRY_VERIFY(menu->isVisible());
}

void clickMenuItem(QMenu *menu, QAction *action)
{
    const QPoint center = menu->actionGeometry(action).center();
    QTest::mouseMove(menu, center);
    QTest::mouseClick(menu, Qt::LeftButton, Qt::NoModifier, center);
    // LanguageChange events are posted, not delivered synchronously.
    QCoreApplication::processEvents();
}

// Runs callback on the visible language dialog once the event loop starts. The dialog is
// rejected afterwards if the callback left it open, so a failed check cannot hang the test.
void onLanguageDialog(const std::function<void(QDialog &)> &callback, bool &found)
{
    QTimer::singleShot(0, [&found, callback] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            auto *dialog = qobject_cast<QDialog *>(widget);
            if (dialog && dialog->objectName() == QLatin1String("languageDialog")
                && dialog->isVisible()) {
                found = true;
                callback(*dialog);
                if (dialog->isVisible())
                    dialog->reject();
                return;
            }
        }
        if (QWidget *modal = QApplication::activeModalWidget())
            modal->close();
    });
}
} // namespace

class TestPreferences : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void defaultLanguageFollowsSystemLocale_data();
    void defaultLanguageFollowsSystemLocale();
    void languageCodesAndNames();
    void installsTranslationForEachLanguage();
    void settingsStartEmpty();
    void settingsRoundTripInIniFile();
    void unknownSavedValuesAreIgnored();
    void appliesLightAndDarkPalettes();
    void switchesLanguageAndThemeAtRuntimeAndPersists();
    void firstLaunchAsksForLanguageAndSavesChoice();
    void dismissingFirstLaunchDialogKeepsPreselectedLanguage_data();
    void dismissingFirstLaunchDialogKeepsPreselectedLanguage();
    void firstLaunchDialogNotShownOnceLanguageSaved();

private:
    QString m_iniPath;
};

void TestPreferences::initTestCase()
{
    m_iniPath = useTemporarySettings();
    setupApplication(*qApp);
}

void TestPreferences::init()
{
    clearSettings();
}

void TestPreferences::defaultLanguageFollowsSystemLocale_data()
{
    QTest::addColumn<QLocale>("locale");
    QTest::addColumn<Language>("expected");
    QTest::newRow("ru_RU") << QLocale(QStringLiteral("ru_RU")) << Language::Russian;
    QTest::newRow("ru_KZ") << QLocale(QStringLiteral("ru_KZ")) << Language::Russian;
    QTest::newRow("kk_KZ") << QLocale(QStringLiteral("kk_KZ")) << Language::Kazakh;
    QTest::newRow("en_US") << QLocale(QStringLiteral("en_US")) << Language::Russian;
    QTest::newRow("de_DE") << QLocale(QStringLiteral("de_DE")) << Language::Russian;
    QTest::newRow("C") << QLocale::c() << Language::Russian;
}

void TestPreferences::defaultLanguageFollowsSystemLocale()
{
    QFETCH(QLocale, locale);
    QFETCH(Language, expected);
    QCOMPARE(defaultLanguage(locale), expected);
}

void TestPreferences::languageCodesAndNames()
{
    QCOMPARE(kAllLanguages, (std::array{ Language::Russian, Language::Kazakh, Language::English }));
    QCOMPARE(languageCode(Language::Russian), QStringLiteral("ru"));
    QCOMPARE(languageCode(Language::Kazakh), QStringLiteral("kk"));
    QCOMPARE(languageCode(Language::English), QStringLiteral("en"));
    QCOMPARE(nativeLanguageName(Language::Russian), QStringLiteral("Русский"));
    QCOMPARE(nativeLanguageName(Language::Kazakh), QStringLiteral("Қазақша"));
    QCOMPARE(nativeLanguageName(Language::English), QStringLiteral("English"));
    for (Language language : kAllLanguages)
        QCOMPARE(languageFromCode(languageCode(language)), language);
    QVERIFY(!languageFromCode(u"de"));
    QVERIFY(!languageFromCode(u"RU"));
    QVERIFY(!languageFromCode(u""));
}

void TestPreferences::installsTranslationForEachLanguage()
{
    installLanguage(Language::Kazakh);
    QCOMPARE(currentLanguage(), Language::Kazakh);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"),
             QStringLiteral("Компоненттер"));
    installLanguage(Language::Russian);
    QCOMPARE(currentLanguage(), Language::Russian);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"), QStringLiteral("Компоненты"));
    installLanguage(Language::English);
    QCOMPARE(currentLanguage(), Language::English);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"), QStringLiteral("Components"));
}

void TestPreferences::settingsStartEmpty()
{
    QVERIFY(!Settings::savedLanguage());
    QCOMPARE(Settings::savedTheme(), Theme::Mode::Dark);
}

void TestPreferences::settingsRoundTripInIniFile()
{
    Settings::saveLanguage(Language::Kazakh);
    Settings::saveTheme(Theme::Mode::Light);
    QCOMPARE(Settings::savedLanguage(), Language::Kazakh);
    QCOMPARE(Settings::savedTheme(), Theme::Mode::Light);
    QVERIFY(QFile::exists(m_iniPath));
    const QSettings raw(m_iniPath, QSettings::IniFormat);
    QCOMPARE(raw.value("language").toString(), QStringLiteral("kk"));
    QCOMPARE(raw.value("theme").toString(), QStringLiteral("light"));
}

void TestPreferences::unknownSavedValuesAreIgnored()
{
    {
        QSettings raw(m_iniPath, QSettings::IniFormat);
        raw.setValue("language", "de");
        raw.setValue("theme", "blue");
    }
    QVERIFY(!Settings::savedLanguage());
    QCOMPARE(Settings::savedTheme(), Theme::Mode::Dark);
}

void TestPreferences::appliesLightAndDarkPalettes()
{
    const QPalette light = Theme::lightPalette();
    QVERIFY(light.color(QPalette::Window).lightness() > 180);
    QVERIFY(light.color(QPalette::WindowText).lightness() < 80);
    QVERIFY(light.color(QPalette::Base).lightness() > 200);
    QVERIFY(light.color(QPalette::Text).lightness() < 80);

    Theme::apply(Theme::Mode::Light);
    QCOMPARE(Theme::currentMode(), Theme::Mode::Light);
    QCOMPARE(QApplication::palette().color(QPalette::Window), light.color(QPalette::Window));
    QCOMPARE(QApplication::style()->name(), QStringLiteral("fusion"));
    Theme::apply(Theme::Mode::Dark);
    QCOMPARE(Theme::currentMode(), Theme::Mode::Dark);
    QCOMPARE(QApplication::palette().color(QPalette::Window),
             Theme::darkPalette().color(QPalette::Window));
}

void TestPreferences::switchesLanguageAndThemeAtRuntimeAndPersists()
{
    Settings::saveLanguage(Language::Russian);
    applySavedPreferences();
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        auto *settingsMenu = window.findChild<QMenu *>("settingsMenu");
        auto *themeMenu = window.findChild<QMenu *>("themeMenu");
        auto *languageMenu = window.findChild<QMenu *>("languageMenu");
        auto *dark = window.findChild<QAction *>("themeAction_dark");
        auto *light = window.findChild<QAction *>("themeAction_light");
        auto *ru = window.findChild<QAction *>("languageAction_ru");
        auto *kk = window.findChild<QAction *>("languageAction_kk");
        auto *en = window.findChild<QAction *>("languageAction_en");
        auto *dock = window.findChild<QDockWidget *>("paletteDock");
        auto *canvas = window.findChild<QGraphicsView *>("canvasView");
        QVERIFY(settingsMenu && themeMenu && languageMenu && dark && light && ru && kk && en && dock
                && canvas);
        QCOMPARE(languageMenu->title(), QStringLiteral("Язык"));
        QCOMPARE(settingsMenu->title(), QStringLiteral("Настройки"));
        QCOMPARE(dock->windowTitle(), QStringLiteral("Компоненты"));
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Готово"));
        QVERIFY(ru->isChecked() && dark->isChecked());
        QVERIFY(saveScreenshot(window, "f03", "ru"));

        clickMenuBarItem(window, languageMenu);
        QVERIFY(saveScreenshot(*languageMenu, "f03", "language-menu"));
        clickMenuItem(languageMenu, kk);
        QCOMPARE(languageMenu->title(), QStringLiteral("Тіл"));
        QCOMPARE(settingsMenu->title(), QStringLiteral("Баптаулар"));
        QCOMPARE(themeMenu->title(), QStringLiteral("Тақырып"));
        QCOMPARE(dark->text(), QStringLiteral("Қараңғы"));
        QCOMPARE(light->text(), QStringLiteral("Ашық"));
        QCOMPARE(dock->windowTitle(), QStringLiteral("Компоненттер"));
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Дайын"));
        QCOMPARE(ru->text(), QStringLiteral("Русский")); // native names never change
        QCOMPARE(en->text(), QStringLiteral("English"));
        QCOMPARE(Settings::savedLanguage(), Language::Kazakh);
        QVERIFY(saveScreenshot(window, "f03", "kk"));

        clickMenuBarItem(window, languageMenu);
        clickMenuItem(languageMenu, en);
        QCOMPARE(languageMenu->title(), QStringLiteral("Language"));
        QCOMPARE(dock->windowTitle(), QStringLiteral("Components"));
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Ready"));
        QVERIFY(saveScreenshot(window, "f03", "en"));

        clickMenuBarItem(window, languageMenu);
        clickMenuItem(languageMenu, kk);
        clickMenuBarItem(window, settingsMenu);
        QTest::mouseMove(settingsMenu,
                         settingsMenu->actionGeometry(themeMenu->menuAction()).center());
        QTest::mouseClick(settingsMenu, Qt::LeftButton, Qt::NoModifier,
                          settingsMenu->actionGeometry(themeMenu->menuAction()).center());
        QTRY_VERIFY(themeMenu->isVisible());
        clickMenuItem(themeMenu, light);
        const QColor lightWindow = Theme::lightPalette().color(QPalette::Window);
        QCOMPARE(window.palette().color(QPalette::Window), lightWindow);
        QCOMPARE(dock->palette().color(QPalette::Window), lightWindow);
        QVERIFY(canvas->palette().color(QPalette::Base).lightness() > 200);
        QCOMPARE(Settings::savedTheme(), Theme::Mode::Light);
        QCOMPARE(window.statusBar()->currentMessage(), QStringLiteral("Дайын"));
        QVERIFY(saveScreenshot(window, "f03", "light"));
    }
    const QSettings raw(m_iniPath, QSettings::IniFormat);
    QCOMPARE(raw.value("language").toString(), QStringLiteral("kk"));
    QCOMPARE(raw.value("theme").toString(), QStringLiteral("light"));

    // Restart: reset the app state, then start up from the saved settings.
    installLanguage(Language::English);
    Theme::apply(Theme::Mode::Dark);
    applySavedPreferences();
    MainWindow restarted;
    restarted.show();
    QVERIFY(QTest::qWaitForWindowExposed(&restarted));
    QCOMPARE(restarted.findChild<QMenu *>("languageMenu")->title(), QStringLiteral("Тіл"));
    QCOMPARE(restarted.findChild<QDockWidget *>("paletteDock")->windowTitle(),
             QStringLiteral("Компоненттер"));
    QVERIFY(restarted.findChild<QAction *>("languageAction_kk")->isChecked());
    QVERIFY(restarted.findChild<QAction *>("themeAction_light")->isChecked());
    QCOMPARE(restarted.palette().color(QPalette::Window),
             Theme::lightPalette().color(QPalette::Window));
    QVERIFY(saveScreenshot(restarted, "f03", "restarted"));
}

void TestPreferences::firstLaunchAsksForLanguageAndSavesChoice()
{
    const Language preselected = defaultLanguage(QLocale::system());
    bool found = false;
    bool headingOk = false;
    bool titleOk = false;
    bool buttonsOk = true;
    bool defaultOk = false;
    bool screenshotOk = false;
    onLanguageDialog(
            [&](QDialog &dialog) {
                auto *heading = dialog.findChild<QLabel *>(QStringLiteral("languageHeading"));
                headingOk = heading
                        && heading->text() == u"Выберите язык · Тілді таңдаңыз · Language";
                titleOk = dialog.windowTitle() == u"Electricity";
                for (const Language language : kAllLanguages) {
                    auto *button = dialog.findChild<QPushButton *>(QStringLiteral("languageButton_")
                                                                   + languageCode(language));
                    if (!button || button->text() != nativeLanguageName(language)
                        || button->width() < 140 || button->height() < 48) {
                        buttonsOk = false;
                        continue;
                    }
                    if (language == preselected)
                        defaultOk = button->isDefault();
                }
                screenshotOk = saveScreenshot(dialog, "f03", "first-launch");
                if (auto *kk = dialog.findChild<QPushButton *>(QStringLiteral("languageButton_kk")))
                    QTest::mouseClick(kk, Qt::LeftButton);
            },
            found);
    askLanguageOnFirstLaunch();
    QVERIFY(found);
    QVERIFY(headingOk);
    QVERIFY(titleOk);
    QVERIFY(buttonsOk);
    QVERIFY(defaultOk);
    QVERIFY(screenshotOk);
    QCOMPARE(Settings::savedLanguage(), Language::Kazakh);
    QCOMPARE(currentLanguage(), Language::Kazakh);
}

void TestPreferences::dismissingFirstLaunchDialogKeepsPreselectedLanguage_data()
{
    QTest::addColumn<bool>("useEscape");
    QTest::newRow("Esc") << true;
    QTest::newRow("close button") << false;
}

void TestPreferences::dismissingFirstLaunchDialogKeepsPreselectedLanguage()
{
    QFETCH(bool, useEscape);
    bool found = false;
    onLanguageDialog(
            [useEscape](QDialog &dialog) {
                if (useEscape)
                    QTest::keyClick(&dialog, Qt::Key_Escape);
                else
                    dialog.close();
            },
            found);
    askLanguageOnFirstLaunch();
    QVERIFY(found);
    QCOMPARE(Settings::savedLanguage(), defaultLanguage(QLocale::system()));
    QCOMPARE(currentLanguage(), defaultLanguage(QLocale::system()));
}

void TestPreferences::firstLaunchDialogNotShownOnceLanguageSaved()
{
    Settings::saveLanguage(Language::English);
    bool dialogShown = false;
    QTimer guard; // fires only if askLanguageOnFirstLaunch() enters an event loop
    guard.setSingleShot(true);
    connect(&guard, &QTimer::timeout, [&] {
        if (QWidget *modal = QApplication::activeModalWidget()) {
            dialogShown = true;
            modal->close();
        }
    });
    guard.start(0);
    askLanguageOnFirstLaunch();
    guard.stop();
    QVERIFY(!dialogShown);
}

QTEST_MAIN(TestPreferences)
#include "tst_preferences.moc"
