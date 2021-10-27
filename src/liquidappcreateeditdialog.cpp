#include <QDir>

#include "globals.h"

#include "liquidappcreateeditdialog.hpp"
#include "mainwindow.hpp"

LiquidAppCreateEditDialog::LiquidAppCreateEditDialog(QWidget *parent, QString liquidAppName) : QDialog(parent)
{
    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

    // Attempt to load liquid app's config file
    QSettings* existingLiquidAppSettings = new QSettings(QSettings::IniFormat,
                                                         QSettings::UserScope,
                                                         QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                         liquidAppName,
                                                         nullptr);

    // Attempt to load app settings from a config file
    if (liquidAppName.size() > 0) {
        isEditingExisting = existingLiquidAppSettings->contains(LQD_CFG_KEY_URL);
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
            addressInput->setText(existingLiquidAppSettings->value(LQD_CFG_KEY_URL).toString());
        }

        addressLabel->setBuddy(addressInput);
        basicLayout->addWidget(addressInput);
    }

    QHBoxLayout *basicButtonsLayout = new QHBoxLayout;
    basicButtonsLayout->setSpacing(0);
    basicButtonsLayout->setMargin(0);
    {
        advancedButton = new QPushButton(tr("Advanced"));
        advancedButton->setCursor(Qt::PointingHandCursor);
        advancedButton->setCheckable(true);
        basicButtonsLayout->addWidget(advancedButton);
    }
    {
        cancelButton = new QPushButton(tr("Cancel"));
        cancelButton->setCursor(Qt::PointingHandCursor);
        basicButtonsLayout->addWidget(cancelButton);
    }
    {
        saveButton = new QPushButton(tr((isEditingExisting) ? "Save" : "Create"));
        saveButton->setCursor(Qt::PointingHandCursor);
        saveButton->setDefault(true);
        basicButtonsLayout->addWidget(saveButton);
    }
    basicLayout->addLayout(basicButtonsLayout);

    QVBoxLayout* advancedLayout = new QVBoxLayout;
    advancedLayout->setMargin(0);
    {
        titleInput = new QLineEdit;
        titleInput->setPlaceholderText(tr("Title"));

        if (isEditingExisting) {
            titleInput->setText(existingLiquidAppSettings->value(LQD_CFG_KEY_TITLE).toString());
        }

        advancedLayout->addWidget(titleInput);
    }
    {
        enableJavaScriptCheckBox = new QCheckBox(tr("Enable JavaScript"));
        enableJavaScriptCheckBox->setCursor(Qt::PointingHandCursor);

        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(LQD_CFG_KEY_ENABLE_JS).toBool();
            enableJavaScriptCheckBox->setChecked(isChecked);
        } else {
            // Checked by default (when creating new Liquid app)
            enableJavaScriptCheckBox->setChecked(true);
        }

        advancedLayout->addWidget(enableJavaScriptCheckBox);
    }
    {
        allowCookiesCheckBox = new QCheckBox(tr("Allow Cookies"));
        allowCookiesCheckBox->setCursor(Qt::PointingHandCursor);

        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(LQD_CFG_KEY_ALLOW_COOKIES).toBool();
            allowCookiesCheckBox->setChecked(isChecked);
        } else {
            // Checked by default (when creating new Liquid app)
            allowCookiesCheckBox->setChecked(true);
        }

        advancedLayout->addWidget(allowCookiesCheckBox);
    }
    {
        allowThirdPartyCookiesCheckBox = new QCheckBox(tr("Allow third-party Cookies"));
        allowThirdPartyCookiesCheckBox->setCursor(Qt::PointingHandCursor);

        if (!allowCookiesCheckBox->isChecked()) {
            allowThirdPartyCookiesCheckBox->setEnabled(false);
        }

        if (isEditingExisting) {
            bool isChecked = existingLiquidAppSettings->value(LQD_CFG_KEY_ALLOW_3RD_PARTY_COOKIES).toBool();
            allowThirdPartyCookiesCheckBox->setChecked(isChecked);
        }

        advancedLayout->addWidget(allowThirdPartyCookiesCheckBox);
    }
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    {
        hideScrollBarsCheckBox = new QCheckBox(tr("Hide scroll bars"));
        hideScrollBarsCheckBox->setCursor(Qt::PointingHandCursor);

        if (isEditingExisting) {
            if (existingLiquidAppSettings->contains(LQD_CFG_KEY_HIDE_SCROLL_BARS)) {
                hideScrollBarsCheckBox->setChecked(
                    existingLiquidAppSettings->value(LQD_CFG_KEY_HIDE_SCROLL_BARS).toBool()
                );
            }
        }

        advancedLayout->addWidget(hideScrollBarsCheckBox);
    }
