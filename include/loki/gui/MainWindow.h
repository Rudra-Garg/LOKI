#pragma once

#include <QMainWindow>
#include <QThread> // Keep for QTimer
#include <QTimer>  // ADDED

class QTextEdit;
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

    ~MainWindow() override;

public slots:
    void update_status(const QString &message);

    void display_response_and_hide(const QString &message);

protected: // It's good practice to put event handlers in the protected section
    // ADD THIS LINE:
    void showEvent(QShowEvent *event) override;

private:
    void setup_ui();

    void clear_logs();

    QLabel *status_label_;
    QTextEdit *log_display_;
    QTimer *hide_timer_; // Timer to auto-hide the window
};
