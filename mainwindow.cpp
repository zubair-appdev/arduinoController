#include "mainwindow.h"
#include "ui_mainwindow.h"

QFile MainWindow::logFile;
QTextStream MainWindow::logStream;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    serialObj =   new serialPortHandler(this);

    connect(ui->pushButton_clear,&QPushButton::clicked,ui->textEdit_rawBytes,&QTextEdit::clear);

    ui->comboBox_ports->addItems(serialObj->availablePorts());

    connect(ui->pushButton_portsRefresh,&QPushButton::clicked,this,&MainWindow::refreshPorts);

    connect(ui->comboBox_ports,SIGNAL(activated(const QString &)),this,SLOT(onPortSelected(const QString &)));

    connect(this,&MainWindow::sendMsgId,serialObj,&serialPortHandler::recvMsgId);


    //writeToNotes from serial class
    connect(serialObj,&serialPortHandler::executeWriteToNotes,this,&MainWindow::writeToNotes);

    //debugging signals
    connect(serialObj,&serialPortHandler::portOpening,this,&MainWindow::portStatus);

    //gui display signal
    connect(serialObj,&serialPortHandler::guiDisplay,this,&MainWindow::showGuiData);

    //reset previous notes #Notes things : Logging file
    resetLogFile();
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Started");
    //#################################################

    //Response Timer *********************************************##############
    responseTimer = new QTimer(this);
    responseTimer->setSingleShot(true); // Ensure it fires only once per use

    // Connect the timer's timeout signal to a slot that handles the timeout
    connect(responseTimer, &QTimer::timeout, this, &MainWindow::handleTimeout);

    connect(serialObj, &serialPortHandler::dataReceived, this, &MainWindow::onDataReceived);
    //************************************************************##############

    writeToNotes("Pointer Size: "+QString::number(sizeof(void *))+" If it is 8 : 64 bit else 4 means 32 bit");

    applyScrollArea();

    //Initializations
    ui->radioButton_pwmManual->setChecked(true);

    initializePlots();

}

MainWindow::~MainWindow()
{
    writeToNotes(+"    ******    "+QCoreApplication::applicationName() +
                 "     Application Closed");
    delete ui;
    delete serialObj;
    delete responseTimer;
    closeLogFile();
}

void MainWindow::initializeLogFile() {
    if (!logFile.isOpen()) {
        logFile.setFileName("debug_notes.txt");
        if (!logFile.open(QIODevice::Append | QIODevice::Text)) {
            qCritical() << "Failed to open log file.";
        } else {
            logStream.setDevice(&logFile);
        }
    }
}

void MainWindow::resetLogFile() {
    // Close the log file if it is open
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }

    // Check if the file exists and delete it
    QFile::remove("debug_notes.txt");

    // Reinitialize the log file
    initializeLogFile();
}


void MainWindow::writeToNotes(const QString &data) {
    if (!logFile.isOpen()) {
        qCritical() << "Log file is not open.";
        return;
    }

    // Add a timestamp for each entry
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    logStream << "[" << timestamp << "] " << data << Qt::endl;
    logStream.flush(); // Ensure immediate write to disk
}

void MainWindow::closeLogFile() {
    if (logFile.isOpen()) {
        logStream.flush();
        logFile.close();
    }
}

quint8 MainWindow::calculateChecksum(const QByteArray &data)
{
    quint8 checkSum = 0;
    for(quint8 byte : data)
    {
        checkSum ^= byte;
    }

    return checkSum;
}

void MainWindow::refreshPorts()
{
    QString currentPort = ui->comboBox_ports->currentText();

    qDebug()<<"Refreshing ports...";
    ui->comboBox_ports->clear();
    QStringList availablePorts;
    ui->comboBox_ports->addItems(serialObj->availablePorts());

    ui->comboBox_ports->setCurrentText(currentPort);
}

void MainWindow::onPortSelected(const QString &portName)
{
    serialObj->setPORTNAME(portName);
}

void MainWindow::handleTimeout()
{
    QMessageBox::warning(this, "Timeout", "Hardware Not Responding!");
}

