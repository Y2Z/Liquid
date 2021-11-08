#include <QDir>

#include "globals.h"

#include "liquidappcreateeditdialog.hpp"
#include "mainwindow.hpp"

LiquidAppCreateEditDialog::LiquidAppCreateEditDialog(QWidget* parent, QString liquidAppName) : QDialog(parent)
{
    setWindowFlags(Qt::Window);

    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

    // Attempt to load liquid app's config file
    QSettings* existingLiquidAppConfig = new QSettings(QSettings::IniFormat,
                                                         QSettings::UserScope,
                                                         QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                                         liquidAppName,
                                                         nullptr);

    // Check to see if Liquid app by this name already has config file
    if (liquidAppName.size() > 0) {
        editingExisting = existingLiquidAppConfig->contains(LQD_CFG_KEY_URL);
    } else {
        delete existingLiquidAppConfig;
    }

    if (editingExisting) {
        setWindowTitle(tr("Editing existing Liquid app “%1”").arg(liquidAppName));
    } else {
        setWindowTitle(tr("Creating new Liquid app"));
    }

    backgroundColor = QColor(Qt::black);

    QVBoxLayout* mainLayout = new QVBoxLayout;
    mainLayout->setSpacing(4);
    mainLayout->setMargin(4);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    QGridLayout* basicLayout = new QGridLayout;


    QWidget* advancedWidget = new QWidget;
    QVBoxLayout* advancedLayout = new QVBoxLayout;
    advancedLayout->setMargin(0);
    advancedWidget->setLayout(advancedLayout);

    QTabWidget* tabWidget = new QTabWidget;

    mainLayout->addLayout(basicLayout);

    // Name input
    {
        QLabel* nameInputLabel = new QLabel(tr("Name:"));
        nameInput = new QLineEdit;
        nameInput->setMinimumSize(480, 0);
        nameInput->setPlaceholderText("my-liquid-app-name");
        nameInput->setText(liquidAppName);

        if (editingExisting) {
            // TODO: make it possible to edit names for existing Liquid apps
            nameInput->setReadOnly(true);
        }

        basicLayout->addWidget(nameInputLabel, 0, 0);
        basicLayout->addWidget(nameInput, 0, 1);
    }

    // "URL" input
    {
        QLabel* addressInputLabel = new QLabel(tr("URL:"));
        addressInput = new QLineEdit;
        addressInput->setPlaceholderText("https://example.com");

        if (editingExisting) {
            addressInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_URL).toString());
        }

        basicLayout->addWidget(addressInputLabel, 1, 0);
        basicLayout->addWidget(addressInput, 1, 1);
    }

    // "Create desktop icon" checkbox
    {
        if (!editingExisting) {
            createIconCheckBox = new QCheckBox(tr("Create desktop icon"));
            createIconCheckBox->setCursor(Qt::PointingHandCursor);
            basicLayout->addWidget(createIconCheckBox, 2, 1);
        }
    }

    // "Run after creation" checkbox
    {
        if (!editingExisting) {
            // TODO
        }
    }

    // Horizontal buttons ("Advanced", "Cancel", "Create"/"Save")
    {
        QHBoxLayout* buttonsLayout = new QHBoxLayout;
        buttonsLayout->setSpacing(4);
        buttonsLayout->setMargin(0);

        {
            advancedButton = new QPushButton(tr("Advanced"));
            advancedButton->setCursor(Qt::PointingHandCursor);
            advancedButton->setCheckable(true);
            buttonsLayout->addWidget(advancedButton);

            connect(advancedButton, SIGNAL(toggled(bool)), advancedWidget, SLOT(setVisible(bool)));
        }

        {
            cancelButton = new QPushButton(tr("Cancel"));
            cancelButton->setCursor(Qt::PointingHandCursor);
            buttonsLayout->addWidget(cancelButton);

            QObject::connect(cancelButton, &QPushButton::clicked, [=]() {
                close();
            });
        }

        {
            saveButton = new QPushButton(tr((editingExisting) ? "Save" : "Create"));
            saveButton->setCursor(Qt::PointingHandCursor);
            saveButton->setDefault(true);
            buttonsLayout->addWidget(saveButton);

            QObject::connect(saveButton, &QPushButton::clicked, [=]() {
                save();
            });
        }

        mainLayout->addLayout(buttonsLayout);
    }

    advancedLayout->addWidget(tabWidget);

    /////////////////
    // General tab //
    /////////////////

    {
        generalTabWidget = new QWidget;
        generalTabWidgetLayout = new QVBoxLayout;
        generalTabWidget->setLayout(generalTabWidgetLayout);
        tabWidget->addTab(generalTabWidget, tr("General"));

        // Title text input
        {
            titleInput = new QLineEdit;
            titleInput->setPlaceholderText(tr("Title"));

            if (editingExisting) {
                titleInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_TITLE).toString());
            }

            generalTabWidgetLayout->addWidget(titleInput);
        }

        // Additional domains list view
        {
            generalTabWidgetLayout->addWidget(separator());

            additionalDomainsListLabel = new QLabel(tr("Additonal domains:"));
            additionalDomainsListView = new QListView(this);
            additionalDomainsModel = new QStandardItemModel(this);

            // Assign model
            additionalDomainsListView->setModel(additionalDomainsModel);

            // Fill model items
            if (editingExisting) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_ADDITIONAL_DOMAINS)) {
                    const QStringList additionalDomainsList = existingLiquidAppConfig->value(LQD_CFG_KEY_ADDITIONAL_DOMAINS).toString().split(" ");

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

            generalTabWidgetLayout->addWidget(additionalDomainsListLabel);
            generalTabWidgetLayout->addWidget(additionalDomainsListView);
        }

        // Custom user-agent text input
        {
            generalTabWidgetLayout->addWidget(separator());

            userAgentInput = new QLineEdit;
            userAgentInput->setPlaceholderText(tr("Custom user-agent"));

            if (editingExisting) {
                userAgentInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_USER_AGENT).toString());
            }

            generalTabWidgetLayout->addWidget(userAgentInput);
        }

        // Notes text area
        {
            generalTabWidgetLayout->addWidget(separator());

            notesArea = new QPlainTextEdit();
            notesArea->setPlaceholderText(tr("Notes"));

            if (editingExisting) {
                notesArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_NOTES).toString());
            }

            generalTabWidgetLayout->addWidget(notesArea);
        }
    }

    ////////////////////
    // Appearance tab //
    ////////////////////

    {
        appearanceTabWidget = new QWidget;
        appearanceTabWidgetLayout = new QVBoxLayout;
        appearanceTabWidget->setLayout(appearanceTabWidgetLayout);
        tabWidget->addTab(appearanceTabWidget, tr("Appearance"));

        // Hide scroll bars checkbox
        #if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        {
            hideScrollBarsCheckBox = new QCheckBox(tr("Hide scroll bars"));
            hideScrollBarsCheckBox->setCursor(Qt::PointingHandCursor);

            if (editingExisting) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_HIDE_SCROLL_BARS)) {
                    hideScrollBarsCheckBox->setChecked(
                        existingLiquidAppConfig->value(LQD_CFG_KEY_HIDE_SCROLL_BARS).toBool()
                    );
                }
            }

            appearanceTabWidgetLayout->addWidget(hideScrollBarsCheckBox);
        }
        #endif

        // Custom background color
        {
            appearanceTabWidgetLayout->addWidget(separator());

            // Use custom background checkbox
            {
                useCustomBackgroundCheckBox = new QCheckBox(tr("Use custom background color"));
                useCustomBackgroundCheckBox->setCursor(Qt::PointingHandCursor);

                appearanceTabWidgetLayout->addWidget(useCustomBackgroundCheckBox);
            }

            // Custom background color button
            {
                QHBoxLayout* customBackgroundColorButtonLayout = new QHBoxLayout;

                QLabel* customBackgroundColorButtonLabel = new QLabel(tr("Custom background color:"));

                customBackgroundColorButton = new QPushButton;
                customBackgroundColorButton->setCursor(Qt::PointingHandCursor);
                customBackgroundColorButton->setText("█");
                customBackgroundColorButton->setEnabled(false);
                customBackgroundColorButton->setFixedSize(customBackgroundColorButton->width(), 24);
                customBackgroundColorButton->setIconSize(QSize(customBackgroundColorButton->width(), 24));

                if (editingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_CUSTOM_BG_COLOR)) {
                    backgroundColor = QColor(QRgba64::fromRgba64(existingLiquidAppConfig->value(LQD_CFG_KEY_CUSTOM_BG_COLOR).toString().toULongLong(Q_NULLPTR, 16)));
                    customBackgroundColorButton->setStyleSheet(QString("background-image: url(:/images/checkers.svg); border: 0; border-radius: 4px; padding: 0; color: %1; font-size: 9999px;").arg(colorToRgba(&backgroundColor)));
                } else {
                    const QColor defaultColor = QColor(LQD_DEFAULT_BG_COLOR);
                    customBackgroundColorButton->setStyleSheet(QString("background-image: url(:/images/checkers.svg); border: 0; border-radius: 4px; padding: 0; color: %1; font-size: 9999px;").arg(colorToRgba(&defaultColor)));
                }

                customBackgroundColorButtonLayout->addWidget(customBackgroundColorButtonLabel);
                customBackgroundColorButtonLayout->addWidget(customBackgroundColorButton);
                appearanceTabWidgetLayout->addLayout(customBackgroundColorButtonLayout);
            }

            QObject::connect(customBackgroundColorButton, &QPushButton::clicked, [=]() {
                const QColorDialog::ColorDialogOptions options = QFlag(QColorDialog::ShowAlphaChannel);
                QColor color = QColorDialog::getColor(backgroundColor, this, tr("Pick custom background color"), options);

                if (color.isValid()) {
                    backgroundColor = color;
                    customBackgroundColorButton->setStyleSheet(QString("background-image: url(:/images/checkers.svg); border: 0; border-radius: 4px; padding: 0; color: %1; font-size: 9999px;").arg(colorToRgba(&backgroundColor)));
                }
            });

            connect(useCustomBackgroundCheckBox, SIGNAL(toggled(bool)), customBackgroundColorButton, SLOT(setEnabled(bool)));

            if (editingExisting) {
                bool enabledInConfig = existingLiquidAppConfig->value(LQD_CFG_KEY_USE_CUSTOM_BG, false).toBool();
                useCustomBackgroundCheckBox->setChecked(enabledInConfig);
            }
        }

        // Additional CSS text area
        {
            appearanceTabWidgetLayout->addWidget(separator());

            additionalCssTextArea = new QPlainTextEdit();
            additionalCssTextArea->setPlaceholderText(tr("Additional CSS"));

            if (editingExisting) {
                additionalCssTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_ADDITIONAL_CSS).toString());
            }

            appearanceTabWidgetLayout->addWidget(additionalCssTextArea);
        }
    }

    ////////////////////
    // JavaScript tab //
    ////////////////////

    {
        jsTabWidget = new QWidget;
        jsTabWidgetLayout = new QVBoxLayout;
        jsTabWidget->setLayout(jsTabWidgetLayout);
        tabWidget->addTab(jsTabWidget, tr("JavaScript"));

        // Enable JavaScript checkbox
        {
            enableJavaScriptCheckBox = new QCheckBox(tr("Enable JavaScript"));
            enableJavaScriptCheckBox->setCursor(Qt::PointingHandCursor);

            if (editingExisting) {
                bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_ENABLE_JS).toBool();
                enableJavaScriptCheckBox->setChecked(isChecked);
            } else {
                // Checked by default (when creating new Liquid app)
                enableJavaScriptCheckBox->setChecked(true);
            }

            jsTabWidgetLayout->addWidget(enableJavaScriptCheckBox);
        }

        // Additonal JavaScript code text area
        {
            jsTabWidgetLayout->addWidget(separator());

            additionalJsLabel = new QLabel(tr("Additonal JavaScript code:"));
            additionalJsTextArea = new QPlainTextEdit();
            additionalJsTextArea->setPlaceholderText(tr("// This will be executed even when JS is disabled"));

            if (editingExisting) {
                additionalJsTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_ADDITIONAL_JS).toString());
            }

            jsTabWidgetLayout->addWidget(additionalJsLabel);
            jsTabWidgetLayout->addWidget(additionalJsTextArea);
        }
    }

    /////////////////
    // Cookies tab //
    /////////////////

    {
        cookiesTabWidget = new QWidget;
        cookiesTabWidgetLayout = new QVBoxLayout;
        cookiesTabWidget->setLayout(cookiesTabWidgetLayout);
        tabWidget->addTab(cookiesTabWidget, tr("Cookies"));

        // Allow Cookies checkbox
        {
            allowCookiesCheckBox = new QCheckBox(tr("Allow Cookies"));
            allowCookiesCheckBox->setCursor(Qt::PointingHandCursor);

            if (editingExisting) {
                bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_ALLOW_COOKIES).toBool();
                allowCookiesCheckBox->setChecked(isChecked);
            } else {
                // Checked by default (when creating new Liquid app)
                allowCookiesCheckBox->setChecked(true);
            }

            cookiesTabWidgetLayout->addWidget(allowCookiesCheckBox);
        }

        // Allow third-party Cookies checkbox
        {
            allowThirdPartyCookiesCheckBox = new QCheckBox(tr("Allow third-party Cookies"));
            allowThirdPartyCookiesCheckBox->setCursor(Qt::PointingHandCursor);

            if (!allowCookiesCheckBox->isChecked()) {
                allowThirdPartyCookiesCheckBox->setEnabled(false);
            }

            if (editingExisting) {
                bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_ALLOW_3RD_PARTY_COOKIES).toBool();
                allowThirdPartyCookiesCheckBox->setChecked(isChecked);
            }

            cookiesTabWidgetLayout->addWidget(allowThirdPartyCookiesCheckBox);

            connect(allowCookiesCheckBox, SIGNAL(toggled(bool)), allowThirdPartyCookiesCheckBox, SLOT(setEnabled(bool)));
        }

        // Spacer
        {
            QWidget* spacer = new QWidget();
            spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            cookiesTabWidgetLayout->addWidget(spacer);
        }
    }

    /////////////////
    // Network tab //
    /////////////////

    {
        networkTabWidget = new QWidget;
        networkTabWidgetLayout = new QVBoxLayout;
        networkTabWidget->setLayout(networkTabWidgetLayout);
        tabWidget->addTab(networkTabWidget, tr("Network"));

        // Proxy
        {
            proxyModeSystemRadioButton = new QRadioButton(tr("Use global system settings"));
            proxyModeDirectRadioButton = new QRadioButton(tr("Direct internet connection"));
            proxyModeCustomRadioButton = new QRadioButton(tr("Custom proxy configuration"));

            if (editingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_USE_PROXY)) {
                const bool proxyEnabled = existingLiquidAppConfig->value(LQD_CFG_KEY_USE_PROXY, false).toBool();

                if (proxyEnabled) {
                    proxyModeCustomRadioButton->setChecked(true);
                } else {
                    proxyModeDirectRadioButton->setChecked(true);
                }
            } else {
                proxyModeSystemRadioButton->setChecked(true);
            }

            useSocksSelectBox = new QComboBox;
            useSocksSelectBox->addItem(tr("HTTP"), false);
            useSocksSelectBox->addItem(tr("SOCKS"), true);

            if (editingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_PROXY_USE_SOCKS)) {
                const bool useSocks = existingLiquidAppConfig->value(LQD_CFG_KEY_PROXY_USE_SOCKS, false).toBool();

                if (useSocks) {
                    useSocksSelectBox->setCurrentIndex(1);
                }
            }

            proxyHostnameInput = new QLineEdit;
            proxyHostnameInput->setPlaceholderText("0.0.0.0");
            if (editingExisting) {
                proxyHostnameInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_PROXY_HOSTNAME).toString());
            }

            proxyPortInput = new QSpinBox;
            proxyPortInput->setRange(0, 65535);
            if (editingExisting) {
                proxyPortInput->setValue(existingLiquidAppConfig->value(LQD_CFG_KEY_PROXY_PORT).toInt());
            } else {
                proxyPortInput->setValue(8080);
            }

            networkTabWidgetLayout->addWidget(proxyModeSystemRadioButton);
            networkTabWidgetLayout->addWidget(proxyModeDirectRadioButton);
            networkTabWidgetLayout->addWidget(proxyModeCustomRadioButton);

            networkTabWidgetLayout->addWidget(useSocksSelectBox);
            networkTabWidgetLayout->addWidget(proxyHostnameInput);
            networkTabWidgetLayout->addWidget(proxyPortInput);
        }

        // Spacer
        {
            QWidget* spacer = new QWidget();
            spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            networkTabWidgetLayout->addWidget(spacer);
        }
    }

    mainLayout->addWidget(advancedWidget);

    setLayout(mainLayout);

    if (editingExisting) {
        // Force advanced section to be visible in edit mode
        advancedButton->toggle();
    } else {
        advancedWidget->hide();
    }

    // Reveal and bring to front
    {
        show();
        raise();
        activateWindow();
    }

    // Connect keyboard shortcuts
    bindShortcuts();
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
    QSettings* tempLiquidAppConfig = new QSettings(QSettings::IniFormat,
                                               QSettings::UserScope,
                                               QString(PROG_NAME "%1" LQD_APPS_DIR_NAME).arg(QDir::separator()),
                                               appName,
                                               nullptr);

    // TODO: check if the given Liquid app name is already in use

    // URL
    {
        QUrl url(QUrl::fromUserInput(addressInput->text()));
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_URL, url.toString());
    }

    // Enable JS
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_ENABLE_JS, enableJavaScriptCheckBox->isChecked());
    }

    // Allow cookies
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_ALLOW_COOKIES, allowCookiesCheckBox->isChecked());
    }

    // Allow third-party cookies
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_ALLOW_3RD_PARTY_COOKIES, allowThirdPartyCookiesCheckBox->isChecked());
    }

    // Hide scroll bars
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_HIDE_SCROLL_BARS, hideScrollBarsCheckBox->isChecked());
    }
