/*******************************************************************************************************
 DkNetwork.h
 Created on:	20.07.2011
 
 nomacs is a fast and small image viewer with the capability of synchronizing multiple instances
 
 Copyright (C) 2011-2013 Markus Diem <markus@nomacs.org>
 Copyright (C) 2011-2013 Stefan Fiel <stefan@nomacs.org>
 Copyright (C) 2011-2013 Florian Kleber <florian@nomacs.org>

 This file is part of nomacs.

 nomacs is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 nomacs is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>..

 *******************************************************************************************************/

#pragma  once

#define localTCPPortStart 45454
#define localTCPPortEnd 45484
#define lanUDPPortStart 28566
#define lanUDPPortEnd 28576
#define rcUDPPort 28565


#include <QtNetwork/QTcpServer>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QTcpSocket>
#include <QStringBuilder>
#include <QDir>
//#include <QWebView>
#include <QNetworkInterface>
#include <QList>
#include <QThread>
#include <QTimer>
#include <QHostInfo>
#include <QUrl>
#include <QNetworkReply>
#include <QNetworkAccessManager>
#include <QApplication>
#include <QDesktopWidget>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkCookieJar>
#include <QDesktopServices>
//#include <QtCore>

#include <math.h>


#ifdef WIN32
	#include <winsock2.h>	// needed since libraw 0.16
	#include <shlobj.h>
#endif

#ifdef WITH_UPNP
#include "DkUpnp.h"
#endif // WITH_UPNP


#include "DkConnection.h"

namespace nmc {

class DkPeer;
class DkLocalTcpServer;
class DkLANTcpServer;
class DkLANUdpSocket;



class DkPeerList {
	public:
		DkPeerList();
		bool addPeer(DkPeer peer);
		bool removePeer(quint16 peerId);
		bool setSynchronized(quint16 peerId, bool synchronized);
		bool setTitle(quint16 peerId, QString title);
		bool setShowInMenu(quint16 peerId, bool showInMenu);
		QList<DkPeer> getPeerList();
		DkPeer getPeerById(quint16 id);
		DkPeer getPeerByAddress(QHostAddress address, quint16 port);

		QList<DkPeer> getSynchronizedPeers();
		QList<quint16> getSynchronizedPeerServerPorts();
		QList<DkPeer> getActivePeers();

		DkPeer getPeerByServerport(quint16 port);
		bool alreadyConnectedTo(QHostAddress address, quint16 port);
		void print();

	private:
		QMultiHash<quint16, DkPeer> peerList;
};

class DkClientManager : public QThread {
	Q_OBJECT;
	public:
		DkClientManager(QString title, QObject* parent = 0);
		~DkClientManager();
		virtual	QList<DkPeer> getPeerList() = 0;

	signals:
		void receivedTransformation(QTransform transform, QTransform imgTransform, QPointF canvasSize);
		void receivedPosition(QRect position, bool opacity, bool overlaid);
		void receivedNewFile(qint16 op, QString filename);
		void receivedImage(QImage image);
		void receivedImageTitle( QString title);
		void sendInfoSignal(QString msg, int time = 3000);
		void sendGreetingMessage(QString title);
		void sendSynchronizeMessage();
		void sendDisableSynchronizeMessage();
		void sendNewTitleMessage(QString newtitle);
		void sendNewPositionMessage(QRect position, bool opacity, bool overlaid);
		void sendNewTransformMessage(QTransform transform, QTransform imgTransform, QPointF canvasSize);
		void sendNewFileMessage(qint16 op, QString filename);
		void sendNewImageMessage(QImage image, QString title);
		void sendNewUpcomingImageMessage(QString imageTitle);
		void sendGoodByeMessage();
		void synchronizedPeersListChanged(QList<quint16> newList);
		void updateConnectionSignal(QList<DkPeer> peers);
		
		void receivedQuit();

	public slots:
		virtual void synchronizeWith(quint16 peerId) = 0;
		virtual void synchronizeWithServerPort(quint16 port) = 0;
		virtual void stopSynchronizeWith(quint16 peerId) = 0;
		virtual void sendTitle(QString newTitle);
		void sendTransform(QTransform transform, QTransform imgTransform, QPointF canvasSize);
		void sendPosition(QRect newRect, bool overlaid);

		void sendNewFile(qint16 op, QString filename);
		virtual void sendNewImage(QImage image, QString title) {qDebug() << "DkClientManager: I am in the dummy method";}; // dummy
		void sendGoodByeToAll();

