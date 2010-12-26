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

#ifndef _OVERLAY_H
#define _OVERLAY_H

#include "../../overlay/overlay.h"
#include "ConfigDialog.h"
#include "ui_Overlay.h"

class Player;

class SharedMemoryPrivate;
class SharedMemory {
	private:
		Q_DISABLE_COPY(SharedMemory)
	protected:
		SharedMemoryPrivate *d;
	public:
		SharedMem *sm;
		SharedMemory();
		~SharedMemory();
		void resolve(QLibrary *lib);
		bool tryLock();
		void unlock();
};

class OverlayConfig : public ConfigWidget, public Ui::OverlayConfig {
	private:
		Q_OBJECT
		Q_DISABLE_COPY(OverlayConfig)
	protected:
		QFont qfFont;
		QColor qcPlayer, qcAltTalking, qcTalking, qcChannel, qcChannelTalking;

		static void setColorLabel(QLabel *label, QColor col);
	public:
		OverlayConfig(Settings &st);
		virtual QString title() const;
		virtual QIcon icon() const;
	public slots:
		void on_qsMaxHeight_valueChanged(int v);
		void on_qpbSetFont_clicked();
		void on_qpbPlayer_clicked();
		void on_qpbTalking_clicked();
		void on_qpbAltTalking_clicked();
		void on_qpbChannel_clicked();
		void on_qpbChannelTalking_clicked();
		void accept() const;
		void save() const;
		void load(const Settings &r);
		bool expert(bool);
};


class OverlayPrivate;
class Overlay : public QObject {
		friend class OverlayConfig;
	private:
		Q_OBJECT
		Q_DISABLE_COPY(Overlay)
	protected:
		OverlayPrivate *d;

		enum Decoration { None, Muted, Deafened };

		struct TextLine {
			QString qsText;
			int iPlayer;
			quint32 uiColor;
			Decoration dDecor;
			TextLine(const QString &t, quint32 c, int p = -1, Decoration d = None) : qsText(t), iPlayer(p), uiColor(c), dDecor(d) { };
		};

		typedef QPair<unsigned int, QByteArray> UserTexture;
		QByteArray qbaMuted, qbaDeafened;
		QList<TextLine> qlCurrentTexts;
		QHash<QString, unsigned char *> qhTextures;
		QHash<int, UserTexture> qhUserTextures;
		QHash<QString, short> qhWidths;
		QHash<int, QString> qhQueried;
		QSet<int> qsForce;
		QLibrary *qlOverlay;
		QTimer *qtTimer;
		QFont qfFont;
		float fFontBase;
		SharedMemory sm;
		void platformInit();
		void setTexts(const QList<TextLine> &lines);
		void fixFont();
		void clearCache();
	public:
		Overlay();
		~Overlay();
		bool isActive() const;
		void textureResponse(int id, const QByteArray &texture);
	public slots:
		void on_Timer_timeout();
		void updateOverlay();
		void setActive(bool act);
		void toggleShow();
		void forceSettings();
};

#endif
