#pragma once

#include <QAction>
#include <QColorDialog>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QLocalSocket>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTabWidget>
#include <QtGui>
#include <QtWidgets>
#include <QJsonObject>

#include "liquidappicon.hpp"

class LiquidAppConfigDialog : public QDialog
{
public:
    LiquidAppConfigDialog(QWidget* parent = Q_NULLPTR, QString liquidAppName = "");
    ~LiquidAppConfigDialog(void);

    QString getName(void);
    bool isPlanningToRun(void);
    void setPlanningToRun(const bool state);

    // When opened by the Liquid app itself, the dialog doesn't apply saved settings to it (the Liquid app does that)
    void setOpenedFromLiquidApp(const bool state);
    // Whether saved settings can only take effect once the Liquid app restarts
    bool isRestartRequired(void);

public slots:
    void save();
    void reject() override;

private:
    void bindShortcuts(void);
    static QFrame* separator(void);
    static QString getSettingsRequiringRestart(QSettings* liquidAppConfig);
    void previewBackground(const QColor& color);
    static QString getStorageItemId(const QString& origin, const QString& key);
    void setUpStorageTableView(QTableView* tableView, QStandardItemModel* model, const int stretchingColumn);
    static QStandardItem* newRemoveButtonItem(void);
    void addRemoveButtons(QTableView* tableView, QStandardItemModel* model);
    void removeStorageItem(QTableView* tableView, const int row);
    void fillCookiesModel(QSettings* liquidAppConfig);
    void fillLocalStorageModel(QSettings* liquidAppConfig);
    void fillSessionStorageModel(void);
    void refreshStorage(const QString& liquidAppName);
    bool connectToLiquidApp(void);
    void sendPreview(const QByteArray& message);

    bool isEditingExistingBool = false;
    bool isPlanningToRunBool = false;
    bool isOpenedFromLiquidAppBool = false;
    bool isRestartRequiredBool = false;

    // Connection to the Liquid app being edited (if running), for previewing changes
    QLocalSocket* liquidAppConnection = Q_NULLPTR;
    bool hasPreviewedChanges = false;
    QTimer* cssPreviewTimer = Q_NULLPTR;

    QAction* quitAction;

    QColor* backgroundColor;

    QLineEdit* nameInput;
    QLineEdit* addressInput;
    QCheckBox* createIconCheckBox;
    QToolButton* iconButton;
    LiquidAppIcon chosenIcon;
    bool iconWasEdited = false;
    QCheckBox* planningToRunCheckBox = Q_NULLPTR;

    // General tab
    QLineEdit* titleInput;
    QListView* additionalDomainsListView;
    QStandardItemModel* additionalDomainsModel;
    QLineEdit* userAgentInput;
    QPlainTextEdit* notesTextArea;

    // Appearance tab
    QCheckBox* hideScrollBarsCheckBox;
    QCheckBox* removeWindowFrameCheckBox;
    QCheckBox* useCustomBackgroundCheckBox;
    QPushButton* customBackgroundColorButton;
    QPlainTextEdit* additionalCssTextArea;

    // JavaScript tab
    QCheckBox* enableJavaScriptCheckBox;
    QLabel* additionalJsLabel;
    QPlainTextEdit* additionalJsTextArea;

    // Cookies tab
    QCheckBox* allowCookiesCheckBox;
    QCheckBox* allowThirdPartyCookiesCheckBox;
    QTableView* cookiesTableView;
    QStandardItemModel* cookiesModel;
    QCheckBox* rememberLocalStorageCheckBox;
    QTableView* localStorageTableView;
    QStandardItemModel* localStorageModel;
    QTableView* sessionStorageTableView;
    QStandardItemModel* sessionStorageModel;
    // What's been removed or edited here, kept on top of what the Liquid app stores (until saved, or discarded)
    QSet<QString> removedCookieIds; // As identified in the Liquid app's file
    QSet<QString> removedLocalStorageIds; // See getStorageItemId()
    QMap<QString, QJsonObject> localStorageEdits; // { "origin", "key", "value" } by ID
    QSet<QString> removedSessionStorageIds;
    // Latest session storage the Liquid app has sent over: { "origin", "items" }
    QJsonObject sessionStorage;
    // For showing cookies and local storage items as the Liquid app stores them
    QFileSystemWatcher* liquidAppFileWatcher = Q_NULLPTR;
    QTimer* storageRefreshTimer = Q_NULLPTR;

    // Network tab
    QRadioButton* proxyModeSystemRadioButton;
    QRadioButton* proxyModeDirectRadioButton;
    QRadioButton* proxyModeCustomRadioButton;
    QComboBox* useSocksSelectBox;
    QLineEdit* proxyHostInput;
    QSpinBox* proxyPortInput;
    QCheckBox* proxyUseAuthCheckBox;
    QLineEdit* proxyUsernameInput;
    QLineEdit* proxyPasswordInput;
};
