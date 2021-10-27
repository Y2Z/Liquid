#pragma once

#include <QAction>
#include <QColorDialog>
#include <QDialog>
#include <QListView>
#include <QPlainTextEdit>
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

protected:
    QString backgroundColorName;

private:
    void bindShortcuts(void);

    bool isEditingExisting = false;

    QAction quitAction;

    QLabel* nameLabel;
    QLabel* addressLabel;
    QLineEdit* nameInput;
    QLineEdit* addressInput;
    QPushButton* advancedButton;
    QPushButton* cancelButton;
    QPushButton* saveButton;
    QWidget* advanced;
    QLineEdit* titleInput;
    QCheckBox* enableJavaScriptCheckBox;
    QCheckBox* allowCookiesCheckBox;
    QCheckBox* allowThirdPartyCookiesCheckBox;
    QCheckBox* hideScrollBarsCheckBox;
    QPushButton* customBackgroundButton;
    QListView* additionalDomainsListView;
    QStandardItemModel* additionalDomainsModel;
    QPlainTextEdit* additionalCssTextArea;
    QPlainTextEdit* additionalJsTextArea;
    QLineEdit* userAgentInput;
    QPlainTextEdit* notesArea;
    QCheckBox* createIconCheckBox;
};
