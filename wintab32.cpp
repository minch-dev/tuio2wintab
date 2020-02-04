/*------------------------------------------------------------------------------
WintabEmulator - WintabEmulator.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#include "stdafx.h"
#include <tchar.h>
#include <assert.h>
#include <map>

#define API __declspec(dllexport) WINAPI
#include "wintab.h"
#include "TuioToWinTab.h"


#define INI_FILE                _T("wintab32.ini")
#define DEFAULT_LOG_FILE        _T("C:\\wintab32.txt")
#define DEFAULT_WINTAB_DLL      _T("C:\\WINDOWS\\WINTAB32.DLL")

static BOOL     initialised     = FALSE;

static BOOL     logging = TRUE;
static BOOL     consoleOutput = FALSE;
static BOOL     debug           = TRUE;
static BOOL     useEmulation    = TRUE;
static LPTSTR   logFile         = NULL;
static LPTSTR   wintabDLL       = NULL;
static FILE     *fhLog          = NULL;

static WTPKT    packetData      = 0;
static WTPKT    packetMode      = 0;

#define WTAPI WINAPI

typedef UINT ( WTAPI * WTINFOA ) ( UINT, UINT, LPVOID );
typedef HCTX ( WTAPI * WTOPENA )( HWND, LPLOGCONTEXTA, BOOL );
typedef UINT ( WTAPI * WTINFOW ) ( UINT, UINT, LPVOID );
typedef HCTX ( WTAPI * WTOPENW )( HWND, LPLOGCONTEXTW, BOOL );
typedef BOOL ( WTAPI * WTGETA ) ( HCTX, LPLOGCONTEXTA );
typedef BOOL ( WTAPI * WTSETA ) ( HCTX, LPLOGCONTEXTA );
typedef BOOL ( WTAPI * WTGETW ) ( HCTX, LPLOGCONTEXTW );
typedef BOOL ( WTAPI * WTSETW ) ( HCTX, LPLOGCONTEXTW );
typedef BOOL ( WTAPI * WTCLOSE ) ( HCTX );
typedef BOOL ( WTAPI * WTENABLE ) ( HCTX, BOOL );
typedef BOOL ( WTAPI * WTPACKET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( WTAPI * WTOVERLAP ) ( HCTX, BOOL );
typedef BOOL ( WTAPI * WTSAVE ) ( HCTX, LPVOID );
typedef BOOL ( WTAPI * WTCONFIG ) ( HCTX, HWND );
typedef HCTX ( WTAPI * WTRESTORE ) ( HWND, LPVOID, BOOL );
typedef BOOL ( WTAPI * WTEXTSET ) ( HCTX, UINT, LPVOID );
typedef BOOL ( WTAPI * WTEXTGET ) ( HCTX, UINT, LPVOID );
typedef int ( WTAPI * WTPACKETSPEEK ) ( HCTX, int, LPVOID );
typedef int ( WTAPI * WTQUEUESIZEGET ) ( HCTX );
typedef BOOL ( WTAPI * WTQUEUESIZESET ) ( HCTX, int );
typedef int  ( WTAPI * WTDATAGET ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( WTAPI * WTDATAPEEK ) ( HCTX, UINT, UINT, int, LPVOID, LPINT);
typedef int  ( WTAPI * WTPACKETSGET ) (HCTX, int, LPVOID);
typedef HMGR ( WTAPI * WTMGROPEN ) ( HWND, UINT );
typedef BOOL ( WTAPI * WTMGRCLOSE ) ( HMGR );
typedef HCTX ( WTAPI * WTMGRDEFCONTEXT ) ( HMGR, BOOL );
typedef HCTX ( WTAPI * WTMGRDEFCONTEXTEX ) ( HMGR, UINT, BOOL );

static HINSTANCE        hOrigWintab         = NULL;
static WTINFOA          origWTInfoA         = NULL;
static WTINFOW          origWTInfoW         = NULL;
static WTOPENA          origWTOpenA         = NULL;
static WTOPENW          origWTOpenW         = NULL;
static WTGETA           origWTGetA          = NULL;
static WTSETA           origWTSetA          = NULL;
static WTGETW           origWTGetW          = NULL;
static WTSETW           origWTSetW          = NULL;
static WTCLOSE          origWTClose         = NULL;
static WTPACKET         origWTPacket        = NULL;
static WTENABLE         origWTEnable        = NULL;
static WTOVERLAP        origWTOverlap       = NULL;
static WTSAVE           origWTSave          = NULL;
static WTCONFIG         origWTConfig        = NULL;
static WTRESTORE        origWTRestore       = NULL;
static WTEXTSET         origWTExtSet        = NULL;
static WTEXTGET         origWTExtGet        = NULL;
static WTPACKETSPEEK    origWTPacketsPeek   = NULL;
static WTQUEUESIZEGET   origWTQueueSizeGet  = NULL;
static WTQUEUESIZESET   origWTQueueSizeSet  = NULL;
static WTDATAGET        origWTDataGet       = NULL;
static WTDATAPEEK       origWTDataPeek      = NULL;
static WTPACKETSGET     origWTPacketsGet    = NULL;
static WTMGROPEN        origWTMgrOpen       = NULL;
static WTMGRCLOSE       origWTMgrClose      = NULL;
static WTMGRDEFCONTEXT  origWTMgrDefContext = NULL;
static WTMGRDEFCONTEXTEX origWTMgrDefContextEx = NULL;

//this function is needed to load resources from original wintab32.dll
#define GETPROCADDRESS(type, func) \
	orig##func = (type)GetProcAddress(hOrigWintab, #func);


//
//#define IND(D){D,#D},
//#define CAT(C){C, { {0,#C},
//#define END }},


//std::map <int, std::map<int, std::string>> wCategories = {
//	{ WTI_INTERFACE, { { 0, "WTI_INTERFACE" }, } },
//	{ WTI_STATUS, { { 0, "WTI_STATUS" }, } },
//};

//nested std::map not workz in 64bit build, so here is a dirty hackz
#define CAT(C)				\
case C:						\
	switch (wIndex){		\
	case 0:					\
		return #C;			\
		break;

#define IND(D)				\
	case D:					\
		return #D;			\
		break;

#define END					\
	default:				\
		return "NO_IDEA";	\
		break;				\
	}

#define CATCONTEXT			\
case WTI_DEFCONTEXT:		\
case WTI_DEFSYSCTX:			\
case WTI_DDCTXS:			\
case WTI_DDCTX_1:			\
case WTI_DDCTX_2:			\
case WTI_DSCTXS:			\
	switch (wIndex){		\
	case 0:					\
		switch (wCategory){	\
			IND(WTI_DEFCONTEXT)			\
			IND(WTI_DEFSYSCTX)			\
			IND(WTI_DDCTXS)				\
			IND(WTI_DDCTX_1)			\
			IND(WTI_DDCTX_2)			\
			IND(WTI_DSCTXS)				\
		}					\
		break;


	//itoa(wCategory,wCategoryChar,10);			
	//itoa(wIndex,wIndexChar,10);	


static const char* category_string(int wCategory, int wIndex){
	char wCategoryChar[33];
	char wIndexChar[33];
	switch (wCategory){
		CAT(WTI_INTERFACE)
			IND(IFC_WINTABID)
			IND(IFC_SPECVERSION)
			IND(IFC_IMPLVERSION)
			IND(IFC_NDEVICES)
			IND(IFC_NCURSORS)
			IND(IFC_NCONTEXTS)
			IND(IFC_CTXOPTIONS)
			IND(IFC_CTXSAVESIZE)
			IND(IFC_NEXTENSIONS)
			IND(IFC_NMANAGERS)
			//IND(IFC_MAX)
		END
		CAT(WTI_STATUS)
			IND(IFC_WINTABID)
			IND(IFC_SPECVERSION)
			IND(IFC_IMPLVERSION)
			IND(IFC_NDEVICES)
			IND(IFC_NCURSORS)
			IND(IFC_NCONTEXTS)
			IND(IFC_CTXOPTIONS)
			IND(IFC_CTXSAVESIZE)
			IND(IFC_NEXTENSIONS)
			IND(IFC_NMANAGERS)
			//DEF(IFC_MAX)
		END
		CAT(WTI_VIRTUAL_DEVICE)
			IND(DVC_X)
			IND(DVC_Y)
			IND(DVC_Z)
		END
		CAT(WTI_DEVICES)
			IND(DVC_NAME)
			IND(DVC_HARDWARE)
			IND(DVC_NCSRTYPES)
			IND(DVC_FIRSTCSR)
			IND(DVC_PKTRATE)
			IND(DVC_PKTDATA)
			IND(DVC_PKTMODE)
			IND(DVC_CSRDATA)
			IND(DVC_XMARGIN)
			IND(DVC_YMARGIN)
			IND(DVC_ZMARGIN)
			IND(DVC_X)
			IND(DVC_Y)
			IND(DVC_Z)
			IND(DVC_NPRESSURE)
			IND(DVC_TPRESSURE)
			IND(DVC_ORIENTATION)
			IND(DVC_ROTATION)
			IND(DVC_PNPID)
			//IND(DVC_MAX)
		END
		CAT(CSR_NAME_PUCK)
			IND(CSR_NAME)
			IND(CSR_ACTIVE)
			IND(CSR_PKTDATA)
			IND(CSR_BUTTONS)
			IND(CSR_BUTTONBITS)
			IND(CSR_BTNNAMES)
			IND(CSR_BUTTONMAP)
			IND(CSR_SYSBTNMAP)
			IND(CSR_NPBUTTON)
			IND(CSR_NPBTNMARKS)
			IND(CSR_NPRESPONSE)
			IND(CSR_TPBUTTON)
			IND(CSR_TPBTNMARKS)
			IND(CSR_TPRESPONSE)
			IND(CSR_PHYSID)
			IND(CSR_MODE)
			IND(CSR_MINPKTDATA)
			IND(CSR_MINBUTTONS)
			IND(CSR_CAPABILITIES)
			IND(CSR_TYPE)
			//IND(CSR_MAX)
		END
		CAT(CSR_NAME_PRESSURE_STYLUS)
			IND(CSR_NAME)
			IND(CSR_ACTIVE)
			IND(CSR_PKTDATA)
			IND(CSR_BUTTONS)
			IND(CSR_BUTTONBITS)
			IND(CSR_BTNNAMES)
			IND(CSR_BUTTONMAP)
			IND(CSR_SYSBTNMAP)
			IND(CSR_NPBUTTON)
			IND(CSR_NPBTNMARKS)
			IND(CSR_NPRESPONSE)
			IND(CSR_TPBUTTON)
			IND(CSR_TPBTNMARKS)
			IND(CSR_TPRESPONSE)
			IND(CSR_PHYSID)
			IND(CSR_MODE)
			IND(CSR_MINPKTDATA)
			IND(CSR_MINBUTTONS)
			IND(CSR_CAPABILITIES)
			IND(CSR_TYPE)
			//IND(CSR_MAX)
		END
		CAT(CSR_NAME_ERASER)
			IND(CSR_NAME)
			IND(CSR_ACTIVE)
			IND(CSR_PKTDATA)
			IND(CSR_BUTTONS)
			IND(CSR_BUTTONBITS)
			IND(CSR_BTNNAMES)
			IND(CSR_BUTTONMAP)
			IND(CSR_SYSBTNMAP)
			IND(CSR_NPBUTTON)
			IND(CSR_NPBTNMARKS)
			IND(CSR_NPRESPONSE)
			IND(CSR_TPBUTTON)
			IND(CSR_TPBTNMARKS)
			IND(CSR_TPRESPONSE)
			IND(CSR_PHYSID)
			IND(CSR_MODE)
			IND(CSR_MINPKTDATA)
			IND(CSR_MINBUTTONS)
			IND(CSR_CAPABILITIES)
			IND(CSR_TYPE)
			//IND(CSR_MAX)
		END
		CAT(WTI_EXTENSIONS)
			IND(EXT_NAME)
			IND(EXT_TAG)
			IND(EXT_MASK)
			IND(EXT_SIZE)
			IND(EXT_AXES)
			IND(EXT_DEFAULT)
			IND(EXT_DEFCONTEXT)
			IND(EXT_DEFSYSCTX)
			IND(EXT_CURSORS)
			IND(EXT_DEVICES)
			//IND(EXT_MAX)
		END
		CATCONTEXT
			IND(CTX_NAME)
			IND(CTX_OPTIONS)
			IND(CTX_STATUS)
			IND(CTX_LOCKS)
			IND(CTX_MSGBASE)
			IND(CTX_DEVICE)
			IND(CTX_PKTRATE)
			IND(CTX_PKTDATA)
			IND(CTX_PKTMODE)
			IND(CTX_MOVEMASK)
			IND(CTX_BTNDNMASK)
			IND(CTX_BTNUPMASK)
			IND(CTX_INORGX)
			IND(CTX_INORGY)
			IND(CTX_INORGZ)
			IND(CTX_INEXTX)
			IND(CTX_INEXTY)
			IND(CTX_INEXTZ)
			IND(CTX_OUTORGX)
			IND(CTX_OUTORGY)
			IND(CTX_OUTORGZ)
			IND(CTX_OUTEXTX)
			IND(CTX_OUTEXTY)
			IND(CTX_OUTEXTZ)
			IND(CTX_SENSX)
			IND(CTX_SENSY)
			IND(CTX_SENSZ)
			IND(CTX_SYSMODE)
			IND(CTX_SYSORGX)
			IND(CTX_SYSORGY)
			IND(CTX_SYSEXTX)
			IND(CTX_SYSEXTY)
			IND(CTX_SYSSENSX)
			IND(CTX_SYSSENSY)
			//IND(CTX_MAX)
		END
	case 0:
		return "ZERO";
		break;
	default:
		return "NO_IDEA";
		break;
	}
}

//std::map <int, std::map<int, const char*>> wCategories = {
//	{ WTI_INTERFACE, { { 0, "WTI_INTERFACE" }, } },
//	{ WTI_STATUS, { { 0, "WTI_STATUS" }, } },
//};
////more verbose output to log
//std::map <int, std::map<int, const char*>> wCategories = {

	

//	CAT(WTI_INTERFACE)
//		IND(IFC_WINTABID)
//		IND(IFC_SPECVERSION)
//		IND(IFC_IMPLVERSION)
//		IND(IFC_NDEVICES)
//		IND(IFC_NCURSORS)
//		IND(IFC_NCONTEXTS)
//		IND(IFC_CTXOPTIONS)
//		IND(IFC_CTXSAVESIZE)
//		IND(IFC_NEXTENSIONS)
//		IND(IFC_NMANAGERS)
//		IND(IFC_MAX)
//	END
//	CAT(WTI_STATUS)
//		IND(IFC_WINTABID)
//		IND(IFC_SPECVERSION)
//		IND(IFC_IMPLVERSION)
//		IND(IFC_NDEVICES)
//		IND(IFC_NCURSORS)
//		IND(IFC_NCONTEXTS)
//		IND(IFC_CTXOPTIONS)
//		IND(IFC_CTXSAVESIZE)
//		IND(IFC_NEXTENSIONS)
//		IND(IFC_NMANAGERS)
//		//DEF(IFC_MAX)
//	END
//	CAT(WTI_DEFCONTEXT)
//		IND(CTX_NAME)
//		IND(CTX_OPTIONS)
//		IND(CTX_STATUS)
//		IND(CTX_LOCKS)
//		IND(CTX_MSGBASE)
//		IND(CTX_DEVICE)
//		IND(CTX_PKTRATE)
//		IND(CTX_PKTDATA)
//		IND(CTX_PKTMODE)
//		IND(CTX_MOVEMASK)
//		IND(CTX_BTNDNMASK)
//		IND(CTX_BTNUPMASK)
//		IND(CTX_INORGX)
//		IND(CTX_INORGY)
//		IND(CTX_INORGZ)
//		IND(CTX_INEXTX)
//		IND(CTX_INEXTY)
//		IND(CTX_INEXTZ)
//		IND(CTX_OUTORGX)
//		IND(CTX_OUTORGY)
//		IND(CTX_OUTORGZ)
//		IND(CTX_OUTEXTX)
//		IND(CTX_OUTEXTY)
//		IND(CTX_OUTEXTZ)
//		IND(CTX_SENSX)
//		IND(CTX_SENSY)
//		IND(CTX_SENSZ)
//		IND(CTX_SYSMODE)
//		IND(CTX_SYSORGX)
//		IND(CTX_SYSORGY)
//		IND(CTX_SYSEXTX)
//		IND(CTX_SYSEXTY)
//		IND(CTX_SYSSENSX)
//		IND(CTX_SYSSENSY)
//		//IND(CTX_MAX)
//	END
//	CAT(WTI_DEFSYSCTX)
//		IND(CTX_NAME)
//		IND(CTX_OPTIONS)
//		IND(CTX_STATUS)
//		IND(CTX_LOCKS)
//		IND(CTX_MSGBASE)
//		IND(CTX_DEVICE)
//		IND(CTX_PKTRATE)
//		IND(CTX_PKTDATA)
//		IND(CTX_PKTMODE)
//		IND(CTX_MOVEMASK)
//		IND(CTX_BTNDNMASK)
//		IND(CTX_BTNUPMASK)
//		IND(CTX_INORGX)
//		IND(CTX_INORGY)
//		IND(CTX_INORGZ)
//		IND(CTX_INEXTX)
//		IND(CTX_INEXTY)
//		IND(CTX_INEXTZ)
//		IND(CTX_OUTORGX)
//		IND(CTX_OUTORGY)
//		IND(CTX_OUTORGZ)
//		IND(CTX_OUTEXTX)
//		IND(CTX_OUTEXTY)
//		IND(CTX_OUTEXTZ)
//		IND(CTX_SENSX)
//		IND(CTX_SENSY)
//		IND(CTX_SENSZ)
//		IND(CTX_SYSMODE)
//		IND(CTX_SYSORGX)
//		IND(CTX_SYSORGY)
//		IND(CTX_SYSEXTX)
//		IND(CTX_SYSEXTY)
//		IND(CTX_SYSSENSX)
//		IND(CTX_SYSSENSY)
//		//IND(CTX_MAX)
//	END
//	CAT(WTI_DEVICES)
//		IND(DVC_NAME)
//		IND(DVC_HARDWARE)
//		IND(DVC_NCSRTYPES)
//		IND(DVC_FIRSTCSR)
//		IND(DVC_PKTRATE)
//		IND(DVC_PKTDATA)
//		IND(DVC_PKTMODE)
//		IND(DVC_CSRDATA)
//		IND(DVC_XMARGIN)
//		IND(DVC_YMARGIN)
//		IND(DVC_ZMARGIN)
//		IND(DVC_X)
//		IND(DVC_Y)
//		IND(DVC_Z)
//		IND(DVC_NPRESSURE)
//		IND(DVC_TPRESSURE)
//		IND(DVC_ORIENTATION)
//		IND(DVC_ROTATION)
//		IND(DVC_PNPID)
//		//IND(DVC_MAX)
//	END
//	CAT(CSR_NAME_PUCK)
//		IND(CSR_NAME)
//		IND(CSR_ACTIVE)
//		IND(CSR_PKTDATA)
//		IND(CSR_BUTTONS)
//		IND(CSR_BUTTONBITS)
//		IND(CSR_BTNNAMES)
//		IND(CSR_BUTTONMAP)
//		IND(CSR_SYSBTNMAP)
//		IND(CSR_NPBUTTON)
//		IND(CSR_NPBTNMARKS)
//		IND(CSR_NPRESPONSE)
//		IND(CSR_TPBUTTON)
//		IND(CSR_TPBTNMARKS)
//		IND(CSR_TPRESPONSE)
//		IND(CSR_PHYSID)
//		IND(CSR_MODE)
//		IND(CSR_MINPKTDATA)
//		IND(CSR_MINBUTTONS)
//		IND(CSR_CAPABILITIES)
//		IND(CSR_TYPE)
//		//IND(CSR_MAX)
//	END
//	CAT(CSR_NAME_PRESSURE_STYLUS)
//		IND(CSR_NAME)
//		IND(CSR_ACTIVE)
//		IND(CSR_PKTDATA)
//		IND(CSR_BUTTONS)
//		IND(CSR_BUTTONBITS)
//		IND(CSR_BTNNAMES)
//		IND(CSR_BUTTONMAP)
//		IND(CSR_SYSBTNMAP)
//		IND(CSR_NPBUTTON)
//		IND(CSR_NPBTNMARKS)
//		IND(CSR_NPRESPONSE)
//		IND(CSR_TPBUTTON)
//		IND(CSR_TPBTNMARKS)
//		IND(CSR_TPRESPONSE)
//		IND(CSR_PHYSID)
//		IND(CSR_MODE)
//		IND(CSR_MINPKTDATA)
//		IND(CSR_MINBUTTONS)
//		IND(CSR_CAPABILITIES)
//		IND(CSR_TYPE)
//		//IND(CSR_MAX)
//	END
//	CAT(CSR_NAME_ERASER)
//		IND(CSR_NAME)
//		IND(CSR_ACTIVE)
//		IND(CSR_PKTDATA)
//		IND(CSR_BUTTONS)
//		IND(CSR_BUTTONBITS)
//		IND(CSR_BTNNAMES)
//		IND(CSR_BUTTONMAP)
//		IND(CSR_SYSBTNMAP)
//		IND(CSR_NPBUTTON)
//		IND(CSR_NPBTNMARKS)
//		IND(CSR_NPRESPONSE)
//		IND(CSR_TPBUTTON)
//		IND(CSR_TPBTNMARKS)
//		IND(CSR_TPRESPONSE)
//		IND(CSR_PHYSID)
//		IND(CSR_MODE)
//		IND(CSR_MINPKTDATA)
//		IND(CSR_MINBUTTONS)
//		IND(CSR_CAPABILITIES)
//		IND(CSR_TYPE)
//		//IND(CSR_MAX)
//	END
//	CAT(WTI_EXTENSIONS)
//		IND(EXT_NAME)
//		IND(EXT_TAG)
//		IND(EXT_MASK)
//		IND(EXT_SIZE)
//		IND(EXT_AXES)
//		IND(EXT_DEFAULT)
//		IND(EXT_DEFCONTEXT)
//		IND(EXT_DEFSYSCTX)
//		IND(EXT_CURSORS)
//		IND(EXT_DEVICES)
//		//IND(EXT_MAX)
//	END
//	CAT(WTI_DDCTXS)
//		IND(CTX_NAME)
//		IND(CTX_OPTIONS)
//		IND(CTX_STATUS)
//		IND(CTX_LOCKS)
//		IND(CTX_MSGBASE)
//		IND(CTX_DEVICE)
//		IND(CTX_PKTRATE)
//		IND(CTX_PKTDATA)
//		IND(CTX_PKTMODE)
//		IND(CTX_MOVEMASK)
//		IND(CTX_BTNDNMASK)
//		IND(CTX_BTNUPMASK)
//		IND(CTX_INORGX)
//		IND(CTX_INORGY)
//		IND(CTX_INORGZ)
//		IND(CTX_INEXTX)
//		IND(CTX_INEXTY)
//		IND(CTX_INEXTZ)
//		IND(CTX_OUTORGX)
//		IND(CTX_OUTORGY)
//		IND(CTX_OUTORGZ)
//		IND(CTX_OUTEXTX)
//		IND(CTX_OUTEXTY)
//		IND(CTX_OUTEXTZ)
//		IND(CTX_SENSX)
//		IND(CTX_SENSY)
//		IND(CTX_SENSZ)
//		IND(CTX_SYSMODE)
//		IND(CTX_SYSORGX)
//		IND(CTX_SYSORGY)
//		IND(CTX_SYSEXTX)
//		IND(CTX_SYSEXTY)
//		IND(CTX_SYSSENSX)
//		IND(CTX_SYSSENSY)
//		//IND(CTX_MAX)
//	END
//	CAT(WTI_DSCTXS)
//		IND(CTX_NAME)
//		IND(CTX_OPTIONS)
//		IND(CTX_STATUS)
//		IND(CTX_LOCKS)
//		IND(CTX_MSGBASE)
//		IND(CTX_DEVICE)
//		IND(CTX_PKTRATE)
//		IND(CTX_PKTDATA)
//		IND(CTX_PKTMODE)
//		IND(CTX_MOVEMASK)
//		IND(CTX_BTNDNMASK)
//		IND(CTX_BTNUPMASK)
//		IND(CTX_INORGX)
//		IND(CTX_INORGY)
//		IND(CTX_INORGZ)
//		IND(CTX_INEXTX)
//		IND(CTX_INEXTY)
//		IND(CTX_INEXTZ)
//		IND(CTX_OUTORGX)
//		IND(CTX_OUTORGY)
//		IND(CTX_OUTORGZ)
//		IND(CTX_OUTEXTX)
//		IND(CTX_OUTEXTY)
//		IND(CTX_OUTEXTZ)
//		IND(CTX_SENSX)
//		IND(CTX_SENSY)
//		IND(CTX_SENSZ)
//		IND(CTX_SYSMODE)
//		IND(CTX_SYSORGX)
//		IND(CTX_SYSORGY)
//		IND(CTX_SYSEXTX)
//		IND(CTX_SYSEXTY)
//		IND(CTX_SYSSENSX)
//		IND(CTX_SYSSENSY)
//		//IND(CTX_MAX)
//	END



//};



//Loads original DLL if we don't need emulation
static BOOL LoadWintab(TCHAR *path)
{
	hOrigWintab = LoadLibrary(path);

	if (!hOrigWintab) {
		return FALSE;
	}

	GETPROCADDRESS(WTOPENA, WTOpenA);
	GETPROCADDRESS(WTINFOA, WTInfoA);
	GETPROCADDRESS(WTGETA, WTGetA);
	GETPROCADDRESS(WTSETA, WTSetA);
	GETPROCADDRESS(WTOPENW, WTOpenW);
	GETPROCADDRESS(WTINFOW, WTInfoW);
	GETPROCADDRESS(WTGETW, WTGetW);
	GETPROCADDRESS(WTSETW, WTSetW);
	GETPROCADDRESS(WTPACKET, WTPacket);
	GETPROCADDRESS(WTCLOSE, WTClose);
	GETPROCADDRESS(WTENABLE, WTEnable);
	GETPROCADDRESS(WTOVERLAP, WTOverlap);
	GETPROCADDRESS(WTSAVE, WTSave);
	GETPROCADDRESS(WTCONFIG, WTConfig);
	GETPROCADDRESS(WTRESTORE, WTRestore);
	GETPROCADDRESS(WTEXTSET, WTExtSet);
	GETPROCADDRESS(WTEXTGET, WTExtGet);
	GETPROCADDRESS(WTPACKETSPEEK, WTPacketsPeek);
	GETPROCADDRESS(WTQUEUESIZEGET, WTQueueSizeGet);
	GETPROCADDRESS(WTQUEUESIZESET, WTQueueSizeSet);
	GETPROCADDRESS(WTDATAGET, WTDataGet);
	GETPROCADDRESS(WTDATAPEEK, WTDataPeek);
	GETPROCADDRESS(WTPACKETSGET, WTPacketsGet);
	GETPROCADDRESS(WTMGROPEN, WTMgrOpen);
	GETPROCADDRESS(WTMGRCLOSE, WTMgrClose);
	GETPROCADDRESS(WTMGRDEFCONTEXT, WTMgrDefContext);
	GETPROCADDRESS(WTMGRDEFCONTEXTEX, WTMgrDefContextEx);

	return TRUE;
}

//Unloads original DLL
static void UnloadWintab(void)
{
    origWTInfoA         = NULL;
    origWTInfoW         = NULL;
    origWTOpenA         = NULL;
    origWTOpenW         = NULL;
    origWTGetA          = NULL;
    origWTSetA          = NULL;
    origWTGetW          = NULL;
    origWTSetW          = NULL;
    origWTClose         = NULL;
    origWTPacket        = NULL;
    origWTEnable        = NULL;
    origWTOverlap       = NULL;
    origWTSave          = NULL;
    origWTConfig        = NULL;
    origWTRestore       = NULL;
    origWTExtSet        = NULL;
    origWTExtGet        = NULL;
    origWTPacketsPeek   = NULL;
    origWTQueueSizeGet  = NULL;
    origWTQueueSizeSet  = NULL;
    origWTDataGet       = NULL;
    origWTDataPeek      = NULL;
    origWTPacketsGet    = NULL;
    origWTMgrOpen       = NULL;
    origWTMgrClose      = NULL;
    origWTMgrDefContext = NULL;
    origWTMgrDefContextEx = NULL;
    
    if (hOrigWintab) {
        FreeLibrary(hOrigWintab);
        hOrigWintab = NULL;
    }
}

//simply opens log file
static BOOL OpenLogFile(void)
{
	_tfopen_s (&fhLog, logFile, _T("w"));
	return (fhLog != NULL ? TRUE : FALSE);
}

//flushes log to a file
void FlushLog(void)
{
    if (fhLog) {
        fflush(fhLog);
    }
}

//adds entry to a log file
void LogEntry(char *fmt, ...)
{
	char LogEntryMaxCount = 1024;
	char LogEntryBuffer[1024] = "";
	va_list ap;

	if (fhLog) {
		va_start(ap, fmt);
		if (logging){
			vfprintf(fhLog, fmt, ap);
		}
		if(consoleOutput){
			//currently doesn't work
			vsnprintf(LogEntryBuffer, LogEntryMaxCount, fmt, ap);
			std::cout << LogEntryBuffer;
		}
		va_end(ap);
	}
}

//logging stuff
static void LogLogContextA(LPLOGCONTEXTA lpCtx)
{
    if (!fhLog)
        return;

    if (!lpCtx) {
        fprintf(fhLog, "lpCtx(A) = NULL\n");
    } else {
        fprintf(fhLog, "lpCtx(A) = %p\n"
            " lpOptions = %x\n"
            " lcStatus = %x\n"
            " lcLocks = %d\n"
            " lcMsgBase = %x\n"
            " lcDevice = %x\n"
            " lcPktRate = %x\n"
            " lcPktData = %x\n"
            " lcPktMode = %x\n"
            " lcMoveMask = %x\n"
            " lcBtnDnMask = %x\n"
            " lcBtnUpMask = %x\n"
            " lcInOrgX = %ld\n"
            " lcInOrgY = %ld\n"
            " lcInOrgZ = %ld\n"
            " lcInExtX = %ld\n"
            " lcInExtY = %ld\n"
            " lcInExtZ = %ld\n"
            " lcOutOrgX = %ld\n"
            " lcOutOrgY = %ld\n"
            " lcOutOrgZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcSensX = %d.%d\n"
            " lcSensY = %d.%d\n"
            " lcSensZ = %d.%d\n"
            " lcSysMode = %d\n"
            " lcSysOrgX = %d\n"
            " lcSysOrgY = %d\n"
            " lcSysExtX = %d\n"
            " lcSysExtY = %d\n"
            " lcSysSensX = %d.%d\n"
            " lcSysSensX = %d.%d\n",
            lpCtx,
            lpCtx->lcOptions,
            lpCtx->lcStatus,
            lpCtx->lcLocks,
            lpCtx->lcMsgBase,
            lpCtx->lcDevice,
            lpCtx->lcPktRate,
            lpCtx->lcPktData,
            lpCtx->lcPktMode,
            lpCtx->lcMoveMask,
            lpCtx->lcBtnDnMask,
            lpCtx->lcBtnUpMask,
            lpCtx->lcInOrgX,
            lpCtx->lcInOrgY,
            lpCtx->lcInOrgZ,
            lpCtx->lcInExtX,
            lpCtx->lcInExtY,
            lpCtx->lcInExtZ,
            lpCtx->lcOutOrgX,
            lpCtx->lcOutOrgY,
            lpCtx->lcOutOrgZ,
            lpCtx->lcOutExtX,
            lpCtx->lcOutExtY,
            lpCtx->lcOutExtZ,
            INT(lpCtx->lcSensX), FRAC(lpCtx->lcSensX),
            INT(lpCtx->lcSensY), FRAC(lpCtx->lcSensY),
            INT(lpCtx->lcSensZ), FRAC(lpCtx->lcSensZ),
            lpCtx->lcSysMode,
            lpCtx->lcSysOrgX,
            lpCtx->lcSysOrgY,
            lpCtx->lcSysExtX,
            lpCtx->lcSysExtY,
            INT(lpCtx->lcSysSensX), FRAC(lpCtx->lcSysSensX),
            INT(lpCtx->lcSysSensY), FRAC(lpCtx->lcSysSensY)
        );
    }
}

static void LogLogContextW(LPLOGCONTEXTW lpCtx)
{
    if (!fhLog)
        return;

    if (!lpCtx) {
        fprintf(fhLog, "lpCtx(W) = NULL\n");
    } else {
        fprintf(fhLog, "lpCtx(W) = %p\n"
            " lpOptions = %x\n"
            " lcStatus = %x\n"
            " lcLocks = %d\n"
            " lcMsgBase = %x\n"
            " lcDevice = %x\n"
            " lcPktRate = %x\n"
            " lcPktData = %x\n"
            " lcPktMode = %x\n"
            " lcMoveMask = %x\n"
            " lcBtnDnMask = %x\n"
            " lcBtnUpMask = %x\n"
            " lcInOrgX = %ld\n"
            " lcInOrgY = %ld\n"
            " lcInOrgZ = %ld\n"
            " lcInExtX = %ld\n"
            " lcInExtY = %ld\n"
            " lcInExtZ = %ld\n"
            " lcOutOrgX = %ld\n"
            " lcOutOrgY = %ld\n"
            " lcOutOrgZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcOutExtX = %ld\n"
            " lcOutExtY = %ld\n"
            " lcOutExtZ = %ld\n"
            " lcSensX = %d.%d\n"
            " lcSensY = %d.%d\n"
            " lcSensZ = %d.%d\n"
            " lcSysMode = %d\n"
            " lcSysOrgX = %d\n"
            " lcSysOrgY = %d\n"
            " lcSysExtX = %d\n"
            " lcSysExtY = %d\n"
            " lcSysSensX = %d.%d\n"
            " lcSysSensX = %d.%d\n",
            lpCtx,
            lpCtx->lcOptions,
            lpCtx->lcStatus,
            lpCtx->lcLocks,
            lpCtx->lcMsgBase,
            lpCtx->lcDevice,
            lpCtx->lcPktRate,
            lpCtx->lcPktData,
            lpCtx->lcPktMode,
            lpCtx->lcMoveMask,
            lpCtx->lcBtnDnMask,
            lpCtx->lcBtnUpMask,
            lpCtx->lcInOrgX,
            lpCtx->lcInOrgY,
            lpCtx->lcInOrgZ,
            lpCtx->lcInExtX,
            lpCtx->lcInExtY,
            lpCtx->lcInExtZ,
            lpCtx->lcOutOrgX,
            lpCtx->lcOutOrgY,
            lpCtx->lcOutOrgZ,
            lpCtx->lcOutExtX,
            lpCtx->lcOutExtY,
            lpCtx->lcOutExtZ,
            INT(lpCtx->lcSensX), FRAC(lpCtx->lcSensX),
            INT(lpCtx->lcSensY), FRAC(lpCtx->lcSensY),
            INT(lpCtx->lcSensZ), FRAC(lpCtx->lcSensZ),
            lpCtx->lcSysMode,
            lpCtx->lcSysOrgX,
            lpCtx->lcSysOrgY,
            lpCtx->lcSysExtX,
            lpCtx->lcSysExtY,
            INT(lpCtx->lcSysSensX), FRAC(lpCtx->lcSysSensX),
            INT(lpCtx->lcSysSensY), FRAC(lpCtx->lcSysSensY)
        );
    }
}

static UINT PacketBytes(UINT data, UINT mode)
{
	UINT n = 0;

	if (data & PK_CONTEXT)
		n += sizeof(HCTX);
	if (data & PK_STATUS)
		n += sizeof(UINT);
	if (data & PK_TIME)
		n += sizeof(DWORD);
	if (data & PK_CHANGED)
		n += sizeof(WTPKT);
	if (data & PK_SERIAL_NUMBER)
		n += sizeof(UINT);
	if (data & PK_CURSOR)
		n += sizeof(UINT);
	if (data & PK_BUTTONS)
		n += sizeof(UINT);
	if (data & PK_X)
		n += sizeof(LONG);
	if (data & PK_Y)
		n += sizeof(LONG);
	if (data & PK_Z)
		n += sizeof(LONG);
	if (data & PK_NORMAL_PRESSURE) {
		if (mode & PK_NORMAL_PRESSURE)
			n += sizeof(int);
		else
			n += sizeof(UINT);
	}
	if (data & PK_TANGENT_PRESSURE) {
		if (mode & PK_TANGENT_PRESSURE)
			n += sizeof(int);
		else
			n += sizeof(UINT);
	}
	if (data & PK_ORIENTATION)
		n += sizeof(ORIENTATION);
	if (data & PK_ROTATION)
		n += sizeof(ROTATION);

	return n;
}

static void LogPacket(UINT data, UINT mode, LPVOID lpData)
{
    BYTE *bData = (BYTE *)lpData;

    if (!fhLog)
        return;

    fprintf(fhLog, "packet = %p (data=%d, mode=%x)\n", lpData, data, mode);

	if (data & PK_CONTEXT) {
		fprintf(fhLog, " PK_CONTEXT = %p\n", *((HCTX *)bData));
        bData += sizeof(HCTX);
    }
	if (data & PK_STATUS) {
		fprintf(fhLog, " PK_STATUS = %x\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_TIME) {
		fprintf(fhLog, " PK_TIME = %d\n", *((DWORD *)bData));
        bData += sizeof(DWORD);
    }
	if (data & PK_CHANGED) {
		fprintf(fhLog, " PK_CHANGED = %x\n", *((WTPKT *)bData));
        bData += sizeof(WTPKT);
    }
	if (data & PK_SERIAL_NUMBER) {
		fprintf(fhLog, " PK_SERIAL_NUMBER = %u\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_CURSOR) {
		fprintf(fhLog, " PK_CURSOR = %u\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_BUTTONS) {
		fprintf(fhLog, " PK_BUTTONS = %x\n", *((UINT *)bData));
        bData += sizeof(UINT);
    }
	if (data & PK_X) {
		fprintf(fhLog, " PK_X = %ld\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_Y) {
		fprintf(fhLog, " PK_Y = %d\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_Z) {
		fprintf(fhLog, " PK_Z = %d\n", *((LONG *)bData));
        bData += sizeof(LONG);
    }
	if (data & PK_NORMAL_PRESSURE) {
		if (mode & PK_NORMAL_PRESSURE) {
		    fprintf(fhLog, " PK_NORMAL_PRESSURE = %d\n", *((int *)bData));
            bData += sizeof(int);
		} else {
		    fprintf(fhLog, " PK_NORMAL_PRESSURE = %d\n", *((UINT *)bData));
            bData += sizeof(UINT);
        }
	}
	if (data & PK_TANGENT_PRESSURE) {
		if (mode & PK_TANGENT_PRESSURE) {
		    fprintf(fhLog, " PK_TANGENT_PRESSURE = %d\n", *((int *)bData));
            bData += sizeof(int);
		} else {
		    fprintf(fhLog, " PK_TANGENT_PRESSURE = %d\n", *((UINT *)bData));
            bData += sizeof(UINT);
        }
	}
	if (data & PK_ORIENTATION) {
		ORIENTATION *o = (ORIENTATION *)bData;
		fprintf(fhLog, " PK_ORIENTATION = %d %d %d\n", o->orAzimuth, o->orAltitude, o->orTwist);
        bData += sizeof(ORIENTATION);
    }
	if (data & PK_ROTATION) {
		ROTATION *r = (ROTATION *)bData;
		fprintf(fhLog, " PK_ROTATION = %d %d %d\n", r->roPitch, r->roRoll, r->roYaw);
        bData += sizeof(ROTATION);
    }
}

static void LogBytes(LPVOID lpData, UINT nBytes)
{
	BYTE *dataPtr = (BYTE *)lpData;

	if (!fhLog)
		return;

	fprintf(fhLog, "data =");
	if (dataPtr) {
		UINT n;
		for (n = 0; n < nBytes; ++n) {
			fprintf(fhLog, " %02x", dataPtr[n]);
		}
	} else {
		fprintf(fhLog, " NULL");
	}
	fprintf(fhLog, "\n");
}

//BOOL WritePrivateProfileInt(LPCTSTR lpAppName, LPCTSTR lpKeyName, int Value, LPCTSTR lpFileName){
//	char Buffer[256];
//	//_itoa(Value, Buffer, 10);
//	sprintf(Buffer, "%i", Value);
//	return WritePrivateProfileString(lpAppName, lpKeyName, (LPCWSTR)Buffer, lpFileName);
//}


// gets ini path?
static void getINIPath(TCHAR *out, UINT length)
{
    TCHAR pwd[MAX_PATH];
    
    GetCurrentDirectory(MAX_PATH, pwd);
    _sntprintf_s(out, length, _TRUNCATE, _T("%s\\%s"), pwd, INI_FILE);
}

// MMMkay, it sets defaults before loading .ini file
static void SetDefaults(emu_settings_t *settings)
{
	//these are the switches
	settings->tuio_udp = 1;
	settings->tuio_udp_port = 3333;
	settings->tuio_tcp = 0;
	settings->tuio_tcp_port = 3000;
	settings->tuio_mouse = 0; //0=nomouse,1=mouseOnly,2=mousePlusWintab

    settings->disableFeedback       = TRUE;
    settings->disableGestures       = TRUE;
    settings->shiftX                = 0;
    settings->shiftY                = 0;
    settings->pressureExpand        = TRUE;
    settings->pressureMin           = 0;
    settings->pressureMax           = 1023;
    settings->pressureCurve         = FALSE;
    //looks like it recognises 5 lvls of pressure
    settings->pressurePoint[0]      = 0;
    settings->pressurePoint[1]      = 253;
    settings->pressurePoint[2]      = 511;
    settings->pressurePoint[3]      = 767;
    settings->pressurePoint[4]      = 1023;
    settings->pressureValue[0]      = 0;
    settings->pressureValue[1]      = 253;
    settings->pressureValue[2]      = 511;
    settings->pressureValue[3]      = 767;
    settings->pressureValue[4]      = 1023;

	//these are calculated or environment based one way or another
	settings->screen_width = 1024;
	settings->screen_height = 768;
	settings->tuio_x = 0.0;
	settings->tuio_y = 0.0;
	settings->tuio_w = 1.0;
	settings->tuio_h = 1.0;

	settings->wintab_x = 0;
	settings->wintab_y = 0;
	settings->wintab_w = 0xffff;
	settings->wintab_h = 0xffff;

	//these are set in stone, but it doesn't hurt to include em
	settings->mouse_x = 0;
	settings->mouse_y = 0;
	settings->mouse_w = 0xffff;
	settings->mouse_h = 0xffff;
	settings->tablet_height = 0xffff;
	settings->tablet_width = 0xffff;
	logFile = DEFAULT_LOG_FILE;
}

// this whole wall of text just to load .ini file
// need to make this part work for my config settings to make .ini useful
static void LoadSettings(emu_settings_t *settings)
{
    const UINT stringLength = MAX_PATH;
    TCHAR iniPath[MAX_PATH];
    DWORD dwRet;
    UINT nRet;
	float fRet;

    getINIPath(iniPath, MAX_PATH);
    
    nRet = GetPrivateProfileInt(
        _T("Logging"),
        _T("Mode"),
        logging ? 1 : 0,
        iniPath
    );
    logging = (nRet != 0);

    logFile = (TCHAR *) malloc(sizeof(TCHAR) * stringLength);
    dwRet = GetPrivateProfileString(
        _T("Logging"),
        _T("LogFile"),
        DEFAULT_LOG_FILE,
        logFile,
        stringLength,
        iniPath
    );


	//we can't normaly store floating point numbers in ini files with these
	//built in methods and libraries are a pain in the ass, so we just store
	//it like this tuio_x=0507234 tuio_w=9130515 and divide those by 10000000 to get the floats
	//lame? i don't give a fuck!



	settings->tuio_mouse = GetPrivateProfileInt(
		_T("Switches"),
		_T("tuio_mouse"),
		settings->tuio_mouse,
		iniPath
		);
	settings->tuio_udp = GetPrivateProfileInt(
		_T("Switches"),
		_T("tuio_udp"),
		settings->tuio_udp,
		iniPath
		);
	settings->tuio_udp_port = GetPrivateProfileInt(
		_T("Switches"),
		_T("tuio_udp_port"),
		settings->tuio_udp_port,
		iniPath
		);
	settings->tuio_tcp = GetPrivateProfileInt(
		_T("Switches"),
		_T("tuio_tcp"),
		settings->tuio_tcp,
		iniPath
		);
	settings->tuio_tcp_port = GetPrivateProfileInt(
		_T("Switches"),
		_T("tuio_tcp_port"),
		settings->tuio_tcp_port,
		iniPath
		);


	nRet = GetPrivateProfileInt(
		_T("Metrics"), _T("tuio_x"), -1, iniPath
		);
	if (nRet != -1){
		settings->tuio_x = (float)nRet / 10000000;
	}

	nRet = GetPrivateProfileInt(
		_T("Metrics"),_T("tuio_y"),-1,iniPath
	);
	if (nRet != -1){
		settings->tuio_y = (float)nRet / 10000000;
	}

	nRet = GetPrivateProfileInt(
		_T("Metrics"), _T("tuio_w"), -1, iniPath
		);
	if (nRet != -1){
		settings->tuio_w = (float)nRet / 10000000;
	}

	nRet = GetPrivateProfileInt(
		_T("Metrics"), _T("tuio_h"), -1, iniPath
		);
	if (nRet != -1){
		settings->tuio_h = (float)nRet / 10000000;
	}


	settings->wintab_x = GetPrivateProfileInt(
		_T("Metrics"),
		_T("wintab_x"),
		settings->wintab_x,
		iniPath
	);
	settings->wintab_y = GetPrivateProfileInt(
		_T("Metrics"),
		_T("wintab_y"),
		settings->wintab_y,
		iniPath
	);
	settings->wintab_w = GetPrivateProfileInt(
		_T("Metrics"),
		_T("wintab_w"),
		settings->wintab_w,
		iniPath
	);
	settings->wintab_h = GetPrivateProfileInt(
		_T("Metrics"),
		_T("wintab_h"),
		settings->wintab_h,
		iniPath
	);



	settings->mouse_x = GetPrivateProfileInt(
		_T("Metrics"),
		_T("mouse_x"),
		settings->mouse_x,
		iniPath
	);
	settings->mouse_y = GetPrivateProfileInt(
		_T("Metrics"),
		_T("mouse_y"),
		settings->mouse_y,
		iniPath
	);
	settings->mouse_w = GetPrivateProfileInt(
		_T("Metrics"),
		_T("mouse_w"),
		settings->mouse_w,
		iniPath
	);
	settings->mouse_h = GetPrivateProfileInt(
		_T("Metrics"),
		_T("mouse_h"),
		settings->mouse_h,
		iniPath
	);

	settings->tablet_height = GetPrivateProfileInt(
		_T("Metrics"),
		_T("tablet_height"),
		settings->tablet_height,
		iniPath
	);
	settings->tablet_width = GetPrivateProfileInt(
		_T("Metrics"),
		_T("tablet_width"),
		settings->tablet_width,
		iniPath
	);

    //nRet = GetPrivateProfileInt(
    //    _T("Emulation"),
    //    _T("Mode"),
    //    useEmulation ? 1 : 0,
    //    iniPath
    //);
    //useEmulation = (nRet != 0);
    
    //nRet = GetPrivateProfileInt(
    //    _T("Emulation"),
    //    _T("Debug"),
    //    debug ? 1 : 0,
    //    iniPath
    //);
    //debug = (nRet != 0);

    //wintabDLL = (TCHAR *) malloc(sizeof(TCHAR) * stringLength);
    //dwRet = GetPrivateProfileString(
    //    _T("Emulation"),
    //    _T("WintabDLL"),
    //    DEFAULT_LOG_FILE,
    //    wintabDLL,
    //    stringLength,
    //    iniPath
    //);
    //
    //nRet = GetPrivateProfileInt(
    //    _T("Emulation"),
    //    _T("DisableFeedback"),
    //    settings->disableFeedback ? 1 : 0,
    //    iniPath
    //);
    //settings->disableFeedback = (nRet != 0);
    //
    //nRet = GetPrivateProfileInt(
    //    _T("Emulation"),
    //    _T("DisableGestures"),
    //    settings->disableGestures ? 1 : 0,
    //    iniPath
    //);
    //settings->disableGestures = (nRet != 0);
    //
    //// Adjustment of positions
    //settings->shiftX = GetPrivateProfileInt(
    //    _T("Adjust"), _T("ShiftX"),
    //    settings->shiftX,
    //    iniPath
    //);
    //settings->shiftY = GetPrivateProfileInt(
    //    _T("Adjust"), _T("ShiftY"),
    //    settings->shiftY,
    //    iniPath
    //);
    //
    //// Pressure clamping
    //nRet = GetPrivateProfileInt(
    //    _T("Adjust"),
    //    _T("PressureExpand"),
    //    settings->pressureExpand ? 1 : 0,
    //    iniPath
    //);
    //settings->pressureExpand = (nRet != 0);
    //
    //settings->pressureMin = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureMin"),
    //    settings->pressureMin,
    //    iniPath
    //);
    //settings->pressureMax = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureMax"),
    //    settings->pressureMax,
    //    iniPath
    //);

    //// Pressure curve
    //nRet = GetPrivateProfileInt(
    //    _T("Adjust"),
    //    _T("PressureCurve"),
    //    settings->pressureCurve ? 1 : 0,
    //    iniPath
    //);
    //settings->pressureCurve = (nRet != 0);

    //settings->pressurePoint[0] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP0"),
    //    settings->pressurePoint[0],
    //    iniPath
    //);
    //settings->pressureValue[0] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP0V"),
    //    settings->pressureValue[0],
    //    iniPath
    //);
    //settings->pressurePoint[1] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP1"),
    //    settings->pressurePoint[1],
    //    iniPath
    //);
    //settings->pressureValue[1] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP1V"),
    //    settings->pressureValue[1],
    //    iniPath
    //);
    //settings->pressurePoint[2] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP2"),
    //    settings->pressurePoint[2],
    //    iniPath
    //);
    //settings->pressureValue[2] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP2V"),
    //    settings->pressureValue[2],
    //    iniPath
    //);
    //settings->pressurePoint[3] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP3"),
    //    settings->pressurePoint[3],
    //    iniPath
    //);
    //settings->pressureValue[3] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP3V"),
    //    settings->pressureValue[3],
    //    iniPath
    //);
    //settings->pressurePoint[4] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP4"),
    //    settings->pressurePoint[4],
    //    iniPath
    //);
    //settings->pressureValue[4] = GetPrivateProfileInt(
    //    _T("Adjust"), _T("PressureCurveP4V"),
    //    settings->pressureValue[4],
    //    iniPath
    //);
}

// looks like this code is used everywhere
// so if any function is called it first goes here and checks if wintab is initialised and if not initialises it.
// removing static from this one to be able to call it from dllmain directly
void Init(void)
{
    // defines variable to store settings and loads it from .ini file
    emu_settings_t settings;
    if (initialised) return;
    SetDefaults(&settings);
	// let's LOG!
    LoadSettings(&settings);
	OpenLogFile();

	LogEntry(
		"tuio_mouse = %d, wintab_x = %d, wintab_y = %d, wintab_w = %d, wintab_h = %d, \n tuio_x = %f, tuio_y = %f, tuio_w = %f, tuio_h = %f, \n mouse_x = %d, mouse_y = %d, mouse_w = %d, mouse_h = %d \n tablet_height = %d, tablet_width = %d \n",
		settings.tuio_mouse, settings.wintab_x, settings.wintab_y, settings.wintab_w, settings.wintab_h,
		settings.tuio_x, settings.tuio_y, settings.tuio_w, settings.tuio_h,
		settings.mouse_x, settings.mouse_y, settings.mouse_w, settings.mouse_h,
		settings.tablet_height, settings.tablet_width
	);
	
    LogEntry(
        "init, logging = %d, debug = %d, useEmulation = %d\n", 
        logging, debug, useEmulation
    );

    // switch in case we don't need this whole program at all
    //if (useEmulation) {
        // I've put some init in your init, YAY!
        emuInit(logging, debug, &settings);
    //} else {
        // oh well, no thanks
        //LoadWintab(wintabDLL);
    //}

    // no need to initialise again
    initialised = TRUE;
}

// Referenced nowhere here //FIXME arrange for this to be called?
static void Shutdown(void)
{
    LogEntry("shutdown started\n");
    
    if (useEmulation) {
        emuShutdown();
    } else {
        UnloadWintab();
    }
    
    LogEntry("shutdown finished\n");
    
    if (fhLog) {
        fclose(fhLog);
        fhLog = NULL;
    }
    if (logFile) {
        free(logFile);
        logFile = NULL;
    }
    if (wintabDLL) {
        free(wintabDLL);
        wintabDLL = NULL;
    }
}

// those are just wrappers that point to Emulation.cpp file that handles all the stuff
// MMMkay, so all these functions are used both in emulation mode and in original mode. fine, I'm finished here

//1, identification info, specification and soft version, tablet vendor and model
//2, capability information, dimensions, resolutions, features, cursor types.
//3, categories that give defaults for all tablet Context attributes.
UINT API WTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    Init();
	UINT ret = 0;
    //if (useEmulation) {
        ret = emuWTInfoA(wCategory, nIndex, lpOutput);
    //} else if (hOrigWintab) {
    //    ret = origWTInfoA(wCategory, nIndex, lpOutput);
    //}
		LogEntry("WinTab Info [ASCII] %d->%d, %s->%s,  %p returned 0x%X\n", wCategory, nIndex, category_string(wCategory, 0), category_string(wCategory, nIndex), lpOutput, ret);
	   // LogBytes(lpOutput, ret);
	return ret;
}
UINT API WTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
	Init();
	UINT ret = 0;
    //if (useEmulation) {
        ret = emuWTInfoW(wCategory, nIndex, lpOutput);
    //} else if (hOrigWintab) {
    //    ret = origWTInfoW(wCategory, nIndex, lpOutput);
    //}
		LogEntry("WinTab Info [UTF8] %d->%d, %s->%s,  %p returned 0x%X\n", wCategory, nIndex, category_string(wCategory, 0), category_string(wCategory, nIndex), lpOutput, ret);
	   // LogBytes(lpOutput, ret);
    //}
	return ret;
}

HCTX API WTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();
    
    if (useEmulation) {
        ret = emuWTOpenA(hWnd, lpLogCtx, fEnable);
    } else if (hOrigWintab) {
        ret = origWTOpenA(hWnd, lpLogCtx, fEnable);
    }
        
    if (lpLogCtx) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
	LogEntry("WTOpenA(%x, %p, %d) = %x\n", hWnd, lpLogCtx, fEnable, ret);
	//    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
	//	LogLogContextA(lpLogCtx);


	return ret;
}
HCTX API WTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();

    if (useEmulation) {
	    ret = emuWTOpenW(hWnd, lpLogCtx, fEnable);
    } else if (hOrigWintab) {
	    ret = origWTOpenW(hWnd, lpLogCtx, fEnable);
    }
    
    if (lpLogCtx) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
    
	LogEntry("WTOpenW(%x, %p, %d) = %x\n", hWnd, lpLogCtx, fEnable, ret);
	 //   LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		//LogLogContextW(lpLogCtx);
	return ret;
}

BOOL API WTClose(HCTX hCtx)
{
	BOOL ret = FALSE;
    
    Init();

    //if (useEmulation) {
        ret = emuWTClose(hCtx);
    //} else if (hOrigWintab) {
    //    ret = origWTClose(hCtx);
    //}

    LogEntry("WTClose(%x) = %d\n", hCtx, ret);

	return ret;
}

int API WTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt)
{
	int ret = 0;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTPacketsGet(hCtx, cMaxPkts, lpPkt);
    //} else if (hOrigWintab) {
    //    ret = origWTPacketsGet(hCtx, cMaxPkts, lpPkt);
    //}

    LogEntry("WTPacketGet(%x, %d, %p) = %d\n", hCtx, cMaxPkts, lpPkt, ret);
	   // if (ret > 0)
		  //  LogBytes(lpPkt, PacketBytes(packetData, packetMode) * ret);
    return ret;
}

BOOL API WTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt)
{
	BOOL ret = FALSE;
    Init();
    
    //if (useEmulation) {
        ret = emuWTPacket(hCtx, wSerial, lpPkt);
    //} else if (hOrigWintab) {
    //    ret = origWTPacket(hCtx, wSerial, lpPkt);
    //}

    //LogEntry("WTPacket(%x, %x, %p) = %d\n", hCtx, wSerial, lpPkt, ret);
    //    if (ret) {
    //        LogBytes(lpPkt, PacketBytes(packetData, packetMode));
    //    }

	return ret;
}

BOOL API WTEnable(HCTX hCtx, BOOL fEnable)
{
	BOOL ret = FALSE;
    Init();
    //if (useEmulation) {
	    ret = emuWTEnable(hCtx, fEnable);
    //} else if (hOrigWintab) {
	   // ret = origWTEnable(hCtx, fEnable);
    //}

    LogEntry("WTEnable(%x, %d) = %d\n", hCtx, fEnable, ret);

    return ret;
}

BOOL API WTOverlap(HCTX hCtx, BOOL fToTop)
{
	BOOL ret = FALSE;
    Init();
    
    //if (useEmulation) {
        ret = emuWTOverlap(hCtx, fToTop);
    //} else if (hOrigWintab) {
    //    ret = origWTOverlap(hCtx, fToTop);
    //}
    //
	LogEntry("WTOverlap(%x, %d) = %d\n", hCtx, fToTop, ret);

	return ret;
}

BOOL API WTConfig(HCTX hCtx, HWND hWnd)
{
	BOOL ret = FALSE;
    Init();
    
    //if (useEmulation) {
        ret = emuWTConfig(hCtx, hWnd);
    //} else if (hOrigWintab) {
    //    ret = origWTConfig(hCtx, hWnd);
    //}

    LogEntry("WTConfig(%x, %x) = %d\n", hCtx, hWnd, ret);
	return ret;
}


BOOL API WTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTGetA(hCtx, lpLogCtx);
 //   } else if (hOrigWintab) {
 //       ret = origWTGetA(hCtx, lpLogCtx);
	//}
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }

    LogEntry("WTGetA(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
    //    LogBytes(lpLogCtx, sizeof(*lpLogCtx));
    //    LogLogContextA(lpLogCtx);

	return ret;
}
BOOL API WTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTGetW(hCtx, lpLogCtx);
    //} else if (hOrigWintab) {
    //    ret = origWTGetW(hCtx, lpLogCtx);
    //}
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }

	LogEntry("WTGetW(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	 //   LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		//LogLogContextW(lpLogCtx);

	return ret;
}


BOOL API WTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTSetA(hCtx, lpLogCtx);
    //} else if (hOrigWintab) {
    //    ret = origWTSetA(hCtx, lpLogCtx);
    //}
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
	
	LogEntry("WTSetA(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	 //   LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		//LogLogContextA(lpLogCtx);

	return ret;
}
BOOL API WTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTSetW(hCtx, lpLogCtx);
    //} else if (hOrigWintab) {
    //    ret = origWTSetW(hCtx, lpLogCtx);
    //}
    
    if (lpLogCtx && ret) {
        // snoop packet mode
        packetData = lpLogCtx->lcPktData;
        packetMode = lpLogCtx->lcPktMode;
    }
	
	LogEntry("WTSetW(%x, %p) = %d\n", hCtx, lpLogCtx, ret);
	 //   LogBytes(lpLogCtx, sizeof(*lpLogCtx));
		//LogLogContextW(lpLogCtx);

    return ret;
}

BOOL API WTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTExtGet(hCtx, wExt, lpData);
    //} else if (hOrigWintab) {
    //    ret = origWTExtGet(hCtx, wExt, lpData);
    //}

    LogEntry("WTExtGet(%x, %x, %p) = %d\n", hCtx, wExt, lpData, ret);
    //    //LogBytes(lpLogCtx, sizeof(*lpLogCtx));

	return ret;
}
BOOL API WTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTExtSet(hCtx, wExt, lpData);
    //} else if (hOrigWintab) {
    //    ret = origWTExtSet(hCtx, wExt, lpData);
    //}

    LogEntry("WTExtSet(%x, %x, %p) = %d\n", hCtx, wExt, lpData, ret);
	   // //LogBytes(lpLogCtx, sizeof(*lpLogCtx));

	return ret;
}

BOOL API WTSave(HCTX hCtx, LPVOID lpData)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTSave(hCtx, lpData);
    //} else if (hOrigWintab) {
    //    ret = origWTSave(hCtx, lpData);
    //}

    LogEntry("WTSave(%x, %p) = %d\n", hCtx, lpData, ret);
	   // //LogBytes(lpLogCtx, sizeof(*lpLogCtx));

	return ret;
}
HCTX API WTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable)
{
	HCTX ret = NULL;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTRestore(hWnd, lpSaveInfo, fEnable);
    //} else if (hOrigWintab) {
    //    ret = origWTRestore(hWnd, lpSaveInfo, fEnable);
    //}

    LogEntry("WTRestore(%x, %p, %d) = %x\n", hWnd, lpSaveInfo, fEnable, ret);
	   // //LogBytes(lpLogCtx, sizeof(*lpLogCtx));

	return ret;
}

int API WTPacketsPeek(HCTX hWnd, int cMaxPkt, LPVOID lpPkts)
{
	int ret = 0;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTPacketsPeek(hWnd, cMaxPkt, lpPkts);
    //} else if (hOrigWintab) {
    //    ret = origWTPacketsPeek(hWnd, cMaxPkt, lpPkts);
    //}

    LogEntry("WTPacketsPeek(%x, %d, %p) = %d\n", hWnd, cMaxPkt, lpPkts, ret);
    //    if (ret > 0)
    //        LogBytes(lpPkts, PacketBytes(packetData, packetMode) * ret);

	return ret;
}

int API WTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTDataGet(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    //} else if (hOrigWintab) {
    //    ret = origWTDataGet(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    //}
    //
	LogEntry("WTDataGet(%x, %x, %x, %d, %p, %p) = %d\n", hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts, ret);
    //    if (lpNPkts)
    //        LogBytes(lpPkts, PacketBytes(packetData, packetMode) * (*lpNPkts));

	return ret;
}
int API WTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
	int ret = 0;
    
    Init();
    
   // if (useEmulation) {
        ret = emuWTDataPeek(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    //} else if (hOrigWintab) {
    //    ret = origWTDataPeek(hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts);
    //}

    LogEntry("WTDataPeek(%x, %x, %x, %d, %p, %p) = %d\n", hCtx, wBegin, wEnd, cMaxPkts, lpPkts, lpNPkts, ret);
    //    if (lpNPkts)
    //        LogBytes(lpPkts, PacketBytes(packetData, packetMode) * (*lpNPkts));
    //}
	return ret;
}

int API WTQueueSizeGet(HCTX hCtx)
{
	int ret = 0;
    
    Init();
    
   // if (useEmulation) {
        ret = emuWTQueueSizeGet(hCtx);
    //} else if (hOrigWintab) {
    //    ret = origWTQueueSizeGet(hCtx);
    //}


	LogEntry("WTQueueSizeGet(%x) = %d\n", hCtx, ret);
	return ret;
}
BOOL API WTQueueSizeSet(HCTX hCtx, int nPkts)
{
	BOOL ret = FALSE;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTQueueSizeSet(hCtx, nPkts);
    //} else if (hOrigWintab) {
    //    ret = origWTQueueSizeSet(hCtx, nPkts);
    //}
    LogEntry("WTQueueSizeSet(%x, %d) = %d\n", hCtx, nPkts, ret);

	return ret;
}

HMGR API WTMgrOpen(HWND hWnd, UINT wMsgBase)
{
	HMGR ret = NULL;
    
    Init();
    
    //if (useEmulation) {
        ret = emuWTMgrOpen(hWnd, wMsgBase);
 //   } else if (hOrigWintab) {
 //       ret = origWTMgrOpen(hWnd, wMsgBase);
 //   }

	LogEntry("WTMgrOpen(%x, %x) = %x\n", hWnd, wMsgBase, ret);
    return ret;
}
BOOL API WTMgrClose(HMGR hMgr)
{
	BOOL ret = FALSE;
    
    Init();
    
   // if (useEmulation) {
	    ret = emuWTMgrClose(hMgr);
    //} else if (hOrigWintab) {
	   // ret = origWTMgrClose(hMgr);
    //}
	
    LogEntry("WTMgrClose(%x) = %d\n", hMgr, ret);
	return ret;
}

    // XXX: unsupported
BOOL API WTMgrContextEnum(HMGR hMgr, WTENUMPROC lpEnumFunc, LPARAM lParam)
{
    Init();
    LogEntry("Unsupported WTMgrContextEnum(%x, %p, %x)\n", hMgr, lpEnumFunc, lParam);
	return FALSE;
}
    // XXX: unsupported
HWND API WTMgrContextOwner(HMGR hMgr, HCTX hCtx)
{
    Init();
	LogEntry("Unsupported WTMgrContextOwner(%x, %x)\n", hMgr, hCtx);
	return NULL;
}
    // XXX: unsupported
HCTX API WTMgrDefContext(HMGR hMgr, BOOL fSystem)
{
    Init();
	LogEntry("Unsupported WTMgrDefContext(%x, %d)\n", hMgr, fSystem);
	return NULL;
}
    // XXX: unsupported
HCTX API WTMgrDefContextEx(HMGR hMgr, UINT wDevice, BOOL fSystem)
{
    Init();
	LogEntry("Unsupported WTMgrDefContextEx(%x, %x, %d)\n", hMgr, wDevice, fSystem);
	return NULL;
}
    // XXX: unsupported
UINT API WTMgrDeviceConfig(HMGR hMgr, UINT wDevice, HWND hWnd)
{
    Init();
	LogEntry("Unsupported WTMgrDeviceConfig(%x, %x, %x)\n", hMgr, wDevice, hWnd);
	return 0;
}
    // XXX: unsupported
BOOL API WTMgrExt(HMGR hMgr, UINT wParam1, LPVOID lpParam1)
{
    Init();
	LogEntry("Unsupported WTMgrExt(%x, %p, %x)\n", hMgr, wParam1, lpParam1);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrCsrEnable(HMGR hMgr, UINT wParam1, BOOL fParam1)
{
    Init();
	LogEntry("Unsupported WTMgrCsrEnable(%x, %x, %d)\n", hMgr, wParam1, fParam1);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrCsrButtonMap(HMGR hMgr, UINT wCursor, LPBYTE lpLogBtns, LPBYTE lpSysBtns)
{
    Init();
	LogEntry("Unsupported WTMgrCsrButtonMap(%x, %x, %p, %x)\n", hMgr, wCursor, lpLogBtns, lpSysBtns);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrCsrPressureBtnMarks(HMGR hMgr, UINT wCsr, DWORD lpNMarks, DWORD lpTMarks)
{
    Init();
	LogEntry("Unsupported WTMgrCsrPressureBtnMarks(%x, %x, %x, %x)\n", hMgr, wCsr, lpNMarks, lpTMarks);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrCsrPressureResponse(HMGR hMgr, UINT wCsr, UINT FAR *lpNResp, UINT FAR *lpTResp)
{
    Init();
	LogEntry("Unsupported WTMgrCsrPressureResponse(%x, %x, %p, %p)\n", hMgr, wCsr, lpNResp, lpTResp);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrCsrExt(HMGR hMgr, UINT wCsr, UINT wParam1, LPVOID lpParam1)
{
    Init();
	LogEntry("Unsupported WTMgrCsrExt(%x, %x, %x, %p)\n", hMgr, wCsr, wParam1, lpParam1);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTQueuePacketsEx(HCTX hCtx, UINT FAR *lpParam1, UINT FAR *lpParam2)
{
    Init();
	LogEntry("Unsupported WTQueuePacketsEx(%x, %p, %p)\n", hCtx, lpParam1, lpParam2);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrConfigReplaceExA(HMGR hMgr, BOOL fParam1, LPSTR lpParam1, LPSTR lpParam2)
{
    Init();
	LogEntry("Unsupported WTMgrConfigReplaceExA(%x, %d, %p, %p)\n", hMgr, fParam1, lpParam1, lpParam2);
	return FALSE;
}
    // XXX: unsupported
BOOL API WTMgrConfigReplaceExW(HMGR hMgr, BOOL fParam1, LPWSTR lpParam1, LPSTR lpParam2)
{
    Init();
	LogEntry("Unsupported WTMgrConfigReplaceExW(%x, %d, %p, %p)\n", hMgr, fParam1, lpParam1, lpParam2);
	return FALSE;
}
// XXX: unsupported
HWTHOOK API WTMgrPacketHookExA(HMGR hMgr, int cParam1, LPSTR lpParam1, LPSTR lpParam2)
{
    Init();
	LogEntry("Unsupported WTMgrPacketHookExA(%x, %d, %p, %p)\n", hMgr, cParam1, lpParam1, lpParam2);
	return NULL;
}
// XXX: unsupported
HWTHOOK API WTMgrPacketHookExW(HMGR hMgr, int cParam1, LPWSTR lpParam1, LPSTR lpParam2)
{
    Init();
	LogEntry("Unsupported WTMgrPacketHookExW(%x, %d, %p, %p)\n", hMgr, cParam1, lpParam1, lpParam2);
	return NULL;
}
// XXX: unsupported
BOOL API WTMgrPacketUnhook(HWTHOOK hWTHook)
{
    Init();
	LogEntry("Unsupported WTMgrPacketUnhook(%x)\n", hWTHook);
	return FALSE;
}
// XXX: unsupported
LRESULT API WTMgrPacketHookNext(HWTHOOK hWTHook, int cParam1, WPARAM wParam1, LPARAM lpParam1)
{
    Init();
	LogEntry("Unsupported WTMgrPacketHookNext(%x, %d, %x, %x)\n", hWTHook, cParam1, wParam1, lpParam1);
	return NULL;
}
// XXX: unsupported
BOOL API WTMgrCsrPressureBtnMarksEx(HMGR hMgr, UINT wCsr, UINT FAR *lpParam1, UINT FAR *lpParam2)
{
    Init();
	LogEntry("Unsupported WTMgrCsrPressureBtnMarksEx(%x, %x, %p, %p)\n", hMgr, wCsr, lpParam1, lpParam2);
	return FALSE;
}

