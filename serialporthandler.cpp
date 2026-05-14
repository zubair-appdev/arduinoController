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
    serial->setBaudRate(9600);
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
        qDebug() << "Serial port "<<serial->portName()<<" opened successfully at baud rate 9600";
        emit portOpening("Serial port "+serial->portName()+" opened successfully at baud rate 9600");
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
    qDebug()<<"------------------------------------------------------------------------------------";
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

    qDebug()<<buffer.toHex()<<" Raw buffer data";
    qDebug()<<buffer.size()<<" :size";

    //Direct taking msgId from mainWindow
    quint8 msgId = id;
    //powerId to avoid that warning QByteRef calling out of bond error
    quint8 powerId = 0x00;

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
    if(msgId == 0x02)
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
