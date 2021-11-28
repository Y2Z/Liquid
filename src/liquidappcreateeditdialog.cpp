#include <QDir>
#include <QWebEngineProfile>

#include "lqd.h"
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
                                                       Q_NULLPTR);

    // Check to see if Liquid app by this name already has config file
    if (liquidAppName.size() > 0) {
        isEditingExisting = existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_URL);
    } else {
        delete existingLiquidAppConfig;
    }

    if (isEditingExisting) {
        setWindowTitle(tr("Editing existing Liquid App “%1”").arg(liquidAppName));
    } else {
        setWindowTitle(tr("Adding new Liquid App"));
    }

    backgroundColor = new QColor(Qt::black);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(4);
    mainLayout->setMargin(4);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    QGridLayout* basicLayout = new QGridLayout();

    QWidget* advancedWidget = new QWidget(this);
    QVBoxLayout* advancedLayout = new QVBoxLayout();
    advancedLayout->setMargin(0);
    advancedWidget->setLayout(advancedLayout);

    QTabWidget* tabWidget = new QTabWidget;

    mainLayout->addLayout(basicLayout);

    // Name input
    {
        QLabel* nameInputLabel = new QLabel(tr("Name:"), this);
        nameInput = new QLineEdit;
        nameInput->setMinimumSize(480, 0);
        nameInput->setPlaceholderText("my-liquid-app-name");
        nameInput->setText(liquidAppName);

        if (isEditingExisting) {
            // TODO: make it possible to edit names for existing Liquid apps
            nameInput->setReadOnly(true);
        }

        basicLayout->addWidget(nameInputLabel, 0, 0);
        basicLayout->addWidget(nameInput, 0, 1);
    }

    // "URL" input
    {
        QLabel* addressInputLabel = new QLabel(tr("URL:"), this);
        addressInput = new QLineEdit;
        addressInput->setPlaceholderText("https://example.com");

        if (isEditingExisting) {
            addressInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString());
        }

        basicLayout->addWidget(addressInputLabel, 1, 0);
        basicLayout->addWidget(addressInput, 1, 1);
    }

    // Extra checkboxes visible only in "Create" mode
    if (!isEditingExisting) {
        QHBoxLayout* extraCheckboxesLayout = new QHBoxLayout();

        // "Create desktop icon" checkbox
        {
            createIconCheckBox = new QCheckBox(tr("Create desktop icon"), this);
            createIconCheckBox->setCursor(Qt::PointingHandCursor);
            extraCheckboxesLayout->addWidget(createIconCheckBox);
        }

        // "Run after creation" checkbox
        {
            planningToRunCheckBox = new QCheckBox(tr("Run after creation"), this);
            planningToRunCheckBox->setCursor(Qt::PointingHandCursor);
            extraCheckboxesLayout->addWidget(planningToRunCheckBox, 0, Qt::AlignRight);
        }

        basicLayout->addLayout(extraCheckboxesLayout, 2, 1);
    }

    QPushButton* advancedButton;
    QPushButton* cancelButton;
    QPushButton* saveButton;

    // Horizontal buttons ("Advanced", "Cancel", "Create"/"Save")
    {
        QHBoxLayout* buttonsLayout = new QHBoxLayout();
        buttonsLayout->setSpacing(4);
        buttonsLayout->setMargin(0);

        {
            advancedButton = new QPushButton(tr("Advanced"), this);
            advancedButton->setCursor(Qt::PointingHandCursor);
            advancedButton->setCheckable(true);
            buttonsLayout->addWidget(advancedButton);

            connect(advancedButton, SIGNAL(toggled(bool)), advancedWidget, SLOT(setVisible(bool)));
        }

        {
            cancelButton = new QPushButton(tr("Cancel"), this);
            cancelButton->setCursor(Qt::PointingHandCursor);
            buttonsLayout->addWidget(cancelButton);

            QObject::connect(cancelButton, &QPushButton::clicked, [=]() {
                close();
            });
        }

        {
            saveButton = new QPushButton(tr((isEditingExisting) ? "Save" : "Add"), this);
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
        QWidget* generalTabWidget = new QWidget(this);
        QVBoxLayout* generalTabWidgetLayout = new QVBoxLayout();
        generalTabWidget->setLayout(generalTabWidgetLayout);
        tabWidget->addTab(generalTabWidget, tr("General"));

        // Title text input
        {
            QHBoxLayout* titleLayout = new QHBoxLayout();

            // Title label
            {
                QLabel* textLabel = new QLabel(tr("Title:"), this);

                titleLayout->addWidget(textLabel);
            }

            // Title text input
            {
                titleInput = new QLineEdit(this);
                titleInput->setPlaceholderText(tr("Title"));

                if (isEditingExisting) {
                    titleInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_TITLE).toString());
                }

                titleLayout->addWidget(titleInput);
            }

            generalTabWidgetLayout->addLayout(titleLayout);
        }

        // Additional domains list view
        {
            generalTabWidgetLayout->addWidget(separator());

            // Additional domains label
            {
                QLabel* additionalDomainsListLabel = new QLabel(tr("Additional domains:"), this);

                generalTabWidgetLayout->addWidget(additionalDomainsListLabel);
            }

            // Editable list of additional domains
            {
                additionalDomainsListView = new QListView(this);
                additionalDomainsModel = new QStandardItemModel(this);

                // Assign model
                additionalDomainsListView->setModel(additionalDomainsModel);

                // Fill model items
                if (isEditingExisting) {
                    if (existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS)) {
                        const QStringList additionalDomainsList = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS).toString().split(" ");

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
            }

            generalTabWidgetLayout->addWidget(additionalDomainsListView);
        }

        // Custom user-agent text input
        {
            generalTabWidgetLayout->addWidget(separator());

            QHBoxLayout* customUserAgentLayout = new QHBoxLayout();

            // Custom user-agent label
            {
                QLabel* customUserAgentLabel = new QLabel(tr("Custom user-agent string:"), this);

                customUserAgentLayout->addWidget(customUserAgentLabel);
            }

            // Custom user-agent text input
            {
                userAgentInput = new QLineEdit(this);
                // TODO: set placeholder to whatever QWebEngineProfile has by default
                userAgentInput->setPlaceholderText(tr("Browser identifier string"));

                if (isEditingExisting) {
                    userAgentInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_USER_AGENT).toString());
                }

                customUserAgentLayout->addWidget(userAgentInput);
            }

            generalTabWidgetLayout->addLayout(customUserAgentLayout);
        }

        // Notes text area
        {
            generalTabWidgetLayout->addWidget(separator());

            QLabel* notesLabel = new QLabel(tr("Notes:"), this);
            notesTextArea = new QPlainTextEdit(this);
            notesTextArea->setPlaceholderText(tr("Intentionally left blank"));

            if (isEditingExisting) {
                notesTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_NOTES).toString());
            }

            generalTabWidgetLayout->addWidget(notesLabel);
            generalTabWidgetLayout->addWidget(notesTextArea);
        }
    }

    ////////////////////
    // Appearance tab //
    ////////////////////

    {
        QWidget* appearanceTabWidget = new QWidget(this);
        QVBoxLayout* appearanceTabWidgetLayout = new QVBoxLayout();
        appearanceTabWidget->setLayout(appearanceTabWidgetLayout);
        tabWidget->addTab(appearanceTabWidget, tr("Appearance"));

        // Hide scrollbars checkbox
        #if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        {
            hideScrollBarsCheckBox = new QCheckBox(tr("Hide scrollbars"), this);
            hideScrollBarsCheckBox->setCursor(Qt::PointingHandCursor);

            if (isEditingExisting) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS)) {
                    hideScrollBarsCheckBox->setChecked(
                        existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS).toBool()
                    );
                }
            }

            appearanceTabWidgetLayout->addWidget(hideScrollBarsCheckBox);
        }
        #endif

        // Remove window frame
        {
            appearanceTabWidgetLayout->addWidget(separator());

            removeWindowFrameCheckBox = new QCheckBox(tr("Remove window frame"), this);
            removeWindowFrameCheckBox->setCursor(Qt::PointingHandCursor);

            if (isEditingExisting) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME)) {
                    removeWindowFrameCheckBox->setChecked(
                        existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME).toBool()
                    );
                }
            }

            appearanceTabWidgetLayout->addWidget(removeWindowFrameCheckBox);
        }

        // Custom background color
        {
            appearanceTabWidgetLayout->addWidget(separator());

            QHBoxLayout* customBackgroundColorButtonLayout = new QHBoxLayout();

            // Use custom background checkbox
            {
                useCustomBackgroundCheckBox = new QCheckBox(tr("Use custom background color:"), this);
                useCustomBackgroundCheckBox->setCursor(Qt::PointingHandCursor);

                customBackgroundColorButtonLayout->addWidget(useCustomBackgroundCheckBox);
            }

            // Custom background color button
            {
                customBackgroundColorButton = new QPushButton("█");
                customBackgroundColorButton->setCursor(Qt::PointingHandCursor);
                customBackgroundColorButton->setFlat(true);
                customBackgroundColorButton->setFixedSize(customBackgroundColorButton->width(), 24);

                static const QString buttonStyle = QString("background-image: url(:/images/checkers.svg); border-radius: 4px; padding: 0; color: %1; font-size: %2px;");
                // TODO: animate background pattern
                static const int fontSize = customBackgroundColorButton->width() * 0.9;

                if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR)) {
                    backgroundColor = new QColor(QRgba64::fromRgba64(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR).toString().toULongLong(Q_NULLPTR, 16)));
                    customBackgroundColorButton->setStyleSheet(buttonStyle.arg(colorToRgba(backgroundColor)).arg(fontSize));
                } else {
                    static const QColor defaultColor = QColor(LQD_DEFAULT_BG_COLOR);
                    customBackgroundColorButton->setStyleSheet(buttonStyle.arg(colorToRgba(&defaultColor)).arg(fontSize));
                }

                customBackgroundColorButtonLayout->addWidget(customBackgroundColorButton);

                QObject::connect(customBackgroundColorButton, &QPushButton::clicked, [=]() {
                    const QColorDialog::ColorDialogOptions options = QFlag(QColorDialog::ShowAlphaChannel);
                    QColor color = QColorDialog::getColor(*backgroundColor, this, tr("Pick custom background color"), options);

                    if (color.isValid()) {
                        if (!useCustomBackgroundCheckBox->isChecked()) {
                            useCustomBackgroundCheckBox->setChecked(true);
                        }

                        backgroundColor = &color;
                        customBackgroundColorButton->setStyleSheet(buttonStyle.arg(colorToRgba(backgroundColor)).arg(fontSize));
                    }
                });
            }

            appearanceTabWidgetLayout->addLayout(customBackgroundColorButtonLayout);

            if (isEditingExisting) {
                bool enabledInConfig = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, false).toBool();
                useCustomBackgroundCheckBox->setChecked(enabledInConfig);
            }
        }

        // Additional CSS text area
        {
            appearanceTabWidgetLayout->addWidget(separator());

            {
                QLabel* additionalCssLabel = new QLabel(tr("Additional CSS:"), this);
                appearanceTabWidgetLayout->addWidget(additionalCssLabel);
            }
            additionalCssTextArea = new QPlainTextEdit(this);
            additionalCssTextArea->setPlaceholderText(tr("/* put your custom CSS here */"));

            if (isEditingExisting) {
                additionalCssTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString());
            }

            appearanceTabWidgetLayout->addWidget(additionalCssTextArea);
        }
    }

    ////////////////////
    // JavaScript tab //
    ////////////////////

    {
        QWidget* jsTabWidget = new QWidget(this);
        QVBoxLayout* jsTabWidgetLayout = new QVBoxLayout();
        jsTabWidget->setLayout(jsTabWidgetLayout);
        tabWidget->addTab(jsTabWidget, tr("JavaScript"));

        // Enable JavaScript checkbox
        {
            enableJavaScriptCheckBox = new QCheckBox(tr("Enable JavaScript"), this);
            enableJavaScriptCheckBox->setCursor(Qt::PointingHandCursor);

            if (isEditingExisting) {
                bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ENABLE_JS).toBool();
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

            additionalJsLabel = new QLabel(tr("Additonal JavaScript code:"), this);
            additionalJsTextArea = new QPlainTextEdit(this);
            additionalJsTextArea->setPlaceholderText(tr("// This code will run even when JS is disabled"));

            if (isEditingExisting) {
                additionalJsTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_JS).toString());
            }

            jsTabWidgetLayout->addWidget(additionalJsLabel);
            jsTabWidgetLayout->addWidget(additionalJsTextArea);
        }
    }

    /////////////////
    // Cookies tab //
    /////////////////

    {
        QWidget* cookiesTabWidget = new QWidget(this);
        QVBoxLayout* cookiesTabWidgetLayout = new QVBoxLayout();
        cookiesTabWidget->setLayout(cookiesTabWidgetLayout);
        tabWidget->addTab(cookiesTabWidget, tr("Cookies"));

        // Allow cookies & allow third-party cookies checkboxes
        {
            // Allow cookies checkbox
            {
                allowCookiesCheckBox = new QCheckBox(tr("Allow cookies"), this);
                allowCookiesCheckBox->setCursor(Qt::PointingHandCursor);

                if (isEditingExisting) {
                    bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_COOKIES).toBool();
                    allowCookiesCheckBox->setChecked(isChecked);
                } else {
                    // Checked by default (when creating new Liquid App)
                    allowCookiesCheckBox->setChecked(true);
                }

                cookiesTabWidgetLayout->addWidget(allowCookiesCheckBox);
            }

            // Allow third-party cookies checkbox
            {
                QHBoxLayout* allowThirdPartyCookiesLayout = new QHBoxLayout();
                allowThirdPartyCookiesLayout->setContentsMargins(LQD_UI_MARGIN, 0, 0, 0);

                allowThirdPartyCookiesCheckBox = new QCheckBox(tr("Allow third-party cookies"), this);
                allowThirdPartyCookiesCheckBox->setCursor(Qt::PointingHandCursor);

                if (isEditingExisting) {
                    bool isChecked = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES).toBool();
                    allowThirdPartyCookiesCheckBox->setChecked(isChecked);
                }

                allowThirdPartyCookiesLayout->addWidget(allowThirdPartyCookiesCheckBox);

                connect(allowCookiesCheckBox, &QCheckBox::toggled, [&](const bool isOn){
                    if (!isOn) {
                        allowThirdPartyCookiesCheckBox->setChecked(false);
                    }
                });
                connect(allowThirdPartyCookiesCheckBox, &QCheckBox::toggled, [&](const bool isOn){
                    if (isOn) {
                        allowCookiesCheckBox->setChecked(isOn);
                    }
                });

                cookiesTabWidgetLayout->addLayout(allowThirdPartyCookiesLayout);
            }
        }

        // TODO: add QListView to add,edit, and remove cookies

        // Spacer
        {
            QWidget* spacer = new QWidget(this);
            spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            cookiesTabWidgetLayout->addWidget(spacer);
        }
    }

    /////////////////
    // Network tab //
    /////////////////

    {
        QWidget* networkTabWidget = new QWidget(this);
        QVBoxLayout* networkTabWidgetLayout = new QVBoxLayout();
        networkTabWidget->setLayout(networkTabWidgetLayout);
        tabWidget->addTab(networkTabWidget, tr("Network"));

        // Proxy
        {
            // Option 1: Use global system proxy settings (default)
            {
                proxyModeSystemRadioButton = new QRadioButton(tr("Use global system settings"), this);
                proxyModeSystemRadioButton->setCursor(Qt::PointingHandCursor);

                networkTabWidgetLayout->addWidget(proxyModeSystemRadioButton);
            }

            // Option 2: Use direct internet connection
            {
                proxyModeDirectRadioButton = new QRadioButton(tr("Direct internet connection"), this);
                proxyModeDirectRadioButton->setCursor(Qt::PointingHandCursor);

                networkTabWidgetLayout->addWidget(proxyModeDirectRadioButton);
            }

            // Option 3: Use custom proxy configuration
            {
                QHBoxLayout* customProxyModeLayout = new QHBoxLayout();

                // Radio box
                proxyModeCustomRadioButton = new QRadioButton(tr("Custom proxy configuration:"), this);
                proxyModeCustomRadioButton->setCursor(Qt::PointingHandCursor);

                customProxyModeLayout->addWidget(proxyModeCustomRadioButton, 0, Qt::AlignTop);

                // Custom proxy configuration
                {
                    QVBoxLayout* proxyConfigLayout = new QVBoxLayout();

                    // Row 1 (type, host, port)
                    {
                        QHBoxLayout* proxyTypeHostPortLayout = new QHBoxLayout();

                        // Proxy type
                        {
                            useSocksSelectBox = new QComboBox(this);
                            useSocksSelectBox->addItem(tr("HTTP"), false);
                            useSocksSelectBox->addItem(tr("SOCKS"), true);

                            if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS)) {
                                const bool useSocks = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS, false).toBool();

                                if (useSocks) {
                                    useSocksSelectBox->setCurrentIndex(1);
                                }
                            }

                            proxyTypeHostPortLayout->addWidget(useSocksSelectBox);
                        }

                        // Proxy host
                        {
                            proxyHostInput = new QLineEdit(this);
                            proxyHostInput->setPlaceholderText(LQD_DEFAULT_PROXY_HOST);

                            if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_HOST)) {
                                proxyHostInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_HOST).toString());
                            }

                            proxyTypeHostPortLayout->addWidget(proxyHostInput);
                        }

                        // Proxy port
                        {
                            proxyPortInput = new QSpinBox(this);
                            proxyPortInput->setRange(0, 65535);
                            proxyPortInput->setValue(LQD_DEFAULT_PROXY_PORT);

                            if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_PORT)) {
                                proxyPortInput->setValue(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_PORT).toInt());
                            }

                            proxyTypeHostPortLayout->addWidget(proxyPortInput);
                        }

                        proxyConfigLayout->addLayout(proxyTypeHostPortLayout);
                    }

                    // Row 2 (use authentication checkbox, username, password)
                    {
                        QHBoxLayout* proxyCredentialsLayout = new QHBoxLayout();

                        // Use credentials
                        {
                            proxyUseAuthCheckBox = new QCheckBox(tr("Authenticate using credentials:"), this);

                            proxyCredentialsLayout->addWidget(proxyUseAuthCheckBox);
                        }

                        // Username
                        {
                            proxyUsernameInput = new QLineEdit(this);
                            proxyUsernameInput->setPlaceholderText(tr("Username"));

                            if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME)) {
                                proxyUsernameInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString());
                            }

                            proxyCredentialsLayout->addWidget(proxyUsernameInput);
                        }

                        // Password
                        {
                            proxyPasswordInput = new QLineEdit(this);
                            proxyPasswordInput->setPlaceholderText(tr("Password"));
                            proxyPasswordInput->setEchoMode(QLineEdit::Password);

                            if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME)) {
                                proxyPasswordInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString());
                            }

                            proxyCredentialsLayout->addWidget(proxyPasswordInput);
                        }

                        proxyConfigLayout->addLayout(proxyCredentialsLayout);
                    }

                    if (isEditingExisting && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_USE_PROXY)) {
                        const bool proxyEnabled = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_USE_PROXY, false).toBool();

                        if (proxyEnabled) {
                            proxyModeCustomRadioButton->setChecked(true);
                        } else {
                            proxyModeDirectRadioButton->setChecked(true);
                        }
                    } else {
                        proxyModeSystemRadioButton->setChecked(true);
                    }

                    if (isEditingExisting && existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USE_AUTH, false).toBool()) {
                        proxyUseAuthCheckBox->setChecked(true);
                    }

                    customProxyModeLayout->addLayout(proxyConfigLayout);
                }

                networkTabWidgetLayout->addLayout(customProxyModeLayout);
            }
        }

        // Spacer
        {
            QWidget* spacer = new QWidget(this);
            spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
            networkTabWidgetLayout->addWidget(spacer);
        }
    }

    mainLayout->addWidget(advancedWidget);

    setLayout(mainLayout);

    if (isEditingExisting) {
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
                                                   QString(PROG_NAME) + QDir::separator() + LQD_APPS_DIR_NAME,
                                                   appName,
                                                   Q_NULLPTR);

    // TODO: check if the given Liquid app name is already in use

    // URL
    {
        QUrl url(QUrl::fromUserInput(addressInput->text()));
        // TODO: if was given only hostname and prepending http:// didn't help, prepend https://
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_URL, url.toString());
    }

    // Enable JS
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ENABLE_JS, enableJavaScriptCheckBox->isChecked());
    }

    // Allow cookies
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ALLOW_COOKIES, allowCookiesCheckBox->isChecked());
    }

    // Allow third-party cookies
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ALLOW_3RD_PARTY_COOKIES, allowThirdPartyCookiesCheckBox->isChecked());
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    // Hide scrollbars
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS) && !hideScrollBarsCheckBox->isChecked()) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS);
            } else {
                if (hideScrollBarsCheckBox->isChecked()) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS, true);
                }
            }
        } else {
            if (hideScrollBarsCheckBox->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS, true);
            }
        }
    }
