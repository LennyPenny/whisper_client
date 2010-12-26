/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>
   Copyright (C) 2009, Stefan Hacker <dd0t@users.sourceforge.net>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef _SERVER_H
#define _SERVER_H

#include "murmur_pch.h"
#include "Message.h"
#include "Timer.h"
#include "User.h"
#include "Connection.h"
#include "Net.h"
#include "ACL.h"
#include "DBus.h"

#ifdef USE_BONJOUR
#include "BonjourServer.h"
#endif

class Channel;
class PacketDataStream;

typedef QPair<quint32, quint16> Peer;

// Unfortunately, this needs to be "large enough" to hold
// enough frames to account for both short-term and
// long-term "maladjustments".

#define N_BANDWIDTH_SLOTS 360

struct BandwidthRecord {
	int iRecNum;
	int iSum;
	Timer qtFirst;
	unsigned short a_iBW[N_BANDWIDTH_SLOTS];
	Timer a_qtWhen[N_BANDWIDTH_SLOTS];

	BandwidthRecord();
	bool addFrame(int size, int maxpersec);
	int onlineSeconds() const;
	int idleSeconds() const;
	int bandwidth() const;
};

class LogEmitter : public QObject {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(LogEmitter)
	signals:
		void newLogEntry(const QString &msg);
	public:
		LogEmitter(QObject *parent = NULL);
		void addLogEntry(const QString &msg);
};

class SslServer : public QTcpServer {
	private:
		Q_OBJECT;
		Q_DISABLE_COPY(SslServer)
	protected:
		QList<QSslSocket *> qlSockets;
		void incomingConnection(int);
	public:
		QSslSocket *nextPendingSSLConnection();
		SslServer(QObject *parent = NULL);
};

class Server;

struct WhisperTarget {
	struct Channel {
		int iId;
		bool bChildren;
		bool bLinks;
		QString qsGroup;
	};
	QList<unsigned int> qlSessions;
	QList<WhisperTarget::Channel> qlChannels;
};

#define EXEC_QEVENT (QEvent::User + 959)

class ExecEvent : public QEvent {
		Q_DISABLE_COPY(ExecEvent);
	protected:
		boost::function<void ()> func;
	public:
		ExecEvent(boost::function<void ()>);
		void execute();
};

class ServerUser : public Connection, public User {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(ServerUser)
	protected:
		Server *s;
	public:
		enum State { Connected, Authenticated };
		State sState;
		operator const QString() const;

		double dUDPPingAvg, dUDPPingVar;
		double dTCPPingAvg, dTCPPingVar;
		quint64 uiUDPPackets, uiTCPPackets;

		unsigned int uiVersion;
		QString qsRelease;
		QString qsOS;
		QString qsOSVersion;

		std::string ssContext;
		QString qsIdentity;

		bool bVerified;
		QStringList qslEmail;

		HostAddress haAddress;
		bool bUdp;

		QList<int> qlCodecs;

		QStringList qslAccessTokens;

		QMap<int, WhisperTarget> qmTargets;
		typedef QPair<QSet<ServerUser *>, QSet<ServerUser *> > TargetCache;
		QMap<int, TargetCache> qmTargetCache;
		QMap<QString, QString> qmWhisperRedirect;

		int iLastPermissionCheck;
		QMap<int, unsigned int> qmPermissionSent;
#ifdef Q_OS_UNIX
		int sUdpSocket;
#else
		SOCKET sUdpSocket;
#endif
		BandwidthRecord bwr;
		struct sockaddr_storage saiUdpAddress;
		ServerUser(Server *parent, QSslSocket *socket);
};

class Server : public QThread {
	private:
		Q_OBJECT;
		Q_DISABLE_COPY(Server);
	protected:
		bool bRunning;

#ifdef USE_BONJOUR
		BonjourServer *bsRegistration;
#endif
		void startThread();
		void stopThread();

		void customEvent(QEvent *evt);
		// Former ServerParams
	public:
		QList<QHostAddress> qlBind;
		unsigned short usPort;
		int iTimeout;
		int iMaxBandwidth;
		int iMaxUsers;
		int iMaxUsersPerChannel;
		int iDefaultChan;
		int iMaxTextMessageLength;
		bool bAllowHTML;
		QString qsPassword;
		QString qsWelcomeText;
		bool bCertRequired;

