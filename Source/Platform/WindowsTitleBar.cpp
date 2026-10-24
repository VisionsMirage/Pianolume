#include "WindowsTitleBar.h"

// JUCE のヘッダとは混ぜず、Windows API だけを使う
#if defined (_WIN32)
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
 #include <dwmapi.h>
#endif

namespace pianolume
{

void applyDarkTitleBar (void* nativeWindowHandle)
{
   #if defined (_WIN32)
    if (nativeWindowHandle == nullptr)
        return;

    const auto window = static_cast<HWND> (nativeWindowHandle);

    // DWMWA_USE_IMMERSIVE_DARK_MODE（Windows 10 20H1 以降）
    constexpr DWORD useImmersiveDarkMode = 20;
    const BOOL enabled = TRUE;
    DwmSetWindowAttribute (window, useImmersiveDarkMode, &enabled, sizeof (enabled));

    // DWMWA_CAPTION_COLOR（Windows 11 以降。未対応の OS では無視される）。画面上部のパネル色 #0E1117
    constexpr DWORD captionColour = 35;
    const COLORREF colour = RGB (0x0e, 0x11, 0x17);
    DwmSetWindowAttribute (window, captionColour, &colour, sizeof (colour));
   #else
    (void) nativeWindowHandle;
   #endif
}

} // namespace pianolume
