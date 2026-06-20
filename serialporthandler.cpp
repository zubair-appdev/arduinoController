#include "serialporthandler.h"

serialPortHandler::serialPortHandler(QObject *parent) : QObject(parent)
{
    serial = new QSerialPort;
    connect(serial, &QSerialPort::readyRead, this, &serialPortHandler::readData);

}

serialPortHandler::~serialPortHandler()
{
    delete serial;
}

QStringList serialPortHandler::availablePorts()
{
    QStringList ports;
    foreach(const QSerialPortInfo &info, QSerialPortInfo::availablePorts())
    {
        ports<<info.portName();
    }
    return ports;
}

void serialPortHandler::setPORTNAME(const QString &portName)
{
    buffer.clear();

    if(serial->isOpen())
    {
        serial->close();
    }

    serial->setPortName(portName);
    serial->setBaudRate(115200);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);


    if(!serial->open(QIODevice::ReadWrite))
    {
        qDebug()<<"Failed to open port"<<serial->portName();
        emit portOpening("Failed to open port "+serial->portName());
    }
    else
    {
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 115200";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 115200");
    }
}

float serialPortHandler::convertBytesToFloat(const QByteArray &data)
{
    if(data.size() != 4)
    {
        qDebug()<<"Insuffient data to convert into float";
    }

    // Assuming little-endian format
    QByteArray floatBytes = data;
    std::reverse(floatBytes.begin(), floatBytes.end()); // Convert to big-endian if needed

    float value;
    memcpy(&value, floatBytes.constData(), sizeof(float));
    return value;
}

quint8 serialPortHandler::chkSum(const QByteArray &data)
{
    // Ensure the QByteArray has at least two bytes (data + checksum)
    if (data.size() < 2) {
        throw std::invalid_argument("Data size must be at least 2 for checksum calculation.");
    }

    // Initialize checksum to 0
    quint8 checksum = 0;

    // Perform XOR for all bytes except the last one
    for (int i = 0; i < data.size() - 1; ++i) {
        checksum ^= static_cast<quint8>(data[i]);
    }

    qDebug()<<hex<<checksum<<"DEBUG_CHKSUM";
    return checksum;
}

QString serialPortHandler::hexBytesSerial(QByteArray &cmd)
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