	protected slots:
		void newConnection( int socketDescriptor );
		virtual void connectionReadyForUse(quint16 peerId, QString title, DkConnection* connection);
		virtual void connectionSynchronized(QList<quint16> synchronizedPeersOfOtherClient, DkConnection* connection) = 0;
		virtual void connectionStopSynchronized(DkConnection* connection) = 0;
		virtual void connectionSentNewTitle(DkConnection* connection, QString newTitle);
		virtual void connectionReceivedTransformation(DkConnection* connection, QTransform transform, QTransform imgTransform, QPointF canvasSize);
		virtual void connectionReceivedPosition(DkConnection* connection, QRect rect, bool opacity, bool overlaid);
		virtual void connectionReceivedNewFile(DkConnection* connection, qint16 op, QString filename);
		void connectionShowStatusMessage(DkConnection* connection, QString msg);
		void connectionReceivedGoodBye(DkConnection* connection);
		void disconnected();

	protected:
		void removeConnection(DkConnection* connection);
		void connectConnection(DkConnection* connection);
		virtual DkConnection* createConnection() = 0;

		DkPeerList peerList;
		QString currentTitle;
		quint16 newPeerId;
		QList<DkConnection*> startUpConnections;

};

class DkLocalClientManager : public DkClientManager {
	Q_OBJECT;
	public:
		DkLocalClientManager(QString title, QObject* parent = 0);
		QList<DkPeer> getPeerList();
		quint16 getServerPort();
		void run();

	signals:
		void receivedQuit();
		void sendQuitMessage();

	public slots:
		void stopSynchronizeWith(quint16 peerId);
		void synchronizeWithServerPort(quint16 port);
		void synchronizeWith(quint16 peerId);
		void sendArrangeInstances(bool overlaid);
		void sendQuitMessageToPeers();

	private slots:
		void connectionSynchronized(QList<quint16> synchronizedPeersOfOtherClient, DkConnection* connection);
		virtual void connectionStopSynchronized(DkConnection* connection);
		void connectionReceivedQuit(); 

	private:
		DkLocalConnection* createConnection();
		void searchForOtherClients();

		DkLocalTcpServer* server;
};



class DkLANClientManager : public DkClientManager {
	Q_OBJECT;
	public:
		DkLANClientManager(QString title, QObject* parent = 0, quint16 updServerPortRangeStart = lanUDPPortStart, quint16 udpServerPortRangeEnd = lanUDPPortEnd);
		virtual ~DkLANClientManager(); 
		virtual QList<DkPeer> getPeerList();

	signals:
		void sendSwitchServerMessage(QHostAddress address, quint16 port);
		void serverPortChanged(quint16 port);

	public slots:
		void sendTitle(QString newTitle);
		virtual void synchronizeWithServerPort(quint16 port) {}; // dummy
		void stopSynchronizeWith(quint16 peerId = -1);
		void startServer(bool flag);
		void sendNewImage(QImage image, QString title);
		void synchronizeWith(quint16 peerId);

	protected:
		void connectConnection(DkConnection* connection);
		
		DkLANTcpServer* server;

	protected slots:
		virtual void connectionReadyForUse(quint16 peerServerPort, QString title, DkConnection* connection);

	private slots:
		void connectionReceivedNewImage(DkConnection* connection, QImage image, QString title);
		void startConnection(QHostAddress address, quint16 port, QString clientName);
		void sendStopSynchronizationToAll();
		
		virtual void connectionSynchronized(QList<quint16> synchronizedPeersOfOtherClient, DkConnection* connection);
		virtual void connectionStopSynchronized(DkConnection* connection);
		void connectionSentNewTitle(DkConnection* connection, QString newTitle);
		void connectionReceivedTransformation(DkConnection* connection, QTransform transform, QTransform imgTransform, QPointF canvasSize);
		void connectionReceivedPosition(DkConnection* connection, QRect rect, bool opacity, bool overlaid);
		void connectionReceivedNewFile(DkConnection* connection, qint16 op, QString filename);
		void connectionReceivedUpcomingImage(DkConnection* connection, QString imageTitle);
		void connectionReceivedSwitchServer(DkConnection* connection, QHostAddress address, quint16 port);
	private:
		virtual DkLANConnection* createConnection();

};

class DkRCClientManager : public DkLANClientManager {
	Q_OBJECT
	public:
		DkRCClientManager(QString title, QObject* parent = 0);
		QList<DkPeer> getPeerList();

	public slots:
		//void sendAskForPermission(); // todo: muss das ein slot sein?
		virtual void synchronizeWith(quint16 peerId);
		virtual void sendNewMode(int mode);

signals:
		void sendNewModeMessage(int mode);

	protected:
		void connectConnection(DkConnection* connection);

	private slots:
		void connectionSynchronized(QList<quint16> synchronizedPeersOfOtherClient, DkConnection* connection);
		void connectionReceivedPermission(DkConnection* connection, bool allowedToConnect);
		void connectionReceivedRCType(DkConnection* connection, int type);
		virtual void connectionReadyForUse(quint16 peerServerPort, QString title, DkConnection* connection);

