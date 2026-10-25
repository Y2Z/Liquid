// Kept separate from the rest of the code, since X11 headers define macros (None, Bool, Status, etc) that clash with Qt

#include <X11/Xlib.h>

#include "liquidx11.h"

void liquidX11ActivateWindow(const unsigned long windowId)
{
    Display* display = XOpenDisplay(nullptr);

    if (display == nullptr) {
        return;
    }

    const Atom activeWindowAtom = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);

    if (activeWindowAtom != None) {
        XEvent event = {};

        event.xclient.type = ClientMessage;
        event.xclient.send_event = True;
        event.xclient.display = display;
        event.xclient.window = windowId;
        event.xclient.message_type = activeWindowAtom;
        event.xclient.format = 32;
        event.xclient.data.l[0] = 2; // Source indication: pager (acting on behalf of the user, like taskbars do)
        event.xclient.data.l[1] = CurrentTime; // Lets the window manager use its own current time
        event.xclient.data.l[2] = 0;

        XSendEvent(display, DefaultRootWindow(display), False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    }

    XFlush(display);
    XCloseDisplay(display);
}