#endif

    // Custom window title
    {
        if (editingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_TITLE) && titleInput->text().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_TITLE);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_TITLE).toString().size() > 0
                    || titleInput->text().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_TITLE, titleInput->text());
                }
            }
        } else {
            if (titleInput->text() != "") {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_TITLE, titleInput->text());
            }
        }
    }

    // Custom CSS
    {
        if (editingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_ADDITIONAL_CSS) && additionalCssTextArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_ADDITIONAL_CSS);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_ADDITIONAL_CSS).toString().size() > 0
                    || additionalCssTextArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
                }
            }
        } else {
            if (additionalCssTextArea->toPlainText() != "") {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
            }
        }
    }

    // Custom JS
    {
        if (editingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_ADDITIONAL_JS) && additionalJsTextArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_ADDITIONAL_JS);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_ADDITIONAL_JS).toString().size() > 0
                    || additionalJsTextArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
                }
            }
        } else {
            if (additionalJsTextArea->toPlainText() != "") {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
            }
        }
    }

    // Custom user-agent
    {
        if (editingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_USER_AGENT) && userAgentInput->text().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_USER_AGENT);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_USER_AGENT).toString().size() > 0
                    || userAgentInput->text().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_USER_AGENT, userAgentInput->text());
                }
            }
        } else {
            if (userAgentInput->text() != "") {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_USER_AGENT, userAgentInput->text());
            }
        }
    }

    // Notes
    {
        if (editingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NOTES) && notesArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NOTES);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NOTES).toString().size() > 0
                    || notesArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NOTES, notesArea->toPlainText());
                }
            }
        } else {
            if (notesArea->toPlainText() != "") {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NOTES, notesArea->toPlainText());
            }
        }
    }

    // Create desktop icon
    {
        // TODO: make it possible to remove and (re-)create desktop icons for existing Liquid apps

        if (!editingExisting) {
            if (createIconCheckBox->isChecked()) {
                QUrl url(QUrl::fromUserInput(addressInput->text()));
                ((MainWindow*)parent())->createDesktopFile(appName, url);
            }
        }
    }

    // Custom background color
    {
        // Use custom background checkbox
        {
            if (editingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_USE_CUSTOM_BG) && !useCustomBackgroundCheckBox->isChecked()) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_USE_CUSTOM_BG);
                } else {
                    if (useCustomBackgroundCheckBox->isChecked()) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_USE_CUSTOM_BG, true);
                    }
                }
            } else {
                if (useCustomBackgroundCheckBox->isChecked()) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_USE_CUSTOM_BG, true);
                }
            }
        }

        // Custom background color
        {
            const QString rgba64Hex = QString("%1").arg(backgroundColor.rgba64(), 8, 16, QLatin1Char('0'));
            tempLiquidAppConfig->setValue(LQD_CFG_KEY_CUSTOM_BG_COLOR, rgba64Hex);
        }
    }

    // Additional domains
    {
        if (editingExisting && tempLiquidAppConfig->contains(LQD_CFG_KEY_ADDITIONAL_DOMAINS) && additionalDomainsModel->rowCount() == 1) {
            tempLiquidAppConfig->remove(LQD_CFG_KEY_ADDITIONAL_DOMAINS);
        } else {
            QString additionalDomains;

            for (int i = 0, ilen = additionalDomainsModel->rowCount() - 1; i < ilen; i++) {
                if (i > 0) {
                    additionalDomains += " ";
                }

                additionalDomains += additionalDomainsModel->data(additionalDomainsModel->index(i, 0)).toString();
            }

            tempLiquidAppConfig->setValue(LQD_CFG_KEY_ADDITIONAL_DOMAINS, additionalDomains);
        }
    }

    // Proxy
    {
        // Proxy mode
        {
            if (proxyModeSystemRadioButton->isChecked()) {
                if (editingExisting) {
                    if (tempLiquidAppConfig->contains(LQD_CFG_KEY_USE_PROXY)) {
                         tempLiquidAppConfig->remove(LQD_CFG_KEY_USE_PROXY);
                    }
                }
            } else if (proxyModeDirectRadioButton->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_USE_PROXY, false);
            } else if (proxyModeCustomRadioButton->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_USE_PROXY, true);
            }
        }

        // Proxy type
        {
            // TODO
        }

        // Proxy hostname
        {
            if (editingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_PROXY_HOSTNAME) && proxyHostnameInput->text().size() == 0) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_PROXY_HOSTNAME);
                } else {
                    if (tempLiquidAppConfig->value(LQD_CFG_KEY_PROXY_HOSTNAME).toString().size() > 0
                        || proxyHostnameInput->text().size() > 0
                    ) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_PROXY_HOSTNAME, proxyHostnameInput->text());
                    }
                }
            } else {
                if (proxyHostnameInput->text() != "") {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_PROXY_HOSTNAME, proxyHostnameInput->text());
                }
            }
        }

        // Proxy port number
        {
            tempLiquidAppConfig->setValue(LQD_CFG_KEY_PROXY_PORT, proxyPortInput->value());
        }

        // Proxy authentication
        {
            // TODO
        }

        // Proxy username
        {
            // TODO
        }

        // Proxy password
        {
            // TODO
        }
    }

    tempLiquidAppConfig->sync();

    accept();
}

void LiquidAppCreateEditDialog::bindShortcuts(void)
{
    // Connect keyboard shortcut that closes the dialog
    quitAction = new QAction();
    quitAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(quitAction);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
}

QString LiquidAppCreateEditDialog::colorToRgba(const QColor* color)
{
    return QString("rgba(%1, %2, %3, %4)")
        .arg(color->red())
        .arg(color->green())
        .arg(color->blue())
        .arg(color->alphaF());
}

QString LiquidAppCreateEditDialog::getName(void)
{
    return nameInput->text();
}

QFrame* LiquidAppCreateEditDialog::separator(void)
{
    QFrame* separatorFrame = new QFrame;

    separatorFrame->setFrameShape(QFrame::HLine);
    separatorFrame->setFrameShadow(QFrame::Sunken);

    return separatorFrame;
}
