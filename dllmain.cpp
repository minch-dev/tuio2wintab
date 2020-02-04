/*------------------------------------------------------------------------------
WintabEmulator - dllmain.cpp
Copyright (c) 2013 Carl G. Ritson <critson@perlfu.co.uk>

This file may be freely used, copied, or distributed without compensation 
or licensing restrictions, but is done so without any warranty or implication
of merchantability or fitness for any particular purpose.
------------------------------------------------------------------------------*/
#include "stdafx.h"
#include "wintab.h"
#include "TuioToWinTab.h"
#include "logging.h"
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
	switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
			emuSetModule(hModule);
			Init(); //now the Init thing is going to be called at load time
			// We can read the INI file here, but would the variables be available to other parts of the program?
			// this part should fire when the dll is loaded and the settings variable should be available everywhere

			break;
		case DLL_THREAD_ATTACH:
            emuEnableThread(GetCurrentThreadId());
			break;
		case DLL_THREAD_DETACH:
            emuDisableThread(GetCurrentThreadId());
			break;
		case DLL_PROCESS_DETACH:
			break;
	}
	return TRUE;
}