void MainWindow::onDataReceived()
{
    // Stop the timer since data has been received
    //qDebug()<<"Hello stop it";
    if (responseTimer->isActive()) {
        responseTimer->stop();
    }
}


//The below function is intended for providing space between hex bytes
QString MainWindow::hexBytes(QByteArray &cmd)
{
    //**************************Visuals*******************
    QString hexOutput = cmd.toHex().toUpper();
    QString formattedHexOutput;

    for (int i = 0; i < hexOutput.size(); i += 2) {
        if (i > 0) {
            formattedHexOutput += " ";
        }
        formattedHexOutput += hexOutput.mid(i, 2);
    }
    return formattedHexOutput;
    //**************************Visuals*******************
}

void MainWindow::printMemoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX memInfo;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memInfo, sizeof(memInfo))) {
        SIZE_T workingSet = memInfo.WorkingSetSize;
        SIZE_T privateUsage = memInfo.PrivateUsage;

        qDebug() << "Working Set (RAM):"
                 << workingSet / 1024 << "KB ("
                 << QString::number(workingSet / (1024.0 * 1024.0), 'f', 2) << "MB)";

        qDebug() << "Private Bytes:"
                 << privateUsage / 1024 << "KB ("
                 << QString::number(privateUsage / (1024.0 * 1024.0), 'f', 2) << "MB)";
    } else {
        qDebug() << "Failed to get memory info!";
    }
}

void MainWindow::elapseStart()
{
    elapsedTimer.start();
}

void MainWindow::elapseEnd(bool goFurther, const QString &label)
{
    qint64 ns = elapsedTimer.nsecsElapsed();
    double ms = ns / 1000000.0;

    if (label.isEmpty()) {
        qDebug() << "Time taken from elapseStart() to elapseEnd():"
                 << ns << "ns (" << ms << "ms)";
    } else {
        qDebug() << "Elapsed [" << label << "]:"
                 << ns << "ns (" << ms << "ms)";
    }

    if (!goFurther)
        elapsedTimer.restart();
}

QDialog* MainWindow::createPleaseWaitDialog(const QString &text, int timeSeconds)
{
    // --- Create dialog ---
    QDialog *dlg = new QDialog(this);
    dlg->setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setModal(true);

    // --- Styling ---
    dlg->setStyleSheet(R"(
                       QDialog {
                       background-color: #f8f8f8;
                       border: 2px solid #0078D7;
                       border-radius: 8px;
                       }
                       QLabel {
                       font-size: 16px;
                       padding: 10px;
                       }
                       )");

    // --- Layout and main label ---
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    QLabel *mainLabel = new QLabel(text, dlg);
    layout->addWidget(mainLabel);

    QLabel *timerLabel = nullptr;

    // --- Optional countdown ---
    if (timeSeconds > 0)
    {
        timerLabel = new QLabel(QString("Remaining: %1s").arg(timeSeconds), dlg);
        timerLabel->setAlignment(Qt::AlignCenter);
        timerLabel->setStyleSheet("color: #0078D7; font-weight: bold;");
        layout->addWidget(timerLabel);

        QTimer *countdown = new QTimer(dlg);
        countdown->setInterval(1000);

        int *remaining = new int(timeSeconds);

        QObject::connect(countdown, &QTimer::timeout, dlg, [countdown, remaining, timerLabel]() {
            (*remaining)--;
            if (*remaining <= 0)
            {
                countdown->stop();
                delete remaining;
            }
            else
            {
                timerLabel->setText(QString("Remaining: %1s").arg(*remaining));
            }
        });

        countdown->start();
    }

    dlg->setLayout(layout);
    dlg->adjustSize();
    dlg->setFixedSize(dlg->sizeHint());
    dlg->show();

    QApplication::processEvents(); // ensures dialog appears immediately

    return dlg;

    //    Using of this function
    //    QDialog *dlg = createPleaseWaitDialog("⏳ Please Wait ...");
    //    OR
    //    QDialog *dlg = createPleaseWaitDialog("⏳ Please Wait ...",10); //if you know how much time to wait
    //    dlg->close();
}

