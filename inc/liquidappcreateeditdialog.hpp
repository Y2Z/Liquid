#pragma once

#include <QAction>
#include <QColorDialog>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QListView>
#include <QPlainTextEdit>
#include <QTabWidget>
#include <QtGui>
#include <QtWidgets>

class LiquidAppCreateEditDialog : public QDialog
{
public:
    LiquidAppCreateEditDialog(QWidget* parent = Q_NULLPTR, QString liquidAppName = "");
    ~LiquidAppCreateEditDialog(void);

    QString getName(void);

public slots:
    void save();

private:
    void bindShortcuts(void);

    QAction* quitAction;

    bool isEditingExisting = false;

    QString backgroundColorName;

    QVBoxLayout* mainLayout;

    QGridLayout* basicLayout;
    QLabel* nameInputLabel;
    QLineEdit* nameInput;
    QLabel* addressInputLabel;
    QLineEdit* addressInput;
    QCheckBox* createIconCheckBox;

    QHBoxLayout* buttonsLayout;
    QPushButton* advancedButton;
    QPushButton* cancelButton;
    QPushButton* saveButton;

    QWidget* advancedWidget;
    QVBoxLayout* advancedLayout;

    QTabWidget* tabWidget;

    // General tab
    QWidget* generalTabWidget;
    QVBoxLayout* generalTabWidgetLayout;
    QLineEdit* titleInput;
    QLabel* additionalDomainsListLabel;
    QListView* additionalDomainsListView;
    QStandardItemModel* additionalDomainsModel;
    QLineEdit* userAgentInput;
    QPlainTextEdit* notesArea;

    // Appearance tab
    QWidget* appearanceTabWidget;
    QVBoxLayout* appearanceTabWidgetLayout;
    QPushButton* customBackgroundColorButton;
    QPlainTextEdit* additionalCssTextArea;
    QCheckBox* hideScrollBarsCheckBox;

    // JavaScript tab
    QWidget* jsTabWidget;
    QVBoxLayout* jsTabWidgetLayout;
    QCheckBox* enableJavaScriptCheckBox;
    QLabel* additionalJsLabel;
    QPlainTextEdit* additionalJsTextArea;

    // Cookies tab
    QWidget* cookiesTabWidget;
    QVBoxLayout* cookiesTabWidgetLayout;
    QCheckBox* allowCookiesCheckBox;
    QCheckBox* allowThirdPartyCookiesCheckBox;

    // Network tab
    QWidget* networkTabWidget;
    QVBoxLayout* networkTabWidgetLayout;
    QRadioButton* proxyModeSystemRadioButton;
    QRadioButton* proxyModeDirectRadioButton;
    QRadioButton* proxyModeCustomRadioButton;
    QComboBox* useSocksSelectBox;
    QLineEdit* proxyHostnameInput;
    QSpinBox* proxyPortInput;
};
