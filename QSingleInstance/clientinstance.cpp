#include "clientinstance.h"
#include <QtEndian>
#include <QCoreApplication>

ClientInstance::ClientInstance(QLocalSocket *socket, QSingleInstancePrivate *parent) :
	QObject(parent),
	instance(parent),
	socket(socket),
	argSizeLeft(-1),
	argData()
{
	connect(this->socket, &QLocalSocket::readyRead, this, &ClientInstance::newData);
	connect(this->socket, SIGNAL(error(QLocalSocket::LocalSocketError)),
			this, SLOT(socketError(QLocalSocket::LocalSocketError)));
	connect(this->socket, &QLocalSocket::disconnected, this, &ClientInstance::deleteLater);
}

ClientInstance::~ClientInstance()
{
	if(this->instance->isRunning) {
		if(this->socket->isOpen())
			this->socket->close();
		this->socket->deleteLater();
	}
}

void ClientInstance::newData()
{
	if(this->argSizeLeft == -1) {
		if(this->socket->bytesAvailable() >= sizeof(qint32)) {
			qint64 read = this->socket->read((char*)&(this->argSizeLeft), sizeof(qint32));
			if(read != sizeof(qint32)) {
				qCWarning(logQSingleInstance, "Client lost. Client send invalid data");
				this->deleteLater();
				return;
			}
			this->argSizeLeft = qFromLittleEndian<qint32>(this->argSizeLeft);
			this->argData.reserve(this->argSizeLeft);
		}
	}
	if(this->argSizeLeft != -1) {
		this->argData += this->socket->readAll();
		if(this->argData.size() >= this->argSizeLeft) {
			QStringList arguments = QString::fromUtf8(this->argData).split(SPLIT_CHAR);
			this->socket->write(ACK);
			QTimer::singleShot(5000, this->socket, &QLocalSocket::disconnectFromServer);
			this->instance->newArgsReceived(arguments);
		}
	}
}

void ClientInstance::socketError(QLocalSocket::LocalSocketError)
{
	qpCWarning(logQSingleInstance, QStringLiteral("Failed to receive arguments from client with error \"%1\"")
			   .arg(this->socket->errorString()));
	this->deleteLater();
}