void MainWindow::blinkLabel(QLabel *label,
                            int durationMs,
                            const QString &text)
{
    if (!label)
        return;

    // Create timer if not exists for this label
    if (!blinkTimers.contains(label)) {
        QTimer *timer = new QTimer(this);
        timer->setSingleShot(true);

        connect(timer, &QTimer::timeout, this, [=]() {
            label->setStyleSheet("");
            label->setText("Status");
        });

        blinkTimers[label] = timer;
    }

    QTimer *timer = blinkTimers[label];

    // 🔥 KEY: cancel previous pending reset
    timer->stop();

    // Update UI immediately
    label->setText(text);

    label->setStyleSheet("background-color: yellow;");

    // Start fresh timer
    timer->start(durationMs);
}

void MainWindow::applyScrollArea()
{
    //In scroll area
    // Only retrieve and embed the central widget in a scroll area
    QWidget *existingCentralWidget = takeCentralWidget(); // Take the existing central widget
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidget(existingCentralWidget);         // Embed it in the scroll area
    scrollArea->setWidgetResizable(true);                 // Allow resizing within the scroll area

    // Set the scroll area as the new central widget
    setCentralWidget(scrollArea);
}

void MainWindow::initializePlots()
{
    ui->label_percent->setText(QString("Battery %1% Voltage %2V").arg(0).arg(0));

    ui->customPlot_adc->clearGraphs();
    ui->customPlot_adc->clearItems();

    ui->customPlot_adc->addGraph();

    auto graph = ui->customPlot_adc->graph(0);

    graph->setPen(QPen(Qt::darkGreen));

    //Setting Labels
    ui->customPlot_adc->xAxis->setLabel("Time(msec)");
    ui->customPlot_adc->yAxis->setLabel("Voltage(V)");

    ui->customPlot_adc->yAxis->setRange(0,5);

    //Enable Interactions
    ui->customPlot_adc->setInteractions(QCP::iRangeDrag |
                                        QCP::iRangeZoom);

    ui->customPlot_adc->replot();
}

void MainWindow::plotADC_voltage(quint64 count,
                                 float voltage)
{
    auto graph = ui->customPlot_adc->graph(0);

    graph->addData(count,voltage);

    quint64 lowerBound = 0;

    quint64 msec = 30000; //30 Sec plot

    if(count > msec)
    {
        lowerBound = count - msec;

        if(count % 1000 == 0)
        {
            graph->data()->removeBefore(lowerBound);
        }
    }

    ui->customPlot_adc->xAxis->setRange(
                count,msec,Qt::AlignRight);

    if(count % msec == 0)
    {
        ui->customPlot_adc->yAxis->setRange(0,5);
    }

    ui->customPlot_adc->replot(
                QCustomPlot::rpQueuedReplot);
}

void MainWindow::portStatus(const QString &data)
{
    if(data.startsWith("Serial object is not initialized/port not selected"))
    {
        QMessageBox::critical(this,"Port Error","Please Select Port Using Above Dropdown");
    }

    if(data.startsWith("Serial port ") && data.endsWith(" opened successfully at baud rate 115200"))
    {
        QMessageBox::information(this,"Success",data);
    }

    if(data.startsWith("Failed to open port"))
    {
        QMessageBox::critical(this,"Error",data);
    }

    ui->textEdit_rawBytes->append(data);
}

