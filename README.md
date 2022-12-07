# :ocean: Liquid

Liquid is a tool that turns web pages into desktop applications.


## Features

| | Transparent websites |
|-:|:-:|
| Liquid is capable of making websites semi-opaque, utilizing 32 bits of depth instead of typical 24.  Not only this increases the amount of information on the screen, it literally adds one more creative dimension to web design, giving a glance at how websites will appear after mass adaption of transparent OLED screens. | [![Transparency snapshot](assets/images/transparency-square.png)](assets/images/transparency.png) |

| Full control and complete isolation | |
|:-:|:-|
| [![Transparency snapshot](assets/images/transparency-square.png)](assets/images/transparency.png) | Inspired by software such as Docker and Virtual Box, Liquid apps behave a lot like separate mobile device emulators rather than browser windows, providing full control over the encapsulated website, preventing it from accessing or performing actions without user's permission.<br>Those can be things such as: accessing unwanted network resources, entering full-screen mode, displaying desktop notifications, opening pop-up windows, playing sounds, using third-party cookies, etc. |

| | Next-generation snapshots |
|-:|:-:|
| PNG snapshots of Liquid apps that feature semi-opaque background will retain their transparency when saved to disk.<br>Additionally, support for vector (SVG) snapshots is another great feature that will ensure impeccable quality of the saved visual representation of the page.<br>Both raster and vector snapshots can be made in viewport and full-page modes. | [![Transparency snapshot](assets/images/transparency-square.png)](assets/images/transparency.png) |

| Saving complete web pages as single HTML file | |
|:-:|:-|
| [![Transparency snapshot](assets/images/transparency-square.png)](assets/images/transparency.png) | Similarly to MHT and Webarchive formats, Liquid is capable of saving current page as monolithic HTML documents by utilizing data URLs, which makes it much easier to store, share, and edit those files. |


## Working with the codebase

#### Build

```console
qmake
make -j
```

#### Install

```console
sudo make install
```

#### Uninstall

```console
sudo make uninstall
```


## Customization

Placing a file named `liquid.qss` into `~/.config/liquid/` will serve as additional stylesheet for the program.
You can use [base.qss](res/styles/base.qss) as reference.
