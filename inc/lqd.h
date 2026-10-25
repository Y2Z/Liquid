#pragma once

/* Various globals */
#define LQD_PROG_TITLE         "Liquid"
#define LQD_APPS_DIR_NAME      "apps"
#define LQD_APP_FILE_EXTENSION "liquid"
#define LQD_ICONS_DIR_NAME     "icons" // Copies of Liquid apps' icons, for .desktop files to point at
#define LQD_DEFAULT_BG_COLOR   Qt::white
#define LQD_DEFAULT_PROXY_HOST "0.0.0.0"
#define LQD_DEFAULT_PROXY_PORT 8080
#define LQD_WIN_MIN_SIZE_W     200
#define LQD_WIN_MIN_SIZE_H     400
#define LQD_APP_WIN_MIN_SIZE_W 160
#define LQD_APP_WIN_MIN_SIZE_H 120
#define LQD_UI_MARGIN          24
#define LQD_SNAPSHOT_FLASH_HOLD     50 // Milliseconds at full brightness
#define LQD_SNAPSHOT_FLASH_FADE     300 // Milliseconds to fade out
#define LQD_SNAPSHOT_FLASH_OPACITY  0.8
#define LQD_FRAME_WAIT_WITHOUT_JS   200 // Milliseconds, used by full-page snapshots when JS is disabled
#define LQD_SNAPSHOT_LOAD_WAIT_TIMEOUT 30000 // Milliseconds, for how long snapshots wait for the page to finish loading
#define LQD_VECTOR_SNAPSHOT_TIMEOUT 60000 // Milliseconds
#define LQD_SNAPSHOT_MAX_PIXELS 134217728 // Full-page snapshot size limit, in logical pixels (128 megapixels, 512 MB of RAM)
#define LQD_ZOOM_LVL_MIN       0.25
#define LQD_ZOOM_LVL_MAX       5.0 // Limited to 5.0 by Chromium
#define LQD_ZOOM_LVL_STEP      0.04
#define LQD_ZOOM_LVL_STEP_FINE (LQD_ZOOM_LVL_STEP / 10)

/* Instance IDs (for preventing duplicate instances, and for talking to running ones) */
#define LQD_INSTANCE_ID_MAIN       "main"
#define LQD_INSTANCE_ID_APP_PREFIX "app/"

/* Messages that can be sent to running instances */
#define LQD_INSTANCE_MSG_RAISE           "raise"
#define LQD_INSTANCE_MSG_QUIT            "quit"            // Liquid apps only
#define LQD_INSTANCE_MSG_RELOAD_SETTINGS "reload-settings" // Liquid apps only
#define LQD_INSTANCE_MSG_PREVIEW_BG       "preview-background" // Liquid apps only, followed by a space and #AARRGGBB (or "none")
#define LQD_INSTANCE_MSG_PREVIEW_CSS      "preview-css"        // Liquid apps only, followed by a space and base64-encoded CSS
#define LQD_INSTANCE_MSG_PREVIEW_FRAME    "preview-frame"      // Liquid apps only, followed by a space and "on" or "off"
#define LQD_INSTANCE_MSG_PREVIEW_SCROLL   "preview-scrollbars" // Liquid apps only, followed by a space and "on" or "off"
#define LQD_INSTANCE_MSG_ICON_CHANGED    "icon-changed"    // Main window only, followed by a space and Liquid app's name
#define LQD_INSTANCE_MSG_STARTED         "started"         // Main window only, followed by a space and Liquid app's name
#define LQD_ACTIVATION_TOKEN_TIMEOUT     500               // Milliseconds to wait for the compositor to issue an activation token
#define LQD_PREVIEW_CONNECT_TIMEOUT      100               // Milliseconds to wait for connecting to a Liquid app for previewing settings
#define LQD_RECREATED_WINDOW_RAISE_DELAY 100               // Milliseconds to wait before bringing a re-created native window to front
#define LQD_SCROLLBARS_RELAYOUT_DELAY    100               // Milliseconds to keep the document unscrollable for, to make it redo its scrollbars
#define LQD_CSS_PREVIEW_DELAY            150               // Milliseconds to wait after the last keystroke before previewing CSS
#define LQD_ICON_DOWNLOAD_TIMEOUT        10000             // Milliseconds to wait for an icon (or web app manifest) to download
#define LQD_ICON_MAX_CANDIDATES          16                // How many icons a website gets to offer at most
#define LQD_ICON_MAX_FILE_SIZE           (4 * 1024 * 1024) // Bytes
#define LQD_ICON_MAX_SVG_SIZE            (512 * 1024)      // Bytes, larger SVG icons get rasterized
#define LQD_ICON_MAX_SIZE                256               // Pixels, larger icons get scaled down before they get stored
#define LQD_INSTANCE_STOP_TIMEOUT        5000              // Milliseconds to wait for a Liquid app to quit