void MainWindow::showGuiData(const QByteArray &byteArrayData)
{
    QByteArray data = byteArrayData;

    if(data.startsWith("LED"))
    {
        blinkLabel(ui->label_led,400,"ACK");
    }
    else if(data.startsWith("PWM"))
    {
        blinkLabel(ui->label_pwm,400,"ACK");
    }
    else if(data.startsWith("ADC_ON"))
    {
        blinkLabel(ui->label_adc,400,"ACK");
    }
    else if(data.startsWith("DATA"))
    {
        QByteArray processed = data.mid(4);

        quint8 lsb =
                static_cast<quint8>(
                    processed[1]);

        quint8 msb =
                static_cast<quint8>(
                    processed[2]);

        quint16 adcValue =
                (msb << 8) | lsb;

        // Convert ADC -> Voltage
        float voltage =
                (adcValue * 5.0f) / 1023.0f;

        //        qDebug() << "ADC:"
        //                 << adcValue
        //                 << "Voltage:"
        //                 << QString::number(
        //                        voltage,
        //                        'f',
        //                        2)
        //                 << "V";


        // Convert Voltage -> Battery %
        int batteryPercent =
                qRound((voltage / ui->doubleSpinBox_refVoltage->value())
                       * 100.0f);

        if(count % 200 == 0)
        {
            if(batteryPercent > 100)
            {
                ui->label_percent->setStyleSheet("border : 2px solid black;"
                                                 "border-radius : 3px;"
                                                 "padding : 2px;"
                                                 "background-color : #e50d2a;");
            }
            else
            {
                ui->label_percent->setStyleSheet("border : 2px solid black;"
                                                 "border-radius : 3px;"
                                                 "padding : 2px;"
                                                 "background-color : #ceffd0;");
            }

            ui->label_percent->setText(QString("Battery %1% Voltage %2V")
                                       .arg(batteryPercent).arg(QString::number(voltage,'f',2)));
        }

        plotADC_voltage(count+=2,voltage);

    }
    else if(data.startsWith("ADC_OFF"))
    {
        blinkLabel(ui->label_adc,400,"ACK");
    }
    else if(data.startsWith("INTERRUPT"))
    {
        blinkLabel(ui->label_interrupt,400,"ACK");
    }
    else
    {
        qDebug()<<"Error #001";
    }
}


void MainWindow::on_pushButton_calibrateScreen_clicked()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QSize res = screen->size();

    QSettings settings("settings.ini", QSettings::IniFormat);

    QMessageBox::StandardButton choice = QMessageBox::question(
                this, "Calibrate Screen",
                "Do you want to enter custom screen details (width, height, diagonal) or reset to system default?",
                QMessageBox::Yes | QMessageBox::No);

    if (choice == QMessageBox::No) {
        // Reset to default
        settings.remove("Display/calibratedDPI");
        settings.remove("Display/width");
        settings.remove("Display/height");
        settings.remove("Display/diagonal");
        QMessageBox::information(this, "Calibration Removed",
                                 "Screen DPI reset to system default.\nRestart app to apply.");
        return;
    }

    // Custom input
    bool ok = false;
    int width = QInputDialog::getInt(this, "Screen Width",
                                     "Enter screen width (pixels):",
                                     res.width(), 100, 10000, 1, &ok);
    if (!ok) return;

    int height = QInputDialog::getInt(this, "Screen Height",
                                      "Enter screen height (pixels):",
                                      res.height(), 100, 10000, 1, &ok);
    if (!ok) return;

    double diagonalInches = QInputDialog::getDouble(
                this, "Screen Diagonal",
                "Enter screen diagonal size (in inches):",
                settings.value("Display/diagonal", 14.0).toDouble(), 3.0, 100.0, 1, &ok);
    if (!ok) return;

    // Calculate DPI
    double ppi = std::sqrt(width * width + height * height) / diagonalInches;

    // Save all values
    settings.setValue("Display/width", width);
    settings.setValue("Display/height", height);
    settings.setValue("Display/diagonal", diagonalInches);
    settings.setValue("Display/calibratedDPI", static_cast<int>(ppi));

    QMessageBox::information(this, "Calibration Done",
                             QString("Resolution: %1 x %2\nDiagonal: %3 in\nDPI set to %4.\nRestart app to apply.")
                             .arg(width).arg(height).arg(diagonalInches).arg(ppi, 0, 'f', 2));
}


