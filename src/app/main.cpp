// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/appsetup.h"
#include "app/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    setupApplication(app);
    askLanguageOnFirstLaunch();
    MainWindow window;
    window.show();
    return app.exec();
}
