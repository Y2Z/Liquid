#pragma once

#include <QAction>
#include <QDialog>
#include <QPlainTextEdit>
#include <QSettings>
#include <QtGui>
#include <QtWidgets>
#include <QColorDialog>

class QCheckBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;

class LiquidAppCreateEditDialog : public QDialog
{
public:
    LiquidAppCreateEditDialog(QWidget *parent = nullptr, QString liquidAppName = "");
    ~LiquidAppCreateEditDialog();

    QString getName();

public slots:
    void save();

protected:
    QString backgroundColorName;

private:
    void bindShortcuts();

    bool isEditingExisting = false;

    QAction quitAction;
    QLabel *nameLabel;
    QLabel *addressLabel;
    QLineEdit *nameInput;
    QLineEdit *addressInput;
    QPushButton *advancedButton;
    QPushButton *cancelButton;
    QPushButton *saveButton;
    QWidget *advanced;
    QLineEdit *titleInput;
    QCheckBox *privateModeCheckBox;
    QCheckBox *allowCookiesCheckBox;
    QCheckBox *allowThirdPartyCookiesCheckBox;
    QCheckBox *enableJavaScriptCheckBox;
    QPlainTextEdit *customCSSarea;
    QPlainTextEdit *customJSarea;
    QLineEdit *userAgentInput;
    QCheckBox *createIconCheckBox;
    QPushButton *customBackgroundButton;
};