	private:
		virtual DkRCConnection* createConnection();
		QHash<quint16, bool> permissionList;
};

class DkLocalTcpServer : public QTcpServer {
	Q_OBJECT;
	public:
		DkLocalTcpServer(QObject* parent = 0);

		quint16 startServer();

		quint16 startPort;
		quint16 endPort;
		

	signals:
		void serverReiceivedNewConnection(int DkDescriptor);


	protected:
		void incomingConnection(int socketDescriptor);
};

class DkLANTcpServer : public QTcpServer {
	Q_OBJECT;
	public:
		DkLANTcpServer(QObject* parent = 0, quint16 updServerPortRangeStart = lanUDPPortStart, quint16 udpServerPortRangeEnd = lanUDPPortEnd);

	signals:
		void serverReiceivedNewConnection(QHostAddress address , quint16 port , QString clientName);
		void serverReiceivedNewConnection(int DkDescriptor);
		void sendStopSynchronizationToAll();
		void sendNewClientBroadcast();

	public slots:
		void startServer(bool flag);

	private slots:
		void udpNewServerFound(QHostAddress address , quint16 port , QString clientName);
	
	protected:
		void incomingConnection(int socketDescriptor);
		DkLANUdpSocket* udpSocket;

	private:

};

class DkLANUdpSocket : public QUdpSocket {
	Q_OBJECT;

	public:
		DkLANUdpSocket(quint16 startPort = lanUDPPortStart, quint16 endPort = lanUDPPortEnd, QObject* parent = 0);
		void startBroadcast(quint16 tcpServerPort);
		void stopBroadcast();
		
	
	signals:
		void udpSocketNewServerOnline(QHostAddress address, quint16 port, QString clientName);
	
	public slots:
		void sendBroadcast();
		void sendNewClientBroadcast();

	private slots:
		void readBroadcast();

	private:
		bool isLocalHostAddress(const QHostAddress & address);
		quint16 startPort;
		quint16 endPort;
		quint16 serverPort;
		quint16 tcpServerPort;
		QList<QHostAddress> localIpAddresses;
		QTimer* broadcastTimer;
		bool broadcasting;
};

class DkPeer : public QObject{
	Q_OBJECT;
	
	public:
		DkPeer();
		DkPeer(quint16 port, quint16 peerId, QHostAddress hostAddress, quint16 peerServerPort, QString title, DkConnection* connection, bool sychronized = false, QString clientName="", bool showInMenu = false);
		
		DkPeer(const DkPeer& peer);
		~DkPeer();

		bool operator==(const DkPeer& peer) const {

			return localServerPort == peer.localServerPort && sychronized == peer.sychronized && title == peer.title && hostAddress == peer.hostAddress;
		}

		DkPeer& operator=(const DkPeer& peer) {
			this->peerId = peer.peerId;
			this->localServerPort = peer.localServerPort;
			this->peerServerPort = peer.peerServerPort;
			this->sychronized = peer.sychronized;
			this->title = peer.title;
			this->connection = peer.connection;
			this->hasChangedRecently = peer.hasChangedRecently;
			this->timer = peer.timer; 
			this->hostAddress = peer.hostAddress;
			this->clientName = peer.clientName;
			this->showInMenu = peer.showInMenu;

			connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeout()), Qt::UniqueConnection);
			//connect(timer, SIGNAL(timeout()), this, SLOT(timerTimeout()));

			return *this;
		}

		bool isActive() {return hasChangedRecently;};
		void setSynchronized(bool flag);
		bool isSynchronized() {return sychronized;};
		bool isLocal() {
			return hostAddress == QHostAddress::LocalHost;
		};

		quint16 peerId;
		quint16 localServerPort;
		quint16 peerServerPort;
		QHostAddress hostAddress;
		QString clientName;
		QString title;
		DkConnection* connection;
		QTimer* timer;
		bool showInMenu;

	signals:
		void sendGoodByeMessage();
	private slots:
		void timerTimeout() {hasChangedRecently = false;};

	private:
		bool hasChangedRecently;
		bool sychronized;
};

class DkNoMacs;

class DkManagerThread : public QThread {
	Q_OBJECT

public:
	DkManagerThread(DkNoMacs *parent = 0);
	~DkManagerThread() {
		
		if (clientManager) 
			delete clientManager;
		clientManager = 0;
	};
	
	virtual void connectClient();
	void run();

	QList<DkPeer> getPeerList() {

		if (!clientManager)
			return QList<DkPeer>();

		QMutexLocker locker(&mutex);
		return clientManager->getPeerList();	// critical section
	};

signals:
	void clientInitializedSignal();
	void syncWithSignal(quint16);
	void stopSyncWithSignal(quint16);
	void goodByeToAllSignal();

public slots:
	void synchronizeWith(quint16 peerId) {

		emit syncWithSignal(peerId);
	};
	void stopSynchronizeWith(quint16 peerId) {
		emit stopSyncWithSignal(peerId);
	};
	void sendGoodByeToAll() {
		emit goodByeToAllSignal();
	};