#endif
    {
        if (!isEditingExisting) {
            createIconCheckBox = new QCheckBox(tr("Create desktop icon"));
            createIconCheckBox->setCursor(Qt::PointingHandCursor);
            // Checked by default (when creating new Liquid app)
            createIconCheckBox->setChecked(true);
            advancedLayout->addWidget(createIconCheckBox);
        }
    }
    {
        customBackgroundButton = new QPushButton(tr("Set custom background color"));
        customBackgroundButton->setCursor(Qt::PointingHandCursor);

        if (isEditingExisting) {
            if (existingLiquidAppSettings->contains(LQD_CFG_KEY_BACKGROUND_COLOR)) {
                backgroundColorName = existingLiquidAppSettings->value(LQD_CFG_KEY_BACKGROUND_COLOR).toString();
                backgroundColorName = QColor(backgroundColorName).name();

                customBackgroundButton->setText(tr("Custom background color: %1").arg(backgroundColorName));
            }
        }

        // TODO: make it possible to remove background color setting
        advancedLayout->addWidget(customBackgroundButton);
    }
    {
        additionalDomainsListView = new QListView(this);
        additionalDomainsModel = new QStandardItemModel(this);

        // Assign model
        additionalDomainsListView->setModel(additionalDomainsModel);

        // Fill model items
        if (isEditingExisting) {
            if (existingLiquidAppSettings->contains(LQD_CFG_KEY_ADDITIONAL_DOMAINS)) {
                const QStringList additionalDomainsList = existingLiquidAppSettings->value(LQD_CFG_KEY_ADDITIONAL_DOMAINS).toString().split(" ");

                for (int i = 0; i < additionalDomainsList.size(); i++) {
                    QStandardItem* item = new QStandardItem(additionalDomainsList[i]);
                    additionalDomainsModel->appendRow(item);
                }
            }
        }

        // Append empty row
        additionalDomainsModel->appendRow(new QStandardItem());

        connect(additionalDomainsModel, &QStandardItemModel::itemChanged, [=](QStandardItem* item){
            const int itemIndex = item->row();
            const bool isLastItem = itemIndex == additionalDomainsModel->rowCount() - 1;
            static const QRegExp allowedCharacters = QRegExp("[^a-z0-9\\.:\\-]");

            // Format domain name
            item->setText(item->text().toLower().remove(allowedCharacters));

            if (item->text().size() == 0) {
                // Automatically remove empty rows from the list
                if (!isLastItem) {
                    additionalDomainsModel->removeRows(itemIndex, 1);
                }
            } else {
                if (isLastItem) {
                    // Append empty row
                    additionalDomainsModel->appendRow(new QStandardItem());
                }
            }
        });

        advancedLayout->addWidget(additionalDomainsListView);
    }
    {
        additionalCssTextArea = new QPlainTextEdit();
        additionalCssTextArea->setPlaceholderText(tr("Additional CSS"));

        if (isEditingExisting) {
            additionalCssTextArea->setPlainText(existingLiquidAppSettings->value(LQD_CFG_KEY_ADDITIONAL_CSS).toString());
        }

        advancedLayout->addWidget(additionalCssTextArea);
    }
    {
        additionalJsTextArea = new QPlainTextEdit();
        additionalJsTextArea->setPlaceholderText(tr("Additonal JavaScript code"));

        if (isEditingExisting) {
            additionalJsTextArea->setPlainText(existingLiquidAppSettings->value(LQD_CFG_KEY_ADDITIONAL_JS).toString());
        }

        advancedLayout->addWidget(additionalJsTextArea);
    }
    {
        userAgentInput = new QLineEdit;
        userAgentInput->setPlaceholderText(tr("Custom user-agent string"));

        if (isEditingExisting) {
            userAgentInput->setText(existingLiquidAppSettings->value(LQD_CFG_KEY_USER_AGENT).toString());
        }

        advancedLayout->addWidget(userAgentInput);
    }
    {
        notesArea = new QPlainTextEdit();
        notesArea->setPlaceholderText(tr("Notes"));

        if (isEditingExisting) {
            notesArea->setPlainText(existingLiquidAppSettings->value(LQD_CFG_KEY_NOTES).toString());
        }

        advancedLayout->addWidget(notesArea);
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

LiquidAppCreateEditDialog::~LiquidAppCreateEditDialog(void)
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
                                               QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                               appName,
                                               nullptr);

    // TODO: check if the given Liquid app name is already in use

    // Starting URL
    {
        QUrl url(QUrl::fromUserInput(addressInput->text()));
        tempAppSettings->setValue(LQD_CFG_KEY_URL, url.toString());
    }

    // Enable JS
    {
        tempAppSettings->setValue(LQD_CFG_KEY_ENABLE_JS, enableJavaScriptCheckBox->isChecked());
    }

    // Allow cookies
    {
        tempAppSettings->setValue(LQD_CFG_KEY_ALLOW_COOKIES, allowCookiesCheckBox->isChecked());
    }

    // Allow third-party cookies
    {
        tempAppSettings->setValue(LQD_CFG_KEY_ALLOW_3RD_PARTY_COOKIES, allowThirdPartyCookiesCheckBox->isChecked());
    }

    // Hide scroll bars
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    {
        tempAppSettings->setValue(LQD_CFG_KEY_HIDE_SCROLL_BARS, hideScrollBarsCheckBox->isChecked());
    }
