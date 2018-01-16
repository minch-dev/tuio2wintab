/*
	TUIO C++ Example - part of the reacTIVision project
	http://reactivision.sourceforge.net/

	Copyright (c) 2005-2017 Martin Kaltenbrunner <martin@tuio.org>

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
WintabEmulator - Emulation.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/

#include "stdafx.h"
#include "TuioToWinTab.h"

//
#include <tlhelp32.h>
#include <assert.h>

#include "wintab.h"
#include "pktdef.h"
#include "logging.h"
#include <wchar.h>

//my mistake is that I treat a DLL as a program running in memory, while DLL is just a library of functions\
// that means there is no starting point here, external program wants some function -> gets it from this library -> it runs and returns results.
// well, kind of, it must be more complicated than that

//this is a touch input signature
#define MI_WP_SIGNATURE 0xFF515700
#define MI_SIGNATURE_MASK 0xFFFFFF00
#define IsPenEvent(dw) (((dw) & MI_SIGNATURE_MASK) == MI_WP_SIGNATURE)


#define MAX_STRING_BYTES LC_NAMELEN
#define MAX_HOOKS 16
#define MAX_POINTERS 4
#define TIME_CLOSE_MS 2

#define MOUSE_POINTER_ID 1
BOOL LDOWN = FALSE;
BOOL RDOWN = FALSE;
BOOL MDOWN = FALSE;
BOOL TUIOCURSOR = FALSE;

static BOOL logging = TRUE;
static BOOL debug = TRUE;

typedef struct _packet_data_t {
    UINT serial;
    UINT contact;
    LONG time;
    DWORD x, y;
    DWORD buttons;
    UINT pressure;
    UINT pad[9];
} packet_data_t;

typedef struct _hook_t {
    HHOOK handle;
    DWORD thread;
} hook_t;


static BOOL enabled = FALSE;
static BOOL processing = FALSE;
static emu_settings_t config;
static HMODULE module = NULL;

static HWND window = NULL;
static HWND real_window = NULL;
static hook_t hooks[MAX_HOOKS];

static UINT32 pointers[MAX_POINTERS];
static UINT n_pointers = 0;

static packet_data_t *queue = NULL;
static UINT next_serial = 1;
static UINT q_start, q_end, q_length;
static CRITICAL_SECTION q_lock;
static UINT tablet_height = 0xffff; //0xffff
static UINT tablet_width = 0xffff;

int screen_width = GetSystemMetrics(SM_CXSCREEN);
int screen_height = GetSystemMetrics(SM_CYSCREEN);

static LOGCONTEXTA default_context;
static LPLOGCONTEXTA context = NULL;

std::vector<LPLOGCONTEXTA> ctx;

static std::string _address("localhost");
static bool _udp = true;
static int _port = 3333;
static BOOL listening = FALSE;
static UINT32 max_pressure = 1023;
static UINT32 min_pressure = 0;
//static LPLOGCONTEXTA contexts[MAX_CONTEXTS];



// check if our vector has needed context
BOOL ctxHas(HCTX hCtx){
	context = (LPLOGCONTEXTA)hCtx;
	return hCtx && std::find(ctx.begin(), ctx.end(), context) != ctx.end();
}

// EX emulation.cpp
// initialises wintab Context (for our emulation purposes?)
static void init_context(LOGCONTEXTA *ctx)
{
    strncpy_s(ctx->lcName, "Windows", LC_NAMELEN);
    ctx->lcOptions = CXO_SYSTEM | CXO_PEN;
    ctx->lcStatus = 0;
    ctx->lcLocks = 0;
    ctx->lcMsgBase = 0x7ff0;
    ctx->lcDevice = 0;
    ctx->lcPktRate = 64;
	ctx->lcPktData = PK_CURSOR | PK_X | PK_Y | PK_BUTTONS; // | PK_NORMAL_PRESSURE
    ctx->lcPktMode = 0;
    ctx->lcMoveMask = ctx->lcPktData;
	ctx->lcBtnDnMask = 0xF8; // 11111000 5 btns for 5 btn pucks
	ctx->lcBtnUpMask = 0xF8; // 11111000 5 btns
    ctx->lcInOrgX = 0;
    ctx->lcInOrgY = 0;
    ctx->lcInOrgZ = 0;
	ctx->lcInExtX = tablet_width;
	ctx->lcInExtY = tablet_height;
    ctx->lcInExtZ = 0;
    ctx->lcOutOrgX = ctx->lcInOrgX;
    ctx->lcOutOrgY = ctx->lcInOrgY;
    ctx->lcOutOrgZ = ctx->lcInOrgZ;
    ctx->lcOutExtX = ctx->lcInExtX;
    ctx->lcOutExtY = ctx->lcInExtY;
    ctx->lcOutExtZ = ctx->lcInExtZ;
    ctx->lcSensX = 0; // FIXME
    ctx->lcSensY = 0; // FIXME
    ctx->lcSensZ = 0; // FIXME
    ctx->lcSysMode = 0; // FIXME
    ctx->lcSysOrgX = 0; // FIXME
    ctx->lcSysOrgY = 0; // FIXME
    ctx->lcSysExtX = 0; // FIXME
    ctx->lcSysExtY = 0; // FIXME
    ctx->lcSysSensX = 0; // FIXME
    ctx->lcSysSensY = 0; // FIXME
}

static BOOL update_screen_metrics(LOGCONTEXTA *ctx)
{
    // FIXME: need to think about how to handle multiple displays...
    // hey pal, fuck multiple displays lol
    BOOL changed = FALSE;
    // he gets width and height of a default(?) screen
    // I kinda get it, he assumes, that win8 touch events are pixel based, so he sets Context size to the same values, but I can choose any value I want. right?
    //  int width = tablet_width = GetSystemMetrics(SM_CXSCREEN);
    //  int height = tablet_height = GetSystemMetrics(SM_CYSCREEN);

    // some useless log file line
    LogEntry("screen metrics, width: %d, height: %d\n", tablet_width, tablet_height);

    //  // sets those values we initialised at the start if resolution have changed
    //  if (ctx->lcInExtX != width) {
    //      ctx->lcInExtX = width;
    //      changed = TRUE;
    //  }
    //  if (ctx->lcInExtY != height) {
    //      ctx->lcInExtY = height;
    //      changed = TRUE;
    //  }
    //  if (changed) {
    //      ctx->lcOutOrgX  = ctx->lcInOrgX;
    //      ctx->lcOutOrgY  = ctx->lcInOrgY;
    //      ctx->lcOutOrgZ  = ctx->lcInOrgZ;
    //      ctx->lcOutExtX  = ctx->lcInExtX;
    //      ctx->lcOutExtY  = ctx->lcInExtY;
    //      ctx->lcOutExtZ  = ctx->lcInExtZ;
    //
    //ctx->lcSysOrgX  = ctx->lcInOrgX;
    //ctx->lcSysOrgY  = ctx->lcInOrgY;
    //ctx->lcSysExtX  = ctx->lcInExtX;
    //ctx->lcSysExtY  = ctx->lcInExtY;
    //  }
    // yep and tells back if changed (maybe he uses this result at some point? idk)
    return changed;
}

static UINT convert_contextw(LPLOGCONTEXTW dst, LPLOGCONTEXTA src)
{
    if (dst) {
        _snwprintf_s(dst->lcName, LC_NAMELEN - 1, L"%s", src->lcName);
        dst->lcName[LC_NAMELEN - 1] = L'\0';
        dst->lcOptions = src->lcOptions;
        dst->lcStatus = src->lcStatus;
        dst->lcLocks = src->lcLocks;
        dst->lcMsgBase = src->lcMsgBase;
        dst->lcDevice = src->lcDevice;
        dst->lcPktRate = src->lcPktRate;
        dst->lcPktData = src->lcPktData;
        dst->lcPktMode = src->lcPktMode;
        dst->lcMoveMask = src->lcMoveMask;
        dst->lcBtnDnMask = src->lcBtnDnMask;
        dst->lcBtnUpMask = src->lcBtnUpMask;
        dst->lcInOrgX = src->lcInOrgX;
        dst->lcInOrgY = src->lcInOrgY;
        dst->lcInOrgZ = src->lcInOrgZ;
        dst->lcInExtX = src->lcInExtX;
        dst->lcInExtY = src->lcInExtY;
        dst->lcInExtZ = src->lcInExtZ;
        dst->lcOutOrgX = src->lcOutOrgX;
        dst->lcOutOrgY = src->lcOutOrgY;
        dst->lcOutOrgZ = src->lcOutOrgZ;
        dst->lcOutExtX = src->lcOutExtX;
        dst->lcOutExtY = src->lcOutExtY;
        dst->lcOutExtZ = src->lcOutExtZ;
        dst->lcSensX = src->lcSensX;
        dst->lcSensY = src->lcSensY;
        dst->lcSensZ = src->lcSensZ;
        dst->lcSysMode = src->lcSysMode;
        dst->lcSysOrgX = src->lcSysOrgX;
        dst->lcSysOrgY = src->lcSysOrgY;
        dst->lcSysExtX = src->lcSysExtX;
        dst->lcSysExtY = src->lcSysExtY;
        dst->lcSysSensX = src->lcSysSensX;
        dst->lcSysSensY = src->lcSysSensY;
    }
    return sizeof(LOGCONTEXTW);
}

static UINT convert_contexta(LPLOGCONTEXTA dst, LPLOGCONTEXTW src)
{
    if (dst) {
        _snprintf_s(dst->lcName, LC_NAMELEN - 1, "%S", src->lcName);
        dst->lcName[LC_NAMELEN - 1] = '\0';
        dst->lcOptions = src->lcOptions;
        dst->lcStatus = src->lcStatus;
        dst->lcLocks = src->lcLocks;
        dst->lcMsgBase = src->lcMsgBase;
        dst->lcDevice = src->lcDevice;
        dst->lcPktRate = src->lcPktRate;
        dst->lcPktData = src->lcPktData;
        dst->lcPktMode = src->lcPktMode;
        dst->lcMoveMask = src->lcMoveMask;
        dst->lcBtnDnMask = src->lcBtnDnMask;
        dst->lcBtnUpMask = src->lcBtnUpMask;
        dst->lcInOrgX = src->lcInOrgX;
        dst->lcInOrgY = src->lcInOrgY;
        dst->lcInOrgZ = src->lcInOrgZ;
        dst->lcInExtX = src->lcInExtX;
        dst->lcInExtY = src->lcInExtY;
        dst->lcInExtZ = src->lcInExtZ;
        dst->lcOutOrgX = src->lcOutOrgX;
        dst->lcOutOrgY = src->lcOutOrgY;
        dst->lcOutOrgZ = src->lcOutOrgZ;
        dst->lcOutExtX = src->lcOutExtX;
        dst->lcOutExtY = src->lcOutExtY;
        dst->lcOutExtZ = src->lcOutExtZ;
        dst->lcSensX = src->lcSensX;
        dst->lcSensY = src->lcSensY;
        dst->lcSensZ = src->lcSensZ;
        dst->lcSysMode = src->lcSysMode;
        dst->lcSysOrgX = src->lcSysOrgX;
        dst->lcSysOrgY = src->lcSysOrgY;
        dst->lcSysExtX = src->lcSysExtX;
        dst->lcSysExtY = src->lcSysExtY;
        dst->lcSysSensX = src->lcSysSensX;
        dst->lcSysSensY = src->lcSysSensY;
    }
    return sizeof(LOGCONTEXTA);
}

static UINT copy_contexta(LOGCONTEXTA *dst, LOGCONTEXTA *src)
{
    if (dst) {
        memcpy(dst, src, sizeof(LOGCONTEXTA));
    }
    return sizeof(LOGCONTEXTA);
}

static void _allocate_queue(void)
{
    UINT queue_bytes = sizeof(packet_data_t) * q_length;

    if (queue) free(queue);

    queue = (packet_data_t *)malloc(queue_bytes);
    memset(queue, 0, queue_bytes);
    q_start = 0;
    q_end = 0;
}

static void allocate_queue(void)
{
    EnterCriticalSection(&q_lock);
    _allocate_queue();
    LeaveCriticalSection(&q_lock);
}

static void release_queue(void)
{
    EnterCriticalSection(&q_lock);

    free(queue);
    q_length = 0;
    q_start = 0;
    q_end = 0;

    LeaveCriticalSection(&q_lock);
}

static void set_queue_length(int length)
{
    EnterCriticalSection(&q_lock);
    q_length = length + 1;
    _allocate_queue();
    LeaveCriticalSection(&q_lock);
}

// XXX: only call this when holding the queue lock
static UINT queue_size(void)
{
    if (q_start <= q_end) {
        return (q_end - q_start);
    }
    else {
        return (q_length - q_start) + q_end;
    }
}

static inline BOOL time_is_close(LONG a, LONG b)
{
    LONG d = (a - b);
    d *= d;
    return (d <= (TIME_CLOSE_MS * TIME_CLOSE_MS));
}

// XXX: only call this when holding the queue lock
static BOOL duplicate_packet(packet_data_t *pkt)
{
    UINT idx = (q_start + (queue_size() - 1)) % q_length;
    return (pkt->x == queue[idx].x)
        && (pkt->y == queue[idx].y)
        && time_is_close(pkt->time, queue[idx].time)
        && (pkt->pressure == queue[idx].pressure)
        && (pkt->contact == queue[idx].contact)
        && (pkt->buttons == queue[idx].buttons);
}

static BOOL enqueue_packet(packet_data_t *pkt)
{
    UINT size, idx;
    BOOL ret = FALSE;

    EnterCriticalSection(&q_lock);

    if (q_length > 0) {
        if (!duplicate_packet(pkt)) {
            size = queue_size();
            idx = q_end;

            q_end = (q_end + 1) % q_length;
            if (size >= (q_length - 1))
                q_start = (q_start + 1) % q_length;

            pkt->serial = next_serial++;
            memcpy(&(queue[idx]), pkt, sizeof(packet_data_t));
            ret = TRUE;
        }
    }

    LeaveCriticalSection(&q_lock);

    return ret;
}

static BOOL dequeue_packet(UINT serial, packet_data_t *pkt)
{
    UINT idx;
    BOOL ret = FALSE;

    EnterCriticalSection(&q_lock);

    idx = q_start;
    while (idx != q_end) {
        if (queue[idx].serial == serial) {
            break;
        }
        else {
            idx = (idx + 1) % q_length;
        }
    }
    if (idx != q_end) {
        q_start = (idx + 1) % q_length;
        memcpy(pkt, &(queue[idx]), sizeof(packet_data_t));
        ret = TRUE;
    }

    LeaveCriticalSection(&q_lock);

    return ret;
}

static UINT copy_handle(LPVOID lpOutput, HANDLE hVal)
{
    if (lpOutput) {
        *((HANDLE *)lpOutput) = hVal;
    }
    return sizeof(HANDLE);
}

static UINT copy_wtpkt(LPVOID lpOutput, WTPKT wtVal)
{
    if (lpOutput) {
        *((WTPKT *)lpOutput) = wtVal;
    }
    return sizeof(WTPKT);
}

static UINT copy_int(LPVOID lpOutput, int nVal)
{
    if (lpOutput) {
        *((int *)lpOutput) = nVal;
    }
    return sizeof(int);
}

static UINT copy_uint(LPVOID lpOutput, UINT nVal)
{
    if (lpOutput) {
        *((UINT *)lpOutput) = nVal;
    }
    return sizeof(UINT);
}

static UINT copy_long(LPVOID lpOutput, LONG nVal)
{
    if (lpOutput) {
        *((LONG *)lpOutput) = nVal;
    }
    return sizeof(LONG);
}

static UINT copy_dword(LPVOID lpOutput, DWORD nVal)
{
    if (lpOutput) {
        *((DWORD *)lpOutput) = nVal;
    }
    return sizeof(DWORD);
}

static UINT copy_strw(LPVOID lpOutput, wchar_t *str)
{
    int ret = 0;
    if (lpOutput) {
		wchar_t *out = (wchar_t *)lpOutput;
        _snwprintf_s(out, MAX_STRING_BYTES, _TRUNCATE, str);
		//out[MAX_STRING_BYTES - 1] = L'\0';
    }
    return ((wcslen(str)) * sizeof(wchar_t));
}

static UINT copy_stra(LPVOID lpOutput, CHAR *str)
{
    int ret = 0;
    if (lpOutput) {
        char *out = (char *)lpOutput;
        _snprintf_s(out, MAX_STRING_BYTES, _TRUNCATE, "%s", str);
        out[MAX_STRING_BYTES - 1] = '\0';
    }
    return ((strlen(str) + 1) * sizeof(char));
}

static UINT copy_axis(LPVOID lpOutput, LONG axMin, LONG axMax, LONG axUnits, FIX32 axResolution)
{
    if (lpOutput) {
        LPAXIS ax = (LPAXIS)lpOutput;
        ax->axMin = axMin;
        ax->axMax = axMax;
        ax->axUnits = axUnits;
        ax->axResolution = axResolution;
    }
    return sizeof(AXIS);
}

static UINT fill_orientation(LPVOID lpOutput)
{
    if (lpOutput) {
        LPORIENTATION o = (LPORIENTATION)lpOutput;
        memset(o, 0, sizeof(ORIENTATION));
    }
    return sizeof(ORIENTATION);
}

static UINT fill_rotation(LPVOID lpOutput)
{
    if (lpOutput) {
        LPROTATION o = (LPROTATION)lpOutput;
        memset(o, 0, sizeof(ROTATION));
    }
    return sizeof(ROTATION);
}

static UINT write_packet(LPVOID lpPtr, packet_data_t *pkt)
{
    LPBYTE ptr = (LPBYTE)lpPtr;
	UINT data = ctx.back()->lcPktData;
	UINT mode = ctx.back()->lcPktMode;
    UINT n = 0;

    if (data & PK_CONTEXT)
		n += copy_handle((LPVOID)(ptr ? ptr + n : NULL), ctx[0]);
    if (data & PK_STATUS)
        n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), 0);
    if (data & PK_TIME)
        n += copy_dword((LPVOID)(ptr ? ptr + n : NULL), pkt->time);
    if (data & PK_CHANGED)
        n += copy_wtpkt((LPVOID)(ptr ? ptr + n : NULL), 0xffff); // FIXME
    if (data & PK_SERIAL_NUMBER)
        n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->serial);
    if (data & PK_CURSOR)
        n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), 0x0); // XXX: check
    if (data & PK_BUTTONS)
        n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->buttons);
    if (data & PK_X)
        n += copy_long((LPVOID)(ptr ? ptr + n : NULL), pkt->x);
    if (data & PK_Y)
        n += copy_long((LPVOID)(ptr ? ptr + n : NULL), pkt->y);
    if (data & PK_Z)
        n += copy_long((LPVOID)(ptr ? ptr + n : NULL), 0);
    if (data & PK_NORMAL_PRESSURE) {
        if (mode & PK_NORMAL_PRESSURE)
            n += copy_int((LPVOID)(ptr ? ptr + n : NULL), 0); // FIXME
        else
            n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->pressure);
    }
    if (data & PK_TANGENT_PRESSURE) {
        if (mode & PK_TANGENT_PRESSURE)
            n += copy_int((LPVOID)(ptr ? ptr + n : NULL), 0); // FIXME
        else
            n += copy_uint((LPVOID)(ptr ? ptr + n : NULL), pkt->pressure); // FIXME
    }
    if (data & PK_ORIENTATION)
        n += fill_orientation((LPVOID)(ptr ? ptr + n : NULL));
    if (data & PK_ROTATION)
        n += fill_rotation((LPVOID)(ptr ? ptr + n : NULL));

    return n;
}

static void LogPointerInfo(POINTER_INFO *pi)
{
    LogEntry(
        "pointerType           = %x\n"
        "pointerId             = %x\n"
        "frameId               = %x\n"
        "pointerFlags          = %x\n"
        "sourceDevice          = %x\n"
        "hwndTarget            = %x\n"
        "ptPixelLocation       = %d %d\n"
        "ptHimetricLocation    = %d %d\n"
        "ptPixelLocationRaw    = %d %d\n"
        "ptHimetricLocationRaw = %d %d\n"
        "dwTime                = %x\n"
        "historyCount          = %d\n"
        "inputData             = %x\n"
        "dwKeyStates           = %x\n"
        "ButtonChangeType      = %x\n",
        pi->pointerType,
        pi->pointerId,
        pi->frameId,
        pi->pointerFlags,
        pi->sourceDevice,
        pi->hwndTarget,
        pi->ptPixelLocation.x, pi->ptPixelLocation.y,
        pi->ptHimetricLocation.x, pi->ptHimetricLocation.y,
        pi->ptPixelLocationRaw.x, pi->ptPixelLocationRaw.y,
        pi->ptHimetricLocationRaw.x, pi->ptHimetricLocationRaw.y,
        pi->dwTime,
        pi->historyCount,
        pi->InputData,
        pi->dwKeyStates,
        pi->ButtonChangeType);
}

static void LogPacket(packet_data_t *pkt)
{
    LogEntry(
        "packet:\n"
        " serial     = %x\n"
        " contact    = %d\n"
        " time       = %d\n"
        " x          = %d\n"
        " y          = %d\n"
        " buttons    = %x\n"
        " pressure   = %d\n",
        pkt->serial,
        pkt->contact,
        pkt->time,
        pkt->x, pkt->y,
        pkt->buttons,
        pkt->pressure);
}

static void adjustPosition(packet_data_t *pkt)
{
    pkt->x = pkt->x + config.shiftX;
    pkt->y = pkt->y + config.shiftY;
}

static void adjustPressure(packet_data_t *pkt)
{
    if (config.pressureCurve) {
        double p0, p1, v0, v1;
        double v = (double)pkt->pressure;
        int i0, i1;

        for (i1 = 1; i1 < 4; ++i1) {
            if (config.pressurePoint[i1] > pkt->pressure)
                break;
        }
        i0 = i1 - 1;

        p0 = (double)config.pressurePoint[i0];
        p1 = (double)config.pressurePoint[i1];
        v0 = (double)config.pressureValue[i0];
        v1 = (double)config.pressureValue[i1];

        v0 = v0 * ((p1 - v) / (p1 - p0));
        v1 = v1 * ((v - p0) / (p1 - p0));

        pkt->pressure = (UINT)(v0 + v1);
    }
    if (config.pressureExpand) {
        double midPoint = (double)(config.pressureMin + ((config.pressureMax - config.pressureMin) / 2));
        double expand = ((double)(config.pressureMax - config.pressureMin)) / 1024.0;
        double value = (double)pkt->pressure;

        value -= midPoint;
        value /= expand;
        value += 512.0;
        if (value < 0.0)
            value = 0.0;
        else if (value > 1023.0)
            value = 1023.0;

        pkt->pressure = (UINT)value;
    }
    else {
        if (pkt->pressure < config.pressureMin)
            pkt->pressure = config.pressureMin;
        else if (pkt->pressure > config.pressureMax)
            pkt->pressure = config.pressureMax;
    }
}

static void eraseMessage(LPMSG msg)
{
	// we can't actually delete messages, so change its type
	//if (logging && debug)
	//    LogEntry("erase %04x\n", msg->message);
	msg->message = 0x0;
}




static void setWindowFeedback(HWND hWnd)
{
    FEEDBACK_TYPE settings[] = {
        FEEDBACK_PEN_BARRELVISUALIZATION,
        FEEDBACK_PEN_TAP,
        FEEDBACK_PEN_DOUBLETAP,
        FEEDBACK_PEN_PRESSANDHOLD,
        FEEDBACK_PEN_RIGHTTAP
    };
    BOOL setting;
    BOOL ret;
    int i;

        LogEntry("configuring feedback for window: %p\n", hWnd);

    for (i = 0; i < (sizeof(settings) / sizeof(FEEDBACK_TYPE)); ++i) {
        setting = FALSE;
        //ret = SetWindowFeedbackSetting(hWnd,
        //    settings[i],
        //    0,
        //    sizeof(BOOL), 
        //    &setting
        //);
        //LogEntry(" setting: %d, ret: %d\n", settings[i], ret);
    }
}

static BOOL CALLBACK setFeedbackForThreadWindow(HWND hWnd, LPARAM lParam)
{
    setWindowFeedback(hWnd);
    return TRUE;
}

// visual feedback for windows we've got messages from (like tap animation etc)
static void setFeedbackForWindows(void)
{
    if (config.disableFeedback) {
        // enumerate windows of each thread (previouly detected)
        int i;
        for (i = 0; i < MAX_HOOKS; ++i) {
            if (hooks[i].thread)
                EnumThreadWindows(hooks[i].thread, setFeedbackForThreadWindow, NULL);
        }
    }

}

// this thing processes hooked events
// my problem is that he uses hooks to receive messages, but TUIO mechanism is http/udp based and has nothing to do with these events
// I'm going to use mouse presses to translate them to wintab for now
LRESULT CALLBACK emuHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    //LPCWPSTRUCT msg = (LPCWPSTRUCT)lParam;
    LPMSG msg = (LPMSG)lParam;
    DWORD thread = GetCurrentThreadId();
    HHOOK hook = NULL;
    UINT i;

    if (nCode < 0)
        goto end;

    for (i = 0; (i < MAX_HOOKS) && !hook; ++i) {
        if (hooks[i].thread == thread)
            hook = hooks[i].handle;
    }

    if (enabled && processing && queue) {
        POINTER_INPUT_TYPE pointerType = PT_POINTER;
        UINT32 pointerId = MOUSE_POINTER_ID;
        BOOL leavingWindow = FALSE;
        BOOL ignore = FALSE;
        LPARAM ext;

        switch (msg->message) {
		//this handles MOUSE buttons, for now it's the default behaviour, pointer from TUIO, buttons from mouse.
		//in future versions this should become an option loaded from the wintab32.ini file
			//if (pointerType == PT_PEN) {
			//    // win8 only
			//    ret = GetPointerPenInfo(pointerId, &info);
			//    if (!leavingWindow) {
			//        buttons[0] = IS_POINTER_FIRSTBUTTON_WPARAM(msg->wParam);
			//        buttons[1] = IS_POINTER_SECONDBUTTON_WPARAM(msg->wParam);
			//        buttons[2] = IS_POINTER_THIRDBUTTON_WPARAM(msg->wParam);
			//        buttons[3] = IS_POINTER_FOURTHBUTTON_WPARAM(msg->wParam);
			//        buttons[4] = IS_POINTER_FIFTHBUTTON_WPARAM(msg->wParam);
			//        contact = IS_POINTER_INCONTACT_WPARAM(msg->wParam);
			//    }
			//} else 

			/// do we need to do the following?
			/// SkipPointerFrameMessages(info.pointerInfo.frameId);

			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_NCLBUTTONDBLCLK:
			case WM_NCRBUTTONDBLCLK:
			case WM_LBUTTONDOWN:
			case WM_NCLBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_NCLBUTTONUP:
			case WM_RBUTTONDOWN:
			case WM_NCRBUTTONDOWN:
			case WM_RBUTTONUP:
			case WM_NCRBUTTONUP:
			case WM_MBUTTONDOWN:
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK:
				//if (!leavingWindow) {
				LDOWN = msg->wParam == 0x0001;
				RDOWN = msg->wParam == 0x0002;
				MDOWN = msg->wParam == 0x0010;
				//}
				//we only need to delete mouse button events if there is a tuio cursor at the tablet surface currently
				//if it's not on a table then we're going to grab some mouse to manipulate some things with it
				if (TUIOCURSOR) {
					eraseMessage(msg);
				}
				break;
			//case WM_NCMOUSELEAVE:
				//ext = GetMessageExtraInfo();
				//leavingWindow = (msg->message == WM_NCMOUSELEAVE);
				//LogEntry("%p %p %04x wParam:%x lParam:%x ext:%x, ignore: %d\n", hook, msg->hwnd, msg->message, msg->wParam, msg->lParam, ext, ignore);
				//LogEntry(" x:%d y:%d\n", GET_X_LPARAM(msg->lParam), GET_Y_LPARAM(msg->lParam));
        default:
            break;
        }
    }

end:
    //And THE next hook, yaaay 
    return CallNextHookEx(hook, nCode, wParam, lParam);
}

// this thing counts threads and returns the number
static int findThreadsForHooking(void)
{
    THREADENTRY32 te32;
    HANDLE hThreadSnap;

    // this is winapi function that return process id
    DWORD pid = GetCurrentProcessId();
    int n = 0;
    // gets thread snapshot
    hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hThreadSnap == INVALID_HANDLE_VALUE)
        return 0;

    te32.dwSize = sizeof(THREADENTRY32);
    if (!Thread32First(hThreadSnap, &te32)) {
        CloseHandle(hThreadSnap);
        return 0;
    }

    do {
        if (te32.th32OwnerProcessID == pid) {
            hooks[n].thread = te32.th32ThreadID;
            n++;
        }
    } while (Thread32Next(hThreadSnap, &te32) && (n < MAX_HOOKS));

    CloseHandle(hThreadSnap);
    // counts threads in current process
    return n;
}

// I've heard that hooks are slow, but at this pont I just need a working driver, so we'll stick to this
// hmmmm, we actually can duplicate wintab messages with mouse events when any button isn't pressed so that way cursor will be visible and at the same time will not interfere with precise drawing process
static void installHook(int id, hook_t *hook)
{
    hook->handle = SetWindowsHookEx(
        WH_GETMESSAGE, //WH_CALLWNDPROCRET,
        emuHookProc,
        NULL,
        hook->thread
        );
     LogEntry("hook %d, thread = %08x, handle = %p\n", id, hook->thread, hook->handle);
}

// hmmm, it hooks something
static void installHooks(void)
{
    int n_hooks;
    int i;

    // clear array
    for (i = 0; i < MAX_HOOKS; ++i) {
        hooks[i].handle = NULL;
        hooks[i].thread = 0;
    }

    // map threads
    // he gets number of hooks needed by counting threads of current process
    n_hooks = findThreadsForHooking();

    // install hooks per thread
    for (i = 0; i < n_hooks; ++i) {
        installHook(i, &(hooks[i]));
    }

    // setup feedback overrides
    // whatever that means
    setFeedbackForWindows();

}

static void uninstallHooks(void)
{
    int i = 0;

    while ((hooks[i].thread != 0) && (i < MAX_HOOKS)) {
        if (hooks[i].handle)
            UnhookWindowsHookEx(hooks[i].handle);
        hooks[i].handle = NULL;
        hooks[i].thread = 0;
        i++;
    }
}

void emuEnableThread(DWORD thread)
{
    int i;

    // don't hook if we are not enabled
    if (!enabled)
        return;

    LogEntry("emuEnableThread(%08x)\n", thread);

    // find a free hook structure
    for (i = 0; i < MAX_HOOKS; ++i) {
        if (hooks[i].handle && hooks[i].thread == thread) {
            // thread is already hooked
            return;
        }
        else if (hooks[i].thread == 0) {
            hooks[i].thread = thread;
            installHook(i, &(hooks[i]));
            // FIXME: override windows feedback?
            return;
        }
    }

    // all hook structures are in use
}

void emuDisableThread(DWORD thread)
{
    int i;

    // no need to do anything if we are not enabled
    if (!enabled)
        return;

    LogEntry("emuDisableThread(%08x)\n", thread);

    // find hook and remove it
    for (i = 0; i < MAX_HOOKS; ++i) {
        if (hooks[i].thread == thread) {
            if (hooks[i].handle)
                UnhookWindowsHookEx(hooks[i].handle);
            hooks[i].handle = NULL;
            hooks[i].thread = 0;
            return;
        }
    }
}

//Start listening to TUIO messages
static void enableProcessing(void)
{
	
    if (!enabled) {
        installHooks();
		setupReceiver();
        enabled = TRUE;
    }
    processing = TRUE;
}

static void disableProcessing(BOOL hard)
{
    processing = FALSE;
    if (hard) {
        uninstallHooks();
        enabled = FALSE;
    }
}

void emuSetModule(HMODULE hModule)
{
    module = hModule;
}

#if 0
static void findPointers(void)
{
    POINTER_DEVICE_INFO deviceData[MAX_POINTERS];
    UINT32 deviceCount = 0;
    BOOL ret = FALSE;
    UINT i;

    n_pointers = 0;

    ret = GetPointerDevices(&deviceCount, NULL);
    LogEntry("GetPointerDevices = %d, count = %d\n", ret, deviceCount);
    if (!ret)
        return;

    if (deviceCount > MAX_POINTERS) {
        deviceCount = MAX_POINTERS;
    }
    else if (deviceCount == 0) {
        // emulate for debugging
        pointers[n_pointers++] = MOUSE_POINTER_ID;
        return;
    }

    ret = GetPointerDevices(&deviceCount, deviceData);
    LogEntry("GetPointerDevices = %d, count = %d\n", ret, deviceCount);
    if (!ret)
        return;

    for (i = 0; i < deviceCount; ++i) {
        LogEntry("deviceData[%d] = %x\n", deviceData[i].pointerDeviceType);

        if (deviceData[i].pointerDeviceType == POINTER_DEVICE_TYPE_INTEGRATED_PEN
            || deviceData[i].pointerDeviceType == POINTER_DEVICE_TYPE_EXTERNAL_PEN) {
            // FIXME: work out pointerID
            // pointers[n_pointers++] = deviceData[i].;

            /*
            POINTER_DEVICE_CURSOR_INFO cursorData[MAX_CURSORS];
            UINT32 cursorCount = 0;
            UINT j;

            ret = GetPointerDeviceCursors(deviceData[i].device, &cursorCount, cursorData);
            LogEntry("GetPointerDeviceCursors = %d, count = %d\n", cursorCount);
            if (!ret)
            continue;

            for (j = 0; j < cursorCount && n_cursors < MAX_CURSORS; ++j) {
            LogEntry("%x %x\n", cursorData[j].cursorId, cursorData[j].cursor);
            cursors[n_cursors++] = cursorData[j].cursorId;
            }
            */
        }
    }
}
#endif

