# Architectural Decision Records

This document contains overview of Liquid's software design represented as sections of nested lists.


## General

 - provide user with functionality maximally close to what is offered by popular web browsers
   - [x] uploading files
   - [ ] downloading files
   - [ ] camera support
   - [ ] microphone support

## Security and Privacy

 - default to maximum security and privacy settings, enable features on on-demand basis
   - [x] allow user to specify starting URL and additional domains to navigate within
     - [ ] add support for using wildcard (\*) pattern matching for domain names
     - [ ] allow user to completely cut off network requests to certain domains
       - [ ] allow user to use domain blacklist as domain whitelist
     - [x] do not let the user navigate to resources outside of specified domains
       - [x] make it possible to use modifier key to open system browser in order to navigate to external resources
   - [x] throw an error in case the web browser engine is not in OTR mode
   - [ ] always send Do Not Track HTTP header along with every network request
   - full control over cookies
     - [ ] option to store cookies inside the application config file
     - [x] option to reject all cookies
       - [x] option to specifically reject all third-party cookies
   - [ ] ability to disable localStorage

 - make the program provide both CLI and GUI i/o methods
   - [x] use lower-case flag and option names for CLI, upper-case for GUI
   - [x] make it possible to list all existing apps in CLI mode
   - [x] make it possible to initiate creation of new apps via CLI
   - [x] make it possible to run apps from CLI
   - [ ] make it possible to delete existing app via CLI

## User Interface

 - make the UI as user-friendly and easy to use as possible
   - switch fields automatically using basic logic and common sense, rather than disabling and forbidding the user from modifying them
 - maximal control over the UI
   - abilitiy to change page zoom level
     - [x] make the app zoom level extremely fine
       - [x] make it possible to increase in wider steps when holding additional modifier key
     - [x] make the app remember its zoom level
   - [x] ability to mute the app
     - [x] the app remembers its mute status
   - [x] prevent applications from going full-screen, let web pages go full-window instead
   - [x] completely disallow opening of popup windows
   - [x] ability to permanently "freeze" window size
   - [x] ability to hide scroll bars
   - [x] ability to remove window border
   - [ ] ability to hide window shadow
   - [x] allow application windows to be transparent, display see-through websites
   - [x] provide user with maximal ability of saving the contents of the app's window
     - [x] make it possible to take snapshots of the current view and of the full page
       - [x] allow snapshots to be semi-opaque
       - [ ] make it possible to take vector snapshots
     - [ ] make it possible to save pages as one single HTML file
     - [ ] make it possible to print the current page out
