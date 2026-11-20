# F03 Languages and Settings Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The UI switches between Русский / Қазақша / English and between dark and light themes at runtime, both choices persist in an INI file, and the first launch asks for the language.

**Architecture:** App-layer only (no `panelcore` changes). `language.{h,cpp}` holds the language list, codes, the locale default and the one app-wide `QTranslator`; `settings.{h,cpp}` reads and writes the INI file; `theme.{h,cpp}` gains a light palette. `MainWindow` builds the Settings and Language menus and retranslates itself on `QEvent::LanguageChange`. `LanguageDialog` is the first-launch chooser. Translations are built by `qt_add_translations` and checked by a CMake script that runs `lupdate` before scanning for unfinished entries.

**Tech Stack:** C++20, Qt 6.12.0 static (Widgets, LinguistTools: `lupdate`/`lrelease`), CMake script mode, Qt Test offscreen.

**Spec:** `docs/superpowers/specs/2026-11-20-f03-languages-settings-design.md`

## Global Constraints

- Languages, in menu and dialog order: Russian `ru` "Русский", Kazakh `kk` "Қазақша", English `en` "English". Native names are never translated.
- Default language when none is saved: system locale language Russian → ru, Kazakh → kk, anything else → ru.
- Settings: `QSettings(QSettings::IniFormat, QSettings::UserScope, "electricity", "electricity")`, so the file is `<user config>/electricity/electricity.ini`. Keys: `language` = `ru|kk|en`, `theme` = `dark|light`. Any other value counts as not saved (language) or dark (theme).
- Window title stays `"Electricity"` (not translated).
- First-launch heading, exact and untranslated: `Выберите язык · Тілді таңдаңыз · Language`.
- Translations, context `MainWindow`:

  | Source | ru | kk |
  |---|---|---|
  | Components | Компоненты | Компоненттер |
  | Ready | Готово | Дайын |
  | Settings | Настройки | Баптаулар |
  | Theme | Тема | Тақырып |
  | Dark | Тёмная | Қараңғы |
  | Light | Светлая | Ашық |
  | Language | Язык | Тіл |

- Object names: `settingsMenu`, `themeMenu`, `themeAction_dark`, `themeAction_light`, `languageMenu`, `languageAction_ru|kk|en`, `languageDialog`, `languageHeading`, `languageButton_ru|kk|en`.
- Translation files: `translations/electricity_ru.ts`, `translations/electricity_kk.ts`, `translations/electricity_en.ts` (plural forms only), embedded under `:/i18n/`.
- GUI tests never touch real settings: call `useTemporarySettings()` in `initTestCase()` before anything reads settings.
- Screenshots go to `build/screenshots/f03-<step>.png`.
- Every new file has an SPDX `GPL-3.0-or-later` header; clang-format (Qt style, 100 columns) clean.
- Never `git push`; commit only as the steps say.
- To wait for a long command, run it with `run_in_background: true` and wait for its completion notice. Never poll with `pgrep -f`. Before reporting, stop every background task and Monitor you started.

## Review Focus

