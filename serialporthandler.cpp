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
    if(id != 0x03)
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

    if(msgId != 0x03)
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
        //qDebug() << "msgId:" << hex << msgId;

        powerId = 0x03;
        // -------- ACK Packet --------
        if(buffer.size() >= 3 &&
           static_cast<unsigned char>(buffer[0]) == 0x41 &&
           static_cast<unsigned char>(buffer[1]) == 0x42 &&
           static_cast<unsigned char>(buffer[2]) == 0x43)
        {
            powerId = 0x04;

            executeWriteToNotes(
                "ADC ACK received: "
                + buffer.left(3).toHex()
            );

            ResponseData = "ADC_1_TIME"+buffer;
            // Remove ACK from buffer
            buffer.remove(0, 3);
        }

        // -------- ADC Streaming Packet --------
        while(buffer.size() >= 4)
        {
            // Validate packet
            if(static_cast<unsigned char>(buffer[0]) == 0xAA &&
               static_cast<unsigned char>(buffer[3]) == 0xFF)
            {
                QByteArray adcPacket =
                        buffer.left(4);

                ResponseData = adcPacket;

                emit guiDisplay("DATA"+ResponseData);

                // Remove processed packet
                buffer.remove(0, 4);
            }
            else
            {
                // Bad synchronization
                executeWriteToNotes(
                    "Invalid ADC byte dropped: "
                    + buffer.left(1).toHex()
                );

                // Shift buffer by one byte
                buffer.remove(0,1);
            }
        }
    }
    else if(msgId == 0x04)
    {
        qDebug() << "msgId:" << hex << msgId;

        powerId = 0x04;

        // Keep searching until ACK found
        while(buffer.size() >= 3)
        {
            // ACK Found
            if(static_cast<unsigned char>(buffer[0]) == 0x41 &&
               static_cast<unsigned char>(buffer[1]) == 0x42 &&
               static_cast<unsigned char>(buffer[2]) == 0x43)
            {

                ResponseData =
                        buffer.left(3);

                executeWriteToNotes(
                    "Stop ADC ACK received bytes: "
                    + ResponseData.toHex()
                );

                // Remove ACK
                buffer.remove(0, 3);

                break;
            }

            // Ignore leftover ADC packet
            else if(buffer.size() >= 4 &&
                    static_cast<unsigned char>(buffer[0]) == 0xAA &&
                    static_cast<unsigned char>(buffer[3]) == 0xFF)
            {
                QByteArray adcPacket =
                        buffer.left(4);

                executeWriteToNotes(
                    "Ignored leftover ADC packet: "
                    + adcPacket.toHex()
                );

                // Remove ADC packet
                buffer.remove(0,4);
            }
            else
            {
                // Unknown garbage byte
                executeWriteToNotes(
                    "Dropped invalid byte: "
                    + buffer.left(1).toHex()
                );

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
        if(ResponseData.contains("ADC_1_TIME"))
        {
            emit guiDisplay("ADC_ON"+ResponseData);
        }
    }
        break;

    case 0x04:
    {
        //adc stop response
        emit guiDisplay("ADC_OFF"+ResponseData);
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
