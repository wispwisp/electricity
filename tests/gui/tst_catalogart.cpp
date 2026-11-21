// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/svgpixmap.h"
#include "app/theme.h"
#include "core/catalog.h"
#include "screenshot.h"
#include "testsettings.h"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QTest>

#include <array>

class TestCatalogArt : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void everyPictureExistsAndFollowsArtRules();
    void devicePicturesMatchCatalog();
    void barAndEnclosurePicturesMatchCatalog();
    void rendersAtDevicePixelRatio();
    void rendersEveryPictureAt32px();
};

namespace {

QStringList pictures()
{
    const Catalog &catalog = embeddedCatalog();
    QStringList result;
    for (const DeviceType &type : catalog.deviceTypes)
        for (const DeviceVariant &variant : type.variants)
            result << variant.picture;
    for (const BarType &bar : catalog.bars)
        result << bar.picture;
    for (const EnclosureType &enclosure : catalog.enclosures)
        result << enclosure.picture;
    return result;
}

QRectF mappedBounds(const QSvgRenderer &renderer, const QString &id)
{
    return renderer.transformForElement(id).mapRect(renderer.boundsOnElement(id));
}

bool hasOpaquePixel(const QPixmap &pixmap)
{
    const QImage image = pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (qAlpha(image.pixel(x, y)) == 255)
                return true;
    return false;
}

// Every picture in a row, then each bar tiled 8 times, 8 px apart on the background.
QPixmap sheet(const QColor &background, int height)
{
    const int gap = 8;
    QList<QPixmap> items;
    for (const QString &picture : pictures())
        items << svgPixmap(picturePath(picture), height);
    for (const BarType &bar : embeddedCatalog().bars) {
        const QPixmap cell = svgPixmap(picturePath(bar.picture), height);
        QPixmap strip(cell.width() * 8, height);
        strip.fill(Qt::transparent);
        QPainter painter(&strip);
        for (int i = 0; i < 8; ++i)
            painter.drawPixmap(i * cell.width(), 0, cell);
        painter.end();
        items << strip;
    }

    int width = gap;
    for (const QPixmap &item : items)
        width += item.width() + gap;
    QPixmap result(width, height + 2 * gap);
    result.fill(background);
    QPainter painter(&result);
    int x = gap;
    for (const QPixmap &item : items) {
        painter.drawPixmap(x, gap, item);
        x += item.width() + gap;
    }
    return result;
}

} // namespace

void TestCatalogArt::initTestCase()
{
    useTemporarySettings();
    setupApplication(*qApp);
}

void TestCatalogArt::everyPictureExistsAndFollowsArtRules()
{
    QTest::failOnWarning(QRegularExpression(QStringLiteral(".*")));
    const std::array forbidden
            = { "<text", "<style", "style=", "<image", "<filter", "<script", "<foreignObject" };
    QStringList paths;
    for (const QString &picture : pictures())
        paths << picturePath(picture);
    const QString supply = QStringLiteral(":/svg/devices/supply-1p.svg");
    paths << supply << QStringLiteral(":/svg/ui/undo.svg") << QStringLiteral(":/svg/ui/redo.svg");
    for (const QString &path : paths) {
        QVERIFY2(QFile::exists(path), qPrintable(path));
        QVERIFY2(QSvgRenderer(path).isValid(), qPrintable(path));

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QByteArray text = file.readAll();
        QVERIFY2(text.contains("SPDX-License-Identifier: GPL-3.0-or-later"), qPrintable(path));
        for (const char *word : forbidden)
            QVERIFY2(!text.contains(word), qPrintable(path + QStringLiteral(": ") + word));
    }
    const QSvgRenderer supplyRenderer(supply);
    for (const char *id : { "terminal-L", "terminal-N", "terminal-PE" })
        QVERIFY2(supplyRenderer.elementExists(QLatin1StringView(id)), id);
}

