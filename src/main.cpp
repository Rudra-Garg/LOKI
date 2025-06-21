#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QThread> // ADD THIS INCLUDE
#include "loki/gui/MainWindow.h"
#include "loki/core/LokiWorker.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    QThread worker_thread;
    worker_thread.setObjectName("LokiWorkerThread");
    auto *loki_worker = new LokiWorker();
    loki_worker->moveToThread(&worker_thread);

    MainWindow main_window;

    // The Tray Icon code should have a resource file, but for now we can pass an empty icon.
    // To fix this properly later, you'd create a qrc file and add your icon to it.
    QSystemTrayIcon tray_icon;
    tray_icon.setToolTip("Loki is running in the background");
    QMenu tray_menu;
    QAction *quit_action = tray_menu.addAction("Quit Loki");
    tray_icon.setContextMenu(&tray_menu);
    tray_icon.show();

    // --- Connect Signals and Slots ---
    QObject::connect(&worker_thread, &QThread::started, loki_worker, &LokiWorker::initialize);
    QObject::connect(&worker_thread, &QThread::started, loki_worker, &LokiWorker::start_processing);
    QObject::connect(loki_worker, &LokiWorker::wake_word_detected_signal, &main_window, &MainWindow::show);
    QObject::connect(loki_worker, &LokiWorker::status_updated, &main_window, &MainWindow::update_status);
    QObject::connect(loki_worker, &LokiWorker::loki_response, &main_window, &MainWindow::display_response_and_hide);

    // --- MODIFIED Shutdown Sequence ---
    QObject::connect(quit_action, &QAction::triggered, &app, &QCoreApplication::quit);
    // When the app is about to quit, tell the worker to stop processing.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, loki_worker, &LokiWorker::stop_processing);
    // Tell the thread to quit its event loop.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &worker_thread, &QThread::quit);
    // After the thread has finished, delete the worker.
    QObject::connect(&worker_thread, &QThread::finished, loki_worker, &LokiWorker::deleteLater);
    // The main application will automatically wait for the thread to finish.

    worker_thread.start();

    return QApplication::exec();
}