void serialPortHandler::readData()
{
    if(id == 0x03 || id == 0x06)
    {
        // do nothing
    }
    else
    {
        qDebug()<<"------------------------------------------------------------------------------------";

    }

    QByteArray ResponseData;

    // Read data from the serial port
    if (serial->bytesAvailable() == 0) {
        qWarning() << "No bytes available from serial port";
        return;  // Early return if no data is available
    }

    // Create a QMutexLocker to manage the mutex
    QMutexLocker locker(&bufferMutex); // Lock the mutex


    if (serial->bytesAvailable() < std::numeric_limits<int>::max()) {
        buffer.append(serial->readAll()); // Append only if it won't exceed max size
        if (!buffer.isEmpty()) {
            emit dataReceived(); // Signal data has been received
        }
    } else {
        qWarning() << "Attempt to append too much data to QByteArray!";
        return;
    }

    //Direct taking msgId from mainWindow
    quint8 msgId = id;
    //powerId to avoid that warning QByteRef calling out of bond error
    quint8 powerId = 0x00;

    if(msgId != 0x03 && msgId != 0x05 && msgId != 0x06)
    {
        qDebug()<<buffer.toHex()<<" Raw buffer data";
        qDebug()<<buffer.size()<<" :size";
    }

    if(msgId == 0x01)
    {
        qDebug() << "msgId:" <<hex<<msgId;

        if(buffer.size() == 3
                && static_cast<unsigned char>(buffer[0]) == 0x41
                && static_cast<unsigned char>(buffer[1]) == 0x42
                && static_cast<unsigned char>(buffer[2]) == 0x43)
        {
            powerId = 0x01;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("Led cmd received bytes: "+ResponseData.toHex());
        }
        else
        {
            executeWriteToNotes("Required 3 bytes Received bytes: "+QString::number(buffer.size())
                                +" "+buffer.toHex());
        }

    }
    else if(msgId == 0x02)
    {
        qDebug() << "msgId:" <<hex<<msgId;

        if(buffer.size() == 3
                && static_cast<unsigned char>(buffer[0]) == 0x41
                && static_cast<unsigned char>(buffer[1]) == 0x42
                && static_cast<unsigned char>(buffer[2]) == 0x43)
        {
            powerId = 0x02;
            ResponseData = buffer;
            buffer.clear();
            executeWriteToNotes("PWM cmd received bytes: "+ResponseData.toHex());
        }
        else
        {
            executeWriteToNotes("Required 3 bytes Received bytes: "+QString::number(buffer.size())
                                +" "+buffer.toHex());
        }

    }
    else if(msgId == 0x03)
    {

        if(buffer.size() <= 3)
        {
            qDebug()<<"### SPECIAL CONDITION ###";
            qDebug()<<buffer.toHex();
        }
        // -------- ADC START ACK --------
        if(buffer.size() >= 3 &&
           static_cast<unsigned char>(buffer[0]) == 0x41 &&
           static_cast<unsigned char>(buffer[1]) == 0x42 &&
           static_cast<unsigned char>(buffer[2]) == 0x43)
        {
            powerId = 0x03;

            executeWriteToNotes(
                "ADC ACK received: "
                + buffer.left(3).toHex());

            ResponseData = "ADC_1_TIME" + buffer;
            emit guiDisplay("ADC_ON"+ResponseData);

            buffer.remove(0, 3);
        }

        // -------- INTERRUPT ACK --------
        while(buffer.size() >= 5)
        {
            if(static_cast<unsigned char>(buffer[0]) == 0x55 &&
               static_cast<unsigned char>(buffer[1]) == 0x66 &&
               static_cast<unsigned char>(buffer[2]) == 0x77 &&
               static_cast<unsigned char>(buffer[3]) == 0x88 &&
               static_cast<unsigned char>(buffer[4]) == 0x99)
            {
                powerId = 0x05;

                ResponseData = buffer.left(5);

                executeWriteToNotes(
                    "INTERRUPT ACK received bytes: "
                    + ResponseData.toHex());

                emit guiDisplay(
                    "INTERRUPT" + ResponseData);

                buffer.remove(0,5);
            }
            else
            {
                powerId = 0x05;
                break;
            }
        }

        // -------- ADC STREAM --------
        while(buffer.size() >= 4)
        {
            powerId = 0x03;

            // ADC Packet
            if(static_cast<unsigned char>(buffer[0]) == 0xAA &&
               static_cast<unsigned char>(buffer[3]) == 0xFF)
            {
                QByteArray adcPacket =
                        buffer.left(4);

                ResponseData = adcPacket;

                emit guiDisplay(
                    "DATA" + ResponseData);

                buffer.remove(0,4);
            }

            // INTERRUPT Packet sneaked into stream
            else if(buffer.size() >= 5 &&
                    static_cast<unsigned char>(buffer[0]) == 0x55 &&
                    static_cast<unsigned char>(buffer[1]) == 0x66 &&
                    static_cast<unsigned char>(buffer[2]) == 0x77 &&
                    static_cast<unsigned char>(buffer[3]) == 0x88 &&
                    static_cast<unsigned char>(buffer[4]) == 0x99)
            {
                powerId = 0x05;

                ResponseData = buffer.left(5);

                executeWriteToNotes(
                    "INTERRUPT ACK SNEAKED received bytes: "
                    + ResponseData.toHex());

                emit guiDisplay(
                    "INTERRUPT" + ResponseData);

                buffer.remove(0,5);
            }

            // Garbage
            else
            {
                executeWriteToNotes(
                    "Invalid ADC byte dropped: "
                    + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }

    }
    else if(msgId == 0x04)
    {
        qDebug() << "msgId:" << hex << msgId;

        // Keep searching until ACK found
        while(buffer.size() >= 3)
        {
            // ADC STOP ACK
            if(buffer.size() >= 3 &&
               static_cast<unsigned char>(buffer[0]) == 0x41 &&
               static_cast<unsigned char>(buffer[1]) == 0x42 &&
               static_cast<unsigned char>(buffer[2]) == 0x43)
            {
                powerId = 0x04;

                ResponseData = buffer.left(3);

                executeWriteToNotes(
                    "Stop ADC ACK received bytes: "
                    + ResponseData.toHex());

                buffer.remove(0,3);

                break;
            }

            // Leftover ADC packet
            else if(buffer.size() >= 4 &&
                    static_cast<unsigned char>(buffer[0]) == 0xAA &&
                    static_cast<unsigned char>(buffer[3]) == 0xFF)
            {
                QByteArray adcPacket = buffer.left(4);

                executeWriteToNotes(
                    "Ignored leftover ADC packet: "
                    + adcPacket.toHex());

                buffer.remove(0,4);
            }

            // Garbage
            else
            {
                executeWriteToNotes(
                    "Dropped invalid byte: "
                    + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }
    }
    else if(msgId == 0x05)
    {
        qDebug() << "msgId:" << hex << msgId;

        powerId = 0x05;

        while(buffer.size() >= 5)
        {
            if(static_cast<unsigned char>(buffer[0]) == 0x55 &&
               static_cast<unsigned char>(buffer[1]) == 0x66 &&
               static_cast<unsigned char>(buffer[2]) == 0x77 &&
               static_cast<unsigned char>(buffer[3]) == 0x88 &&
               static_cast<unsigned char>(buffer[4]) == 0x99)
            {
                ResponseData = buffer.left(5);

                executeWriteToNotes(
                    "External Interrupt received bytes: "
                    + ResponseData.toHex());

                emit guiDisplay(
                    "INTERRUPT" + ResponseData);

                // Remove processed packet
                buffer.remove(0, 5);
            }
            else
            {
                executeWriteToNotes(
                    "Dropped invalid byte: "
                    + buffer.left(1).toHex());

                // Resync
                buffer.remove(0, 1);
            }
        }
    }
    else if(msgId == 0x06)
    {
        // -------- ACK --------
        if(buffer.size() >= 3 &&
           static_cast<unsigned char>(buffer[0]) == 0x41 &&
           static_cast<unsigned char>(buffer[1]) == 0x42 &&
           static_cast<unsigned char>(buffer[2]) == 0x43)
        {
            powerId = 0x06;

            ResponseData = buffer.left(3);

            executeWriteToNotes(
                "Hardware Timer ACK received: "
                + ResponseData.toHex());

            emit guiDisplay(
                "HT_ON" + ResponseData);

            buffer.remove(0,3);
        }

        // -------- TIMER PACKETS --------
        while(buffer.size() >= 4)
        {
            if(static_cast<unsigned char>(buffer[0]) == 0x11 &&
               static_cast<unsigned char>(buffer[1]) == 0x22 &&
               static_cast<unsigned char>(buffer[2]) == 0x33 &&
               static_cast<unsigned char>(buffer[3]) == 0x44)
            {
                powerId = 0x06;

                ResponseData =
                        buffer.left(4);

                emit guiDisplay(
                    "TIMER_PACKET" +
                    ResponseData);

                buffer.remove(0,4);
            }
            else
            {
                executeWriteToNotes(
                    "Invalid Timer byte dropped: "
                    + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }
    }
    else if(msgId == 0x07)
    {
        qDebug() << "msgId:" <<hex<<msgId;

        // Keep searching until ACK found
        while(buffer.size() >= 3)
        {
            // Hardware Timer STOP ACK
            if(buffer.size() >= 3 &&
               static_cast<unsigned char>(buffer[0]) == 0x41 &&
               static_cast<unsigned char>(buffer[1]) == 0x42 &&
               static_cast<unsigned char>(buffer[2]) == 0x43)
            {
                powerId = 0x07;

                ResponseData = buffer.left(3);

                executeWriteToNotes(
                    "Stop Hardware Timer ACK received bytes: "
                    + ResponseData.toHex());

                buffer.remove(0,3);

                break;
            }

            // Leftover Timer packet
            else if(buffer.size() >= 4 &&
                    static_cast<unsigned char>(buffer[0]) == 0x11 &&
                    static_cast<unsigned char>(buffer[3]) == 0x44)
            {
                QByteArray TimerPacket = buffer.left(4);

                executeWriteToNotes(
                    "Ignored leftover Timer packet: "
                    + TimerPacket.toHex());

                buffer.remove(0,4);
            }

            // Garbage
            else
            {
                executeWriteToNotes(
                    "Dropped invalid Timer  byte: "
                    + buffer.left(1).toHex());

                buffer.remove(0,1);
            }
        }

    }
    else
    {
        //do nothing
        qDebug()<<"do nothing not a specified size/unknown msgId";
        executeWriteToNotes("Fatal Error 404");
    }

    switch(powerId)
    {
    case 0x01:
    {
        //led response
        emit guiDisplay("LED"+ResponseData);
    }
        break;

    case 0x02:
    {
        //pwm response
        emit guiDisplay("PWM"+ResponseData);
    }
        break;

    case 0x03:
    {
        //adc read response

       // do nothing
    }
        break;

    case 0x04:
    {
        //adc stop response
        emit guiDisplay("ADC_OFF"+ResponseData);
    }
        break;

    case 0x05:
    {
        //Interrupt stop response
        emit guiDisplay("INTERRUPT"+ResponseData);
    }
        break;

    case 0x06:
    {
        //do nothing
    }
        break;

    case 0x07:
    {
        //Hardware Timer OFF response
        emit guiDisplay("HT_OFF"+ResponseData);
    }
        break;

    default:
    {
        qDebug() << "Unknown powerId: " <<hex << powerId << " with data: " << ResponseData.size();

    }

    }

}

void serialPortHandler::recvMsgId(quint8 id)
{
    qDebug() << "Received id:" <<hex<< id;
    this->id = id;
    buffer.clear();
}