		QString qsRegName;
		QString qsRegPassword;
		QString qsRegHost;
		QUrl qurlRegWeb;
		bool bBonjour;
		bool bAllowPing;

		QRegExp qrUserName;
		QRegExp qrChannelName;

		QList<QSslCertificate> qlCA;
		QSslCertificate qscCert;
		QSslKey qskKey;

		bool bValid;

		void readParams();

		int iCodecAlpha;
		int iCodecBeta;
		bool bPreferAlpha;
		void recheckCodecVersions();

#ifdef USE_BONJOUR
		void initBonjour();
		void removeBonjour();
#endif
		// Registration, implementation in Register.cpp
		QTimer qtTick;
		QHttp *http;
		QSslSocket *qssReg;
		void initRegister();
	public slots:
		void regSslError(const QList<QSslError> &);
		void done(bool);
		void update();
		void abort();

		// Certificate stuff, implemented partially in Cert.cpp
	public:
		static bool isKeyForCert(const QSslKey &key, const QSslCertificate &cert);
		void initializeCert();
		const QString getDigest() const;

	public slots:
		void newClient();
		void connectionClosed(QAbstractSocket::SocketError, const QString &);
		void sslError(const QList<QSslError> &);
		void message(unsigned int, const QByteArray &, ServerUser *cCon = NULL);
		void checkTimeout();
		void tcpTransmitData(QByteArray, unsigned int);
		void doSync(unsigned int);
		void encrypted();
		void udpActivated(int);
	signals:
		void reqSync(unsigned int);
		void tcpTransmit(QByteArray, unsigned int id);
	public:
		int iServerNum;
		QQueue<int> qqIds;
		QList<SslServer *> qlServer;
		QTimer *qtTimeout;

#ifdef Q_OS_UNIX
		int aiNotify[2];
		QList<int> qlUdpSocket;
#else
		HANDLE hNotify;
		QList<SOCKET> qlUdpSocket;
#endif
		quint32 uiVersionBlob;
		QList<QSocketNotifier *> qlUdpNotifier;

		QHash<unsigned int, ServerUser *> qhUsers;
		QHash<QPair<HostAddress, quint16>, ServerUser *> qhPeerUsers;
		QHash<HostAddress, QSet<ServerUser *> > qhHostUsers;
		QHash<unsigned int, Channel *> qhChannels;
		QReadWriteLock qrwlUsers;
		ChanACL::ACLCache acCache;
		QMutex qmCache;
		QHash<int, QString> qhUserNameCache;
		QHash<QString, int> qhUserIDCache;

		QList<Ban> qlBans;

		void processMsg(ServerUser *u, const char *data, int len);
		void sendMessage(ServerUser *u, const char *data, int len, QByteArray &cache, bool force = false);
		void run();

		bool validateChannelName(const QString &name);
		bool validateUserName(const QString &name);

		bool checkDecrypt(ServerUser *u, const char *encrypted, char *plain, unsigned int cryptlen);

		bool hasPermission(ServerUser *p, Channel *c, QFlags<ChanACL::Perm> perm);
		void sendClientPermission(ServerUser *u, Channel *c, bool updatelast = false);
		void flushClientPermissionCache(ServerUser *u, MumbleProto::PermissionQuery &mpqq);
		void clearACLCache(User *p = NULL);

		void sendProtoAll(const ::google::protobuf::Message &msg, unsigned int msgType);
		void sendProtoExcept(ServerUser *, const ::google::protobuf::Message &msg, unsigned int msgType);
		void sendProtoMessage(ServerUser *, const ::google::protobuf::Message &msg, unsigned int msgType);

#define MUMBLE_MH_MSG(x) \
		void sendAll(const MumbleProto:: x &msg) { sendProtoAll(msg, MessageHandler:: x); } \
		void sendExcept(ServerUser *u, const MumbleProto:: x &msg) { sendProtoExcept(u, msg, MessageHandler:: x); } \
		void sendMessage(ServerUser *u, const MumbleProto:: x &msg) { sendProtoMessage(u, msg, MessageHandler:: x); }

