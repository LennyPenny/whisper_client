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

#include "User.h"
#include "Channel.h"
#include "Overlay.h"
#include "MainWindow.h"
#include "Global.h"

typedef void (__cdecl *HooksProc)();
typedef SharedMem * (__cdecl *GetSharedMemProc)();
typedef unsigned int (__cdecl *GetOverlayMagicVersionProc)();
typedef void (__cdecl *PrepProc)();
typedef void (__cdecl *PrepDXGIProc)();

class SharedMemoryPrivate {
	public:
		HANDLE hMutex;
};

SharedMemory::SharedMemory() {
	d = new SharedMemoryPrivate();
	sm = NULL;
}

SharedMemory::~SharedMemory() {
	CloseHandle(d->hMutex);
	delete d;
	d = NULL;
}

void SharedMemory::resolve(QLibrary *lib) {
	GetOverlayMagicVersionProc gompvp = (GetOverlayMagicVersionProc)lib->resolve("GetOverlayMagicVersion");
	if (! gompvp)
		return;

	if (gompvp() != OVERLAY_MAGIC_NUMBER)
		return;

	GetSharedMemProc gsmp = (GetSharedMemProc)lib->resolve("GetSharedMemory");
	if (gsmp)
		sm=gsmp();
	d->hMutex = CreateMutex(NULL, false, L"MumbleSharedMutex");

	if (sm) {
		PrepProc pp = (PrepProc) lib->resolve("PrepareD3D9");
		if (pp)
			pp();

		PrepDXGIProc pdxgi = (PrepDXGIProc) lib->resolve("PrepareDXGI");
		if (pdxgi)
			pdxgi();
	}
}

bool SharedMemory::tryLock() {
	DWORD dwWaitResult = WaitForSingleObject(d->hMutex, 500L);
	return (dwWaitResult == WAIT_OBJECT_0);
}

void SharedMemory::unlock() {
	ReleaseMutex(d->hMutex);
}

class OverlayPrivateWin : public OverlayPrivate {
	public:
		HooksProc hpInstall, hpRemove;
};

void Overlay::platformInit() {
	d = new OverlayPrivateWin();
	static_cast<OverlayPrivateWin *>(d)->hpInstall = (HooksProc)qlOverlay->resolve("InstallHooks");
	static_cast<OverlayPrivateWin *>(d)->hpRemove = (HooksProc)qlOverlay->resolve("RemoveHooks");
}

void Overlay::setActive(bool act) {
	if (! sm.sm)
		return;

	if (act)
		static_cast<OverlayPrivateWin *>(d)->hpInstall();
	else
		static_cast<OverlayPrivateWin *>(d)->hpRemove();
}

void Overlay::on_Timer_timeout() {
	if (sm.sm)
		sm.sm->lastAppAlive = GetTickCount();
}
