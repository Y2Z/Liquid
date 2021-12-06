# :ocean: Liquid

Liquid is a tool that turns web pages into desktop applications.

It’s capable of making websites appear and behave more like native OS applications: isolated, customizable, and running as separate system processes.

You’ll be able to:
 - encapsulate websites the way virtual machines and Docker do it for operating systems
 - safely and securely utilize different browsing identities
 - remain safe from being tracked by third-party websites
 - make sure you stay within specific domain range


## Comparison table

| Feature                                          | :ocean: Liquid | :earth_africa: Browsers | Notes                             |
|:-------------------------------------------------|:--------------:|:-------------------------------------:|:----------------------------------|
| Custom User-Agent string                         |       ✅       |            ✅           | Some browsers require a plug-in   |
| Window transparency                              |       ✅       |            ❌           | See-through websites              |
| Full-page snapshots                              |       ✅       |            ❌           | Possible with plug-ins            |
| Transparent snapshots                            |       ✅       |            ❌           | See-through snapshots of websites |
| Vector snapshots                                 |       ❌       |            ❌           | Experimental feature, SVG         |
| Ability to save pages as monolithic HTML files   |       ❌       |            ❌           | Possible with plug-ins            |
| Complete absence of pop-up windows               |       ✅       |            ❌           | Can be optionally disabled in most browsers |
| Ability to completely disable JS                 |       ✅       |            ✅           |                                   |
| Ability to disable all cookies                   |       ✅       |            ✅           |                                   |
| Ability to disable third-party cookies           |       ✅       |            ✅           |                                   |
| Ability to inject custom JS code into web pages  |       ✅       |            ❌           | Possible with plug-ins            |
| Ability to inject custom CSS code into web pages |       ✅       |            ❌           | Possible with plug-ins            |
| Limit websites to stay within specific domain(s) |       ✅       |            ❌           |                              |
| Simultaneous usage of multiple user accounts     |       ✅       |            ❌           | Can be achieved using profiles and extensions in some browsers   |
| Per-website proxy settings                       |       ✅       |            ❌           | Possible with plug-ins            |
| Ability to hide scroll bars                      |       ✅       |            ❌           |                                   |
| Window geometry lock                             |       ✅       |            ❌           | Possible with plug-ins            |
| Ability to remove window frame                   |       ✅       |            ❌           |                                   |
| Minimalistic tabless design                      |       ✅       |            ❌           |                                   |
| Fine zoom                                        |       ✅       |            ❌           |                                   |
| Search within the page                           |       ❌       |            ✅           | TODO                              |
| Permanently mute website                         |       ✅       |            ❌           | Browsers automatically unmute, Liquid remembers the state |
| Ability to go full-screen                        |       ✅       |            ✅           |                                   |
| Full control over full-screen capabilities       |       ✅       |            ❌           | Liquid acts more like a mobile device simulator when it comes to full-screen |
| Mandatory off-the-record capabilities            |       ✅       |            ❌           | The only thing that gets stored is cookies, if allowed |


## Keyboard shortcuts

| Action                                    |                    |                                   |
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
| Toggle window size lock                   | `Ctrl`+`L`         |                                   |
| Toggle mute                               | `Ctrl`+`M`         |                                   |
| Refresh current page                      | `Ctrl`+`R`         |                                   |
| Reload app                                | `Ctrl`+`Shift`+`R` |                                   |
| Close app                                 | `Ctrl`+`Q`         | `Ctrl`+`W`                        |
| Go back                                   | `Ctrl`+`←`         | `Backspace`                       |
| Go forward                                | `Ctrl`+`→`         |                                   |
| Open link using default web browser       | `Ctrl`+_click_     |                                   |


## Build

```console
qmake
make -j
```


## Install

```console
sudo make install
```


## Uninstall

```console
sudo make uninstall
```


## Customize

Placing a file named `liquid.qss` into `~/.config/liquid/` will serve as custom stylesheet for the program.
You can use [liquid.qss](res/styles/liquid.qss) for reference.