/* Names of scripts Liquid injects into web pages */
#define LQD_SCRIPT_NAME_ADDITIONAL_CSS "liquid-additional-css"
#define LQD_SCRIPT_NAME_ADDITIONAL_JS  "liquid-additional-js"

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
#define LQD_ICON_STOP    "■"

/* Liquid App config file group names */
#define LQD_CFG_GROUP_NAME_COOKIES               "Cookies"
#define LQD_CFG_GROUP_NAME_PROXY                 "Proxy"

/*
 * Liquid App config key names.
 * Names must be given in a way that would let them answer to questions:
 * - "do what?" (for booleans)
 * - "what is it?" (everything else)
 * e.g.: ShowScrollBars, Icon, ZoomLevel, etc.
 */
#define LQD_CFG_KEY_NAME_ADDITIONAL_CSS          "AdditionalCSS" // text
#define LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS      "AdditionalDomains" // text, whitespace-separated items
#define LQD_CFG_KEY_NAME_ADDITIONAL_JS           "AdditionalJS" // text
#define LQD_CFG_KEY_NAME_ALLOW_COOKIES           "AllowCookies" // boolean, defults to FALSE
#define LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES "AllowThirdPartyCookies" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR         "CustomBackgroundColor" // text
#define LQD_CFG_KEY_NAME_ENABLE_JS               "EnableJS" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_HIDE_SCROLLBARS         "HideScrollBars" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_ICON                    "Icon" // text
#define LQD_CFG_KEY_NAME_LOCK_WIN_GEOM           "LockWindowGeometry" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_MUTE_AUDIO              "MuteAudio" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_NOTES                   "Notes" // text
#define LQD_CFG_KEY_NAME_PROXY_HOST              LQD_CFG_GROUP_NAME_PROXY "/" "Host" // text
#define LQD_CFG_KEY_NAME_PROXY_PORT              LQD_CFG_GROUP_NAME_PROXY "/" "Port" // number
#define LQD_CFG_KEY_NAME_PROXY_USE_AUTH          LQD_CFG_GROUP_NAME_PROXY "/" "UseAuthentication" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_PROXY_USE_SOCKS         LQD_CFG_GROUP_NAME_PROXY "/" "UseSocks" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_PROXY_USER_NAME         LQD_CFG_GROUP_NAME_PROXY "/" "UserName" // text
#define LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD     LQD_CFG_GROUP_NAME_PROXY "/" "UserPassword" // text
#define LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME     "RemoveWindowFrame" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_TITLE                   "Title" // text
#define LQD_CFG_KEY_NAME_USE_PROXY               "UseProxy" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_USE_CUSTOM_BG           "UseCustomBackground" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_USER_AGENT              "UserAgent" // text
#define LQD_CFG_KEY_NAME_URL                     "URL" // text, required
#define LQD_CFG_KEY_NAME_WIN_GEOM                "WindowGeometry" // text (used only by the main window)
#define LQD_CFG_KEY_NAME_WIN_X                   "WindowX" // number
#define LQD_CFG_KEY_NAME_WIN_Y                   "WindowY" // number
#define LQD_CFG_KEY_NAME_WIN_WIDTH               "WindowWidth" // number
#define LQD_CFG_KEY_NAME_WIN_HEIGHT              "WindowHeight" // number
#define LQD_CFG_KEY_NAME_WIN_MAXIMIZED           "WindowMaximized" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_WIN_FULLSCREEN          "WindowFullScreen" // boolean, defaults to FALSE
#define LQD_CFG_KEY_NAME_ZOOM_LVL                "ZoomLevel" // number, defaults to 1

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
#define LQD_KBD_SEQ_SAVE_PAGE            "Ctrl+S"
#define LQD_KBD_SEQ_TAKE_SNAPSHOT_VECTOR "Ctrl+Shift+S"
#define LQD_KBD_SEQ_ZOOM_LVL_INC         "Ctrl+="
#define LQD_KBD_SEQ_ZOOM_LVL_INC_FINE    "Ctrl+Shift+="
#define LQD_KBD_SEQ_ZOOM_LVL_DEC         "Ctrl+-"
#define LQD_KBD_SEQ_ZOOM_LVL_DEC_FINE    "Ctrl+Shift+-"
#define LQD_KBD_SEQ_ZOOM_LVL_RESET       "Ctrl+0"
#define LQD_KBD_SEQ_ZOOM_LVL_RESET_2     "Ctrl+Shift+0"
