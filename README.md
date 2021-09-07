# Liquid

Liquid is a tool that turns web resources into desktop applications.

It's capable of making websites appear and behave more like native OS applications: isolated, customizable, and running as separate system processes.


## Main features

- Fine zoom
- Keyboard shortcuts
- Window geometry (size) lock
- Option to enable or disable (third-party) Cookies
- Ability to inject additional CSS and JS code, disable JS
- Customizable User-Agent string

## Perks

1. Avoid tracking Cookies and protect yourself from phishing attacks
2. Simultaneously use as many web user accounts as you'd like
3. Make the Internet look the way you want by applying additional CSS:
   hide ads and fix those ugly visual quirks for good
4. Play around with remote web pages by injecting additional JS code


## Keyboard controls

- `Ctrl + R`: Refresh the view
- `Ctrl + Shift + R`: Make the app go back to its starting page
- `Ctrl + L`: Freeze window geometry
- `Ctrl + =`: Zoom in
- `Ctrl + -`: Zoom out
- `Ctrl + 0`: Reset zoom level
- `Ctrl + F`: Toggle full-screen mode
- `Escape`: Exit from full-screen mode
- `Ctrl + Q`, `Ctrl + W`: Close the app
- `Alt + Left`, `Backspace`: Go one step back
- `Alt + Right`: Go one step forward


## Build

    qmake
    make -j


## Install

    sudo make install


## Uninstall

    sudo make uninstall


## How to customize

Placing a file named `liquid.qss` into `~/.config/liquid/` will serve as custom stylesheet for the program.
You can use [liquid.qss](res/styles/liquid.qss) for reference.
