#pragma once

/*  Identity of ENH Master's system effect. Never change the CLSID: installed systems and the installer's
    backups refer to it. Shared by the effect DLL (EnhApo.cpp) and the installer (ApoInstaller.cpp). */

#define ENH_APO_CLSID_STRING    L"{166DEBC5-9B90-493B-A812-F7FA9DBA6875}"
#define ENH_APO_FRIENDLY_NAME   L"ENH Master system effect"
#define ENH_APO_COPYRIGHT       L"Khris Audio"
#define ENH_APO_DLL_NAME        L"EnhMasterApo.dll"
#define ENH_APO_MAJOR_VERSION   1
#define ENH_APO_MINOR_VERSION   0

#if defined (_WIN32)
 #include <guiddef.h>
 // {166DEBC5-9B90-493B-A812-F7FA9DBA6875}
 inline constexpr GUID ENH_APO_CLSID = { 0x166debc5, 0x9b90, 0x493b, { 0xa8, 0x12, 0xf7, 0xfa, 0x9d, 0xba, 0x68, 0x75 } };
#endif

/** The DisplayBridge key the effect publishes under (Scope::machine) and the standalone app reads. */
#define ENH_APO_BRIDGE_KEY      "system"
