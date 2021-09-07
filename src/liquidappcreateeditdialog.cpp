#include "config.h"

#include "liquidappcreateeditdialog.hpp"
#include "mainwindow.hpp"

LiquidAppCreateEditDialog::LiquidAppCreateEditDialog(QWidget *parent, QString liquidAppName) : QDialog(parent)
{
    liquidAppName = liquidAppName.replace(QDir::separator(), "_");
    // Attempt to load liquid app's config file
    QSettings *existingLiquidAppSettings = new QSettings(QSettings::IniFormat,
                                                         QSettings::UserScope,
                                                         CONFIG_APPS_PATH,
                                                         liquidAppName,
                                                         nullptr);

    // Attempt to load app settings from a config file
    if (liquidAppName.size() > 0) {
        isEditingExisting = existingLiquidAppSettings->contains(SETTINGS_KEY_URL);
    } else {
        delete existingLiquidAppSettings;
    }

    setWindowTitle(tr((isEditingExisting) ? "Edit existing Liquid app" : "Create new Liquid app"));
    setWindowFlags(Qt::Window);

    backgroundColorName = QColor(Qt::white).name();

    advanced = new QWidget;

    QVBoxLayout *basicLayout = new QVBoxLayout;
    basicLayout->setSpacing(0);
    basicLayout->setMargin(0);
    {
        nameLabel = new QLabel(tr("Name:"));
        nameInput = new QLineEdit;
        nameInput->setMinimumSize(500, 25);
        nameInput->setPlaceholderText(tr("Name"));
        nameInput->setText(liquidAppName);
        if (isEditingExisting) {
            // TODO: make it possible to edit names for existing Liquid apps
            nameInput->setReadOnly(true);
        }
        nameLabel->setBuddy(nameInput);
        basicLayout->addWidget(nameInput);
    }
    {
        addressLabel = new QLabel(tr("Address:"));
        addressInput = new QLineEdit;
        addressInput->setPlaceholderText(tr("URL"));
        if (isEditingExisting) {
            addressInput->setText(existingLiquidAppSettings->value(SETTINGS_KEY_URL).toString());
        }
        addressLabel->setBuddy(addressInput);
        basicLayout->addWidget(addressInput);
    }

    QHBoxLayout *basicButtonsLayout = new QHBoxLayout;
    basicButtonsLayout->setSpacing(0);
    basicButtonsLayout->setMargin(0);
    {
        advancedButton = new QPushButton(tr("Advanced"));
        advancedButton->setCheckable(true);
        basicButtonsLayout->addWidget(advancedButton);
    }
    {
        cancelButton = new QPushButton(tr("Cancel"));
        basicButtonsLayout->addWidget(cancelButton);
    }
    {
        saveButton = new QPushButton(tr((isEditingExisting) ? "Save" : "Create"));
        saveButton->setDefault(true);
        basicButtonsLayout->addWidget(saveButton);
    }
    basicLayout->addLayout(basicButtonsLayout);

    QVBoxLayout *advancedLayout = new QVBoxLayout;
    advancedLayout->setMargin(0);
    {
        titleInput = new QLineEdit;
        titleInput->setPlaceholderText(tr("Title"));
        if (isEditingExisting) {
            titleInput->setText(existingLiquidAppSettings->value(SETTINGS_KEY_TITLE).toString());
        }
        advancedLayout->addWidget(titleInput);
    }
    {
        enableJavaScriptCheckBox = new QCheckBox(tr("Enable JavaScript"));
        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(SETTINGS_KEY_ENABLE_JS).toBool();
            enableJavaScriptCheckBox->setChecked(isChecked);
        } else {
            enableJavaScriptCheckBox->setChecked(true);
        }
        advancedLayout->addWidget(enableJavaScriptCheckBox);
    }
    {
        allowCookiesCheckBox = new QCheckBox(tr("Allow Cookies"));
        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(SETTINGS_KEY_ALLOW_COOKIES).toBool();
            allowCookiesCheckBox->setChecked(isChecked);
        } else {
            allowCookiesCheckBox->setChecked(true);
        }
        advancedLayout->addWidget(allowCookiesCheckBox);
    }
    {
        allowThirdPartyCookiesCheckBox = new QCheckBox(tr("Allow third-party Cookies"));
        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES).toBool();
            allowThirdPartyCookiesCheckBox->setChecked(isChecked);
        }
        advancedLayout->addWidget(allowThirdPartyCookiesCheckBox);
    }
    if (!isEditingExisting) {
        createIconCheckBox = new QCheckBox(tr("Create desktop icon"));
        createIconCheckBox->setChecked(true);
        advancedLayout->addWidget(createIconCheckBox);
    }
    {
        customBackgroundButton = new QPushButton(tr("Set custom background color"));
        if (isEditingExisting) {
            if (existingLiquidAppSettings->contains(SETTINGS_KEY_BACKGROUND_COLOR)) {
                backgroundColorName = existingLiquidAppSettings->value(SETTINGS_KEY_BACKGROUND_COLOR).toString();
                backgroundColorName = QColor(backgroundColorName).name();

                customBackgroundButton->setText(tr("Custom background color: %1").arg(backgroundColorName));
            }
        }
        // TODO: make it possible to remove background color setting
        advancedLayout->addWidget(customBackgroundButton);
    }
    {
        customCSSarea = new QPlainTextEdit();
        customCSSarea->setPlaceholderText(tr("Additional CSS"));
        if (isEditingExisting) {
            customCSSarea->setPlainText(existingLiquidAppSettings->value(SETTINGS_KEY_CUSTOM_CSS).toString());
        }
        advancedLayout->addWidget(customCSSarea);
    }
    {
        customJSarea = new QPlainTextEdit();
        customJSarea->setPlaceholderText(tr("Additonal JavaScript code"));
        if (isEditingExisting) {
            customJSarea->setPlainText(existingLiquidAppSettings->value(SETTINGS_KEY_CUSTOM_JS).toString());
        }
        advancedLayout->addWidget(customJSarea);
    }
    {
        userAgentInput = new QLineEdit;
        userAgentInput->setPlaceholderText(tr("Custom user-agent string"));
        if (isEditingExisting) {
            userAgentInput->setText(existingLiquidAppSettings->value(SETTINGS_KEY_USER_AGENT).toString());
        }
        advancedLayout->addWidget(userAgentInput);
    }
    advanced->setLayout(advancedLayout);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(0);
    mainLayout->setMargin(4);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);
    mainLayout->addLayout(basicLayout);
    mainLayout->addWidget(advanced);

    setLayout(mainLayout);

    // Connect keyboard shortcuts
    bindShortcuts();

    if (isEditingExisting) {
        // Force advanced section to be visible in edit mode
        advancedButton->toggle();
    } else {
        advanced->hide();
    }

    // Connect signals to slots
    connect(advancedButton, SIGNAL(toggled(bool)), advanced, SLOT(setVisible(bool)));
    QObject::connect(cancelButton, &QPushButton::clicked, [=]() {
        close();
    });
    QObject::connect(saveButton, &QPushButton::clicked, [=]() {
        save();
    });
    connect(allowCookiesCheckBox, SIGNAL(toggled(bool)), allowThirdPartyCookiesCheckBox, SLOT(setEnabled(bool)));
    QObject::connect(customBackgroundButton, &QPushButton::clicked, [=]() {
        QColor color = QColorDialog::getColor(QColor(backgroundColorName), this);

        if (color.isValid()) {
            backgroundColorName = color.name();

            customBackgroundButton->setText(tr("Custom background color: %1").arg(backgroundColorName));
        } else {
            if (backgroundColorName.size() > 0) {
                customBackgroundButton->setText(tr("Custom background color: %1").arg(backgroundColorName));
            } else {
                customBackgroundButton->setText(tr("Set custom background color"));
            }
        }
    });
}