#endif

    // Custom window title
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(LQD_CFG_KEY_TITLE) && titleInput->text().size() == 0) {
                 tempAppSettings->remove(LQD_CFG_KEY_TITLE);
            } else {
                if (tempAppSettings->value(LQD_CFG_KEY_TITLE).toString().size() > 0
                    || titleInput->text().size() > 0
                ) {
                    tempAppSettings->setValue(LQD_CFG_KEY_TITLE, titleInput->text());
                }
            }
        } else {
            if (titleInput->text() != "") {
                tempAppSettings->setValue(LQD_CFG_KEY_TITLE, titleInput->text());
            }
        }
    }

    // Custom CSS
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(LQD_CFG_KEY_ADDITIONAL_CSS) && additionalCssTextArea->toPlainText().size() == 0) {
                 tempAppSettings->remove(LQD_CFG_KEY_ADDITIONAL_CSS);
            } else {
                if (tempAppSettings->value(LQD_CFG_KEY_ADDITIONAL_CSS).toString().size() > 0
                    || additionalCssTextArea->toPlainText().size() > 0
                ) {
                    tempAppSettings->setValue(LQD_CFG_KEY_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
                }
            }
        } else {
            if (additionalCssTextArea->toPlainText() != "") {
                tempAppSettings->setValue(LQD_CFG_KEY_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
            }
        }
    }

    // Custom JS
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(LQD_CFG_KEY_ADDITIONAL_JS) && additionalJsTextArea->toPlainText().size() == 0) {
                 tempAppSettings->remove(LQD_CFG_KEY_ADDITIONAL_JS);
            } else {
                if (tempAppSettings->value(LQD_CFG_KEY_ADDITIONAL_JS).toString().size() > 0
                    || additionalJsTextArea->toPlainText().size() > 0
                ) {
                    tempAppSettings->setValue(LQD_CFG_KEY_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
                }
            }
        } else {
            if (additionalJsTextArea->toPlainText() != "") {
                tempAppSettings->setValue(LQD_CFG_KEY_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
            }
        }
    }

    // Custom user-agent
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(LQD_CFG_KEY_USER_AGENT) && userAgentInput->text().size() == 0) {
                 tempAppSettings->remove(LQD_CFG_KEY_USER_AGENT);
            } else {
                if (tempAppSettings->value(LQD_CFG_KEY_USER_AGENT).toString().size() > 0
                    || userAgentInput->text().size() > 0
                ) {
                    tempAppSettings->setValue(LQD_CFG_KEY_USER_AGENT, userAgentInput->text());
                }
            }
        } else {
            if (userAgentInput->text() != "") {
                tempAppSettings->setValue(LQD_CFG_KEY_USER_AGENT, userAgentInput->text());
            }
        }
    }

    // Notes
    {
        if (isEditingExisting) {
            if (tempAppSettings->contains(LQD_CFG_KEY_NOTES) && notesArea->toPlainText().size() == 0) {
                 tempAppSettings->remove(LQD_CFG_KEY_NOTES);
            } else {
                if (tempAppSettings->value(LQD_CFG_KEY_NOTES).toString().size() > 0
                    || notesArea->toPlainText().size() > 0
                ) {
                    tempAppSettings->setValue(LQD_CFG_KEY_NOTES, notesArea->toPlainText());
                }
            }
        } else {
            if (notesArea->toPlainText() != "") {
                tempAppSettings->setValue(LQD_CFG_KEY_NOTES, notesArea->toPlainText());
            }
        }
    }

    // Create desktop icon
    {
        // TODO: make it possible to remove and create desktop icons for existing Liquid apps
        if (!isEditingExisting) {
            if (createIconCheckBox->isChecked()) {
                QUrl url(QUrl::fromUserInput(addressInput->text()));
                ((MainWindow*)parent())->createDesktopFile(appName, url);
            }
        }
    }

    // Custom background color
    {
        if (backgroundColorName.size() > 0) {
            tempAppSettings->setValue(LQD_CFG_KEY_BACKGROUND_COLOR, backgroundColorName);
        }
    }

    // Additional domains
    {
        if (isEditingExisting && tempAppSettings->contains(LQD_CFG_KEY_ADDITIONAL_DOMAINS) && additionalDomainsModel->rowCount() == 1) {
            tempAppSettings->remove(LQD_CFG_KEY_ADDITIONAL_DOMAINS);
        } else {
            QString additionalDomains;

            for (int i = 0, ilen = additionalDomainsModel->rowCount() - 1; i < ilen; i++) {
                if (i > 0) {
                    additionalDomains += " ";
                }

                additionalDomains += additionalDomainsModel->data(additionalDomainsModel->index(i, 0)).toString();
            }

            tempAppSettings->setValue(LQD_CFG_KEY_ADDITIONAL_DOMAINS, additionalDomains);
        }
    }

    tempAppSettings->sync();

    accept();
}

QString LiquidAppCreateEditDialog::getName(void)
{
    return nameInput->text();
}

void LiquidAppCreateEditDialog::bindShortcuts(void)
{
    // Connect the exit keyboard shortcut
    quitAction.setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(&quitAction);
    connect(&quitAction, SIGNAL(triggered()), this, SLOT(close()));
}
