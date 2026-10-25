# :ocean: Liquid

Liquid is a tool that turns web pages into desktop applications.


## How it works

Liquid is capable of making websites appear and behave more like native OS applications: isolated, customizable, and running as separate system processes.

You’ll be able to:
 - encapsulate websites the way virtual machines and Docker do it for operating systems
 - safely and securely utilize different browsing identities
 - remain safe from being tracked by third-party websites
 - make sure you stay within specific domain range

What gets stored (if allowed): cookies.


## Comparison table

| Feature                                          | :ocean: Liquid | :earth_africa: Browsers | Notes                             |
|:-------------------------------------------------|:--------------:|:-------------------------------------:|:----------------------------------|
| Custom User-Agent string                         |       ✅       |            ✅           | Some browsers require a plug-in   |
| Window transparency                              |       ✅       |            ❌           | See-through websites              |
| Full-page snapshots                              |       ✅       |            ❌           | Possible with plug-ins            |
| Transparent snapshots                            |       ✅       |            ❌           | See-through snapshots of websites |
| Vector snapshots                                 |       ✅       |            ❌           | Experimental feature, SVG with selectable text |
| Ability to save pages as monolithic HTML files   |       ❌       |            ❌           | Possible with plug-ins            |
| Complete absence of pop-up windows               |       ✅       |            ❌           | Can be optionally disabled in most browsers |
| Ability to completely disable JS                 |       ✅       |            ✅           |                                   |
| Ability to disable all cookies                   |       ✅       |            ✅           |                                   |
| Ability to disable third-party cookies           |       ✅       |            ✅           |                                   |
| Ability to inject custom JS code into web pages  |       ✅       |            ❌           | Possible with plug-ins            |
| Support for HTTP basic authentication mechanism  |       ✅       |            ✅           |                                   |
| Ability to inject custom CSS code into web pages |       ✅       |            ❌           | Possible with plug-ins            |
| Limit websites to stay within specific domain(s) |       ✅       |            ❌           |                              |
| Simultaneous usage of multiple user accounts     |       ✅       |            ❌           | Can be achieved using profiles and extensions in some browsers   |
| Per-website proxy settings                       |       ✅       |            ❌           | Possible with plug-ins            |
| Ability to hide scroll bars                      |       ✅       |            ❌           |                                   |
| Window geometry lock                             |       ✅       |            ❌           | Possible with plug-ins            |
| Ability to remove window frame                   |       ✅       |            ❌           |                                   |
| Minimalistic tabless design                      |       ✅       |            ❌           |                                   |
| Fine zoom                                        |       ✅       |            ❌           |                                   |
| Search within the page                           |       ❌       |            ✅           |                                   |
| Permanently mute website                         |       ✅       |            ❌           | Browsers automatically unmute, Liquid remembers the state |
| Ability to go full-screen                        |       ✅       |            ✅           |                                   |
| Full control over full-screen capabilities       |       ✅       |            ❌           | Liquid acts more like a mobile device simulator when it comes to full-screen |
| Mandatory off-the-record capabilities            |       ✅       |            ❌           |                                   |


## Keyboard shortcuts

| Action                                    | Primary            | Alternative                       |
|:------------------------------------------|:------------------:|:---------------------------------:|
| Zoom in                                   | `Ctrl`+`=`         | `Ctrl`+_mouse wheel up_           |
| Zoom out                                  | `Ctrl`+`-`         | `Ctrl`+_mouse wheel down_         |
| Fine zoom in                              | `Ctrl`+`Shift`+`=` | `Ctrl`+`Shift`+_mouse wheel up_   |
| Fine zoom out                             | `Ctrl`+`Shift`+`-` | `Ctrl`+`Shift`+_mouse wheel down_ |
| Reset zoom level                          | `Ctrl`+`0`         | `Ctrl`+`Shift`+`0`                |
| Toggle full-screen mode                   | `Ctrl`+`Shift`+`F` | `F11`                             |
| Stop loading / exit from full-screen mode | `Esc`              |                                   |
| Take snapshot                             | `Ctrl`+`T`         |                                   |
| Take full-page snapshot                   | `Ctrl`+`Shift`+`T` |                                   |
| Take vector snapshot (SVG)                | `Ctrl`+`Shift`+`S` |                                   |
| Toggle window size lock                   | `Ctrl`+`L`         |                                   |
| Toggle mute                               | `Ctrl`+`M`         |                                   |
| Refresh current page                      | `Ctrl`+`R`         |                                   |
| Reload app                                | `Ctrl`+`Shift`+`R` |                                   |
| Close app                                 | `Ctrl`+`Q`         | `Ctrl`+`W`                        |
| Go back                                   | `Ctrl`+`←`         | `Backspace`                       |
| Go forward                                | `Ctrl`+`→`         |                                   |
| Open link using default web browser       | `Ctrl`+_click_     |                                   |


## Working with the codebase

#### Dependencies

Liquid is built with Qt 6 (Qt 5.15 is still supported), Qt WebEngine, and Qt SVG.

On Debian/Ubuntu:

```console
sudo apt install qt6-base-dev qt6-webengine-dev qt6-svg-dev qmake6 libx11-dev build-essential
```

For Qt 5.15, the packages are `qtbase5-dev qtwebengine5-dev libqt5svg5-dev qt5-qmake` instead.

Optionally, also install `qt6-base-private-dev` (requires Qt 6.5 or newer): it lets Liquid bring windows of already running Liquid apps to front on Wayland, instead of the desktop environment merely showing a notification.

On macOS and Windows, install Qt 6 with the Qt WebEngine, Qt WebChannel, and Qt Positioning modules (Qt SVG comes with Qt itself) using the [Qt Online Installer](https://www.qt.io/download-qt-installer).

#### Build

```console
qmake6
make -j
```

Use `qmake` instead of `qmake6` if it already points to Qt 6 (or to Qt 5.15) on your system.

#### Install

```console
sudo make install
```

#### Uninstall

```console
sudo make uninstall
```


## Where things are stored

Each Liquid app is a single `.liquid` file: an XML document containing everything about that app
(settings, custom CSS and JavaScript, window state, cookies, icon).
Liquid apps reside in the `apps` directory inside of Liquid's data directory, which is located at:

 - GNU/Linux, BSD, and others: `~/.liquid/`
 - macOS: `~/Library/Application Support/Liquid/`
 - Windows: `%LOCALAPPDATA%\Liquid\`

Keep in mind that `.liquid` files may contain secrets (cookies, proxy credentials).


## Customize

Placing a file named `liquid.qss` into Liquid's data directory will serve as additional stylesheet for the program.
You can use [base.qss](res/styles/base.qss) as reference.
