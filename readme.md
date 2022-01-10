# TUIO to WINTAB driver
Copyright (c) [Dmitry Minchenko](https://github.com/minch-yoda). 

## Description:
Provides wintab32.dll implementation for TUIO interface to increase cursor
positioning precision. Available TUIO mouse and touch drivers only allow
pixel-precision, which is tied to screen resolution, which limits the quality
of strokes and lines drawn with the TUIO device. By contrast TUIO2WINTAB driver allows
to transfer the cursor positions reported by TUIO as precisely as possible to
any software that supports WINTAB library, essentially turning CCV device into a
proper graphics tablet.

## Usage:
Ready to use DLLs are provided in \DLL folder alongside with bat scripts that can copy
the dlls into respective folders (if you're too lazy to do it yourself). The wordy and
quite descriptive example .ini file is also there. Dlls are unsigned, but somehow it
works for me this way, maybe it's because I didn't implement any installation process and
just copy-pasted drivers with batch files or by hand.

## Settings
Initially it worked like this:
Global Wintab32.ini file can optinally be placed in windows folder, and you can override
it per application by placing separate Wintab32.ini alongside program's .exe file. If you don't
need CCV touch functionality to be available system-wide you can avoid global settings file and
simply create an individual .ini for each desired application.
This driver only works when .ini is present in at least one of the two places.

But at some point I decided to get rid of the global file and only left per app .ini files. Also
the "driver only works when .ini is present" went out of the window, so it just loads defaults
when no .ini file is there. It might be a good idea to bring initial behaviour back.

## Inkscape
Might be quite frustrating to be desperately trying to make it work in Inkscape to only
realize that you have to manually turn on tablet functionality through "input devices" panel.
Oh and don't forget that it requires restart as well! I love Inkscape anyways.

## Illustrator
To my surprise it actually works in Illustrator. Not that I'm going to use it any time soon,
but someone might find it quite handy.

## Build:
Use Visual Studio Express 2013 to build dlls. Solution is configured to copy x86/x64
driver versions right into SysWOW64/System32 folders respectively when the build is finished.

## License:
This library is free software; you can redistribute it and/or modify it 
under the terms of the GNU Lesser General Public License as published by 
the Free Software Foundation; either version 3.0 of the License, or (at 
your option) any later version.

This library is distributed in the hope that it will be useful, but 
WITHOUT ANY WARRANTY; without even the implied warranty of 
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser 
General Public License for more details.

You should have received a copy of the GNU Lesser General Public License 
along with this library; if not, write to the Free Software Foundation, 
Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307USA

## References:
This driver uses the TUIO library,
some structures from WintabEmulator,
also borrows some essential structural parts from sample wintab32 project provided by Wacom