// let's init wintab emulation!
//This is where we need to start implementing TUIO. But first we need to know how it is implemented in demo
void emuInit(BOOL fLogging, BOOL fDebug, emu_settings_t *settings)
{
    logging = fLogging; debug = fDebug;

    // so we get all the stuff we initialised and loaded from .ini file and just memcopy it, brilliant
    memcpy(&config, settings, sizeof(emu_settings_t));

    // critical section is needed when we have a concurrent resource that we don't want to overwrite while still writing
    InitializeCriticalSection(&q_lock);

    // let's have some wintab Context, amerite?
    init_context(&default_context);
    // checks if screen res changed and reports back the result and sets the values of some of the parameters we initialised before
    //update_screen_metrics(&default_context);

    enabled = FALSE;
    processing = FALSE;
    // sets the event queue length I guess?
    q_length = 128;

    // then it allocates memory for event queue and frees it if allocated
    allocate_queue();



}

void emuShutdown(void)
{
    if (enabled)
        disableProcessing(TRUE);

    release_queue();

    window = NULL;
    module = NULL;

	for (auto c=0;c<ctx.size();c++){
		if (ctx[c]) {
			free(ctx[c]);
			ctx[c] = NULL;
		}
	}
}

//1, identification info, specification and soft version, tablet vendor and model
//2, capability information, dimensions, resolutions, features, cursor types.
//3, categories that give defaults for all tablet Context attributes.
//etc
static UINT emuWTInfo(BOOL fUnicode, UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    UINT ret = 0;
	if (lpOutput == NULL){
		return 1; //we need to return the required buffer size
	}

    switch (wCategory) {
		case 0:
			ret = TRUE;  //if program just checks if wintab exists
			// we have to return the largest complete category buffer size in bytes
			// and which one would it exactly be?
		case WTI_INTERFACE:
			switch (nIndex) {
				case 0:
					ret = TRUE;
					// we should return all of the information entries in the category in a single data structure
					// !!!! I have no idea how to do that!
					break;
				case IFC_WINTABID:
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"CCV Multitouch Tablet 1.5");
					}
					else {
						ret = copy_stra(lpOutput, "CCV Multitouch Tablet 1.5");
					}
					break;
				// high-order byte = major version, low-order byte = minor version number.
				case IFC_SPECVERSION:
					ret = copy_uint(lpOutput, 0x0104);
					break;
				case IFC_IMPLVERSION:
					ret = copy_uint(lpOutput, 0x0104); //
					break;
				case IFC_NDEVICES:
					ret = copy_uint(lpOutput, 1);
					break;
				case IFC_NCURSORS:
					ret = copy_uint(lpOutput, 3); //number of cursor types supported
					break;
				case IFC_NCONTEXTS:
					ret = copy_uint(lpOutput, 1);
					break;
				case IFC_CTXOPTIONS:
					ret = copy_uint(lpOutput, CXO_MESSAGES); // FIXME  flags indicating which Context options are supported 
					break;
				case IFC_CTXSAVESIZE:
					ret = copy_uint(lpOutput, 0); // FIXME	the size of the save information returned from WTSave.
					break;
				case IFC_NEXTENSIONS:
					ret = copy_uint(lpOutput, 0);
					break;
				case IFC_NMANAGERS:
					ret = copy_uint(lpOutput, 0);
					break;
			}
			break;
		case WTI_STATUS:
			switch (nIndex) {
				case STA_CONTEXTS:
					//the number of contexts currently open.
					ret = copy_uint(lpOutput, 1);
					break;
				case STA_SYSCTXS:
					//the number of system contexts currently open.
					ret = copy_uint(lpOutput, 1);
					break;
				case STA_PKTRATE:
					//maximum packet report rate currently being re­ceived by any Context, in Hertz.
					break;
				case STA_PKTDATA:
					//mask indicating which packet data items are re­quested by at least one Context.
					break;
				case STA_MANAGERS:
					ret = copy_uint(lpOutput, 0); //the number of manager handles currently open.
					break;
				case STA_SYSTEM:
					ret = TRUE;	// non-zero value if system pointing is available to the whole screen; zero otherwise.
					break;
				case STA_BUTTONUSE:
					//button mask indicating the logical buttons whose events are requested by at least one Context
					ret = copy_dword(lpOutput, 0x7);
					break;
				case STA_SYSBTNUSE:
					//button mask indicating which logical buttons are as­signed a system button function by the current cursor's system button map.
					ret = copy_dword(lpOutput, 0x7);
					break;
				default:
					//The problem with inkscape is that it tries to find any active pen by adding a number to 200 up until it reaches some sky high number
					//actually it was my bug
					break;
			}
			break;
		//all these are default contexts
		//WTI_DDCTXS and WTI_DSCTXS are multiplex categories! 400-499, 500-599
		case WTI_DEFCONTEXT:
		case WTI_DEFSYSCTX:
		case WTI_DDCTXS:
		case WTI_DDCTX_1:
		case WTI_DDCTX_2:
		case WTI_DSCTXS:
			switch (nIndex) {
				case 0:
					if (fUnicode) {
						ret = convert_contextw((LPLOGCONTEXTW)lpOutput, &default_context);
					}
					else {
						ret = copy_contexta((LPLOGCONTEXTA)lpOutput, &default_context);
					}
					break;
				case CTX_NAME:
					break;
				case CTX_OPTIONS:
					//Returns option flags.
					//For the default digitizing Context, CXO_MARGIN and CXO_MGNINSIDE are allowed.
					//For the default system Context, CXO_SYSTEM is required; CXO_PEN, CXO_MARGIN, and CXO_MGNINSIDE are allowed.
					ret = copy_uint(lpOutput, default_context.lcOptions);
					break;
				case CTX_STATUS:
					ret = copy_uint(lpOutput, default_context.lcStatus);
					break;
				case CTX_LOCKS:
					ret = copy_uint(lpOutput, default_context.lcLocks);
					break;
				case CTX_MSGBASE:
					ret = copy_uint(lpOutput, default_context.lcMsgBase);
					break;
				case CTX_DEVICE:
					ret = copy_uint(lpOutput, default_context.lcDevice);
					break;
				case CTX_PKTRATE:
					ret = copy_uint(lpOutput, default_context.lcPktRate);
					break;
				case CTX_PKTDATA:
					ret = copy_uint(lpOutput, default_context.lcPktData);
					break;
				case CTX_PKTMODE:
					ret = copy_uint(lpOutput, default_context.lcPktMode);
					break;
				case CTX_MOVEMASK:
					ret = copy_dword(lpOutput, default_context.lcMoveMask);
					break;
				case CTX_BTNDNMASK:
					ret = copy_dword(lpOutput, default_context.lcBtnDnMask);
					break;
				case CTX_BTNUPMASK:
					ret = copy_dword(lpOutput, default_context.lcBtnUpMask);
					break;
				case CTX_INORGX:
					ret = copy_uint(lpOutput, default_context.lcInOrgX);
					break;
				case CTX_INORGY:
					ret = copy_uint(lpOutput, default_context.lcInOrgY);
					break;
				case CTX_INORGZ:
					ret = copy_uint(lpOutput, default_context.lcInOrgZ);
					break;
				case CTX_INEXTX:
					ret = copy_uint(lpOutput, default_context.lcInExtX);
					break;
				case CTX_INEXTY:
					ret = copy_uint(lpOutput, default_context.lcInExtY);
					break;
				case CTX_INEXTZ:
					ret = copy_uint(lpOutput, default_context.lcInExtZ);
					break;
				case CTX_OUTORGX:
					ret = copy_uint(lpOutput, default_context.lcOutOrgX);
					break;
				case CTX_OUTORGY:
					ret = copy_uint(lpOutput, default_context.lcOutOrgY);
					break;
				case CTX_OUTORGZ:
					ret = copy_uint(lpOutput, default_context.lcOutOrgZ);
					break;
				case CTX_OUTEXTX:
					ret = copy_uint(lpOutput, default_context.lcOutExtX);
					break;
				case CTX_OUTEXTY:
					ret = copy_uint(lpOutput, default_context.lcOutExtY);
					break;
				case CTX_OUTEXTZ:
					ret = copy_uint(lpOutput, default_context.lcOutExtZ);
					break;
				case CTX_SENSX:
					ret = copy_uint(lpOutput, default_context.lcSensX);
					break;
				case CTX_SENSY:
					ret = copy_uint(lpOutput, default_context.lcSensY);
					break;
				case CTX_SENSZ:
					ret = copy_uint(lpOutput, default_context.lcSensZ);
					break;
				case CTX_SYSMODE:
					ret = copy_uint(lpOutput, default_context.lcSysMode);
					break;
				case CTX_SYSORGX:
					ret = copy_uint(lpOutput, default_context.lcSysOrgX);
					break;
				case CTX_SYSORGY:
					ret = copy_uint(lpOutput, default_context.lcSysOrgY);
					break;
				case CTX_SYSEXTX:
					ret = copy_uint(lpOutput, default_context.lcSysExtX);
					break;
				case CTX_SYSEXTY:
					ret = copy_uint(lpOutput, default_context.lcSysExtY);
					break;
				case CTX_SYSSENSX:
					ret = copy_uint(lpOutput, default_context.lcSysSensX);
					break;
				case CTX_SYSSENSY:
					ret = copy_uint(lpOutput, default_context.lcSysSensY);
					break;
				default:

					//The problem with inkscape is that it tries to find any active pen by adding a number to 200 up until it reaches some sky high number
					break;
			}
			break;

			break;
		case WTI_VIRTUAL_DEVICE:
		case WTI_DEVICES:
			switch (nIndex) {
				case DVC_NAME:
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"TUIO to WINTAB interface");
					}
					else {
						ret = copy_stra(lpOutput, "TUIO to WINTAB interface");
					}
					break;
				case DVC_HARDWARE:
					ret = copy_uint(lpOutput, HWC_INTEGRATED | HWC_TOUCH);
					//HWC_INTEGRATED Indicates that the display and digitizer share the same surface.
					//HWC_TOUCH Indicates that the cursor must be in physical contact with the device to report position.
					//HWC_HARDPROX Indicates that device can generate events when the cursor is entering and leaving the physical detection range.
					//HWC_PHYSID_CURSORS(1.1) Indicates that device can uniquely iden­tify the active cursor in hardware.
					break;
				case DVC_FIRSTCSR:
					//Returns the first cursor type number for the device.
					ret = copy_uint(lpOutput, 0);
					break;
				case DVC_PKTRATE:
					// Returns the maximum packet report rate in Hertz.
					ret = copy_uint(lpOutput, 240);
					break;
				case DVC_NCSRTYPES:
					ret = copy_uint(lpOutput, 3);
					break;
				case DVC_PKTDATA:
				case DVC_PKTMODE:
				case DVC_CSRDATA:
				case DVC_XMARGIN:
				case DVC_YMARGIN:
				case DVC_ZMARGIN:
					break;
				//AXIS  Each returns the tablet's range and resolution capabilities, in the x, y, and z axes, respectively.
				case DVC_X:
					ret = copy_axis(lpOutput, 0, tablet_width, TU_NONE, 0xFFFF);	//65535 == 0xFFFF //38.5 TU_CENTIMETERS 1600*1000/38.5 = 41558.44155 = A256.AC7B  
					break;
				case DVC_Y:
					ret = copy_axis(lpOutput, 0, tablet_height, TU_NONE, 0xFFFF);	//0xFFFF //29 TU_CENTIMETERS 900*1000/29 = 31034.48275 = 793A.BC93
					break;
				case DVC_Z:
					ret = copy_axis(lpOutput, 0, 0, 0, 0); //nothing
					break;
				case DVC_NPRESSURE:
					//ret = copy_axis(lpOutput, 0, 1023, 0, 0); //0-1024
					ret = copy_axis(lpOutput, 0, 0, 0, 0); //nothing
					break;
				case DVC_TPRESSURE:
					ret = copy_axis(lpOutput, 0, 0, 0, 0); //nothing
					break;
				case DVC_ORIENTATION:
					ret = copy_axis(lpOutput, 0, 0, 0, 0); //nothing
					break;
				case DVC_ROTATION: /* 1.1 */
					ret = copy_axis(lpOutput, 0, 0, 0, 0); //nothing
					break;
				case DVC_PNPID: /* 1.1 */
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"CCVTUIO");
					}
					else {
						ret = copy_stra(lpOutput, "CCVTUIO");
					}
					break;
			}
			break;
		case CSR_NAME_PUCK:
			switch (nIndex) {
				case CSR_NAME:
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"TUIO puck");
					}
					else {
						ret = copy_stra(lpOutput, "TUIO puck");
					}
					break;
				case CSR_ACTIVE:
					ret = TRUE;
					copy_int(lpOutput, TRUE);
					break;
				case CSR_PKTDATA:
					//bit mask with options
					ret = copy_dword(lpOutput, default_context.lcPktData);
					break;
				case CSR_BUTTONS:
					ret = copy_uint(lpOutput, 5);
					break;
				case CSR_BUTTONBITS:
					//BYTE  Returns the number of bits of raw button data returned by the hardware.
					break;
				case CSR_BTNNAMES:
					//TCHAR[] Returns a list of zero - terminated strings containing the names of the cursor's buttons.
					//The number of names in the list is the same as the number of buttons on the cursor.
					//The names are sepa­rated by a single zero character; the list is terminated by two zero characters.
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"btn_tip\0btn_rclick\0btn_eraser\0btn_four\0btn_five\0\0");
					}
					else {
						ret = copy_stra(lpOutput, "btn_tip\0btn_rclick\0btn_eraser\0btn_four\0btn_five\0\0");
					}
					break;
				case CSR_BUTTONMAP:
					//BYTE[]  Returns a 32 byte array of logical button numbers, one for each physical button.
					break;
				case CSR_SYSBTNMAP:
					//BYTE[]  Returns a 32 byte array of button action codes, one for each logical button.
					break;
				case CSR_NPBUTTON:
				case CSR_NPBTNMARKS:
				case CSR_NPRESPONSE:
				case CSR_TPBUTTON:
				case CSR_TPBTNMARKS:
				case CSR_TPRESPONSE:
					break;
				case CSR_PHYSID: /* 1.1 */
					//problims
					//if (fUnicode) {
					//	ret = copy_strw(lpOutput, L"CCVPUG1");
					//}
					//else {
					//	ret = copy_stra(lpOutput, "CCVPUG1");
					//}
					break;
				case CSR_MODE: /* 1.1 */
				case CSR_MINPKTDATA: /* 1.1 */
				case CSR_MINBUTTONS: /* 1.1 */
				case CSR_CAPABILITIES: /* 1.1 */
					break;
				case CSR_TYPE: /* 1.2 */
					ret = copy_uint(lpOutput, 0x0F06); //0x0802 general stylus, 0x0F06 5 button puck
					break;
			}
			break;
		case CSR_NAME_PRESSURE_STYLUS:
			switch (nIndex) {
				case CSR_NAME:
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"TUIO pen");
					}
					else {
						ret = copy_stra(lpOutput, "TUIO pen");
					}
					break;
				case CSR_ACTIVE:
					ret = FALSE;
					break;
			}
			break;
		case CSR_NAME_ERASER:
			switch (nIndex) {
				case CSR_NAME:
					if (fUnicode) {
						ret = copy_strw(lpOutput, L"TUIO eraser");
					}
					else {
						ret = copy_stra(lpOutput, "TUIO eraser");
					}
					break;
				case CSR_ACTIVE:
					ret = FALSE;
					break;
			}
			break;
		case WTI_EXTENSIONS:
			break;
		default:
			break;
    }
    return ret;
}
UINT emuWTInfoA(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    return emuWTInfo(FALSE, wCategory, nIndex, lpOutput);
}
UINT emuWTInfoW(UINT wCategory, UINT nIndex, LPVOID lpOutput)
{
    return emuWTInfo(TRUE, wCategory, nIndex, lpOutput);
}