		MUMBLE_MH_ALL
#undef MUMBLE_MH_MSG

		void setLiveConf(const QString &key, const QString &value);

		QString addressToString(const QHostAddress &, unsigned short port);

		void log(const QString &);
		void log(ServerUser *u, const QString &);

		void removeChannel(int id);
		void removeChannel(Channel *c, Channel *dest = NULL);
		void userEnterChannel(User *u, Channel *c, bool quiet = false, bool ignoretemp = false);
		bool unregisterUser(int id);

		Server(int snum, QObject *parent = NULL);
		~Server();

		// RPC functions. Implementation in RPC.cpp
		void connectAuthenticator(QObject *p);
		void disconnectAuthenticator(QObject *p);
		void connectListener(QObject *p);
		void disconnectListener(QObject *p);
		void setTempGroups(const int userid, Channel *cChannel, const QStringList &groups);
	signals:
		void registerUserSig(int &, const QMap<int, QString> &);
		void unregisterUserSig(int &, int);
		void getRegisteredUsersSig(const QString &, QMap<int, QString > &);
		void getRegistrationSig(int &, int, QMap<int, QString> &);
		void authenticateSig(int &, QString &, const QList<QSslCertificate> &, const QString &, bool, const QString &);
		void setInfoSig(int &, int, const QMap<int, QString> &);
		void setTextureSig(int &, int, const QByteArray &);
		void idToNameSig(QString &, int);
		void nameToIdSig(int &, const QString &);
		void idToTextureSig(QByteArray &, int);

		void userStateChanged(const User *);
		void userConnected(const User *);
		void userDisconnected(const User *);
		void channelStateChanged(const Channel *);
		void channelCreated(const Channel *);
		void channelRemoved(const Channel *);

		void contextAction(const User *, const QString &, unsigned int, int);
	public:
		void setUserState(User *p, Channel *parent, bool mute, bool deaf, bool suppressed, const QString &comment = QString());
		bool setChannelState(Channel *c, Channel *parent, const QString &qsName, const QSet<Channel *> &links, const QString &desc = QString(), const int position = 0);
		void sendTextMessage(Channel *cChannel, ServerUser *pUser, bool tree, const QString &text);

		// Database / DBus functions. Implementation in ServerDB.cpp
		void initialize();
		int authenticate(QString &name, const QString &pw, const QStringList &emails = QStringList(), const QString &certhash = QString(), bool bStrongCert = false, const QList<QSslCertificate> & = QList<QSslCertificate>());
		Channel *addChannel(Channel *c, const QString &name, bool temporary = false, int position = 0);
		void removeChannelDB(const Channel *c);
		void readChannels(Channel *p = NULL);
		void readLinks();
		void updateChannel(const Channel *c);
		void readChannelPrivs(Channel *c);
		void setLastChannel(const User *u);
		int readLastChannel(int id);
		void dumpChannel(const Channel *c);
		int getUserID(const QString &name);
		QString getUserName(int id);
		QByteArray getUserTexture(int id);
		QMap<int, QString> getRegistration(int id);
		int registerUser(const QMap<int, QString> &info);
		bool unregisterUserDB(int id);
		QMap<int, QString > getRegisteredUsers(const QString &filter = QString());
		bool setInfo(int id, const QMap<int, QString> &info);
		bool setTexture(int id, const QByteArray &texture);
		bool isUserId(int id);
		void addLink(Channel *c, Channel *l);
		void removeLink(Channel *c, Channel *l);
		void getBans();
		void saveBans();
		QVariant getConf(const QString &key, QVariant def);
		void setConf(const QString &key, const QVariant &value);
		void dblog(const QString &str);

		// From msgHandler. Implementation in Messages.cpp
#define MUMBLE_MH_MSG(x) void msg##x(ServerUser *, MumbleProto:: x &);
		MUMBLE_MH_ALL
#undef MUMBLE_MH_MSG
};


#else
class Server;
#endif