- **Hand-edited or stale settings** (`language=de`, `theme=blue`): treated as not saved, so the first-launch dialog asks again and the theme is dark. Task 1 `unknownSavedValuesAreIgnored`.
- **System locales other than plain ru/kk** (`ru_KZ`, `kk_KZ`, `en_US`, `de_DE`, C): the user gets Russian or Kazakh as the defaults rule says, not English. Task 1 data rows.
- **Switching back to English:** `electricity_en.qm` has no messages, so installing it sends no `LanguageChange`. Only removing the old translator retranslates the UI. A regression would leave Russian text after picking English. Task 3 scenario, ru → kk → en.
- **Dismissing the first-launch dialog** (Esc or the window's close button): the preselected language is saved and the dialog doesn't come back. Task 4 `dismissingFirstLaunchDialogKeepsPreselectedLanguage`.
- **Already-open widgets must recolor** on a theme switch (dock, canvas), not only the main window. Task 3 scenario assertions.

---

### Task 1: Language list, settings file, light theme

**Files:**
- Create: `src/app/language.h`, `src/app/language.cpp`, `src/app/settings.h`, `src/app/settings.cpp`
- Create: `tests/gui/testsettings.h`, `tests/gui/testsettings.cpp`, `tests/gui/tst_preferences.cpp`
- Modify: `src/app/theme.h`, `src/app/theme.cpp`, `src/app/appsetup.cpp` (call `Theme::apply(Theme::Mode::Dark)`), `src/app/CMakeLists.txt` (new sources), `tests/CMakeLists.txt` (add `gui/testsettings.cpp` to every GUI test in `add_gui_test`; `add_gui_test(tst_preferences gui/tst_preferences.cpp)`), `docs/roadmap.md` (F03 ☐ → ◐)

**Interfaces:**
- Produces, `language.h`:
  - `enum class Language { Russian, Kazakh, English };`
  - `inline constexpr std::array<Language, 3> kAllLanguages { Language::Russian, Language::Kazakh, Language::English };`
  - `QString languageCode(Language language);` returns `"ru"`, `"kk"`, `"en"`.
  - `std::optional<Language> languageFromCode(QStringView code);` exact match only.
  - `QString nativeLanguageName(Language language);`
  - `Language defaultLanguage(const QLocale &systemLocale);`
- Produces, `settings.h`, `namespace Settings`:
  - `std::optional<Language> savedLanguage();`
  - `void saveLanguage(Language language);`
  - `Theme::Mode savedTheme();` returns Dark when unset or unknown.
  - `void saveTheme(Theme::Mode mode);`
- Produces, `theme.h`, `namespace Theme`:
  - `enum class Mode { Dark, Light };`
  - `QPalette lightPalette();`
  - `void apply(Mode mode);` replaces `apply()`. It sets the Fusion style only if the current style isn't Fusion (setStyle resets the palette), then sets the palette for `mode`.
  - `Mode currentMode();` returns the last applied mode.
- Produces, `tests/gui/testsettings.h`:
  - `QString useTemporarySettings();` calls `QSettings::setPath(IniFormat, UserScope, <tmp>)` with a `QTemporaryDir` that lives for the whole process, and returns `<tmp>/electricity/electricity.ini`.
  - `void clearSettings();` clears that file through `QSettings(<path>, IniFormat)`.

- [ ] **Step 1: Write the failing tests** in `tests/gui/tst_preferences.cpp` (class `TestPreferences`, `QTEST_MAIN`). `initTestCase()`: `m_iniPath = useTemporarySettings(); setupApplication(*qApp);`. `init()`: `clearSettings();`.

```cpp
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
// defaultLanguageFollowsSystemLocale: QCOMPARE(defaultLanguage(locale), expected);

void TestPreferences::languageCodesAndNames()
{
    QCOMPARE(kAllLanguages, (std::array { Language::Russian, Language::Kazakh, Language::English }));
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
```

- [ ] **Step 2: Verify red.** Run `cmake --build build`. Expected: compile error, because `app/language.h` doesn't exist.
- [ ] **Step 3: Implement** `language.{h,cpp}`, `settings.{h,cpp}`, the `Theme` additions and the test helper, per Interfaces. Light palette uses the same roles as `darkPalette()`, with window (240,240,240), base white, text and window text (30,30,30), button (225,225,225), accent (42,130,218), disabled text (150,150,150). Light is white, Midlight is `button.lighter(105)`, Mid is `button.darker(130)`, Dark is `button.darker(150)`, Shadow is (100,100,100). Register `Language` and `Theme::Mode` with `Q_DECLARE_METATYPE` where the data-driven test needs it.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build -R "tst_(preferences|mainwindow)" --output-on-failure`. Expected: both pass.
- [ ] **Step 5: Commit** all files listed above with the message `F03: language list, INI settings, light palette`.

### Task 2: Translation build, untranslated-text check, language switching

**Files:**
- Create: `cmake/CheckTranslations.cmake`, `tests/translations/untranslated.h`, `tests/translations/untranslated_ru.ts`
- Create: `translations/electricity_ru.ts`, `translations/electricity_kk.ts`, `translations/electricity_en.ts`
- Create: `docs/adr/0022-translation-check.md`
- Modify: `CMakeLists.txt` (`find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test LinguistTools)`), `src/app/CMakeLists.txt`, `src/app/language.h`, `src/app/language.cpp`, `tests/CMakeLists.txt`, `tests/gui/tst_preferences.cpp`, `docs/adr/README.md`

**Interfaces:**
- Consumes: `Language`, `languageCode()` (Task 1).
- Produces: `void installLanguage(Language language);` and `Language currentLanguage();` in `language.h`. Before the first `installLanguage()` call, `currentLanguage()` returns English.
- Produces: `cmake -DLUPDATE=<lupdate> -DSOURCE_DIR=<dir> -DTS_DIR=<dir> -DWORK_DIR=<dir> -P cmake/CheckTranslations.cmake`. It copies every `*.ts` in `TS_DIR` into a fresh `WORK_DIR` and runs `lupdate -silent <SOURCE_DIR> -ts <copy>` on each, adding `-pluralonly` for files ending in `_en.ts`. Every `execute_process` uses `COMMAND_ERROR_IS_FATAL ANY`. It counts `type="unfinished"` in each copy. If any count is non-zero, it fails with one line per such file: `<file name>: <n> unfinished translation(s)`. It also fails with `no .ts files in <TS_DIR>` if there are none. Otherwise it prints `All translations finished`.
- Produces: the build targets `update_translations` (refreshes the `.ts` files from the sources) and `:/i18n/electricity_<code>.qm` resources in `electricity_lib`.

- [ ] **Step 1: Write the fixture and the failing check test.**
  - `tests/translations/untranslated.h`: a header that is never compiled, containing `QT_TRANSLATE_NOOP("Fixture", "Untranslated text")`. It's a `.h` so clang-tidy, which only checks `.cpp` files, skips it.
  - `tests/translations/untranslated_ru.ts`: a valid TS 2.1 file, `language="ru_RU"`, with no messages.
  - Add `LinguistTools` to the top-level `find_package(Qt6 ...)`, so that `Qt6::lupdate` exists.
  - Append to `tests/CMakeLists.txt`:

```cmake
# Strings must be extracted before scanning: the fixture .ts has no unfinished entries,
# only the fixture source has a new one.
add_test(NAME translations_check_extracts_new_text
         COMMAND ${CMAKE_COMMAND} -DLUPDATE=$<TARGET_FILE:Qt6::lupdate>
                 -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/translations
                 -DTS_DIR=${CMAKE_CURRENT_SOURCE_DIR}/translations
                 -DWORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/translations_check_fixture
                 -P ${PROJECT_SOURCE_DIR}/cmake/CheckTranslations.cmake)
