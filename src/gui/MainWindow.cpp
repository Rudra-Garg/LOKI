#include "loki/gui/MainWindow.h"
#include <QVBoxLayout>
#include <QWidget>
#include <QTextEdit>
#include <QLabel>
#include <QScreen> // To center the window

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setup_ui();
    hide_timer_ = new QTimer(this);
    hide_timer_->setSingleShot(true);
    hide_timer_->setInterval(5000); // Hide after 5 seconds
    connect(hide_timer_, &QTimer::timeout, this, &MainWindow::hide);
}

MainWindow::~MainWindow() = default;

void MainWindow::setup_ui() {
    setWindowTitle("Loki");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setFixedSize(400, 200);

    QWidget *central_widget = new QWidget(this);
    central_widget->setStyleSheet("background-color: #2E2E2E; color: #FFFFFF;");
    setCentralWidget(central_widget);

    QVBoxLayout *layout = new QVBoxLayout(central_widget);
    log_display_ = new QTextEdit(this);
    log_display_->setReadOnly(true);
    log_display_->setStyleSheet("font-size: 14px; border: none;");
    layout->addWidget(log_display_);
}

void MainWindow::clear_logs() {
    log_display_->clear();
}

void MainWindow::update_status(const QString &message) {
    log_display_->append(message);
}

void MainWindow::display_response_and_hide(const QString &message) {
    log_display_->append(QString("\nLOKI: %1").arg(message));
    hide_timer_->start(); // Start the countdown to hide
}

// Override showEvent to clear logs and center the window each time it appears
void MainWindow::showEvent(QShowEvent *event) {
    clear_logs();
    // Center the window on the primary screen
    move(screen()->geometry().center() - rect().center());
    QMainWindow::showEvent(event);
}
