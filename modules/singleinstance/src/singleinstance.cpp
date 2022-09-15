#include <QTimer>

#include "singleinstance.hpp"

#if defined(Q_OS_WIN)
#include <windows.h>
#elif !defined(Q_OS_MACOS)
#include <X11/Xlib.h>
#endif

SingleInstance::SingleInstance(QWidget *parent, QString *instanceId) : QObject(parent)
{
    shmemName = instanceId + QString("_shrdmmr");
    smphorName = instanceId + QString("_smphr");
}

SingleInstance::~SingleInstance()
{
}

bool SingleInstance::isAlreadyRunning(const bool raiseExisting)
{
    QSystemSemaphore sem(smphorName, 1);

    // Fix danging memory on Linux
    {
        QSharedMemory fix(shmemName);
        fix.attach();
    }

    shMem = new QSharedMemory(shmemName);
    if (shMem->create(sizeof(SharedData))) { // This is the first instance
        shMem->attach();

        sem.acquire();
        memset(shMem->data(), 0, shMem->size());
        sem.release();

        QTimer* t = new QTimer(this);
        connect(t, &QTimer::timeout, this, [this]() {
            QSystemSemaphore sem(smphorName, 1);
            sem.acquire();
            SharedData* data = reinterpret_cast<SharedData*>(shMem->data());
            if (data->needToRaiseExistingWindow) {
                SingleInstance::raiseWindow((QWidget*)parent());
                data->needToRaiseExistingWindow = false;
            }
            sem.release();
        });
        t->start(0);
    } else { // Another instance is already running
        shMem->attach();

        sem.acquire();
        SharedData* data = reinterpret_cast<SharedData*>(shMem->data());
        data->needToRaiseExistingWindow = raiseExisting;

        return true;
    }

    return false;
}

void SingleInstance::raiseWindow(QWidget *window)
{
#if defined(Q_OS_WIN) // Windows
    Window winId = window->effectiveWinId();

    SetWindowPos(winId, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetWindowPos(winId, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
#elif defined(Q_OS_MACOS) // macOS
    window->show();
    window->raise();
    window->activateWindow();
#else // GNU/Linux, FreeBSD, etc
    Window winId = window->effectiveWinId();

    if (winId > 0) {
        Display* disp = XOpenDisplay(nullptr);

        if (disp) {
            XWindowAttributes attributes;

            if (XGetWindowAttributes(disp, winId, &attributes)) {
                XSetInputFocus(disp, winId, RevertToPointerRoot, CurrentTime);
                XRaiseWindow(disp, winId);

                // Show window if minimized, instead of just highlighting its icon
                {
                    const Atom atom = XInternAtom(disp, "_NET_ACTIVE_WINDOW", True);

                    if (atom != None) {
                        XEvent xev;

                        xev.xclient.type = ClientMessage;
                        xev.xclient.serial = 0;
                        xev.xclient.send_event = True;
                        xev.xclient.message_type = atom;
                        xev.xclient.display = disp;
                        xev.xclient.window = winId;
                        xev.xclient.format = 32;
                        xev.xclient.data.l[0] = 1;
                        xev.xclient.data.l[1] = 0;
                        xev.xclient.data.l[2] = None;
                        xev.xclient.data.l[3] = 0;
                        xev.xclient.data.l[4] = 0;

                        XSendEvent(disp, DefaultRootWindow(disp), False, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
                    }
                }
            }

            XFlush(disp);
            XCloseDisplay(disp);
        }
    }
#endif
}
