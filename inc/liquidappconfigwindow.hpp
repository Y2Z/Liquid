#pragma once

#include <QAction>
#include <QColorDialog>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QTableView>
#include <QTabWidget>
#include <QtGui>
#include <QtWidgets>

class LiquidAppConfigDialog : public QDialog
{
public:
    LiquidAppConfigDialog(QWidget* parent = Q_NULLPTR, QString liquidAppName = "");
    ~LiquidAppConfigDialog(void);

    QString getName(void);
    bool isPlanningToRun(void);
    void setPlanningToRun(const bool state);

public slots:
    void save();

private:
    void bindShortcuts(void);
    static QFrame* separator(void);

    bool isEditingExistingBool = false;
    bool isPlanningToRunBool = false;

    QAction* quitAction;

    QColor* backgroundColor;

    QLineEdit* nameInput;
    QLineEdit* addressInput;
    QCheckBox* createIconCheckBox;
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
    QCheckBox* enableLocalStorageCheckBox;
    QLabel* additionalJsLabel;
    QPlainTextEdit* additionalJsTextArea;

    // Cookies tab
    QCheckBox* allowCookiesCheckBox;
    QCheckBox* allowThirdPartyCookiesCheckBox;
    QTableView* cookiesTableView;
    QStandardItemModel* cookiesModel;

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
