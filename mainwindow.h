#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <serialporthandler.h>
#include <QMessageBox>
#include <QFile>
#include <QDateTime>
#include <QTimer>
#include <windows.h>
#include <psapi.h>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QApplication>
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QLabel>
#include <QScreen>
#include <QInputDialog>

#include <QScrollArea>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// Forward declaration of serialPortHandler
class serialPortHandler;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void refreshPorts();

    //For Saving log Data
    void resetLogFile();
    static void writeToNotes(const QString &data);
    void initializeLogFile();
    void closeLogFile();

    quint8 calculateChecksum(const QByteArray &data);
    QString hexBytes(QByteArray &cmd);

    //Extra features
    void printMemoryUsage();

    void elapseStart();
    void elapseEnd(bool goFurther = false, const QString &label = "");

    QDialog* createPleaseWaitDialog(const QString &text, int timeSeconds = 0);

    inline void pauseFor(int milliseconds) {
        QEventLoop loop;
        QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);  // After delay, quit the event loop
        loop.exec();  // Start the event loop and wait for it to quit
        QApplication::processEvents();  // Keep UI healthy
    }

    void blinkLabel(QLabel *label,
                    int durationMs,
                    const QString &text);

    void applyScrollArea();

    void initializePlots();

    void plotADC_voltage(quint64 count, float voltage);

private slots:
    void onPortSelected(const QString &portName);

    void portStatus(const QString&);

    void showGuiData(const QByteArray &byteArrayData);

    //response time handling

    void handleTimeout();

    void onDataReceived();

    void on_pushButton_calibrateScreen_clicked();

    void on_pushButton_led_clicked();

    void on_pushButton_PWM_clicked();

    void on_pushButton_readADC_clicked();

    void on_pushButton_stopADC_clicked();

    void on_pushButton_interrupt_clicked();

signals:
    void sendMsgId(quint8 id);

private:
    Ui::MainWindow *ui;
    serialPortHandler *serialObj;

    //Log handling
    static QFile logFile;
    static QTextStream logStream;

    //Response Time waiting timer
    QTimer *responseTimer = nullptr; // Timer to track response timeout

    //Extras
    QElapsedTimer elapsedTimer;

    //blinkLabel timers
    QHash<QLabel*, QTimer*> blinkTimers;

    quint64 count = 0;
};
#endif // MAINWINDOW_H