static HCTX emuWTOpen(BOOL unicode, HWND hWnd, LPVOID lpLogCtx, BOOL fEnable)
{
	if (!lpLogCtx)
        return NULL;

    window = hWnd;
	ctx.push_back( (LPLOGCONTEXTA)malloc(sizeof(LOGCONTEXTA)) );
    if (unicode) {
		convert_contexta(ctx.back(), (LPLOGCONTEXTW)lpLogCtx);
    }
    else {
		memcpy(ctx.back(), lpLogCtx, sizeof(LOGCONTEXTA));
    }

    if (fEnable) {
        enableProcessing();
    }

	return (HCTX)ctx.back();
}
HCTX emuWTOpenA(HWND hWnd, LPLOGCONTEXTA lpLogCtx, BOOL fEnable)
{
    return emuWTOpen(FALSE, hWnd, lpLogCtx, fEnable);
}
HCTX emuWTOpenW(HWND hWnd, LPLOGCONTEXTW lpLogCtx, BOOL fEnable)
{
    return emuWTOpen(TRUE, hWnd, lpLogCtx, fEnable);
}

BOOL emuWTClose(HCTX hCtx)
{
	if (ctxHas(hCtx)) {
		//if (enabled) {
		//	disableProcessing(TRUE);
		//}
		ctx.erase(
			std::remove(
				ctx.begin(),ctx.end(),(LPLOGCONTEXTA)hCtx
			), ctx.end()
		);
		free(hCtx);
		return TRUE;
	}
	return FALSE;
}

