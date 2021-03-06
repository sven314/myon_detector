#include "tcpconnection.h"
#include "custom_io_operators.h" //remove after debug
#include <QtNetwork>
#include <sstream>

using namespace std; //remove after debug

const quint16 ping = 123;
const quint16 msgSig = 246;
const quint16 answPing = 231;
const quint16 fileSig = 142;
const quint16 nextPart = 143;
const quint16 quitConnection = 101;
const quint16 timeoutSig = 138;

TcpConnection::TcpConnection(QString newHostName, quint16 newPort, int newVerbose, int newTimeout,
                             int newPingInterval, QObject *parent)
    : QObject(parent)
{
    verbose= newVerbose;
    hostName=newHostName;
    port=newPort;
    timeout=newTimeout;
    pingInterval = newPingInterval;
}

void TcpConnection::makeConnection()
{
    std::stringstream str;
    str << "client tcpConnection reporting from thread " << this->thread();
    emit toConsole(QString::fromStdString(str.str()));
    tcpSocket = new QTcpSocket();
    in = new QDataStream();
    in->setDevice(tcpSocket);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &TcpConnection::onReadyRead);
    //connect(tcpSocket, &QTcpSocket::disconnected, this, &TcpConnection::disconnected);
    tcpSocket->connectToHost(hostName, port);
    firstConnection = time(NULL);
    lastConnection = firstConnection;
    if (!tcpSocket->waitForConnected(timeout)) {
        emit error(tcpSocket->error(), tcpSocket->errorString());
        return;
    }
    //startTimePulser();
}

void TcpConnection::closeConnection(){
    sendData(quitConnection,"");
    this->deleteLater();
    return;
}

void TcpConnection::onReadyRead(){
    if(!in){ return; }
    quint8 someCode;
    QByteArray block;
    in->startTransaction();
    *in >> block;
    //*in >> data;
    if (!in->commitTransaction()){
        return;
    }
    if (someCode == nextPart){
        lastConnection = time(NULL);
        sendFile();
    }
    // do something with data
    if (someCode == ping){
        if (verbose>3){
            emit toConsole("received ping, sending answerping");
        }
        sendData(answPing, "");
        lastConnection = time(NULL);
    }
    if (someCode == answPing){
        if(verbose>3){
            emit toConsole("received answerping");
        }
        lastConnection = time(NULL);
    }
    if (someCode == timeoutSig){
        emit connectionTimeout(hostName,port,(quint32)time(NULL),(quint32)(time(NULL)-firstConnection));
    }
}

/*
void TcpConnection::onPosixTerminate(){
    emit posixTerminate();
}
*/

bool TcpConnection::sendData(const quint16 someCode, QString someData){
    if (!tcpSocket) {
        emit toConsole("in client => tcpConnection:\ntcpSocket not instantiated");
        return false;
    }
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << someCode;
    out << someData;
    // for qt version < 5.7:
    // send the size of the string so that receiver knows when
    // all data has been successfully received
    /*out.device()->seek(0);
    out << (quint16)(block.size() - sizeof(quint16));
    */
    tcpSocket->write(block);
    for(int i = 0; i<3; i++){
        if(!tcpSocket->state()==QTcpSocket::UnconnectedState){
            if(!tcpSocket->waitForBytesWritten(timeout)){
                emit toConsole("wait for bytes written timeout");
                return false;
            }
            return true;
        }else{
            delay(100);
        }
    }
    emit toConsole("tcp unconnected state before wait for bytes written");
    return false;
}

bool TcpConnection::sendMsg(QString message){
    if(!sendData(msgSig,message)){
        emit toConsole("unable to send message");
        return false;
    }
    return true;
}


