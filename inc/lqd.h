#pragma once

/* Various globals */
#define LQD_PROG_TITLE         "Liquid"
#define LQD_APPS_DIR_NAME      "apps"
#define LQD_DEFAULT_BG_COLOR   Qt::white
#define LQD_DEFAULT_PROXY_HOST "0.0.0.0"
#define LQD_DEFAULT_PROXY_PORT 8080
#define LQD_WIN_MIN_SIZE_W     200
#define LQD_WIN_MIN_SIZE_H     400
#define LQD_APP_WIN_MIN_SIZE_W 160
#define LQD_APP_WIN_MIN_SIZE_H 120
#define LQD_ZOOM_LVL_MIN       0.25
#define LQD_ZOOM_LVL_MAX       5.0 // Limited to 5.0 by Chromium
#define LQD_ZOOM_LVL_STEP      0.005

/* Textual icons */
#define LQD_ICON_ADD     "➕"
#define LQD_ICON_EDIT    "⚙"
#define LQD_ICON_ERROR   "❌"
#define LQD_ICON_LOADING "⏳"
#define LQD_ICON_LOCKED  "🖼"
#define LQD_ICON_MUTED   "🔇"
#define LQD_ICON_WARNING "⚠️"
#define LQD_ICON_DELETE  "✖"
#define LQD_ICON_RUN     "➤"

/*
App config key names.
Names must be given in a way that would let them answer to questions:
- "what is it?" (everything else)
- "do what?" (for booleans)
e.g.: ShowScrollBars, Icon, ZoomLevel, etc.
*/
#define LQD_CFG_KEY_ADDITIONAL_CSS          "AdditionalCSS" // text
#define LQD_CFG_KEY_ADDITIONAL_DOMAINS      "AdditionalDomains" // text, whitespace-separated items
#define LQD_CFG_KEY_ADDITIONAL_JS           "AdditionalJS" // text
#define LQD_CFG_KEY_ALLOW_COOKIES           "AllowCookies" // boolean, defults to FALSE
#define LQD_CFG_KEY_ALLOW_3RD_PARTY_COOKIES "AllowThirdPartyCookies" // boolean, defaults to FALSE
#define LQD_CFG_KEY_CUSTOM_BG_COLOR         "CustomBackgroundColor" // text
#define LQD_CFG_KEY_ENABLE_JS               "EnableJS" // boolean, defaults to FALSE
#define LQD_CFG_KEY_HIDE_SCROLL_BARS        "HideScrollBars" // boolean, defaults to FALSE
#define LQD_CFG_KEY_ICON                    "Icon" // text
#define LQD_CFG_KEY_LOCK_WIN_GEOM           "LockWindowGeometry" // boolean, defaults to FALSE
#define LQD_CFG_KEY_MUTE_AUDIO              "MuteAudio" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NOTES                   "Notes" // text
#define LQD_CFG_KEY_PROXY_HOST              "Proxy/Host" // text
#define LQD_CFG_KEY_PROXY_PORT              "Proxy/Port" // number
#define LQD_CFG_KEY_PROXY_USE_AUTH          "Proxy/UseAuthentication" // boolean, defaults to FALSE
#define LQD_CFG_KEY_PROXY_USE_SOCKS         "Proxy/UseSocks" // boolean, defaults to FALSE
#define LQD_CFG_KEY_PROXY_USER_NAME         "Proxy/UserName" // text
#define LQD_CFG_KEY_PROXY_USER_PASSWORD     "Proxy/UserPassword" // text
#define LQD_CFG_KEY_REMOVE_WINDOW_FRAME     "RemoveWindowFrame" // boolean, defaults to FALSE
#define LQD_CFG_KEY_TITLE                   "Title" // text
#define LQD_CFG_KEY_USE_PROXY               "UseProxy" // boolean, defaults to FALSE
#define LQD_CFG_KEY_USE_CUSTOM_BG           "UseCustomBackground" // boolean, defaults to FALSE
#define LQD_CFG_KEY_USER_AGENT              "UserAgent" // text
#define LQD_CFG_KEY_URL                     "URL" // text, required
#define LQD_CFG_KEY_WIN_GEOM                "WindowGeometry" // text
#define LQD_CFG_KEY_ZOOM_LVL                "ZoomLevel" // number, defaults to 1

/* Keyboard shortcuts (all windows and dialog boxes) */
#define LQD_KBD_SEQ_MUTE_AUDIO           "Ctrl+M"
#define LQD_KBD_SEQ_GO_BACK              "Ctrl+Left"
#define LQD_KBD_SEQ_GO_BACK_2            "Backspace"
#define LQD_KBD_SEQ_GO_FORWARD           "Ctrl+Right"
#define LQD_KBD_SEQ_RELOAD               "Ctrl+R"
#define LQD_KBD_SEQ_RELOAD_2             "F5"
#define LQD_KBD_SEQ_HARD_RELOAD          "Ctrl+Shift+R"
#define LQD_KBD_SEQ_TOGGLE_FS_MODE       "Ctrl+Shift+F"
#define LQD_KBD_SEQ_TOGGLE_FS_MODE_2     "F11"
#define LQD_KBD_SEQ_STOP_OR_EXIT_FS_MODE "Escape"
#define LQD_KBD_SEQ_TOGGLE_WIN_GEOM_LOCK "Ctrl+L"
#define LQD_KBD_SEQ_TAKE_SNAPSHOT        "Ctrl+T"
#define LQD_KBD_SEQ_TAKE_SNAPSHOT_FULL   "Ctrl+Shift+T"
#define LQD_KBD_SEQ_QUIT                 "Ctrl+Q"
#define LQD_KBD_SEQ_QUIT_2               "Ctrl+W"
#define LQD_KBD_SEQ_ZOOM_LVL_INC         "Ctrl+="
#define LQD_KBD_SEQ_ZOOM_LVL_DEC         "Ctrl+-"
#define LQD_KBD_SEQ_ZOOM_LVL_RESET       "Ctrl+0"