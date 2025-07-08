#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QIcon>
#include <QThread>
#include <QDebug>
#include <QCoreApplication>
#include "loki/gui/MainWindow.h"
#include "loki/core/LokiWorker.h"


int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    // ===================================================================
    // == Worker Thread Setup
    // ===================================================================
    QThread worker_thread;
    worker_thread.setObjectName("LokiWorkerThread");
    auto *loki_worker = new LokiWorker();
    loki_worker->moveToThread(&worker_thread);

    // ===================================================================
    // == Main Window Setup
    // ===================================================================
    MainWindow main_window;

    // ===================================================================
    // == System Tray Setup
    // ===================================================================
    QSystemTrayIcon tray_icon;
    tray_icon.setToolTip("Loki is running in the background");
    QMenu tray_menu;
    QAction *quit_action = tray_menu.addAction("Quit Loki");
    tray_icon.setContextMenu(&tray_menu);
    tray_icon.show();

    // ===================================================================
    // == Connect Signals and Slots
    // ===================================================================
    // Worker thread connections
    QObject::connect(&worker_thread, &QThread::started, loki_worker, &LokiWorker::initialize);
    // MODIFIED: Start processing only after initialization is fully complete.
    QObject::connect(loki_worker, &LokiWorker::initialization_complete, loki_worker, &LokiWorker::start_processing);
    QObject::connect(loki_worker, &LokiWorker::wake_word_detected_signal, &main_window, &MainWindow::show);
    QObject::connect(loki_worker, &LokiWorker::status_updated, &main_window, &MainWindow::update_status);
    QObject::connect(loki_worker, &LokiWorker::loki_response, &main_window, &MainWindow::display_response_and_hide);

    // ===================================================================
    // == Shutdown Sequence
    // ===================================================================
    QObject::connect(quit_action, &QAction::triggered, &app, &QCoreApplication::quit);

    // Tell the worker to stop processing
    QObject::connect(&app, &QCoreApplication::aboutToQuit, loki_worker, &LokiWorker::stop_processing);

    // Tell the thread to quit its event loop
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &worker_thread, &QThread::quit);

    // After the thread has finished, delete the worker
    QObject::connect(&worker_thread, &QThread::finished, loki_worker, &LokiWorker::deleteLater);

    // ===================================================================
    // == Start Application
    // ===================================================================
    worker_thread.start();

    qDebug() << "LOKI started successfully";
    qDebug() << "Application directory:" << QCoreApplication::applicationDirPath();

    return QApplication::exec();
}
