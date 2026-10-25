#include <QDir>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QNetworkCookie>
#include <QWebEngineProfile>

#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "liquidappconfigwindow.hpp"
#include "lqd.h"
#include "mainwindow.hpp"
#include "singleinstance.hpp"

LiquidAppConfigDialog::LiquidAppConfigDialog(QWidget* parent, QString liquidAppName) : QDialog(parent)
{
    setWindowFlags(windowFlags() | Qt::Window);

    liquidAppName = liquidAppName.replace(QDir::separator(), "_");

    Liquid::applyQtStyleSheets(this);

    // Attempt to load liquid app's config file
    QSettings* existingLiquidAppConfig = LiquidAppFile::open(liquidAppName);

    // Check to see if Liquid app by this name already has config file
    if (liquidAppName.size() > 0) {
        isEditingExistingBool = existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_URL);
    } else {
        delete existingLiquidAppConfig;
    }

    if (isEditingExistingBool) {
        setWindowTitle(tr("Editing existing Liquid App “%1”").arg(liquidAppName));
    } else {
        setWindowTitle(tr("Adding new Liquid App"));
    }

    backgroundColor = new QColor(LQD_DEFAULT_BG_COLOR);

    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    QGridLayout* basicLayout = new QGridLayout();

    QWidget* advancedWidget = new QWidget(this);
    QVBoxLayout* advancedLayout = new QVBoxLayout();
    advancedWidget->setContentsMargins(0, 0, 0, 0);
    advancedWidget->setLayout(advancedLayout);

    QTabWidget* tabWidget = new QTabWidget;
    tabWidget->tabBar()->setCursor(Qt::PointingHandCursor);

    mainLayout->addLayout(basicLayout);

    // Name input
    {
        QLabel* nameInputLabel = new QLabel(tr("Name:"), this);
        nameInput = new QLineEdit;
        nameInput->setMinimumSize(480, 0);
        nameInput->setPlaceholderText("my-liquid-app-name");
        nameInput->setText(liquidAppName);
        connect(nameInput, &QLineEdit::textChanged, [=]{ style()->polish(nameInput); });

        if (isEditingExistingBool) {
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
        connect(addressInput, &QLineEdit::textChanged, [=]{ style()->polish(addressInput); });

        if (isEditingExistingBool) {
            addressInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_URL).toString());
        }

        basicLayout->addWidget(addressInputLabel, 1, 0);
        basicLayout->addWidget(addressInput, 1, 1);
    }

    // Extra checkboxes visible only in "Create" mode
    if (!isEditingExistingBool) {
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
            planningToRunCheckBox->setChecked(isPlanningToRunBool);
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
        buttonsLayout->setContentsMargins(0, 0, 0, 0);

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

            connect(cancelButton, &QPushButton::clicked, [&]() {
                close();
            });
        }

        {
            saveButton = new QPushButton(tr((isEditingExistingBool) ? "Save" : "Add"), this);
            saveButton->setCursor(Qt::PointingHandCursor);
            saveButton->setDefault(true);
            buttonsLayout->addWidget(saveButton);

            connect(saveButton, &QPushButton::clicked, [&]() {
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
                titleInput->setPlaceholderText(tr("Application Title"));
                connect(titleInput, &QLineEdit::textChanged, [=]{ style()->polish(titleInput); });

                if (isEditingExistingBool) {
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
                if (isEditingExistingBool) {
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

                connect(additionalDomainsModel, &QStandardItemModel::itemChanged, [&](QStandardItem* item){
                    const int itemIndex = item->row();
                    const bool isLastItem = itemIndex == additionalDomainsModel->rowCount() - 1;
                    static const QRegularExpression allowedCharacters = QRegularExpression("[^a-z0-9\\.:\\-]");

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
                // Set placeholder to what QWebEngineProfile has by default
                userAgentInput->setPlaceholderText(Liquid::getDefaultUserAgentString());
                connect(userAgentInput, &QLineEdit::textChanged, [=]{ style()->polish(userAgentInput); });

                if (isEditingExistingBool) {
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
            connect(notesTextArea, &QPlainTextEdit::textChanged, [=]{ style()->polish(notesTextArea); });

            if (isEditingExistingBool) {
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

            if (isEditingExistingBool) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS)) {
                    hideScrollBarsCheckBox->setChecked(
                        existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_HIDE_SCROLLBARS).toBool()
                    );
                }
            }

            // Show or hide scrollbars in the running Liquid app right away
            connect(hideScrollBarsCheckBox, &QCheckBox::toggled, this, [this](const bool isChecked) {
                sendPreview(QByteArray(LQD_INSTANCE_MSG_PREVIEW_SCROLL " ") + ((isChecked) ? "off" : "on"));
            });

            appearanceTabWidgetLayout->addWidget(hideScrollBarsCheckBox);
        }
        #endif

        // Remove window frame
        {
            appearanceTabWidgetLayout->addWidget(separator());

            removeWindowFrameCheckBox = new QCheckBox(tr("Remove window frame"), this);
            removeWindowFrameCheckBox->setCursor(Qt::PointingHandCursor);

            if (isEditingExistingBool) {
                if (existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME)) {
                    removeWindowFrameCheckBox->setChecked(
                        existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_REMOVE_WINDOW_FRAME).toBool()
                    );
                }
            }

            // Remove or bring back the running Liquid app's window frame right away
            connect(removeWindowFrameCheckBox, &QCheckBox::toggled, this, [this](const bool isChecked) {
                sendPreview(QByteArray(LQD_INSTANCE_MSG_PREVIEW_FRAME " ") + ((isChecked) ? "off" : "on"));
            });

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

                if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR)) {
                    backgroundColor = new QColor(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR).toString());
                    customBackgroundColorButton->setStyleSheet(buttonStyle.arg(backgroundColor->name(QColor::HexArgb)).arg(fontSize));
                } else {
                    static const QColor defaultColor = QColor(LQD_DEFAULT_BG_COLOR);
                    customBackgroundColorButton->setStyleSheet(buttonStyle.arg(defaultColor.name(QColor::HexArgb)).arg(fontSize));
                }

                customBackgroundColorButtonLayout->addWidget(customBackgroundColorButton);

                connect(customBackgroundColorButton, &QPushButton::clicked, [&]() {
                    QColorDialog colorDialog(*backgroundColor, this);
                    colorDialog.setWindowTitle(tr("Pick custom background color"));
                    colorDialog.setOptions(QColorDialog::ShowAlphaChannel);

                    // Show colors in the running Liquid app as they're being picked
                    connect(&colorDialog, &QColorDialog::currentColorChanged, this, [this](const QColor& color) {
                        previewBackground(color);
                    });

                    if (colorDialog.exec() == QDialog::Accepted && colorDialog.selectedColor().isValid()) {
                        const QColor color = colorDialog.selectedColor();

                        *backgroundColor = color;
                        customBackgroundColorButton->setStyleSheet(buttonStyle.arg(backgroundColor->name(QColor::HexArgb)).arg(fontSize));

                        if (!useCustomBackgroundCheckBox->isChecked()) {
                            useCustomBackgroundCheckBox->setChecked(true); // Previews the color too
                        } else {
                            previewBackground(color);
                        }
                    } else if (hasPreviewedChanges) {
                        // Picking got cancelled, go back to previewing whatever was there before
                        previewBackground((useCustomBackgroundCheckBox->isChecked()) ? *backgroundColor : QColor());
                    }
                });
            }

            appearanceTabWidgetLayout->addLayout(customBackgroundColorButtonLayout);

            if (isEditingExistingBool) {
                bool enabledInConfig = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_USE_CUSTOM_BG, false).toBool();
                useCustomBackgroundCheckBox->setChecked(enabledInConfig);
            }

            connect(useCustomBackgroundCheckBox, &QCheckBox::toggled, this, [this](const bool isChecked) {
                previewBackground((isChecked) ? *backgroundColor : QColor());
            });
        }

        // Additional CSS text area
        {
            appearanceTabWidgetLayout->addWidget(separator());

            {
                QLabel* additionalCssLabel = new QLabel(tr("Additional CSS:"), this);
                appearanceTabWidgetLayout->addWidget(additionalCssLabel);
            }
            additionalCssTextArea = new QPlainTextEdit(this);
            additionalCssTextArea->setProperty("class", "monospace");
            additionalCssTextArea->setPlaceholderText(tr("/* put your custom CSS here */"));
            connect(additionalCssTextArea, &QPlainTextEdit::textChanged, [=]{ style()->polish(additionalCssTextArea); });

            // Repaint the running Liquid app as its CSS is being edited (shortly after typing stops)
            cssPreviewTimer = new QTimer(this);
            cssPreviewTimer->setSingleShot(true);
            cssPreviewTimer->setInterval(LQD_CSS_PREVIEW_DELAY);
            connect(cssPreviewTimer, &QTimer::timeout, this, [this]() {
                // Base64, since messages are newline-terminated and CSS may have line breaks in it
                sendPreview(QByteArray(LQD_INSTANCE_MSG_PREVIEW_CSS " ") + additionalCssTextArea->toPlainText().toUtf8().toBase64());
            });

            if (isEditingExistingBool) {
                additionalCssTextArea->setPlainText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_ADDITIONAL_CSS).toString());
            }

            // Connected only now, to not preview what's been loaded from the file
            connect(additionalCssTextArea, &QPlainTextEdit::textChanged, cssPreviewTimer, QOverload<>::of(&QTimer::start));

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

            if (isEditingExistingBool) {
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
            additionalJsTextArea->setProperty("class", "monospace");
            additionalJsTextArea->setPlaceholderText(tr("// This code will run even when JS is disabled"));
            connect(additionalJsTextArea, &QPlainTextEdit::textChanged, [=]{ style()->polish(additionalJsTextArea); });

            if (isEditingExistingBool) {
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

                if (isEditingExistingBool) {
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

                if (isEditingExistingBool) {
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

        // Cookies list view
        {
            cookiesTabWidgetLayout->addWidget(separator());

            // Cookies list label
            {
                QLabel* cookiesListLabel = new QLabel(tr("Cookie jar:"), this);
                cookiesTabWidgetLayout->addWidget(cookiesListLabel);
            }

            // Cookies list
            {
                cookiesTableView = new QTableView(this);
                cookiesModel = new QStandardItemModel(this);

                cookiesModel->setHorizontalHeaderItem(0, new QStandardItem(tr("Name")));
                cookiesModel->setHorizontalHeaderItem(1, new QStandardItem(tr("Value")));
                cookiesModel->setHorizontalHeaderItem(2, new QStandardItem(tr("Domain")));
                cookiesModel->setHorizontalHeaderItem(3, new QStandardItem(tr("Path")));
                cookiesModel->setHorizontalHeaderItem(4, new QStandardItem(tr("Expires")));
                cookiesModel->setHorizontalHeaderItem(5, new QStandardItem(tr("HttpOnly")));
                cookiesModel->setHorizontalHeaderItem(6, new QStandardItem(tr("Secure")));

                // Assign model
                cookiesTableView->setModel(cookiesModel);

                // Fill model items
                if (isEditingExistingBool) {
                    existingLiquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
                    int i = 0;
                    foreach(QString cookieId, existingLiquidAppConfig->allKeys()) {
                        const QByteArray rawCookie = existingLiquidAppConfig->value(cookieId).toByteArray();
                        QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(rawCookie);
                        if (cookies.size() > 0) {
                            QNetworkCookie cookie = cookies[0];

                            cookiesModel->appendRow(new QStandardItem());
                            cookiesModel->setItem(i, 0, new QStandardItem(QString(cookie.name())));
                            cookiesModel->setItem(i, 1, new QStandardItem(QString(cookie.value())));
                            cookiesModel->setItem(i, 2, new QStandardItem(cookie.domain()));
                            cookiesModel->setItem(i, 3, new QStandardItem(cookie.path()));
                            cookiesModel->setItem(i, 4, new QStandardItem(cookie.expirationDate().toString()));
                            cookiesModel->setItem(i, 5, new QStandardItem(cookie.isHttpOnly() ? "true" : "false"));
                            cookiesModel->setItem(i, 6, new QStandardItem(cookie.isSecure() ? "true" : "false"));

                            i++;
                        }
                    }
                    existingLiquidAppConfig->endGroup();
                }
            }

            cookiesTabWidgetLayout->addWidget(cookiesTableView);
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

                            if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USE_SOCKS)) {
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
                            connect(proxyHostInput, &QLineEdit::textChanged, [=]{ style()->polish(proxyHostInput); });

                            if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_HOST)) {
                                proxyHostInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_HOST).toString());
                            }

                            proxyTypeHostPortLayout->addWidget(proxyHostInput);
                        }

                        // Proxy port
                        {
                            proxyPortInput = new QSpinBox(this);
                            proxyPortInput->setRange(0, 65535);
                            proxyPortInput->setValue(LQD_DEFAULT_PROXY_PORT);

                            if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_PORT)) {
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
                            proxyUseAuthCheckBox = new QCheckBox(tr("Authenticate with credentials:"), this);

                            proxyCredentialsLayout->addWidget(proxyUseAuthCheckBox);
                        }

                        // Username
                        {
                            proxyUsernameInput = new QLineEdit(this);
                            proxyUsernameInput->setPlaceholderText(tr("Username"));
                            connect(proxyUsernameInput, &QLineEdit::textChanged, [=]{ style()->polish(proxyUsernameInput); });

                            if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME)) {
                                proxyUsernameInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString());
                            }

                            proxyCredentialsLayout->addWidget(proxyUsernameInput);
                        }

                        // Password
                        {
                            proxyPasswordInput = new QLineEdit(this);
                            proxyPasswordInput->setPlaceholderText(tr("Password"));
                            proxyPasswordInput->setEchoMode(QLineEdit::Password);
                            connect(proxyPasswordInput, &QLineEdit::textChanged, [=]{ style()->polish(proxyPasswordInput); });

                            if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_PROXY_USER_NAME)) {
                                proxyPasswordInput->setText(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USER_NAME).toString());
                            }

                            proxyCredentialsLayout->addWidget(proxyPasswordInput);
                        }

                        proxyConfigLayout->addLayout(proxyCredentialsLayout);
                    }

                    if (isEditingExistingBool && existingLiquidAppConfig->contains(LQD_CFG_KEY_NAME_USE_PROXY)) {
                        const bool proxyEnabled = existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_USE_PROXY, false).toBool();

                        if (proxyEnabled) {
                            proxyModeCustomRadioButton->setChecked(true);
                        } else {
                            proxyModeDirectRadioButton->setChecked(true);
                        }
                    } else {
                        proxyModeSystemRadioButton->setChecked(true);
                    }

                    if (isEditingExistingBool && existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_PROXY_USE_AUTH, false).toBool()) {
                        proxyUseAuthCheckBox->setChecked(true);
                    }

                    customProxyModeLayout->addLayout(proxyConfigLayout);
                }

                connect(useSocksSelectBox, &QComboBox::currentTextChanged, [&](){
                    proxyModeCustomRadioButton->setChecked(true);
                });
                connect(proxyHostInput, &QLineEdit::textChanged, [&](const QString value){
                    if (value.size() > 0) {
                        proxyModeCustomRadioButton->setChecked(true);
                    } else {
                        proxyModeSystemRadioButton->setChecked(true);
                    }
                });
                connect(proxyPortInput, QOverload<int>::of(&QSpinBox::valueChanged), [&](){
                    proxyModeCustomRadioButton->setChecked(true);
                });
                connect(proxyUseAuthCheckBox, &QCheckBox::toggled, [&](){
                    proxyModeCustomRadioButton->setChecked(true);
                });
                connect(proxyUsernameInput, &QLineEdit::textChanged, [&](const QString value){
                    proxyModeCustomRadioButton->setChecked(true);
                    proxyUseAuthCheckBox->setChecked(value.size() > 0);
                });
                connect(proxyPasswordInput, &QLineEdit::textChanged, [&](const QString value){
                    proxyModeCustomRadioButton->setChecked(true);
                    proxyUseAuthCheckBox->setChecked(value.size() > 0);
                });

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

    if (isEditingExistingBool) {
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

LiquidAppConfigDialog::~LiquidAppConfigDialog(void)
{
}

void LiquidAppConfigDialog::save()
{
    bool isFormValid = nameInput->text().size() > 0 && addressInput->text().size() > 0;

    if (!isFormValid) {
        return;
    }

    QString appName = nameInput->text();
    // Replace directory separators (slashes) with underscores
    // to ensure no sub-directories would get created
    appName = appName.replace(QDir::separator(), "_");

    // Check if given Liquid App name is already in use
    if (!isEditingExistingBool && Liquid::getLiquidAppsList().contains(appName)) {
        return;
    }

    QSettings* tempLiquidAppConfig = LiquidAppFile::open(appName);

    // Some settings can't be changed while the Liquid app is running, remember them to tell whether it needs a restart
    const QString settingsRequiringRestartBefore = getSettingsRequiringRestart(tempLiquidAppConfig);

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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        if (isEditingExistingBool) {
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
        // TODO: make it possible to remove and (re-)create desktop icons for existing Liquid Apps

        if (!isEditingExistingBool) {
            if (createIconCheckBox->isChecked()) {
                QUrl url(QUrl::fromUserInput(addressInput->text()));
                Liquid::createDesktopFile(appName, url.toString());
            }
        }
    }

    // Custom background color
    {
        // Use custom background checkbox
        {
            if (isEditingExistingBool) {
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
            tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_CUSTOM_BG_COLOR, backgroundColor->name(QColor::HexArgb));
        }
    }

    // Additional domains
    {
        if (isEditingExistingBool && tempLiquidAppConfig->contains(LQD_CFG_KEY_NAME_ADDITIONAL_DOMAINS) && additionalDomainsModel->rowCount() == 1) {
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
                if (isEditingExistingBool) {
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
                if (isEditingExistingBool) {
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
            if (isEditingExistingBool) {
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
            if (isEditingExistingBool) {
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
            if (isEditingExistingBool) {
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
            if (isEditingExistingBool) {
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
            if (isEditingExistingBool) {
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

    // A pending CSS preview is no longer needed, the saved CSS gets applied below
    cssPreviewTimer->stop();

    // Apply new settings to the Liquid app if it's running
    if (isEditingExistingBool) {
        const QString instanceId = LQD_INSTANCE_ID_APP_PREFIX + appName;

        if (getSettingsRequiringRestart(tempLiquidAppConfig) != settingsRequiringRestartBefore) {
            // Restart it (wait for it to quit first, it's not possible to have two instances running at once)
            if (SingleInstance::sendMessage(instanceId, LQD_INSTANCE_MSG_QUIT)) {
                QElapsedTimer timer;
                timer.start();

                while (SingleInstance::isRunning(instanceId) && timer.elapsed() < LQD_INSTANCE_STOP_TIMEOUT) {
                    Liquid::sleep(100);
                }

                if (!SingleInstance::isRunning(instanceId)) {
                    Liquid::runLiquidApp(appName);
                }
            }
        } else {
            SingleInstance::sendMessage(instanceId, LQD_INSTANCE_MSG_RELOAD_SETTINGS);
        }
    }

    delete tempLiquidAppConfig;

    accept();
}

void LiquidAppConfigDialog::previewBackground(const QColor& color)
{
    const QByteArray colorName = (color.isValid()) ? color.name(QColor::HexArgb).toUtf8() : QByteArray("none");

    sendPreview(QByteArray(LQD_INSTANCE_MSG_PREVIEW_BG " ") + colorName);
}

void LiquidAppConfigDialog::sendPreview(const QByteArray& message)
{
    // Only applies to Liquid apps that already exist (and are running)
    if (!isEditingExistingBool) {
        return;
    }

    if (liquidAppConnection == Q_NULLPTR) {
        liquidAppConnection = new QLocalSocket(this);
        liquidAppConnection->connectToServer(SingleInstance::serverName(LQD_INSTANCE_ID_APP_PREFIX + nameInput->text()));
        liquidAppConnection->waitForConnected(LQD_PREVIEW_CONNECT_TIMEOUT);
    }

    if (liquidAppConnection->state() == QLocalSocket::ConnectedState) {
        liquidAppConnection->write(message + "\n");
        liquidAppConnection->flush();
        hasPreviewedChanges = true;
    }
}

void LiquidAppConfigDialog::reject()
{
    // Don't let a pending CSS preview arrive after the settings have been restored
    cssPreviewTimer->stop();

    // Bring back what's been previewed so far to how it is in the saved settings
    if (hasPreviewedChanges && liquidAppConnection != Q_NULLPTR && liquidAppConnection->state() == QLocalSocket::ConnectedState) {
        liquidAppConnection->write(LQD_INSTANCE_MSG_RELOAD_SETTINGS "\n");
        liquidAppConnection->flush();
    }

    QDialog::reject();
}

QString LiquidAppConfigDialog::getSettingsRequiringRestart(QSettings* liquidAppConfig)
{
    // Proxy settings get picked up by the web engine only upon start
    static const QStringList keys = {
        LQD_CFG_KEY_NAME_USE_PROXY,
        LQD_CFG_KEY_NAME_PROXY_USE_SOCKS,
        LQD_CFG_KEY_NAME_PROXY_HOST,
        LQD_CFG_KEY_NAME_PROXY_PORT,
        LQD_CFG_KEY_NAME_PROXY_USE_AUTH,
        LQD_CFG_KEY_NAME_PROXY_USER_NAME,
        LQD_CFG_KEY_NAME_PROXY_USER_PASSWORD,
    };

    QStringList values;
    for (const QString& key : keys) {
        values << ((liquidAppConfig->contains(key)) ? liquidAppConfig->value(key).toString() : QStringLiteral("<unset>"));
    }

    return values.join('\n');
}

void LiquidAppConfigDialog::bindShortcuts(void)
{
    // Connect keyboard shortcut that closes the dialog
    quitAction = new QAction();
    quitAction->setShortcut(QKeySequence(tr(LQD_KBD_SEQ_QUIT)));
    addAction(quitAction);
    connect(quitAction, SIGNAL(triggered()), this, SLOT(close()));
}

bool LiquidAppConfigDialog::isPlanningToRun(void)
{
    return !isEditingExistingBool && planningToRunCheckBox->isChecked();
}

QString LiquidAppConfigDialog::getName(void)
{
    return nameInput->text();
}

QFrame* LiquidAppConfigDialog::separator(void)
{
    QFrame* separatorFrame = new QFrame;

    separatorFrame->setFrameShape(QFrame::HLine);
    separatorFrame->setFrameShadow(QFrame::Sunken);

    return separatorFrame;
}

void LiquidAppConfigDialog::setPlanningToRun(const bool state)
{
    if (!isEditingExistingBool) {
        isPlanningToRunBool = state;
        if (planningToRunCheckBox != Q_NULLPTR) {
            planningToRunCheckBox->setChecked(state);
        }
    }
}
