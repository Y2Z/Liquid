#pragma once

/* Global constants */
#define CONFIG_APPS_PATH                PROG_NAME "/" "apps"
#define CONFIG_PROG_NAME                "Liquid"
#define CONFIG_WIN_MINSIZE_W            320
#define CONFIG_WIN_MINSIZE_H            480
#define CONFIG_LIQUID_APP_WIN_MINSIZE_W 240
#define CONFIG_LIQUID_APP_WIN_MINSIZE_H 180
#define CONFIG_ZOOM_STEP                0.1
#define CONFIG_ZOOM_MIN                 0.5
#define CONFIG_ZOOM_MAX                 5

/* Textual (UTF-8) icons */
#define ICON_ADD     "➕"
#define ICON_EDIT    "⚙"
#define ICON_LOADING "⏳"
#define ICON_LOCKED  "🖼"
#define ICON_REMOVE  "✖"
#define ICON_RUN     "➤"

/* App settings key names */
#define SETTINGS_KEY_CUSTOM_CSS                "AdditionalCSS"
#define SETTINGS_KEY_CUSTOM_JS                 "AdditionalJS"
#define SETTINGS_KEY_BACKGROUND_COLOR          "BackgroundColor"
#define SETTINGS_KEY_ALLOW_COOKIES             "CookiesAllowed"
#define SETTINGS_KEY_USER_AGENT                "CustomUserAgent"
#define SETTINGS_KEY_ENABLE_JS                 "JavaScriptEnabled"
#define SETTINGS_KEY_TITLE                     "Title"
#define SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES "ThirdPartyCookiesAllowed"
#define SETTINGS_KEY_URL                       "URL"
#define SETTINGS_KEY_WINDOW_GEOMETRY           "WindowGeometry"
#define SETTINGS_KEY_WINDOW_GEOMETRY_LOCKED    "WindowGeometryLocked"
#define SETTINGS_KEY_ZOOM                      "ZoomLevel"

/* Keyboard shortcuts */
#define KEYBOARD_SHORTCUT_QUIT                                   "Ctrl+Q"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK        "Alt+Left"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_BACK_2      "Backspace"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_NAVIGATION_FORWARD     "Alt+Right"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD                 "Ctrl+R"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RELOAD_2               "F5"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_RESET                  "Ctrl+Shift+R"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_IN                "Ctrl+="
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_OUT               "Ctrl+-"
#define KEYBOARD_SHORTCUT_LIQUID_APP_PAGE_ZOOM_RESET             "Ctrl+0"
#define KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE    "Ctrl+Shift+F"
#define KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_TOGGLE_2  "F11"
#define KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_FULLSCREEN_EXIT      "Escape"
#define KEYBOARD_SHORTCUT_LIQUID_APP_WINDOW_GEOMETRY_LOCK_TOGGLE "Ctrl+L"
#define KEYBOARD_SHORTCUT_LIQUID_APP_QUIT                        "Ctrl+Q"
#define KEYBOARD_SHORTCUT_LIQUID_APP_QUIT_2                      "Ctrl+W"
