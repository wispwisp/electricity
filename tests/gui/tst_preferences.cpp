// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/language.h"
#include "app/settings.h"
#include "app/theme.h"
#include "testsettings.h"

#include <QApplication>
#include <QFile>
#include <QLocale>
#include <QSettings>
#include <QStyle>
#include <QTest>

#include <array>

Q_DECLARE_METATYPE(Language)

class TestPreferences : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void init();
    void defaultLanguageFollowsSystemLocale_data();
    void defaultLanguageFollowsSystemLocale();
    void languageCodesAndNames();
    void settingsStartEmpty();
    void settingsRoundTripInIniFile();
    void unknownSavedValuesAreIgnored();
    void appliesLightAndDarkPalettes();

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

QTEST_MAIN(TestPreferences)
#include "tst_preferences.moc"
