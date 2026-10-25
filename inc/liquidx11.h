#pragma once

// Asks the X11 window manager to activate (un-minimize, raise, and focus) the given window,
// the same way taskbars do it, which window managers honor even for windows that weren't recently interacted with
void liquidX11ActivateWindow(const unsigned long windowId);
