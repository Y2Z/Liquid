# Liquid

Liquid is a tool that turns web pages into desktop applications.

It’s capable of making websites appear and behave more like native OS applications: isolated, customizable, and running as separate system processes.


## Features

- Fine zoom
- Keyboard shortcuts
- Window geometry lock
- Option to allow or forbid (third-party) Cookies
- Ability to inject additional CSS and JS code, disable JS
- Customizable User-Agent string


## Perks

1. Avoid tracking Cookies and protect yourself from phishing attacks
2. Simultaneously use as many web user accounts as you like
3. Customize websites to make them look the way you want by applying additional CSS:
   hide ads, fix those ugly visual quirks for good, etc
4. Play around with remote web pages by injecting additional JS code


## Keyboard shortcuts

#### View
- `Ctrl + =`: Zoom in
- `Ctrl + -`: Zoom out
- `Ctrl + 0`: Reset zoom level
- `Ctrl + F`: Toggle full-screen mode
- `Ctrl + S`: Toggle scrollbar visibility
- `Escape`: Exit from full-screen mode
#### Control
- `Ctrl + R`: Refresh the current page
- `Ctrl + Shift + R`: Reload the app
- `Ctrl + L`: Lock/unlock window geometry
- `Ctrl + Q`, `Ctrl + W`: Close the app
- `Ctrl + <click>`: Open link using system’s browser
- `Alt + Left`, `Backspace`: Go one step back
- `Alt + Right`: Go one step forward


## Build

    qmake
    make -j


## Install

    sudo make install


## Uninstall

    sudo make uninstall


## Customize

Placing a file named `liquid.qss` into `~/.config/liquid/` will serve as custom stylesheet for the program.
You can use [liquid.qss](res/styles/liquid.qss) for reference.
