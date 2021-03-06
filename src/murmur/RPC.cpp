/* Copyright (C) 2005-2010, Thorvald Natvig <thorvald@natvig.com>

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

#include "Server.h"
#include "User.h"
#include "Channel.h"
#include "Group.h"
#include "Meta.h"
#include "ServerDB.h"
#include "Version.h"

void Server::setUserState(User *pUser, Channel *cChannel, bool mute, bool deaf, bool suppressed, const QString &comment) {
	bool changed = false;

	if (deaf)
		mute = true;
	if (! mute)
		deaf = false;

	MumbleProto::UserState mpus;
	mpus.set_session(pUser->uiSession);
	if (mute != pUser->bMute) {
		changed = true;
		mpus.set_mute(mute);
	}
	if (deaf != pUser->bDeaf) {
		changed = true;
		mpus.set_deaf(deaf);
	}
	if (suppressed != pUser->bSuppress) {
		changed = true;
		mpus.set_suppress(suppressed);
	}
	if (comment != pUser->qsComment) {
		changed = true;
		mpus.set_comment(u8(comment));
		if (pUser->iId >= 0) {
			QMap<int, QString> info;
			info.insert(ServerDB::User_Comment, comment);
			setInfo(pUser->iId, info);
		}
	}

	pUser->bDeaf = deaf;
	pUser->bMute = mute;
	pUser->bSuppress = suppressed;
	pUser->qsComment = comment;

	if (cChannel != pUser->cChannel) {
		changed = true;
		mpus.set_channel_id(cChannel->iId);
		userEnterChannel(pUser, cChannel);
	}

	if (changed) {
		sendAll(mpus);
		emit userStateChanged(pUser);
	}
}

bool Server::setChannelState(Channel *cChannel, Channel *cParent, const QString &qsName, const QSet<Channel *> &links, const QString &desc, const int position) {
	bool changed = false;
	bool updated = false;

	MumbleProto::ChannelState mpcs;
	mpcs.set_channel_id(cChannel->iId);

	if (cChannel->qsName != qsName) {
		cChannel->qsName = qsName;
		mpcs.set_name(u8(qsName));
		updated = true;
		changed = true;
	}

	if ((cParent != cChannel) && (cParent != cChannel->cParent)) {
		Channel *p = cParent;
		while (p) {
			if (p == cChannel)
				return false;
			p = p->cParent;
		}

		cChannel->cParent->removeChannel(cChannel);
		cParent->addChannel(cChannel);

		mpcs.set_parent(cParent->iId);

		updated = true;
		changed = true;
	}

	const QSet<Channel *> &oldset = cChannel->qsPermLinks;

	if (links != oldset) {
		// Remove
		foreach(Channel *l, links) {
			if (! links.contains(l)) {
				removeLink(cChannel, l);
				mpcs.add_links_remove(l->iId);
			}
		}

		// Add
		foreach(Channel *l, links) {
			if (! oldset.contains(l)) {
				addLink(cChannel, l);
				mpcs.add_links_add(l->iId);
			}
		}

		changed = true;
	}

	if (position != cChannel->iPosition) {
		changed = true;
		updated = true;
		cChannel->iPosition = position;
		mpcs.set_position(position);
	}

	if (! desc.isNull() && desc != cChannel->qsDesc) {
		updated = true;
		changed = true;
		cChannel->qsDesc = desc;
		mpcs.set_description(u8(desc));
	}

	if (updated)
		updateChannel(cChannel);
	if (changed) {
		sendAll(mpcs);
		emit channelStateChanged(cChannel);
	}

	return true;
}

void Server::sendTextMessage(Channel *cChannel, ServerUser *pUser, bool tree, const QString &text) {
	MumbleProto::TextMessage mptm;
	mptm.set_message(u8(text));

	if (pUser) {
		mptm.add_session(pUser->uiSession);
		sendMessage(pUser, mptm);
	} else {
		if (tree)
			mptm.add_tree_id(cChannel->iId);
		else
			mptm.add_channel_id(cChannel->iId);

		QSet<Channel *> chans;
		QQueue<Channel *> q;
		q << cChannel;
		chans.insert(cChannel);
		Channel *c;

		if (tree) {
			while (! q.isEmpty()) {
				c = q.dequeue();
				chans.insert(c);
				foreach(c, c->qlChannels)
					q.enqueue(c);
			}
		}
		foreach(c, chans) {
			foreach(User *p, c->qlUsers)
				sendMessage(static_cast<ServerUser *>(p), mptm);
		}
	}
}

void Server::setTempGroups(int userid, Channel *cChannel, const QStringList &groups) {
	if (! cChannel)
		cChannel = qhChannels.value(0);

	Group *g;
	foreach(g, cChannel->qhGroups)
		g->qsTemporary.remove(userid);

	QString gname;
	foreach(gname, groups) {
		g = cChannel->qhGroups.value(gname);
		if (! g) {
			g = new Group(cChannel, gname);
		}
		g->qsTemporary.insert(userid);
	}

	User *p = qhUsers.value(userid);
	if (p)
		clearACLCache(p);
}


void Server::connectAuthenticator(QObject *obj) {
	connect(this, SIGNAL(registerUserSig(int &, const QMap<int, QString> &)), obj, SLOT(registerUserSlot(int &, const QMap<int, QString> &)));
	connect(this, SIGNAL(unregisterUserSig(int &, int)), obj, SLOT(unregisterUserSlot(int &, int)));
	connect(this, SIGNAL(getRegisteredUsersSig(const QString &, QMap<int, QString> &)), obj, SLOT(getRegisteredUsersSlot(const QString &, QMap<int, QString> &)));
	connect(this, SIGNAL(getRegistrationSig(int &, int, QMap<int, QString> &)), obj, SLOT(getRegistrationSlot(int &, int, QMap<int, QString> &)));
	connect(this, SIGNAL(authenticateSig(int &, QString &, const QList<QSslCertificate> &, const QString &, bool, const QString &)), obj, SLOT(authenticateSlot(int &, QString &, const QList<QSslCertificate> &, const QString &, bool, const QString &)));
	connect(this, SIGNAL(setInfoSig(int &, int, const QMap<int, QString> &)), obj, SLOT(setInfoSlot(int &, int, const QMap<int, QString> &)));
	connect(this, SIGNAL(setTextureSig(int &, int, const QByteArray &)), obj, SLOT(setTextureSlot(int &, int, const QByteArray &)));
	connect(this, SIGNAL(idToNameSig(QString &, int)), obj, SLOT(idToNameSlot(QString &, int)));
	connect(this, SIGNAL(nameToIdSig(int &, const QString &)), obj, SLOT(nameToIdSlot(int &, const QString &)));
	connect(this, SIGNAL(idToTextureSig(QByteArray &, int)), obj, SLOT(idToTextureSlot(QByteArray &, int)));
}

void Server::disconnectAuthenticator(QObject *obj) {
	disconnect(this, SIGNAL(registerUserSig(int &, const QMap<int, QString> &)), obj, SLOT(registerUserSlot(int &, const QMap<int, QString> &)));
	disconnect(this, SIGNAL(unregisterUserSig(int &, int)), obj, SLOT(unregisterUserSlot(int &, int)));
	disconnect(this, SIGNAL(getRegisteredUsersSig(const QString &, QMap<int, QString> &)), obj, SLOT(getRegisteredUsersSlot(const QString &, QMap<int, QString> &)));
	disconnect(this, SIGNAL(getRegistrationSig(int &, int, QMap<int, QString> &)), obj, SLOT(getRegistrationSlot(int &, int, QMap<int, QString> &)));
	disconnect(this, SIGNAL(authenticateSig(int &, QString &, const QList<QSslCertificate> &, const QString &, bool, const QString &)), obj, SLOT(authenticateSlot(int &, QString &, const QList<QSslCertificate> &, const QString &, bool, const QString &)));
	disconnect(this, SIGNAL(setInfoSig(int &, int, const QMap<int, QString> &)), obj, SLOT(setInfoSlot(int &, int, const QMap<int, QString> &)));
	disconnect(this, SIGNAL(setTextureSig(int &, int, const QByteArray &)), obj, SLOT(setTextureSlot(int &, int, const QByteArray &)));
	disconnect(this, SIGNAL(idToNameSig(QString &, int)), obj, SLOT(idToNameSlot(QString &, int)));
	disconnect(this, SIGNAL(nameToIdSig(int &, const QString &)), obj, SLOT(nameToIdSlot(int &, const QString &)));
	disconnect(this, SIGNAL(idToTextureSig(QByteArray &, int)), obj, SLOT(idToTextureSlot(QByteArray &, int)));
}

void Server::connectListener(QObject *obj) {
	connect(this, SIGNAL(userStateChanged(const User *)), obj, SLOT(userStateChanged(const User *)));
	connect(this, SIGNAL(userConnected(const User *)), obj, SLOT(userConnected(const User *)));
	connect(this, SIGNAL(userDisconnected(const User *)), obj, SLOT(userDisconnected(const User *)));
	connect(this, SIGNAL(channelStateChanged(const Channel *)), obj, SLOT(channelStateChanged(const Channel *)));
	connect(this, SIGNAL(channelCreated(const Channel *)), obj, SLOT(channelCreated(const Channel *)));
	connect(this, SIGNAL(channelRemoved(const Channel *)), obj, SLOT(channelRemoved(const Channel *)));
}

void Server::disconnectListener(QObject *obj) {
	disconnect(this, SIGNAL(userStateChanged(const User *)), obj, SLOT(userStateChanged(const User *)));
	disconnect(this, SIGNAL(userConnected(const User *)), obj, SLOT(userConnected(const User *)));
	disconnect(this, SIGNAL(userDisconnected(const User *)), obj, SLOT(userDisconnected(const User *)));
	disconnect(this, SIGNAL(channelStateChanged(const Channel *)), obj, SLOT(channelStateChanged(const Channel *)));
	disconnect(this, SIGNAL(channelCreated(const Channel *)), obj, SLOT(channelCreated(const Channel *)));
	disconnect(this, SIGNAL(channelRemoved(const Channel *)), obj, SLOT(channelRemoved(const Channel *)));
}

void Meta::connectListener(QObject *obj) {
	connect(this, SIGNAL(started(Server *)), obj, SLOT(started(Server *)));
	connect(this, SIGNAL(stopped(Server *)), obj, SLOT(stopped(Server *)));
}

void Meta::getVersion(int &major, int &minor, int &patch, QString &string) {
	string = QLatin1String(MUMBLE_RELEASE);
	major = minor = patch = 0;
	QRegExp rx(QLatin1String("(\\d+)\\.(\\d+)\\.(\\d+)"));
	if (rx.exactMatch(QLatin1String(MUMTEXT(MUMBLE_VERSION_STRING)))) {
		major = rx.cap(1).toInt();
		minor = rx.cap(2).toInt();
		patch = rx.cap(3).toInt();
	}
}