	void quit();


protected:
	virtual void createClient(QString title) = 0;
	
	DkClientManager* clientManager;
	DkNoMacs *parent;
	QMutex mutex;
};

class DkLocalManagerThread : public DkManagerThread {
	Q_OBJECT

public:
	DkLocalManagerThread(DkNoMacs* parent);
	void connectClient();

	qint16 getServerPort() {
		//TODO: da kann der hund begraben sein...
		QMutexLocker locker(&mutex);
		return dynamic_cast<DkLocalClientManager*>(clientManager)->getServerPort();
	};

protected:
	void createClient(QString title);

};

class DkLanManagerThread : public DkManagerThread {
	Q_OBJECT

public:
	DkLanManagerThread(DkNoMacs* parent);
	virtual void connectClient();

#ifdef WITH_UPNP
	QSharedPointer<DkUpnpControlPoint> upnpControlPoint;
	QSharedPointer<DkUpnpDeviceHost> upnpDeviceHost;
#endif // WITH_UPNP

signals:
	void startServerSignal(bool start);

public slots:
	virtual void startServer(bool start) {
		// re-send
		emit startServerSignal(start);
	};


protected:
	void createClient(QString title);



};

class DkRCManagerThread : public DkLanManagerThread {
	Q_OBJECT

public:
	DkRCManagerThread(DkNoMacs* parent);
	void connectClient();

public slots:
	void sendNewMode(int mode);

signals:
	void newModeSignal(int mode);

protected:
	void createClient(QString title);

};

class DkUpdater : public QObject {
	Q_OBJECT;

public:
	bool silent;
	
	DkUpdater();

public slots:
	void checkForUpdates();
	void replyFinished(QNetworkReply*);
	void replyError(QNetworkReply::NetworkError);
	void performUpdate();
	void downloadFinishedSlot(QNetworkReply* data);
	void updateDownloadProgress(qint64 received, qint64 total) { emit downloadProgress(received, total); };
	void cancelUpdate();

signals:
	void displayUpdateDialog(QString msg, QString title);
	void showUpdaterMessage(QString msg, QString title);
	void downloadFinished(QString filePath);
	void downloadProgress(qint64, qint64);

protected:
	void startDownload(QUrl downloadUrl);
	void downloadUpdate();
	QNetworkAccessManager accessManagerVersion;
	QNetworkAccessManager accessManagerSetup;
	QNetworkReply* reply;

	QNetworkCookieJar* cookie;

	QUrl nomacsSetupUrl;
	QString setupVersion;
	bool updateAborted;
};

class DkTranslationUpdater : public QObject {
	Q_OBJECT;

	public:
		DkTranslationUpdater();

	public slots:
		virtual void checkForUpdates();
		virtual void replyFinished(QNetworkReply*);

	signals:
		void translationUpdated();
		void showUpdaterMessage(QString, QString);
	private:
		QNetworkAccessManager accessManager;
		QNetworkReply* reply;
};

//// this code is based on code from: 
//// http://www.developer.nokia.com/Community/Wiki/Capturing_image_using_QML_Camera_and_uploading_to_Facebook
//class DkFacebook : QObject {
//	Q_OBJECT
//
//public:
//	DkFacebook() : QObject() {
//	
//		view = new QWebView();
//	
//	};
//
//public slots:
//	void login( const QString& scope ){
//		QUrl url("http://www.facebook.com/dialog/oauth");
//		url.addQueryItem("client_id", QApplication::applicationName());
//		url.addQueryItem("redirect_uri",
//			"https://www.facebook.com/connect/login_success.html");
//		url.addQueryItem("response_type","token");
//		url.addQueryItem("scope","read_stream,publish_stream");
//
//		//view = new QWebView();
//		view->load(url);
//		view->show();
//		qDebug() << "login should be loaded...";
//		connect(view,SIGNAL(loadFinished(bool)),this,SLOT(loginResponse(bool)));
//	}
//
//	void loginResponse(bool status) {
//		
//		qDebug() << "login responded...";
//		
//		QUrl url= view->url();
//		QString strUrl = url.toString();
//
//		int sIndex = strUrl.indexOf("access_token=");
//		int eIndex = strUrl.indexOf("&expires_in");
//		if( sIndex != -1 && eIndex != -1 ){
//			mAccessToken= strUrl.mid(sIndex, eIndex - sIndex);
//			mAccessToken = mAccessToken.remove("access_token=");
//			emit authStatus(mAccessToken);
//		}
//	}
//
//signals:
//	void authStatus(QString mAccessToken);
//
//protected:
//
//	QString mAccessToken;
//	QWebView* view;
//};



};