#endif

    // Remove window frame
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME) && !removeWindowFrameCheckBox->isChecked()) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME);
            } else {
                if (removeWindowFrameCheckBox->isChecked()) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME, true);
                }
            }
        } else {
            if (removeWindowFrameCheckBox->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME, true);
            }
        }
    }

    // Custom window title
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_TITLE) && titleInput->text().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_TITLE);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_TITLE).toString().size() > 0
                    || titleInput->text().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_TITLE, titleInput->text());
                }
            }
        } else {
            if (titleInput->text().size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_TITLE, titleInput->text());
            }
        }
    }

    // Custom CSS
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_CSS) && additionalCssTextArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_ADDITIONAL_CSS);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString().size() > 0
                    || additionalCssTextArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
                }
            }
        } else {
            if (additionalCssTextArea->toPlainText().size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ADDITIONAL_CSS, additionalCssTextArea->toPlainText());
            }
        }
    }

    // Custom JS
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_JS) && additionalJsTextArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_ADDITIONAL_JS);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_JS).toString().size() > 0
                    || additionalJsTextArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
                }
            }
        } else {
            if (additionalJsTextArea->toPlainText().size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ADDITIONAL_JS, additionalJsTextArea->toPlainText());
            }
        }
    }

    // Custom user-agent
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_USER_AGENT) && userAgentInput->text().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_USER_AGENT);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_USER_AGENT).toString().size() > 0
                    || userAgentInput->text().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USER_AGENT, userAgentInput->text());
                }
            }
        } else {
            if (userAgentInput->text().size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USER_AGENT, userAgentInput->text());
            }
        }
    }

    // Notes
    {
        if (isEditingExisting) {
            if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_NOTES) && notesTextArea->toPlainText().size() == 0) {
                 tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_NOTES);
            } else {
                if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_NOTES).toString().size() > 0
                    || notesTextArea->toPlainText().size() > 0
                ) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_NOTES, notesTextArea->toPlainText());
                }
            }
        } else {
            if (notesTextArea->toPlainText().size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_NOTES, notesTextArea->toPlainText());
            }
        }
    }

    // Create desktop icon
    {
        // TODO: make it possible to remove and (re-)create desktop icons for existing Liquid apps

        if (!isEditingExisting) {
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
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_USE_CUSTOM_BG) && !useCustomBackgroundCheckBox->isChecked()) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_USE_CUSTOM_BG);
                } else {
                    if (useCustomBackgroundCheckBox->isChecked()) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, true);
                    }
                }
            } else {
                if (useCustomBackgroundCheckBox->isChecked()) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, true);
                }
            }
        }

        // Custom background color
        {
            const QString rgba64Hex = QString("%1").arg(backgroundColor->rgba64(), 8, 16, QLatin1Char('0'));
            tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR, rgba64Hex);
        }
    }

    // Additional domains
    {
        if (isEditingExisting && tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS) && additionalDomainsModel->rowCount() == 1) {
            tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS);
        } else {
            QString additionalDomains;

            for (int i = 0, ilen = additionalDomainsModel->rowCount() - 1; i < ilen; i++) {
                if (i > 0) {
                    additionalDomains += " ";
                }

                additionalDomains += additionalDomainsModel->data(additionalDomainsModel->index(i, 0)).toString();
            }

            if (additionalDomains.size() > 0) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS, additionalDomains);
            }
        }
    }

    // Proxy
    {
        // Proxy mode
        {
            if (proxyModeSystemRadioButton->isChecked()) {
                if (isEditingExisting) {
                    if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_USE_PROXY)) {
                         tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_USE_PROXY);
                    }
                }
            } else if (proxyModeDirectRadioButton->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USE_PROXY, false);
            } else if (proxyModeCustomRadioButton->isChecked()) {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_USE_PROXY, true);
            }
        }

        // Proxy type
        {
            if (useSocksSelectBox->currentIndex() == 0) {
                if (isEditingExisting) {
                    if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS)) {
                         tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS);
                    }
                }
            } else {
                tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS, true);
            }
        }

        // Proxy host
        {
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_HOST) && proxyHostInput->text().size() == 0) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_HOST);
                } else {
                    if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_HOST).toString().size() > 0
                        || proxyHostInput->text().size() > 0
                    ) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_HOST, proxyHostInput->text());
                    }
                }
            } else {
                if (proxyHostInput->text().size() > 0) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_HOST, proxyHostInput->text());
                }
            }
        }

        // Proxy port number
        {
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_PORT) && proxyPortInput->value() == LQD_DEFAULT_PROXY_PORT) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_PORT);
                } else {
                    if (proxyPortInput->value() != LQD_DEFAULT_PROXY_PORT) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_PORT, proxyPortInput->value());
                    }
                }
            } else {
                if (proxyPortInput->value() != LQD_DEFAULT_PROXY_PORT) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_PORT, proxyPortInput->value());
                }
            }
        }

        // Proxy authentication
        {
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USE_AUTH) && !proxyUseAuthCheckBox->isChecked()) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_USE_AUTH);
                } else {
                    if (proxyUseAuthCheckBox->isChecked()) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USE_AUTH, true);
                    }
                }
            } else {
                if (proxyUseAuthCheckBox->isChecked()) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USE_AUTH, true);
                }
            }
        }

        // Proxy username
        {
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME) && proxyUsernameInput->text().size() == 0) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_USER_NAME);
                } else {
                    if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString().size() > 0
                        || proxyUsernameInput->text().size() > 0
                    ) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USER_NAME, proxyUsernameInput->text());
                    }
                }
            } else {
                if (proxyUsernameInput->text().size() > 0) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USER_NAME, proxyUsernameInput->text());
                }
            }
        }

        // Proxy password
        {
            if (isEditingExisting) {
                if (tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD) && proxyPasswordInput->text().size() == 0) {
                     tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD);
                } else {
                    if (tempLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD).toString().size() > 0
                        || proxyPasswordInput->text().size() > 0
                    ) {
                        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD, proxyPasswordInput->text());
                    }
                }
            } else {
                if (proxyPasswordInput->text().size() > 0) {
                    tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD, proxyPasswordInput->text());
                }
            }
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
    return QString("rgba(%1,%2,%3,%4)")
        .arg(color->red())
        .arg(color->green())
        .arg(color->blue())
        .arg(color->alphaF());
}

bool LiquidAppCreateEditDialog::isPlanningToRun(void)
{
    return planningToRunCheckBox->isChecked();
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

void LiquidAppCreateEditDialog::setPlanningToRun(const bool maybe)
{
    planningToRunCheckBox->setChecked(maybe);
}