set_tests_properties(translations_check_extracts_new_text PROPERTIES
    LABELS translations PASS_REGULAR_EXPRESSION "untranslated_ru\\.ts: 1 unfinished")
```

- [ ] **Step 2: Verify red.** Run `cmake --preset dev && ctest --test-dir build -R translations --output-on-failure`. Expected: FAIL, because the script doesn't exist.
- [ ] **Step 3: Write `cmake/CheckTranslations.cmake`** per Interfaces.
- [ ] **Step 4: Verify green.** Run the same ctest command. Expected: PASS.
- [ ] **Step 5: Add the project check** to `tests/CMakeLists.txt`. It's the same command as Step 1, named `translations_complete`, with `SOURCE_DIR=${PROJECT_SOURCE_DIR}/src`, `TS_DIR=${PROJECT_SOURCE_DIR}/translations`, `WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/translations_check`, label `translations` and no pass regex. Then add to the end of `src/app/CMakeLists.txt`:

```cmake
qt_add_translations(TARGETS electricity_lib
    SOURCE_TARGETS electricity_lib electricity
    TS_FILES ${PROJECT_SOURCE_DIR}/translations/electricity_ru.ts
             ${PROJECT_SOURCE_DIR}/translations/electricity_kk.ts
    PLURALS_TS_FILE ${PROJECT_SOURCE_DIR}/translations/electricity_en.ts)