void MainWindow::on_pushButton_led_clicked()
{
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    quint16 ledBlinks = ui->spinBox_led->value();
    quint8 duration = ui->spinBox_led_duration->value();

    QByteArray command;

    command.append(0xAA); //0
    command.append(0xBB); //1
    command.append(0xCC); //2 Header bytes

    command.append(0x01); //3 Cmd Id

    command.append(ledBlinks & 0xFF); //4
    command.append( (ledBlinks >> 8) & 0xFF); //5

    command.append(duration); //6

    command.append(static_cast<quint8>(0x00)); //7
    command.append(static_cast<quint8>(0x00)); //8 dummy bytes

    command.append(0xDD); //9
    command.append(0xEE); //10
    command.append(0xFF); //11 Footer bytes


    qDebug() << "led cmd sent : " + hexBytes(command);
    writeToNotes("led cmd sent : " + hexBytes(command));


    emit sendMsgId(0x01);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_PWM_clicked()
{
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    quint8 brightness = ui->horizontalSlider_pwm->value();
    if(brightness == 1)
    {
        brightness = 10;
    }
    else if(brightness == 2)
    {
        brightness = 40;
    }
    else if(brightness == 3)
    {
        brightness = 100;
    }
    else if(brightness == 4)
    {
        brightness = 170;
    }
    else if(brightness == 5)
    {
        brightness = 255;
    }
    else
    {
        qDebug()<<"Error #003";
    }

    float duration_float = ui->doubleSpinBox_pwm_delay->value();
    quint16 duration = qRound(duration_float * 1000);

    QByteArray command;

    command.append(0xAA); //0
    command.append(0xBB); //1
    command.append(0xCC); //2 Header bytes

    command.append(0x02); //3 Cmd Id

    command.append(brightness); //4
    command.append(duration & 0xFF); //5
    command.append( (duration >> 8) & 0xFF); //6

    if(ui->radioButton_pwmManual->isChecked())
    {
        command.append(0xAB); //7 mode byte
    }
    else if(ui->radioButton_pwmAutomatic->isChecked())
    {
        command.append(0xBC); //7 mode byte
    }
    else
    {
        qDebug()<<"Error #002";
    }

    command.append(static_cast<quint8>(0x00)); //8 dummy byte

    command.append(0xDD); //9
    command.append(0xEE); //10
    command.append(0xFF); //11 Footer bytes


    qDebug() << "pwm cmd sent : " + hexBytes(command);
    writeToNotes("pwm cmd sent : " + hexBytes(command));


    emit sendMsgId(0x02);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_readADC_clicked()
{
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    count = 0;

    QByteArray command;

    command.append(0xAA); //0
    command.append(0xBB); //1
    command.append(0xCC); //2 Header bytes

    command.append(0x03); //3 Cmd Id

    command.append(static_cast<quint8>(0x00)); //4
    command.append(static_cast<quint8>(0x00)); //5
    command.append(static_cast<quint8>(0x00)); //6
    command.append(static_cast<quint8>(0x00)); //7
    command.append(static_cast<quint8>(0x00)); //8 dummy bytes

    command.append(0xDD); //9
    command.append(0xEE); //10
    command.append(0xFF); //11 Footer bytes


    qDebug() << "ADC Read cmd sent : " + hexBytes(command);
    writeToNotes("ADC Read cmd sent : " + hexBytes(command));


    emit sendMsgId(0x03);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_stopADC_clicked()
{
    // Start the timeout timer
    responseTimer->start(2000); // 2 Sec timer

    QByteArray command;

    command.append(0xAA); //0
    command.append(0xBB); //1
    command.append(0xCC); //2 Header bytes

    command.append(0x04); //3 Cmd Id

    command.append(static_cast<quint8>(0x00)); //4
    command.append(static_cast<quint8>(0x00)); //5
    command.append(static_cast<quint8>(0x00)); //6
    command.append(static_cast<quint8>(0x00)); //7
    command.append(static_cast<quint8>(0x00)); //8 dummy bytes

    command.append(0xDD); //9
    command.append(0xEE); //10
    command.append(0xFF); //11 Footer bytes


    qDebug() << "Stop ADC cmd sent : " + hexBytes(command);
    writeToNotes("Stop ADC  cmd sent : " + hexBytes(command));


    emit sendMsgId(0x04);
    serialObj->writeData(command);
}

void MainWindow::on_pushButton_interrupt_clicked()
{
    emit sendMsgId(0x05);
}
