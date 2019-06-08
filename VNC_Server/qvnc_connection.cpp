#include "qvnc_connection.h"
#include "qvnc_client.h"
#include <QtEndian>
#include <QtNetwork/QTcpSocket>
#include <QScreen>
#include <QPixmap>
#include <QDebug>
#include <QApplication>
void QRfbRect::read(QTcpSocket *s)
{
    quint16 buf[4];
    s->read((char*)buf, 8);
    x = qFromBigEndian(buf[0]);
    y = qFromBigEndian(buf[1]);
    w = qFromBigEndian(buf[2]);
    h = qFromBigEndian(buf[3]);
}

void QRfbRect::write(QTcpSocket *s) const
{
    quint16 buf[4];
    buf[0] = qToBigEndian(x);
    buf[1] = qToBigEndian(y);
    buf[2] = qToBigEndian(w);
    buf[3] = qToBigEndian(h);
    s->write((char*)buf, 8);
}

void QRfbPixelFormat::read(QTcpSocket *s)
{
    char buf[16];
    s->read(buf, 16);
    bitsPerPixel = buf[0];
    depth = buf[1];
    bigEndian = buf[2];
    trueColor = buf[3];

    quint16 a = qFromBigEndian(*(quint16 *)(buf + 4));
    redBits = 0;
    while (a) { a >>= 1; redBits++; }

    a = qFromBigEndian(*(quint16 *)(buf + 6));
    greenBits = 0;
    while (a) { a >>= 1; greenBits++; }

    a = qFromBigEndian(*(quint16 *)(buf + 8));
    blueBits = 0;
    while (a) { a >>= 1; blueBits++; }

    redShift = buf[10];
    greenShift = buf[11];
    blueShift = buf[12];
}

void QRfbPixelFormat::write(QTcpSocket *s)
{
    char buf[16];
    buf[0] = bitsPerPixel;
    buf[1] = depth;
    buf[2] = bigEndian;
    buf[3] = trueColor;

    quint16 a = 0;
    for (int i = 0; i < redBits; i++) a = (a << 1) | 1;
    *(quint16 *)(buf + 4) = qToBigEndian(a);

    a = 0;
    for (int i = 0; i < greenBits; i++) a = (a << 1) | 1;
    *(quint16 *)(buf + 6) = qToBigEndian(a);

    a = 0;
    for (int i = 0; i < blueBits; i++) a = (a << 1) | 1;
    *(quint16 *)(buf + 8) = qToBigEndian(a);

    buf[10] = redShift;
    buf[11] = greenShift;
    buf[12] = blueShift;
    s->write(buf, 16);
}

void QRfbServerInit::setName(const char *n)
{
    delete[] name;
    name = new char [strlen(n) + 1];
    strcpy(name, n);
}

void QRfbServerInit::read(QTcpSocket *s)
{
    s->read((char *)&width, 2);
    width = qFromBigEndian(width);
    s->read((char *)&height, 2);
    height = qFromBigEndian(height);
    format.read(s);

    quint32 len;
    s->read((char *)&len, 4);
    len = qFromBigEndian(len);

    name = new char [len + 1];
    s->read(name, len);
    name[len] = '\0';
}

void QRfbServerInit::write(QTcpSocket *s)
{
    quint16 t = qToBigEndian(width);
    s->write((char *)&t, 2);
    t = qToBigEndian(height);
    s->write((char *)&t, 2);
    format.write(s);
    quint32 len = strlen(name);
    len = qToBigEndian(len);
    s->write((char *)&len, 4);
    s->write(name, strlen(name));
}


bool QRfbFrameBufferUpdateRequest::read(QTcpSocket *s)
{
    if (s->bytesAvailable() < 9)
        return false;

    s->read(&incremental, 1);
    rect.read(s);

    return true;
}

void QRfbRawEncoder::write()
{
    qDebug() << "QRfbRawEncoder::write()";
    QTcpSocket *socket = client->clientSocket();

    const char tmp[2] = { 0, 0 }; // msg type, padding
    socket->write(tmp, sizeof(tmp));

    const quint16 count = qToBigEndian(quint16(1)); //rectangle amount
    socket->write((char *)&count, sizeof(count));

    QScreen* const screen = client->server()->screen();
    const QSize screenSize = screen->geometry().size();

    const int bytesPerPixel = client->clientBytesPerPixel();
    const QPixmap originalPixMap = screen->grabWindow(0,0,0,screenSize.width(), screenSize.height());
    const QImage screenImage = originalPixMap.toImage();

    const QRfbRect rect(0,0,screenSize.width(),screenSize.height());
    rect.write(socket);

    const quint32 encoding = qToBigEndian(0); // raw encoding
    socket->write((char *)&encoding, sizeof(encoding));

    int linestep = screenImage.bytesPerLine();
    const uchar *screendata = screenImage.scanLine(rect.y) + rect.x * screenImage.depth() / 8;

    for (int i = 0; i < rect.h; ++i) {
        qApp->processEvents();
        socket->write((const char*)screendata, rect.w * bytesPerPixel);
        screendata += linestep;
    }
    socket->flush();

}
