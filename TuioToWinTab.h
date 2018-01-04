/*
	TUIO C++ Example - part of the reacTIVision project
	http://reactivision.sourceforge.net/
	Copyright (c) 2005-2016 Martin Kaltenbrunner <martin@tuio.org>

	Permission is hereby granted, free of charge, to any person obtaining
	a copy of this software and associated documentation files
	(the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge,
	publish, distribute, sublicense, and/or sell copies of the Software,
	and to permit persons to whom the Software is furnished to do so,
	subject to the following conditions:

	The above copyright notice and this permission notice shall be
	included in all copies or substantial portions of the Software.

	Any person wishing to distribute modifications to the Software is
	requested to send the modifications to the original developer so that
	they can be incorporated into the canonical version.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
	EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
	MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
	ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
	CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
	WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
/*------------------------------------------------------------------------------
WintabEmulator - Emulation.h
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#pragma once
#ifndef INCLUDED_TuioToWinTab_H
#define INCLUDED_TuioToWinTab_H


#include "wintab.h"
#define CSR_NAME_PUCK				WTI_CURSORS+0
#define CSR_NAME_PRESSURE_STYLUS	WTI_CURSORS+1
#define CSR_NAME_ERASER				WTI_CURSORS+2
#define WTI_DDCTX_1					WTI_DDCTXS+1
#define WTI_DDCTX_2					WTI_DDCTXS+2
#define MAX_CONTEXTS 32
#define WTI_VIRTUAL_DEVICE 99


#include "TuioListener.h"
#include "TuioClient.h"
#include "UdpReceiver.h"
#include "TcpReceiver.h"
#include <math.h>
#include <vector>
#include <algorithm>

using namespace TUIO;

class TuioToWinTab : public TuioListener {
	public:
		void addTuioObject(TuioObject *tobj);
		void updateTuioObject(TuioObject *tobj);
		void removeTuioObject(TuioObject *tobj);

		void addTuioCursor(TuioCursor *tcur);
		void updateTuioCursor(TuioCursor *tcur);
		void removeTuioCursor(TuioCursor *tcur);

		void addTuioBlob(TuioBlob *tblb);
		void updateTuioBlob(TuioBlob *tblb);
		void removeTuioBlob(TuioBlob *tblb);

		void refresh(TuioTime frameTime);
};



// EX EMULATIOn.cpp

typedef struct _emu_settings_t {
    BOOL disableFeedback;
    BOOL disableGestures;
    INT shiftX;
    INT shiftY;
    BOOL pressureExpand;
    UINT pressureMin;
    UINT pressureMax;
    BOOL pressureCurve;
    UINT pressurePoint[5];
    UINT pressureValue[5];
} emu_settings_t;

void emuSetModule(HMODULE hModule);
void emuEnableThread(DWORD dwThread);
void emuDisableThread(DWORD dwThread);

void emuInit(BOOL fLogging, BOOL fDebug, emu_settings_t *settings);
void emuShutdown(void);
UINT emuWTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput);
UINT emuWTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput);
HCTX emuWTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable);
HCTX emuWTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable);
BOOL emuWTClose(HCTX hCtx);
int emuWTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt);
BOOL emuWTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt);
BOOL emuWTEnable(HCTX hCtx, BOOL fEnable);
BOOL emuWTOverlap(HCTX hCtx, BOOL fToTop);
BOOL emuWTConfig(HCTX hCtx, HWND hWnd);
BOOL emuWTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx);
BOOL emuWTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx);
BOOL emuWTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx);
BOOL emuWTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx);
BOOL emuWTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData);
BOOL emuWTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData);
BOOL emuWTSave(HCTX hCtx, LPVOID lpData);
HCTX emuWTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable);
int emuWTPacketsPeek(HCTX hCtx, int cMaxPkt, LPVOID lpPkts);
int emuWTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts);
int emuWTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts);
int emuWTQueueSizeGet(HCTX hCtx);
BOOL emuWTQueueSizeSet(HCTX hCtx, int nPkts);
HMGR emuWTMgrOpen(HWND hWnd, UINT wMsgBase);
BOOL emuWTMgrClose(HMGR hMgr);

void setupReceiver(void);
static BOOL handleTuioMessage(TuioCursor *tcur);
#endif /* INCLUDED_TuioToWinTab_H */