void TestCatalogArt::devicePicturesMatchCatalog()
{
    for (const DeviceType &type : embeddedCatalog().deviceTypes) {
        for (const DeviceVariant &variant : type.variants) {
            const QSvgRenderer r(picturePath(variant.picture));
            const auto where = [&](const QString &what) {
                return (variant.picture + QStringLiteral(": ") + what).toLocal8Bit();
            };
            QVERIFY2(r.viewBoxF() == QRectF(0, 0, 18 * variant.modules, 90),
                     where(QStringLiteral("viewBox")).constData());
            for (const char *id : { "body", "handle", "label" })
                QVERIFY2(r.elementExists(QLatin1StringView(id)),
                         where(QLatin1StringView(id)).constData());
            for (const TerminalSpec &terminal : variant.terminals) {
                const QString id = QStringLiteral("terminal-") + terminal.name;
                QVERIFY2(r.elementExists(id), where(id).constData());
                QVERIFY2(mappedBounds(r, id).contains(terminal.position), where(id).constData());
            }
            if (type.id == QLatin1StringView("rcd") || type.id == QLatin1StringView("rcbo"))
                QVERIFY2(r.elementExists(QStringLiteral("test-button")),
                         where(QStringLiteral("test-button")).constData());
        }
    }
}

void TestCatalogArt::barAndEnclosurePicturesMatchCatalog()
{
    for (const BarType &bar : embeddedCatalog().bars) {
        const QSvgRenderer r(picturePath(bar.picture));
        QVERIFY2(r.viewBoxF().width() == bar.pitch, qPrintable(bar.picture));
        QVERIFY2(r.elementExists(QStringLiteral("terminal")), qPrintable(bar.picture));
        QVERIFY2(mappedBounds(r, QStringLiteral("terminal")).contains(bar.terminal),
                 qPrintable(bar.picture));
    }
    for (const EnclosureType &enclosure : embeddedCatalog().enclosures) {
        const QSvgRenderer r(picturePath(enclosure.picture));
        for (int row = 1; row <= enclosure.rows; ++row) {
            const QString id = QStringLiteral("rail-%1").arg(row);
            QVERIFY2(r.elementExists(id), qPrintable(enclosure.picture + u' ' + id));
            QVERIFY2(mappedBounds(r, id).width() >= 216, qPrintable(enclosure.picture + u' ' + id));
        }
        QVERIFY2(r.elementExists(QStringLiteral("nbar-mount")), qPrintable(enclosure.picture));
        QVERIFY2(r.elementExists(QStringLiteral("pebar-mount")), qPrintable(enclosure.picture));
    }
}

void TestCatalogArt::rendersAtDevicePixelRatio()
{
    const QPixmap pixmap = svgPixmap(picturePath(QStringLiteral("breaker-1p.svg")), 48, 2.0);
    QCOMPARE(pixmap.devicePixelRatio(), 2.0);
    QCOMPARE(pixmap.height(), 96);
    QCOMPARE(pixmap.deviceIndependentSize().width(), qRound(48 * 18 / 90.0));
    QVERIFY(svgPixmap(QStringLiteral("nope.svg"), 48).isNull());
}

void TestCatalogArt::rendersEveryPictureAt32px()
{
    for (const QString &picture : pictures()) {
        const QPixmap pixmap = svgPixmap(picturePath(picture), 32);
        QVERIFY2(pixmap.height() == 32, qPrintable(picture));
        QVERIFY2(hasOpaquePixel(pixmap), qPrintable(picture));
    }
    const QString test = QStringLiteral("f04");
    QVERIFY(saveScreenshot(sheet(Theme::darkPalette().color(QPalette::Window), 32), test,
                           QStringLiteral("svg-32px-dark")));
    QVERIFY(saveScreenshot(sheet(Theme::lightPalette().color(QPalette::Window), 32), test,
                           QStringLiteral("svg-32px-light")));
    QVERIFY(saveScreenshot(sheet(Theme::darkPalette().color(QPalette::Window), 180), test,
                           QStringLiteral("svg-large-dark")));
}

QTEST_MAIN(TestCatalogArt)
#include "tst_catalogart.moc"
