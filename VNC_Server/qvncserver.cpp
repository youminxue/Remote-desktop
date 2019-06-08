#include "qvncserver.h"
#include <QDebug>
#include <QtEndian>
#include <QNetworkInterface>
#include "vnc_map.h"
QVNCServer::QVNCServer(QVNCScreen *screen) :
    qvnc_screen(screen)
{
    init(5900);
}

void QVNCServer::init(quint16 port)
{
    qDebug() << "QVNCServer::init" << port;

    handleMsg = false;
    client = 0;
    state = Unconnected;

    refreshRate = 25;
    //Check update is checking whether dirty map was changed
    //Timer is needed to maintain refreshRate and not to check updates very often
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, SIGNAL(timeout()), this, SLOT(checkUpdate()));

    //encoder = new QRfbRawEncoder(this);
    serverSocket = new QTcpServer(this);
    if (!serverSocket->listen(QHostAddress::Any, port))
        qDebug() << "QVNCServer could not connect:" << serverSocket->errorString();
    else
        qDebug("QVNCServer created on port %d", port);

    connect(serverSocket, SIGNAL(newConnection()), this, SLOT(newConnection()));
    fillNetworkInfo(port);
}

void QVNCServer::fillNetworkInfo(quint16 port)
{
    m_port = port;
    const QHostAddress &localhost = QHostAddress(QHostAddress::LocalHost);
    for (const QHostAddress &address: QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != localhost)
            m_ip = address.toString();
    }
}

void QVNCServer::checkUpdate()
{
    if (!wantUpdate)
            return;

    if (dirtyMap()->numDirty > 0) {
        if (encoder)
            encoder->write();
        wantUpdate = false;
    }
}

void QVNCServer::frameBufferUpdateRequest()
{
    qDebug() << "Reading frame buffer update request\n";
    QRfbFrameBufferUpdateRequest ev;


    if (ev.read(client)){
        qDebug() << "Incremental: " << quint8(ev.incremental);
        qDebug() << "Xcor: " << ev.rect.x;
        qDebug() << "Ycor: " << ev.rect.y;
        qDebug() << "Width: " << ev.rect.w;
        qDebug() << "Height: " << ev.rect.h;

        if (!ev.incremental) {
            //qvnc_screen->d_ptr->setDirty(r, true);
            //Mark the area from ev as missed by making screen dirty at that area
        }
        wantUpdate = true;
        checkUpdate();
        handleMsg = false;
    }
}
void QVNCServer::newConnection()
{
    qDebug() << "new Connection registered\n";
    //new connection registered from serverSocket
    if (client)
        delete client;

    client = serverSocket->nextPendingConnection();
    connect(client,SIGNAL(readyRead()),this,SLOT(readClient()));
    connect(client,SIGNAL(disconnected()),this,SLOT(discardClient()));

    handleMsg = false;
    wantUpdate = false;

    timer->start(1000/refreshRate); //send first screen image to the client
    dirtyMap()->reset();

    // send protocol version
    const char *proto = "RFB 003.008\n";
    client->write(proto, 12);
    state = Protocol;

    qDebug() << "RFB protocol has send\n";

    encoder = new QRfbRawEncoder(this);
}

void QVNCServer::readClient()
{
    switch (state) {
    case Protocol:
        //read Protocol from client
            if (client->bytesAvailable() >= 12) {
                char proto[13];
                client->read(proto, 12);
                proto[12] = '\0';
                qDebug("Client protocol version %s", proto);

                // No authentication
                //quint32 auth = htonl(1);
                quint32 auth = qToBigEndian(1);
                client->write((char *)&auth, sizeof(auth));
                state = Init;
            }
            break;
    case Init:
        if (client->bytesAvailable() >= 1) {
            quint8 shared;
            client->read((char *) &shared, 1);

            // Server Init msg
            QRfbServerInit sim;
            QRfbPixelFormat &format = sim.format;
            //screen depth = 32:
            format.bitsPerPixel = 32;
            format.depth = 32;
            format.bigEndian = 0;
            format.trueColor = true;
            format.redBits = 8;
            format.greenBits = 8;
            format.blueBits = 8;
            format.redShift = 16;
            format.greenShift = 8;
            format.blueShift = 0;

            sim.width = qvnc_screen->geometry().width();//qvnc_screen->geometry().width();
            sim.height = qvnc_screen->geometry().height();//qvnc_screen->geometry().height();
            sim.setName("Qt MAC VNC Server");
            sim.write(client);

            pixelFormat = format;
            state = Connected;
        }
        break;
    case Connected:
        do {
            if (!handleMsg) {
                client->read((char *)&msgType, 1);
                handleMsg = true;
            }
            if (handleMsg) {
                switch (msgType ) {
                case FramebufferUpdateRequest:
                    frameBufferUpdateRequest();
                    break;
                 default:
                    qDebug("Unknown message type: %d", (int)msgType);
                    handleMsg = false;
                }
            }
        } while (!handleMsg && client->bytesAvailable());
        break;
    default:
        break;
    }
}

void QVNCServer::discardClient()
{
    state = Unconnected;
    if(client)
    {
        disconnect(client,SIGNAL(readyRead()),this,SLOT(readClient()));
        disconnect(client,SIGNAL(disconnected()),this,SLOT(discardClient()));
        client->disconnect();
        client->deleteLater();
        //client = nullptr;
    }
    qDebug() << "Client has disconnected\n";
}

QVNCServer::~QVNCServer()
{
    /*disconnect(serverSocket, SIGNAL(newConnection()), this, SLOT(newConnection()));
    discardClient();
    serverSocket->close();
    delete serverSocket;
    delete encoder;*/
    delete  encoder;
    encoder = 0;
    delete client;
    client = 0;
}

//
void QVNCServer::setDirty()
{
    if (state == Connected && !timer->isActive() &&
            (dirtyMap()->numDirty > 0)) {
        timer->start();
    }
}

QImage *QVNCServer::screenImage() const
{
    return qvnc_screen->image();
}