bool TcpConnection::sendFile(QString fileName){
    if (!tcpSocket) {
        emit toConsole("in client => tcpConnection:\ntcpSocket not instantiated");
        return false;
    }
    if (myFile){
        myFile->close();
        delete myFile;
    }
    if (!fileName.isEmpty()){
        myFile = new QFile(QCoreApplication::applicationDirPath()+"/"+fileName);
        if (!myFile->open(QIODevice::ReadOnly)) {
            emit toConsole("Could not open file, maybe no permission or file does not exist.");
            return false;
        }
    }else{
        emit toConsole("empty Filename error");
        return false;
    }
    QByteArray fileNameBlock;
    QDataStream out(&fileNameBlock, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_4_0);
    out << fileSig;
    out << fileCounter;
    out << fileName;
    tcpSocket->write(fileNameBlock);
    if(!tcpSocket->waitForBytesWritten(timeout)){
        return false;
    }
    while(true){
        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        fileCounter++;
        out << fileSig;
        out << fileCounter;
        out <<myFile->read(1024);
        //cout << block.toStdString()<<endl;
        tcpSocket->write(block);
        if(!tcpSocket->waitForBytesWritten(timeout)){
            emit toConsole("wait for bytes written timeout while sending file");
            return false;
        }
        if(myFile->atEnd()){
            break;
        }
    }
    return true;
}
/*
bool TcpConnection::sendFile(QString fileName){
    if (!tcpSocket) {
        emit toConsole("in client => tcpConnection:\ntcpSocket not instantiated");
        return false;
    }
    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    if(fileCounter==0){
        if (myFile){
            bool retAfterclose = false;
            if (myFile->atEnd()){
                retAfterclose = true;
            }
            myFile->close();
            delete myFile;
            if (retAfterclose){
                return true;
            }
        }
        if (!fileName.isEmpty()){
            myFile = new QFile(QCoreApplication::applicationDirPath()+"/"+fileName);
            if (!myFile->open(QIODevice::ReadOnly)) {
                emit toConsole("Could not open file, maybe no permission or file does not exist.");
                return false;
            }
        }else{
            emit toConsole("empty Filename error");
            return false;
        }
        out.setVersion(QDataStream::Qt_4_0);
        out << fileSig;
        out << fileCounter;
        out << fileName;
        tcpSocket->write(block);
        if(!tcpSocket->waitForBytesWritten(timeout)){
            return false;
        }
    }else{
        out << fileSig;
        out << fileCounter;
        out << myFile->read(8*32768);
        //cout << block.toStdString()<<endl;
        tcpSocket->write(block);
        if(!tcpSocket->waitForBytesWritten(timeout)){
            emit toConsole("wait for bytes written timeout while sending file");
            return false;
        }
    }
    fileCounter++;
    return true;
}
*/

void TcpConnection::onTimePulse(){
    if (fabs(time(NULL)-lastConnection)>timeout/1000){

        // hier einfügen, was passiert, wenn host nicht auf ping antwortet
        quint32 timeoutTime = (quint32)time(NULL);
        quint32 connectionDuration = (quint32)(time(NULL)-firstConnection);
        emit connectionTimeout(hostName, port, timeoutTime, connectionDuration);
        emit toConsole("until deletelater");
        this->deleteLater();
        return;
    }
    if (tcpSocket/*&&tcpSocket->state()!=QTcpSocket::UnconnectedState*/){
        if (verbose>3){
            emit toConsole("sending ping");
        }
        sendData(ping, "");
    }
}


void TcpConnection::startTimePulser()
{
    t = new QTimer();
    t->setInterval(pingInterval);
    connect(t, &QTimer::timeout, this, &TcpConnection::onTimePulse);
    t->start();
}

void TcpConnection::delay(int millisecondsWait)
{
    QEventLoop loop;
    QTimer delay;
    //connect(this, &TcpConnection::posixTerminate, &loop, &QEventLoop::quit);
    //connect(tcpSocket, &QTcpSocket::disconnected, &loop, &QEventLoop::quit);
    delay.connect(&delay, &QTimer::timeout, &loop, &QEventLoop::quit);
    delay.start(millisecondsWait);
    loop.exec();
}

//void disconnected(){}