LiquidAppCreateEditDialog::~LiquidAppCreateEditDialog()
{
}

void LiquidAppCreateEditDialog::save()
{
    bool isFormValid = nameInput->text().size() > 0 && addressInput->text().size() > 0;

    if (!isFormValid) {
        return;
    }

    QString appName = nameInput->text();
    // Replace directory separators (slashes) with underscores
    // to ensure no sub-directories would get created
    appName = appName.replace(QDir::separator(), "_");
    QSettings *tempAppSettings = new QSettings(QSettings::IniFormat,
                                               QSettings::UserScope,
                                               CONFIG_APPS_PATH,
                                               appName,
                                               nullptr);

    // TODO: check if the given Liquid app name is already in use

    // Starting URL
    {
        QUrl url(QUrl::fromUserInput(addressInput->text()));
        tempAppSettings->setValue(SETTINGS_KEY_URL, url.toString());
    }

    // Enable JS
    {
        tempAppSettings->setValue(SETTINGS_KEY_ENABLE_JS, enableJavaScriptCheckBox->isChecked());
    }

    // Allow cookies
    {
        tempAppSettings->setValue(SETTINGS_KEY_ALLOW_COOKIES, allowCookiesCheckBox->isChecked());
    }

    // Allow third-party cookies
    {
        tempAppSettings->setValue(SETTINGS_KEY_ALLOW_THIRD_PARTY_COOKIES, allowThirdPartyCookiesCheckBox->isChecked());
    }

    // Custom Liquid app window title
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(SETTINGS_KEY_TITLE) && titleInput->text().size() == 0) {
                 tempAppSettings->remove(SETTINGS_KEY_TITLE);
            } else {
                if (tempAppSettings->value(SETTINGS_KEY_TITLE).toString().size() > 0
                    || titleInput->text().size() > 0
                ) {
                    tempAppSettings->setValue(SETTINGS_KEY_TITLE, titleInput->text());
                }
            }
        } else {
            if (titleInput->text() != "") {
                tempAppSettings->setValue(SETTINGS_KEY_TITLE, titleInput->text());
            }
        }
    }

    // Custom CSS textarea
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(SETTINGS_KEY_CUSTOM_CSS) && customCSSarea->toPlainText().size() == 0) {
                 tempAppSettings->remove(SETTINGS_KEY_CUSTOM_CSS);
            } else {
                if (tempAppSettings->value(SETTINGS_KEY_CUSTOM_CSS).toString().size() > 0
                    || customCSSarea->toPlainText().size() > 0
                ) {
                    tempAppSettings->setValue(SETTINGS_KEY_CUSTOM_CSS, customCSSarea->toPlainText());
                }
            }
        } else {
            if (customCSSarea->toPlainText() != "") {
                tempAppSettings->setValue(SETTINGS_KEY_CUSTOM_CSS, customCSSarea->toPlainText());
            }
        }
    }

    // Custom JS textarea
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(SETTINGS_KEY_CUSTOM_JS) && customJSarea->toPlainText().size() == 0) {
                 tempAppSettings->remove(SETTINGS_KEY_CUSTOM_JS);
            } else {
                if (tempAppSettings->value(SETTINGS_KEY_CUSTOM_JS).toString().size() > 0
                    || customJSarea->toPlainText().size() > 0
                ) {
                    tempAppSettings->setValue(SETTINGS_KEY_CUSTOM_JS, customJSarea->toPlainText());
                }
            }
        } else {
            if (customJSarea->toPlainText() != "") {
                tempAppSettings->setValue(SETTINGS_KEY_CUSTOM_JS, customJSarea->toPlainText());
            }
        }
    }

    // Custom user-agent text input
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(SETTINGS_KEY_USER_AGENT) && userAgentInput->text().size() == 0) {
                 tempAppSettings->remove(SETTINGS_KEY_USER_AGENT);
            } else {
                if (tempAppSettings->value(SETTINGS_KEY_USER_AGENT).toString().size() > 0
                    || userAgentInput->text().size() > 0
                ) {
                    tempAppSettings->setValue(SETTINGS_KEY_USER_AGENT, userAgentInput->text());
                }
            }
        } else {
            if (userAgentInput->text() != "") {
                tempAppSettings->setValue(SETTINGS_KEY_USER_AGENT, userAgentInput->text());
            }
        }
    }

    // Create desktop icon checkbox
    {
        // TODO: make it possible to remove and create desktop icons for existing Liquid apps
        if (!isEditingExisting) {
            if (createIconCheckBox->isChecked()) {
                QUrl url(QUrl::fromUserInput(addressInput->text()));
                ((MainWindow*)parent())->createDesktopFile(appName, url);
            }
        }
    }

    // Custom background color button
    {
        if (backgroundColorName.size() > 0) {
            tempAppSettings->setValue(SETTINGS_KEY_BACKGROUND_COLOR, backgroundColorName);
        }
    }

    accept();
}

QString LiquidAppCreateEditDialog::getName()
{
    return nameInput->text();
}

void LiquidAppCreateEditDialog::bindShortcuts()
{
    // Connect the exit keyboard shortcut
    quitAction.setShortcut(QKeySequence(tr(KEYBOARD_SHORTCUT_QUIT)));
    addAction(&quitAction);
    connect(&quitAction, SIGNAL(triggered()), this, SLOT(close()));
}