int emuWTPacketsGet(HCTX hCtx, int cMaxPkts, LPVOID lpPkt)
{
    int ret = 0;

	if (ctxHas(hCtx)) {
        LPBYTE out = (LPBYTE)lpPkt;
        EnterCriticalSection(&q_lock);
        while ((ret < cMaxPkts) && (queue_size() > 0)) {
            out += write_packet((LPVOID)out, &(queue[q_start]));
            q_start = (q_start + 1) % q_length;
            ret++;
        }
        LeaveCriticalSection(&q_lock);
    }

    return ret;
}

BOOL emuWTPacket(HCTX hCtx, UINT wSerial, LPVOID lpPkt)
{
    packet_data_t pkt;
    BOOL ret = FALSE;

    //if (ctxHas(hCtx)) {
        ret = dequeue_packet(wSerial, &pkt);
        if (ret && lpPkt) {
            write_packet(lpPkt, &pkt);
        }
    //}

    return ret;
}

BOOL emuWTEnable(HCTX hCtx, BOOL fEnable)
{
    if (processing != fEnable) {
        if (fEnable) {
            enableProcessing();
        }
        else {
            disableProcessing(FALSE);
        }
    }

    return TRUE;
}

BOOL emuWTOverlap(HCTX hCtx, BOOL fToTop)
{
    return TRUE;
	//This function sends a tablet Context to the top or bottom of the order of over­lapping tablet contexts.
	//Specifies sending the Context to the top of the overlap or­der if non - zero, or to the bottom if zero.
	//The function returns non - zero if successful, zero otherwise.
	//Tablet contexts' input areas are allowed to overlap. The tablet interface main­tains an overlap order that helps determine which Context will process a given event. The topmost Context in the overlap order whose input Context encom­passes the event, and whose event masks select the event will process the event.
}