```

- [ ] **Step 6: Generate the `.ts` files.** Run `cmake --preset dev && cmake --build build --target update_translations && ctest --test-dir build -R translations_complete --output-on-failure`. Expected: FAIL with `electricity_kk.ts: 2 unfinished translation(s)` and `electricity_ru.ts: 2 unfinished translation(s)` (Components, Ready). `electricity_en.ts` has no messages.
- [ ] **Step 7: Translate** "Components" and "Ready" in both files per the Global Constraints table. Replace `<translation type="unfinished"></translation>` with `<translation>…</translation>`. Run Step 6's ctest again. Expected: PASS.
- [ ] **Step 8: Write the failing test** in `tst_preferences.cpp`:

```cpp
void TestPreferences::installsTranslationForEachLanguage()
{
    installLanguage(Language::Kazakh);
    QCOMPARE(currentLanguage(), Language::Kazakh);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"), QStringLiteral("Компоненттер"));
    installLanguage(Language::Russian);
    QCOMPARE(currentLanguage(), Language::Russian);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"), QStringLiteral("Компоненты"));
    installLanguage(Language::English);
    QCOMPARE(currentLanguage(), Language::English);
    QCOMPARE(QCoreApplication::translate("MainWindow", "Components"), QStringLiteral("Components"));
}
```

- [ ] **Step 9: Verify red.** Run `cmake --build build`. Expected: compile error, because `installLanguage` isn't declared.
- [ ] **Step 10: Implement `installLanguage`.** Use one function-static `QTranslator`. Remove it from the application, load `:/i18n/electricity_<code>.qm`, install it, and remember the language. Comment in the code why removal matters: `removeTranslator` sends `LanguageChange`, but installing the message-less English translator does not, so removal is what retranslates the UI when switching to English. Use `qWarning` if `load` fails.
- [ ] **Step 11: Verify green.** Run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all tests pass.
- [ ] **Step 12: Write ADR 0022** "Translation check extracts strings first", following `docs/adr/0000-template.md`, and add its row to `docs/adr/README.md`.
  - **Context:** 0012's scan of `.ts` files misses strings that were never extracted.
  - **Decision:** what `cmake/CheckTranslations.cmake` does, and the developer workflow: after changing UI text, run `cmake --build build --target update_translations`, then translate the ru and kk entries.
  - **Rejected:** running `update_translations` in every build (it rewrites tracked files during normal builds); scanning only (misses new text).
  - **Consequences:** every feature ships its ru and kk text, and a vanished string stays in the `.ts` file until someone removes it.
- [ ] **Step 13: Commit** all files listed above with the message `F03: embedded translations, language switching, untranslated-text check (ADR 0022)`.

### Task 3: Settings and Language menus with instant switch and persistence

**Files:**
- Modify: `src/app/mainwindow.h`, `src/app/mainwindow.cpp`, `src/app/appsetup.h`, `src/app/appsetup.cpp`, `translations/electricity_ru.ts`, `translations/electricity_kk.ts`
- Modify: `tests/gui/tst_mainwindow.cpp` (`initTestCase()` only), `tests/gui/tst_preferences.cpp`

**Interfaces:**
- Consumes: `Settings::*`, `Theme::apply/currentMode`, `installLanguage/currentLanguage`, `defaultLanguage`, `kAllLanguages`, `nativeLanguageName`, `languageCode`.
- Produces: `void applySavedPreferences();` in `appsetup.h`. It applies `Settings::savedTheme()` and installs `Settings::savedLanguage()`, or `defaultLanguage(QLocale::system())` if none is saved. `setupApplication()` calls it instead of `Theme::apply(...)`.
- Produces: `MainWindow` with the menus `settingsMenu` (submenu `themeMenu` with exclusive checkable `themeAction_dark|light`) and `languageMenu` (exclusive checkable `languageAction_<code>`, with the native names as text), in that order on the menu bar. The checked items come from `Theme::currentMode()` and `currentLanguage()` at construction. Triggering a theme item calls `Theme::apply` and `Settings::saveTheme`. Triggering a language item calls `installLanguage` and `Settings::saveLanguage`. `retranslateUi()` sets every translatable text: the menu titles, the theme item texts, the dock title and the "Ready" status message. `changeEvent` calls it on `QEvent::LanguageChange`.

- [ ] **Step 1: Pin `tst_mainwindow` to English in temporary settings.** Change `TestMainWindow::initTestCase()` to `useTemporarySettings(); Settings::saveLanguage(Language::English); setupApplication(*qApp);`. Its assertions stay unchanged.
- [ ] **Step 2: Write the failing scenario test** in `tst_preferences.cpp`. Add helpers `clickMenuBarItem(QMainWindow &, QMenu *)`, which clicks `menuBar()->actionGeometry(menu->menuAction()).center()` and then does `QTRY_VERIFY(menu->isVisible())`, and `clickMenuItem(QMenu *, QAction *)`, which clicks `menu->actionGeometry(action).center()`. For the theme submenu, click its entry in `settingsMenu` and `QTRY_VERIFY(themeMenu->isVisible())`; if a click doesn't open it, `QTest::mouseMove` onto the entry first.

```cpp
void TestPreferences::switchesLanguageAndThemeAtRuntimeAndPersists()
{
    Settings::saveLanguage(Language::Russian);
    applySavedPreferences();
    {
        MainWindow window;
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        // find settingsMenu, themeMenu, languageMenu, the actions, paletteDock, canvasView by objectName
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
        // open themeMenu from settingsMenu, then:
        clickMenuItem(themeMenu, light);
        const QColor lightWindow = Theme::lightPalette().color(QPalette::Window);
        QCOMPARE(window.palette().color(QPalette::Window), lightWindow);
        QCOMPARE(dock->palette().color(QPalette::Window), lightWindow);
        QVERIFY(canvas->palette().color(QPalette::Base).lightness() > 200);
        QCOMPARE(Settings::savedTheme(), Theme::Mode::Light);
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
    // languageMenu title "Тіл", dock "Компоненттер", languageAction_kk and themeAction_light
    // checked, window Window color == lightWindow
    QVERIFY(saveScreenshot(restarted, "f03", "restarted"));
}
```

- [ ] **Step 3: Verify red.** Run `cmake --build build`. Expected: compile error, because `applySavedPreferences` isn't declared.
- [ ] **Step 4: Implement** `applySavedPreferences()` and the menus per Interfaces.
- [ ] **Step 5: Update translations.** Run `cmake --build build --target update_translations && ctest --test-dir build -R translations_complete --output-on-failure`. Expected: FAIL with 5 unfinished in each of ru and kk (Settings, Theme, Dark, Light, Language). Translate them per the table, then rerun. Expected: PASS.
- [ ] **Step 6: Verify green.** Run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass.
- [ ] **Step 7: Inspect screenshots.** Open `build/screenshots/f03-{ru,language-menu,kk,en,light,restarted}.png`. Expected:
  - The menu bar shows the translated titles, and the Kazakh letters (ә, ғ, қ, ң, ө, ұ, ү, һ, і) render.
  - The language menu shows a check mark on the current language.
  - The light theme is legible: dark text on light backgrounds, including the dock title and status bar.
- [ ] **Step 8: Commit** the files listed above with the message `F03: Settings and Language menus, instant switch, persisted preferences`.

### Task 4: First-launch language dialog

**Files:**
- Create: `src/app/languagedialog.h`, `src/app/languagedialog.cpp`
- Modify: `src/app/appsetup.h`, `src/app/appsetup.cpp`, `src/app/main.cpp`, `src/app/CMakeLists.txt`, `tests/gui/tst_preferences.cpp`

**Interfaces:**
- Consumes: `Language`, `kAllLanguages`, `nativeLanguageName`, `languageCode`, `defaultLanguage`, `installLanguage`, `Settings::savedLanguage/saveLanguage`.
- Produces: `class LanguageDialog : public QDialog` with `explicit LanguageDialog(Language preselected, QWidget *parent = nullptr);` and `Language chosenLanguage() const;`.
  - Object name `languageDialog`, window title `"Electricity"`.
  - The `languageHeading` label holds the exact heading. Below it, a row of `languageButton_<code>` push buttons with the native names, at least 140×48 px.
  - The preselected button is the default button and has focus.
  - Clicking a button sets the choice and accepts. Esc or closing the window rejects, and the choice stays the preselected language.
- Produces: `void askLanguageOnFirstLaunch();` in `appsetup.h`. If `Settings::savedLanguage()` is empty, it runs `LanguageDialog(defaultLanguage(QLocale::system()))` modally, then installs and saves `chosenLanguage()`. Otherwise it does nothing.
- `main.cpp`: calls `askLanguageOnFirstLaunch()` after `setupApplication(app)`, before creating `MainWindow`.

- [ ] **Step 1: Write the failing tests.** Add a helper that runs a callback on the visible `languageDialog` once the event loop starts: `QTimer::singleShot(0, ...)`, finding it among `QApplication::topLevelWidgets()`. Each test does its checks inside the callback, stores the results in locals, and asserts them after `askLanguageOnFirstLaunch()` returns.

```cpp
void TestPreferences::firstLaunchAsksForLanguageAndSavesChoice()
{
    // in the callback: heading text == u"Выберите язык · Тілді таңдаңыз · Language";
    // windowTitle == u"Electricity"; buttons ru/kk/en have texts Русский/Қазақша/English;
    // the button for defaultLanguage(QLocale::system()) isDefault();
    // saveScreenshot(dialog, "f03", "first-launch"); then mouseClick(languageButton_kk)
    askLanguageOnFirstLaunch();
    // assert the callback's checks held
    QCOMPARE(Settings::savedLanguage(), Language::Kazakh);
    QCOMPARE(currentLanguage(), Language::Kazakh);
}

void TestPreferences::dismissingFirstLaunchDialogKeepsPreselectedLanguage_data()
{
    QTest::addColumn<bool>("useEscape"); // true: QTest::keyClick(dialog, Qt::Key_Escape); false: dialog->close()
    QTest::newRow("Esc") << true;
    QTest::newRow("close button") << false;
}
// dismissingFirstLaunchDialogKeepsPreselectedLanguage: after askLanguageOnFirstLaunch(),
// QCOMPARE(Settings::savedLanguage(), defaultLanguage(QLocale::system()));

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
```

- [ ] **Step 2: Verify red.** Run `cmake --build build`. Expected: compile error, because `askLanguageOnFirstLaunch` isn't declared.
- [ ] **Step 3: Implement** `LanguageDialog`, `askLanguageOnFirstLaunch()` and the `main.cpp` call per Interfaces.
- [ ] **Step 4: Verify green.** Run `cmake --build build && ctest --test-dir build --output-on-failure`. Expected: all pass, including `translations_complete`, since the dialog has no translatable text.
- [ ] **Step 5: Inspect** `build/screenshots/f03-first-launch.png`. Expected: the trilingual heading is readable, the three large buttons are in the order Русский, Қазақша, English, and the default button is visibly marked.
- [ ] **Step 6: Commit** the files listed above with the message `F03: first-launch language dialog`.

### Task 5: Verification and roadmap

**Files:**
- Modify: `docs/roadmap.md` (F03 ◐ → ☑)

- [ ] **Step 1: Quality gates.** Run `scripts/quality_gates.sh` in the background. Expected last line: `All quality gates passed`.
- [ ] **Step 2: Release binary.** Run `scripts/build_linux_executable.sh`. Expected: the dependency check passes with no new libraries, and the last line is `Created dist/electricity-linux-x86_64`.
- [ ] **Step 3: Smoke test, first launch.** With `CFG=<scratchpad>/f03-config` (new, empty), run `XDG_CONFIG_HOME=$CFG xvfb-run -a -s "-screen 0 1600x900x24" sh -c './build/electricity & sleep 3; import -window root build/screenshots/f03-smoke-first-launch.png; kill $!'`. Open the PNG. Expected: the language dialog is centred on screen.
- [ ] **Step 4: Smoke test, saved preferences.** Write `$CFG/electricity/electricity.ini` containing `[General]`, `language=kk` and `theme=light` on separate lines. Rerun Step 3's command with screenshot `f03-smoke-kk-light.png`. Expected: no dialog, and the main window is light with the menus `Баптаулар` and `Тіл`.
- [ ] **Step 5: Review all `build/screenshots/f03-*.png`** once more as a set. Expected: nothing clipped or unreadable, and the dark and light themes are both legible.
- [ ] **Step 6: Mark F03 ☑** in `docs/roadmap.md` and commit it with the message `F03: mark done`. Then run `ps -eo pid,ppid,etime,cmd` and stop any leftover background process.