BOOL emuWTConfig(HCTX hCtx, HWND hWnd)
{
    BOOL ret = FALSE;
    // FIXME: implement?
    return ret;
}

BOOL emuWTGetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	if (lpLogCtx && ctxHas(hCtx)) {
        memcpy(lpLogCtx, hCtx, sizeof(LOGCONTEXTA));
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL emuWTGetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	if (lpLogCtx && ctxHas(hCtx)) {
		convert_contextw(lpLogCtx, ctx[0]);
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL emuWTSetA(HCTX hCtx, LPLOGCONTEXTA lpLogCtx)
{
	if (lpLogCtx && ctxHas(hCtx)) {
        memcpy(hCtx, lpLogCtx, sizeof(LOGCONTEXTA));
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL emuWTSetW(HCTX hCtx, LPLOGCONTEXTW lpLogCtx)
{
	if (lpLogCtx && ctxHas(hCtx)) {
		convert_contexta(ctx[0], (LPLOGCONTEXTW)lpLogCtx);
        return TRUE;
    }
    else {
        return FALSE;
    }
}

BOOL emuWTExtGet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
    BOOL ret = FALSE;
    return ret;
}

BOOL emuWTExtSet(HCTX hCtx, UINT wExt, LPVOID lpData)
{
    BOOL ret = FALSE;
    return ret;
}

BOOL emuWTSave(HCTX hCtx, LPVOID lpData)
{
    BOOL ret = FALSE;
    return ret;
}

HCTX emuWTRestore(HWND hWnd, LPVOID lpSaveInfo, BOOL fEnable)
{
    HCTX ret = NULL;
    return ret;
}

int emuWTPacketsPeek(HCTX hCtx, int cMaxPkt, LPVOID lpPkts)
{
    int ret = 0;

	if (ctxHas(hCtx)) {
        LPBYTE out = (LPBYTE)lpPkts;
        UINT old_q_start;

        EnterCriticalSection(&q_lock);
        old_q_start = q_start;

        while ((ret < cMaxPkt) && (queue_size() > 0)) {
            out += write_packet((LPVOID)out, &(queue[q_start]));
            q_start = (q_start + 1) % q_length;
            ret++;
        }

        q_start = old_q_start;
        LeaveCriticalSection(&q_lock);
    }

    return ret;
}

int emuWTDataGet(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
    int ret = 0;
    return ret;
}

int emuWTDataPeek(HCTX hCtx, UINT wBegin, UINT wEnd, int cMaxPkts, LPVOID lpPkts, LPINT lpNPkts)
{
    int ret = 0;
    return ret;
}



int emuWTQueueSizeGet(HCTX hCtx)
{
	context = (LPLOGCONTEXTA)hCtx;
	if (ctxHas(hCtx)) {
		return q_length;
    }
    return 0;
}

BOOL emuWTQueueSizeSet(HCTX hCtx, int nPkts)
{
	if (ctxHas(hCtx) && (nPkts > 0)) {
        set_queue_length(nPkts);
        return TRUE;
    }
    return 0;
}

HMGR emuWTMgrOpen(HWND hWnd, UINT wMsgBase)
{
    HMGR ret = NULL;
    return ret;
}

BOOL emuWTMgrClose(HMGR hMgr)
{
    BOOL ret = FALSE;
    return ret;
}




// the problem was: after completing dllmain wipes everything that is not stored anywhere, so we have to store our thread somewhere
// now it doesn't hang the application, but the problem is that it first says it binded to port and then says it cannot bind to port
// the only minor problem is that it tries to bind to that port twice
// because I define this shit twice, obviously
TuioToWinTab TuioListenerInstance;

OscReceiver* OscUdpReceiver = new UdpReceiver(_port);
TuioClient UdpClient(OscUdpReceiver);
//OscReceiver* OscTcpReceiver = new TcpReceiver(_address.c_str(), _port);
//TuioClient TcpClient(OscTcpReceiver);
void setupReceiver(void){
	if (!listening) {
		//TUIO part
		//we need two different sets of variables to process tcp and udp, but right now I only need one
		if (_udp){
			UdpClient.addTuioListener(&TuioListenerInstance);
			UdpClient.connect(false); //had some problems here, but it turns out the author of this library did his job well
		}
		//else {
		//	TcpClient.addTuioListener(&TuioListenerInstance);
		//	TcpClient.connect(false);
		//}

		listening = TRUE;
	}
}


void TuioToWinTab::addTuioObject(TuioObject *tobj) {
	LogEntry("add obj %d (%d/%d) %f %f %f\n", tobj->getSymbolID(), tobj->getSessionID(), tobj->getTuioSourceID(), tobj->getX(), tobj->getY(), tobj->getAngle());
}

void TuioToWinTab::updateTuioObject(TuioObject *tobj) {
	LogEntry("set obj %d (%d/%d) %f %f %f   %f %f %f %f \n", tobj->getSymbolID(), tobj->getSessionID(), tobj->getTuioSourceID(), tobj->getX(), tobj->getY(), tobj->getAngle(), tobj->getMotionSpeed(), tobj->getRotationSpeed(), tobj->getMotionAccel(), tobj->getRotationAccel());
}

void TuioToWinTab::removeTuioObject(TuioObject *tobj) {
	LogEntry("del obj %d (%d/%d) \n", tobj->getSymbolID(), tobj->getSessionID(), tobj->getTuioSourceID());
}

void TuioToWinTab::addTuioCursor(TuioCursor *tcur) {
	LogEntry("add cur %d (%d/%d) %f %f\n", tcur->getCursorID(), tcur->getSessionID(), tcur->getTuioSourceID(), tcur->getX(), tcur->getY());
	TUIOCURSOR = TRUE;
	handleTuioMessage(tcur, NULL);
}

void TuioToWinTab::updateTuioCursor(TuioCursor *tcur) {
	//LogEntry("set cur %d (%d/%d) %f %f    %f %f\n", tcur->getCursorID(), tcur->getSessionID(), tcur->getTuioSourceID(), tcur->getX(), tcur->getY(), tcur->getMotionSpeed(), tcur->getMotionAccel());
	TUIOCURSOR = TRUE;
	handleTuioMessage(tcur, NULL);
}

void TuioToWinTab::removeTuioCursor(TuioCursor *tcur) {
	LogEntry("del cur %d (%d/%d) \n", tcur->getCursorID(), tcur->getSessionID(), tcur->getTuioSourceID(), tcur->getX(), tcur->getY());
	TUIOCURSOR = FALSE;
	handleTuioMessage(tcur,NULL);
}

void TuioToWinTab::addTuioBlob(TuioBlob *tblb) {
	LogEntry("add blb %d (%d/%d) %f %f %f   %f %f %f \n", tblb->getBlobID(), tblb->getSessionID(), tblb->getTuioSourceID(), tblb->getX(), tblb->getY(), tblb->getAngle(), tblb->getWidth(), tblb->getHeight(), tblb->getArea());
}

void TuioToWinTab::updateTuioBlob(TuioBlob *tblb) {
	LogEntry("set blb %d (%d/%d) %f %f %f   %f %f %f   %f %f %f %f \n", tblb->getBlobID(), tblb->getSessionID(), tblb->getTuioSourceID(), tblb->getX(), tblb->getY(), tblb->getAngle(), tblb->getWidth(), tblb->getHeight(), tblb->getArea(), tblb->getMotionSpeed(), tblb->getRotationSpeed(), tblb->getMotionAccel(), tblb->getRotationAccel());
}

void TuioToWinTab::removeTuioBlob(TuioBlob *tblb) {
	LogEntry("del blb %d (%d/%d) \n", tblb->getBlobID(), tblb->getSessionID(), tblb->getTuioSourceID());
}

void  TuioToWinTab::refresh(TuioTime frameTime) {
	//std::cout << "refresh " << frameTime.getTotalMilliseconds() << std::endl;

}

//THIS SHIT DOESN'T WORK CURSOR DOESN'T MOVE
//static BOOL CALLBACK moveCursorForThreadWindow(HWND hWnd, LPARAM lParam)
//{
//	PostMessage(hWnd, WM_MOUSEMOVE, 0x0000, lParam);
//	PostMessage(hWnd, WM_NCMOUSEMOVE, 0x0000, lParam);
//	return TRUE;
//}
//
//static BOOL CALLBACK moveCursorForChildWindows(HWND hWnd, LPARAM lParam)
//{
//	moveCursorForThreadWindow(hWnd, lParam);
//	EnumChildWindows(hWnd, moveCursorForThreadWindow, lParam);
//}
//
//static void moveCursor(float x, float y){
//	int i;
//	LPARAM lParam = MAKELPARAM((int)x, (int)y);
//	for (i = 0; i < MAX_HOOKS; ++i) {
//		if (hooks[i].thread)
//			EnumThreadWindows(hooks[i].thread, moveCursorForThreadWindow, lParam);
//	}
//}

static BOOL handleTuioMessage(TuioCursor *tcur, UINT32 pressure)
{
	packet_data_t pkt;
	BOOL ret;
	UINT i;
	//info.penFlags = 0;
	//info.penMask = 0;
	//info.pressure = 0;
	//info.rotation = 0;
	//info.tiltX = 0;
	//info.tiltY = 0;
	//if (!leavingWindow) {
	//	buttons[0] = msg->lParam == 0x0001;
	//	buttons[1] = msg->lParam == 0x0002;
	//	buttons[2] = msg->lParam == 0x0010;
	//	contact = (buttons[0] || buttons[1] || buttons[2]);
	//	info.pressure = contact ? 100 : 0;
	//}
	pkt.serial = 0;
	pkt.contact = LDOWN || RDOWN || MDOWN;//TUIOCURSOR;//;
	pkt.pressure = pkt.contact ? max_pressure : min_pressure;
	pkt.buttons = (LDOWN ? SBN_LCLICK : SBN_NONE) | (RDOWN ? SBN_RCLICK : SBN_NONE) | (MDOWN ? SBN_MCLICK : SBN_NONE);
	// for some reason gtk+ treats both SBN_RCLICK and SBN_MCLICK as middle click and zooms in. no idea why
	pkt.x = tablet_width * tcur->getX();
	pkt.y = tablet_height * tcur->getY(); /*tablet_height -*/ //ctx[N]->lcInExtY
	pkt.time = GetTickCount();
	
	// adjusts values according to settings
	//adjustPosition(&pkt);
	//adjustPressure(&pkt);

	// And FINALLY posts wintab message according to values we got from TUIO
	if (enqueue_packet(&pkt)) {
		//LogEntry("queued packet\n");
		//LogPacket(&pkt);
		if (window){
			for (auto c=0; c<ctx.size(); c++){
				PostMessage(window, WT_PACKET, (WPARAM)pkt.serial, (LPARAM)ctx[c]);
				//for some reason the test app opens two contexts, that's why tests fail
				//An application can open more than one Context, but most only need one.
				//Applications can customize their contexts, or they can open a Context using a default Context specification that is always available.
				//The WTInfo function provides access to the default Context specification.
				//contexts could be passive or active, so we need to store settings separately
			}
		}
		if (!pkt.contact){
			//for (i = 0; i < MAX_HOOKS; ++i) {
			//	hook = hooks[i].thread;
			//}
			//window? and what is window here, opened context? I guess it's fake window, that's why it doesn't work
			//moveCursor( screen_width*tcur->getX(), screen_height*tcur->getY() );
			//this doesn't work for some reason
			// need to find a way to get source window handler from dll
			// we need to also send mouse cursor movement when no buttons are pressed, well, to at least see where we placed it
			// WM_MOUSEMOVE:
			// WM_NCMOUSEMOVE:
		}
		return TRUE;
		// does it only post a packet to queue, or posts it directly to window?
			//both
	}
	else {
		// packet is probably duplicate, or the queue has been deleted
		return FALSE;
	}
}

