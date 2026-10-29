#include <QDir>
#include <QFileSystemWatcher>
#include <QScopedPointer>
#include <QScrollBar>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QLocalSocket>
#include <QNetworkCookie>
#include <QPair>
#include <QWebEngineProfile>

#include "liquid.hpp"
#include "liquidappfile.hpp"
#include "liquidappicon.hpp"
#include "liquidapplauncher.hpp"
#include "liquidapplocalstorage.hpp"
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

    // Icon (gets fetched from the website the first time it loads, unless one is chosen here)
    {
        if (isEditingExistingBool) {
            chosenIcon = LiquidAppIcon::load(existingLiquidAppConfig);
        }

        iconButton = new QToolButton(this);
        iconButton->setCursor(Qt::PointingHandCursor);
        iconButton->setAutoRaise(true);
        iconButton->setPopupMode(QToolButton::InstantPopup);
        iconButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
        iconButton->setIconSize(QSize(40, 40));
        iconButton->setFixedSize(52, 52);

        QMenu* iconMenu = new QMenu(iconButton);
        QAction* chooseIconAction = iconMenu->addAction(tr("Choose image…"));
        QAction* resetIconAction = iconMenu->addAction(tr("Get from website"));
        iconButton->setMenu(iconMenu);

        const auto updateIconButton = [this, resetIconAction]() {
            iconButton->setIcon((chosenIcon.isNull()) ? QIcon(":/images/" PROG_NAME ".svg") : chosenIcon.toIcon());
            iconButton->setToolTip((chosenIcon.isNull()) ? tr("Icon (will be taken from the website)") : tr("Icon"));
            resetIconAction->setEnabled(!chosenIcon.isNull());
        };
        updateIconButton();

        connect(chooseIconAction, &QAction::triggered, this, [this, updateIconButton]() {
            QStringList patterns;
            for (const QByteArray& format : QImageReader::supportedImageFormats()) {
                patterns << "*." + QString::fromLatin1(format);
            }
            patterns << "*.svg" << "*.svgz";
            patterns.removeDuplicates();

            const QString path = QFileDialog::getOpenFileName(this, tr("Choose icon"), QDir::homePath(),
                                                              tr("Images (%1)").arg(patterns.join(' ')));
            if (path.isEmpty()) {
                return;
            }

            QFile file(path);
            const LiquidAppIcon icon = (file.size() <= LQD_ICON_MAX_FILE_SIZE && file.open(QIODevice::ReadOnly))
                                       ? LiquidAppIcon::fromData(file.readAll()) : LiquidAppIcon();
            if (icon.isNull()) {
                QMessageBox::warning(this, tr("Unable to use image"), tr("“%1” couldn't be read as an image.").arg(path));
                return;
            }

            chosenIcon = icon;
            iconWasEdited = true;
            updateIconButton();
        });

        // Forgetting the icon makes the Liquid app fetch it again (right away, if it's running)
        connect(resetIconAction, &QAction::triggered, this, [this, updateIconButton]() {
            chosenIcon = LiquidAppIcon();
            iconWasEdited = true;
            updateIconButton();
        });

        basicLayout->addWidget(iconButton, 0, 2, 2, 1, Qt::AlignCenter);
    }

    // Launchers (whether there are any is up to whether their files exist), and "Run after creation" (only in "Create" mode)
    if (LiquidAppLauncher::isSupported() || !isEditingExistingBool) {
        QHBoxLayout* extraCheckboxesLayout = new QHBoxLayout();

        // "Show in applications menu" checkbox (makes it an app of its own, Liquid apps get one by default)
        if (LiquidAppLauncher::isSupported(LiquidAppLauncher::ApplicationsMenu)) {
            menuLauncherCheckBox = new QCheckBox(tr("Show in applications menu"), this);
            menuLauncherCheckBox->setCursor(Qt::PointingHandCursor);
            menuLauncherCheckBox->setToolTip(tr("Makes it show up as an app of its own: it can be found by its name, and pinned to the dock"));
            menuLauncherCheckBox->setChecked((isEditingExistingBool)
                                             ? LiquidAppLauncher::exists(LiquidAppLauncher::ApplicationsMenu, liquidAppName) : true);
            extraCheckboxesLayout->addWidget(menuLauncherCheckBox);
        }

        // "Create desktop icon" checkbox
        if (LiquidAppLauncher::isSupported(LiquidAppLauncher::Desktop)) {
            createIconCheckBox = new QCheckBox(tr("Create desktop icon"), this);
            createIconCheckBox->setCursor(Qt::PointingHandCursor);
            createIconCheckBox->setChecked(isEditingExistingBool && LiquidAppLauncher::exists(LiquidAppLauncher::Desktop, liquidAppName));
            extraCheckboxesLayout->addWidget(createIconCheckBox);
        }

        extraCheckboxesLayout->addStretch();

        // "Run after creation" checkbox
        if (!isEditingExistingBool) {
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

        // Color scheme
        {
            QHBoxLayout* colorSchemeLayout = new QHBoxLayout();

            QLabel* colorSchemeLabel = new QLabel(tr("Color scheme:"), this);
            colorSchemeLayout->addWidget(colorSchemeLabel);

            colorSchemeComboBox = new QComboBox(this);
            colorSchemeComboBox->setCursor(Qt::PointingHandCursor);
            colorSchemeComboBox->addItem(tr("System"), QString());
            colorSchemeComboBox->addItem(tr("Light"), QStringLiteral(LQD_COLOR_SCHEME_LIGHT));
            colorSchemeComboBox->addItem(tr("Dark"), QStringLiteral(LQD_COLOR_SCHEME_DARK));

            if (isEditingExistingBool) {
                const int index = colorSchemeComboBox->findData(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_COLOR_SCHEME).toString());
                colorSchemeComboBox->setCurrentIndex((index >= 0) ? index : 0);
            }

            // The running Liquid app switches right away
            connect(colorSchemeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
                const QString colorScheme = colorSchemeComboBox->currentData().toString();
                sendPreview(QByteArray(LQD_INSTANCE_MSG_PREVIEW_SCHEME " ") + ((colorScheme.isEmpty()) ? QByteArray("system") : colorScheme.toUtf8()));
            });

            colorSchemeLayout->addWidget(colorSchemeComboBox);
            colorSchemeLayout->addStretch();

            appearanceTabWidgetLayout->addLayout(colorSchemeLayout);
            appearanceTabWidgetLayout->addWidget(separator());
        }

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
    // Storage tab //
    /////////////////

    {
        QWidget* cookiesTabWidget = new QWidget(this);
        QVBoxLayout* cookiesTabWidgetLayout = new QVBoxLayout();
        cookiesTabWidget->setLayout(cookiesTabWidgetLayout);
        tabWidget->addTab(cookiesTabWidget, tr("Storage"));

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

        // Cookies list
        {
            cookiesTabWidgetLayout->addWidget(separator());
            cookiesTabWidgetLayout->addWidget(new QLabel(tr("Cookies:"), this));

            cookiesTableView = new QTableView(this);
            cookiesModel = new QStandardItemModel(this);

            cookiesModel->setHorizontalHeaderLabels({ tr("Name"), tr("Value"), tr("Domain"), tr("Path"), tr("Expires"), tr("HttpOnly"), tr("Secure"), QString() });
            setUpStorageTableView(cookiesTableView, cookiesModel, 1);
            // The cookie jar belongs to the Liquid app, it's only shown here (and can have cookies removed)
            cookiesTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

            if (isEditingExistingBool) {
                fillCookiesModel(existingLiquidAppConfig);
            }

            cookiesTabWidgetLayout->addWidget(cookiesTableView);
        }

        // Session storage list (only while the Liquid app is running: it's never stored anywhere, so it comes from there)
        {
            cookiesTabWidgetLayout->addWidget(separator());
            cookiesTabWidgetLayout->addWidget(new QLabel(tr("Session storage:"), this));

            sessionStorageTableView = new QTableView(this);
            sessionStorageModel = new QStandardItemModel(this);

            sessionStorageModel->setHorizontalHeaderLabels({ tr("Website"), tr("Key"), tr("Value"), QString() });
            setUpStorageTableView(sessionStorageTableView, sessionStorageModel, 2);
            sessionStorageTableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
            sessionStorageTableView->setToolTip(tr("Session storage of the page the Liquid app is showing (while it's running)"));

            cookiesTabWidgetLayout->addWidget(sessionStorageTableView);
        }

        // Local storage list
        {
            cookiesTabWidgetLayout->addWidget(separator());

            QLabel* localStorageListLabel = new QLabel(tr("Local storage:"), this);
            cookiesTabWidgetLayout->addWidget(localStorageListLabel);

            localStorageTableView = new QTableView(this);
            localStorageModel = new QStandardItemModel(this);

            localStorageModel->setHorizontalHeaderLabels({ tr("Website"), tr("Key"), tr("Value"), QString() });
            setUpStorageTableView(localStorageTableView, localStorageModel, 2);

            if (isEditingExistingBool) {
                fillLocalStorageModel(existingLiquidAppConfig);
            }

            cookiesTabWidgetLayout->addWidget(localStorageTableView);

            // Remember local storage checkbox (local storage works regardless, this is about keeping it after quitting)
            rememberLocalStorageCheckBox = new QCheckBox(tr("Remember local storage"), this);
            rememberLocalStorageCheckBox->setCursor(Qt::PointingHandCursor);
            rememberLocalStorageCheckBox->setToolTip(tr("Keep what websites store in their local storage after quitting"));
            if (isEditingExistingBool) {
                rememberLocalStorageCheckBox->setChecked(existingLiquidAppConfig->value(LQD_CFG_KEY_NAME_REMEMBER_LOCAL_STORAGE, false).toBool());
            } else {
                // Checked by default (when creating new Liquid App)
                rememberLocalStorageCheckBox->setChecked(true);
            }
            cookiesTabWidgetLayout->addWidget(rememberLocalStorageCheckBox);

            // Editing within the list (edits are kept on top of what the Liquid app stores, which keeps getting shown)
            connect(localStorageModel, &QStandardItemModel::itemChanged, this, [this](QStandardItem* item) {
                // Only keys and values can be edited
                if (item->column() != 1 && item->column() != 2) {
                    return;
                }

                const int row = item->row();
                QStandardItem* originItem = localStorageModel->item(row, 0);
                const QString origin = originItem->text();
                const QString key = localStorageModel->item(row, 1)->text();
                const QString previousId = originItem->data(Qt::UserRole).toString();
                const QString id = getStorageItemId(origin, key);

                // A renamed key is the old item removed, and a new one added
                if (id != previousId) {
                    removedLocalStorageIds.insert(previousId);
                    localStorageEdits.remove(previousId);

                    const QSignalBlocker blocker(localStorageModel);
                    originItem->setData(id, Qt::UserRole);
                }

                QJsonObject edit;
                edit.insert("origin", origin);
                edit.insert("key", key);
                edit.insert("value", localStorageModel->item(row, 2)->text());
                localStorageEdits.insert(id, edit);
                removedLocalStorageIds.remove(id);
            });

            // The list only matters if local storage gets remembered (otherwise it gets forgotten upon saving)
            const auto onRememberToggled = [this, localStorageListLabel](const bool isOn) {
                localStorageListLabel->setEnabled(isOn);
                localStorageTableView->setEnabled(isOn);
            };
            onRememberToggled(rememberLocalStorageCheckBox->isChecked());
            connect(rememberLocalStorageCheckBox, &QCheckBox::toggled, this, onRememberToggled);
        }

        // The Delete key removes selected items (just like clicking their "✖")
        for (QTableView* tableView : { cookiesTableView, sessionStorageTableView, localStorageTableView }) {
            QShortcut* removeShortcut = new QShortcut(QKeySequence::Delete, tableView, Q_NULLPTR, Q_NULLPTR, Qt::WidgetShortcut);
            connect(removeShortcut, &QShortcut::activated, this, [this, tableView]() {
                QList<int> rows;
                for (const QModelIndex& index : tableView->selectionModel()->selectedRows()) {
                    rows << index.row();
                }

                // From the bottom up, so that the remaining rows' numbers stay valid
                std::sort(rows.begin(), rows.end(), std::greater<int>());
                for (const int row : rows) {
                    removeStorageItem(tableView, row);
                }
            });
        }

        // Keep the lists up to date while the Liquid app runs
        if (isEditingExistingBool) {
            const QString liquidAppFilePath = Liquid::getLiquidAppFilePath(liquidAppName);

            // It keeps writing cookies and local storage to its file
            storageRefreshTimer = new QTimer(this);
            storageRefreshTimer->setSingleShot(true);
            storageRefreshTimer->setInterval(LQD_STORAGE_REFRESH_DELAY);
            connect(storageRefreshTimer, &QTimer::timeout, this, [this, liquidAppName]() {
                refreshStorage(liquidAppName);
            });

            // The file gets replaced rather than overwritten when saved, which makes the watcher lose track of it,
            // so its directory gets watched too
            liquidAppFileWatcher = new QFileSystemWatcher(this);
            liquidAppFileWatcher->addPath(liquidAppFilePath);
            liquidAppFileWatcher->addPath(QFileInfo(liquidAppFilePath).absolutePath());
            const auto onChanged = [this, liquidAppFilePath]() {
                if (!liquidAppFileWatcher->files().contains(liquidAppFilePath) && QFile::exists(liquidAppFilePath)) {
                    liquidAppFileWatcher->addPath(liquidAppFilePath);
                }

                storageRefreshTimer->start();
            };
            connect(liquidAppFileWatcher, &QFileSystemWatcher::fileChanged, this, onChanged);
            connect(liquidAppFileWatcher, &QFileSystemWatcher::directoryChanged, this, onChanged);

            // Session storage gets sent over by the Liquid app itself
            if (connectToLiquidApp()) {
                liquidAppConnection->write(LQD_INSTANCE_MSG_WATCH_SESSION_STORAGE "\n");
                liquidAppConnection->flush();
            }
            sessionStorageTableView->setEnabled(liquidAppConnection != Q_NULLPTR && liquidAppConnection->state() == QLocalSocket::ConnectedState);
        } else {
            sessionStorageTableView->setEnabled(false);
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
    // Qt deletes child widgets before it cuts their connections, and some children signal things while getting deleted:
    // most notably, the connection to the Liquid app (which goes last) signals that it's been disconnected,
    // by which time the lists it updates are long gone; nothing should reach this half-destroyed dialog
    for (QObject* child : findChildren<QObject*>()) {
        disconnect(child, Q_NULLPTR, this, Q_NULLPTR);
    }
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

    // Cookies removed in settings (only those, the Liquid app may have stored new ones in the meantime)
    if (!removedCookieIds.isEmpty()) {
        tempLiquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
        for (const QString& cookieId : removedCookieIds) {
            tempLiquidAppConfig->remove(cookieId);
        }
        tempLiquidAppConfig->endGroup();
    }

    // Local storage
    {
        tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_REMEMBER_LOCAL_STORAGE, rememberLocalStorageCheckBox->isChecked());

        if (!rememberLocalStorageCheckBox->isChecked()) {
            // Not to be remembered, forget it
            tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_LOCAL_STORAGE);
        } else if (!removedLocalStorageIds.isEmpty() || !localStorageEdits.isEmpty()) {
            // Only what's been removed or edited here: the Liquid app (if running) may have stored more in the meantime
            QJsonObject localStorage = LiquidAppLocalStorage::load(tempLiquidAppConfig);

            for (const QString& id : removedLocalStorageIds) {
                const QString origin = id.section(QChar(0x1F), 0, 0);
                QJsonObject items = localStorage.value(origin).toObject();
                items.remove(id.section(QChar(0x1F), 1));
                if (items.isEmpty()) {
                    localStorage.remove(origin);
                } else {
                    localStorage.insert(origin, items);
                }
            }

            for (const QJsonObject& edit : localStorageEdits) {
                const QString origin = edit.value("origin").toString();
                QJsonObject items = localStorage.value(origin).toObject();
                items.insert(edit.value("key").toString(), edit.value("value"));
                localStorage.insert(origin, items);
            }

            LiquidAppLocalStorage::save(tempLiquidAppConfig, localStorage);
        }
    }

    // Session storage items removed here get removed from the running Liquid app's page
    if (!removedSessionStorageIds.isEmpty() && connectToLiquidApp()) {
        QMap<QString, QJsonArray> keysByOrigin;
        for (const QString& id : removedSessionStorageIds) {
            keysByOrigin[id.section(QChar(0x1F), 0, 0)].append(id.section(QChar(0x1F), 1));
        }

        for (QMap<QString, QJsonArray>::const_iterator it = keysByOrigin.constBegin(); it != keysByOrigin.constEnd(); ++it) {
            QJsonObject removal;
            removal.insert("origin", it.key());
            removal.insert("keys", it.value());

            liquidAppConnection->write(QByteArray(LQD_INSTANCE_MSG_REMOVE_SESSION_STORAGE " ")
                                       + QJsonDocument(removal).toJson(QJsonDocument::Compact).toBase64() + "\n");
        }
        liquidAppConnection->flush();
    }

#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
    // Color scheme (the system's, unless chosen otherwise)
    {
        const QString colorScheme = colorSchemeComboBox->currentData().toString();

        if (colorScheme.isEmpty()) {
            tempLiquidAppConfig->remove(LQD_CFG_KEY_NAME_COLOR_SCHEME);
        } else {
            tempLiquidAppConfig->setValue(LQD_CFG_KEY_NAME_COLOR_SCHEME, colorScheme);
        }
    }

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

    // Icon (has to be in place before the desktop icon gets created, for it to use it)
    if (iconWasEdited) {
        if (chosenIcon.isNull()) {
            LiquidAppIcon::remove(tempLiquidAppConfig, appName);
        } else {
            chosenIcon.save(tempLiquidAppConfig, appName);
        }
    }

    // Launchers (after the icon and the title, for them to use those)
    {
        for (const QPair<LiquidAppLauncher::Kind, QCheckBox*>& launcher : {
                qMakePair(LiquidAppLauncher::ApplicationsMenu, menuLauncherCheckBox),
                qMakePair(LiquidAppLauncher::Desktop, createIconCheckBox),
             }) {
            if (launcher.second == Q_NULLPTR) {
                continue;
            }

            const bool exists = LiquidAppLauncher::exists(launcher.first, appName);

            if (launcher.second->isChecked() && !exists) {
                LiquidAppLauncher::create(launcher.first, appName, tempLiquidAppConfig);
            } else if (!launcher.second->isChecked() && exists) {
                LiquidAppLauncher::remove(launcher.first, appName);
            }
        }

        // The title may have changed
        LiquidAppLauncher::update(appName, tempLiquidAppConfig);
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

        if (isOpenedFromLiquidAppBool) {
            // The Liquid app can't be told to quit (and be waited for) from within itself, it takes care of it on its own
            isRestartRequiredBool = getSettingsRequiringRestart(tempLiquidAppConfig) != settingsRequiringRestartBefore;
        } else if (getSettingsRequiringRestart(tempLiquidAppConfig) != settingsRequiringRestartBefore) {
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
    if (connectToLiquidApp()) {
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

void LiquidAppConfigDialog::setOpenedFromLiquidApp(const bool state)
{
    isOpenedFromLiquidAppBool = state;
}

bool LiquidAppConfigDialog::isRestartRequired(void)
{
    return isRestartRequiredBool;
}

namespace {

// Rows are told apart by the text of some of their columns, so that the selection survives refilling the list
QString getRowId(const QStandardItemModel* model, const int row, const QList<int>& idColumns)
{
    QStringList parts;
    for (const int column : idColumns) {
        parts << model->item(row, column)->text();
    }

    return parts.join(QChar(0x1F));
}

struct TableViewState {
    QSet<QString> selectedRowIds;
    QString currentRowId;
    int verticalScroll;
    int horizontalScroll;
};

TableViewState saveTableViewState(const QTableView* view, const QStandardItemModel* model, const QList<int>& idColumns)
{
    TableViewState state;

    for (const QModelIndex& index : view->selectionModel()->selectedRows()) {
        state.selectedRowIds.insert(getRowId(model, index.row(), idColumns));
    }
    if (view->currentIndex().isValid()) {
        state.currentRowId = getRowId(model, view->currentIndex().row(), idColumns);
    }
    state.verticalScroll = view->verticalScrollBar()->value();
    state.horizontalScroll = view->horizontalScrollBar()->value();

    return state;
}

void restoreTableViewState(QTableView* view, const QStandardItemModel* model, const QList<int>& idColumns, const TableViewState& state)
{
    QItemSelection selection;

    for (int row = 0; row < model->rowCount(); row++) {
        const QString rowId = getRowId(model, row, idColumns);

        if (rowId == state.currentRowId) {
            view->selectionModel()->setCurrentIndex(model->index(row, 0), QItemSelectionModel::NoUpdate);
        }
        if (state.selectedRowIds.contains(rowId)) {
            selection.select(model->index(row, 0), model->index(row, model->columnCount() - 1));
        }
    }

    view->selectionModel()->select(selection, QItemSelectionModel::ClearAndSelect);
    view->verticalScrollBar()->setValue(state.verticalScroll);
    view->horizontalScrollBar()->setValue(state.horizontalScroll);
}

} // namespace

QString LiquidAppConfigDialog::getStorageItemId(const QString& origin, const QString& key)
{
    return origin + QChar(0x1F) + key;
}

void LiquidAppConfigDialog::setUpStorageTableView(QTableView* tableView, QStandardItemModel* model, const int stretchingColumn)
{
    tableView->setModel(model);
    tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tableView->setWordWrap(false);
    tableView->verticalHeader()->hide();

    // The last column has the "✖" buttons
    QHeaderView* header = tableView->horizontalHeader();
    const int removeColumn = model->columnCount() - 1;
    header->setStretchLastSection(false);
    header->setSectionResizeMode(stretchingColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(removeColumn, QHeaderView::Fixed);
    header->resizeSection(removeColumn, LQD_UI_REMOVE_BUTTON_COLUMN_WIDTH);
}

QStandardItem* LiquidAppConfigDialog::newRemoveButtonItem(void)
{
    // An empty cell for the "✖" button to sit in (it has to be part of the row from the start: setting items of rows
    // that already exist counts as editing them)
    QStandardItem* item = new QStandardItem();
    item->setEditable(false);

    return item;
}

void LiquidAppConfigDialog::addRemoveButtons(QTableView* tableView, QStandardItemModel* model)
{
    const int removeColumn = model->columnCount() - 1;

    for (int row = 0; row < model->rowCount(); row++) {
        QToolButton* removeButton = new QToolButton(tableView);
        removeButton->setText(LQD_ICON_DELETE);
        removeButton->setToolTip(tr("Remove"));
        removeButton->setCursor(Qt::PointingHandCursor);
        removeButton->setAutoRaise(true);

        // Rows move around as others get removed, the index keeps track of where this one is
        const QPersistentModelIndex index(model->index(row, 0));
        connect(removeButton, &QToolButton::clicked, this, [this, tableView, index]() {
            if (index.isValid()) {
                removeStorageItem(tableView, index.row());
            }
        });

        tableView->setIndexWidget(model->index(row, removeColumn), removeButton);
    }
}

void LiquidAppConfigDialog::removeStorageItem(QTableView* tableView, const int row)
{
    // Removed items are remembered (the lists get refilled as the Liquid app stores things), and get removed for real
    // only upon saving (until then, they're merely hidden)
    if (tableView == cookiesTableView) {
        removedCookieIds.insert(cookiesModel->item(row, 0)->data(Qt::UserRole).toString());
        cookiesModel->removeRow(row);
    } else if (tableView == sessionStorageTableView) {
        removedSessionStorageIds.insert(sessionStorageModel->item(row, 0)->data(Qt::UserRole).toString());
        sessionStorageModel->removeRow(row);
    } else if (tableView == localStorageTableView) {
        const QString id = localStorageModel->item(row, 0)->data(Qt::UserRole).toString();
        removedLocalStorageIds.insert(id);
        localStorageEdits.remove(id);
        localStorageModel->removeRow(row);
    }
}

void LiquidAppConfigDialog::fillCookiesModel(QSettings* liquidAppConfig)
{
    static const QList<int> idColumns = { 0, 2, 3 }; // Name, domain, path
    const TableViewState state = saveTableViewState(cookiesTableView, cookiesModel, idColumns);

    cookiesModel->removeRows(0, cookiesModel->rowCount());

    liquidAppConfig->beginGroup(LQD_CFG_GROUP_NAME_COOKIES);
    for (const QString& cookieId : liquidAppConfig->allKeys()) {
        const QList<QNetworkCookie> cookies = QNetworkCookie::parseCookies(liquidAppConfig->value(cookieId).toByteArray());

        // Removed here, but the Liquid app doesn't know it yet (it will once settings get saved)
        if (cookies.size() > 0 && !removedCookieIds.contains(cookieId)) {
            const QNetworkCookie& cookie = cookies[0];

            QStandardItem* nameItem = new QStandardItem(QString(cookie.name()));
            nameItem->setData(cookieId, Qt::UserRole);

            cookiesModel->appendRow({
                nameItem,
                new QStandardItem(QString(cookie.value())),
                new QStandardItem(cookie.domain()),
                new QStandardItem(cookie.path()),
                new QStandardItem(cookie.expirationDate().toString()),
                new QStandardItem(cookie.isHttpOnly() ? "true" : "false"),
                new QStandardItem(cookie.isSecure() ? "true" : "false"),
                newRemoveButtonItem(),
            });
        }
    }
    liquidAppConfig->endGroup();

    addRemoveButtons(cookiesTableView, cookiesModel);
    restoreTableViewState(cookiesTableView, cookiesModel, idColumns, state);
}

void LiquidAppConfigDialog::fillLocalStorageModel(QSettings* liquidAppConfig)
{
    static const QList<int> idColumns = { 0, 1 }; // Website, key
    const TableViewState state = saveTableViewState(localStorageTableView, localStorageModel, idColumns);

    // What the Liquid app has stored, with what's been removed or edited here on top of it
    QMap<QString, QJsonObject> rows;
    const QJsonObject localStorage = LiquidAppLocalStorage::load(liquidAppConfig);
    for (QJsonObject::const_iterator origin = localStorage.constBegin(); origin != localStorage.constEnd(); ++origin) {
        const QJsonObject items = origin.value().toObject();

        for (QJsonObject::const_iterator item = items.constBegin(); item != items.constEnd(); ++item) {
            const QString id = getStorageItemId(origin.key(), item.key());

            if (!removedLocalStorageIds.contains(id)) {
                QJsonObject row;
                row.insert("origin", origin.key());
                row.insert("key", item.key());
                row.insert("value", item.value());
                rows.insert(id, row);
            }
        }
    }
    for (QMap<QString, QJsonObject>::const_iterator edit = localStorageEdits.constBegin(); edit != localStorageEdits.constEnd(); ++edit) {
        rows.insert(edit.key(), edit.value());
    }

    localStorageModel->removeRows(0, localStorageModel->rowCount());

    for (QMap<QString, QJsonObject>::const_iterator row = rows.constBegin(); row != rows.constEnd(); ++row) {
        QStandardItem* originItem = new QStandardItem(row.value().value("origin").toString());
        originItem->setEditable(false);
        originItem->setData(row.key(), Qt::UserRole);

        QStandardItem* valueItem = new QStandardItem(row.value().value("value").toString());
        valueItem->setToolTip(valueItem->text().left(LQD_UI_TOOLTIP_MAX_LENGTH));

        // Appending whole rows doesn't count as editing them (unlike setting items of rows that already exist)
        localStorageModel->appendRow({ originItem, new QStandardItem(row.value().value("key").toString()), valueItem, newRemoveButtonItem() });
    }

    addRemoveButtons(localStorageTableView, localStorageModel);

    restoreTableViewState(localStorageTableView, localStorageModel, idColumns, state);
}

void LiquidAppConfigDialog::fillSessionStorageModel(void)
{
    static const QList<int> idColumns = { 0, 1 }; // Website, key
    const TableViewState state = saveTableViewState(sessionStorageTableView, sessionStorageModel, idColumns);

    sessionStorageModel->removeRows(0, sessionStorageModel->rowCount());

    const QString origin = sessionStorage.value("origin").toString();
    const QJsonObject items = sessionStorage.value("items").toObject();
    for (QJsonObject::const_iterator item = items.constBegin(); item != items.constEnd(); ++item) {
        const QString id = getStorageItemId(origin, item.key());

        if (!removedSessionStorageIds.contains(id)) {
            QStandardItem* originItem = new QStandardItem(origin);
            originItem->setData(id, Qt::UserRole);

            QStandardItem* valueItem = new QStandardItem(item.value().toString());
            valueItem->setToolTip(valueItem->text().left(LQD_UI_TOOLTIP_MAX_LENGTH));

            sessionStorageModel->appendRow({ originItem, new QStandardItem(item.key()), valueItem, newRemoveButtonItem() });
        }
    }

    addRemoveButtons(sessionStorageTableView, sessionStorageModel);
    restoreTableViewState(sessionStorageTableView, sessionStorageModel, idColumns, state);
}

void LiquidAppConfigDialog::refreshStorage(const QString& liquidAppName)
{
    QScopedPointer<QSettings> liquidAppConfig(LiquidAppFile::open(liquidAppName));

    fillCookiesModel(liquidAppConfig.data());

    // Don't pull the rug from under an edit in progress, try again once it's done
    // (cells get edited in editor widgets placed within the list)
    const QWidget* focusWidget = QApplication::focusWidget();
    const bool isEditingCell = focusWidget != Q_NULLPTR && focusWidget != localStorageTableView
                               && localStorageTableView->isAncestorOf(focusWidget) && qobject_cast<const QToolButton*>(focusWidget) == Q_NULLPTR;
    if (isEditingCell) {
        storageRefreshTimer->start();
        return;
    }

    fillLocalStorageModel(liquidAppConfig.data());
}

bool LiquidAppConfigDialog::connectToLiquidApp(void)
{
    // Only Liquid apps that already exist (and are running) can be talked to
    if (!isEditingExistingBool) {
        return false;
    }

    if (liquidAppConnection == Q_NULLPTR) {
        liquidAppConnection = new QLocalSocket(this);
        liquidAppConnection->connectToServer(SingleInstance::serverName(LQD_INSTANCE_ID_APP_PREFIX + nameInput->text()));
        liquidAppConnection->waitForConnected(LQD_PREVIEW_CONNECT_TIMEOUT);

        // Things the Liquid app sends back (newline-terminated, like everything else sent between instances)
        connect(liquidAppConnection, &QLocalSocket::readyRead, this, [this]() {
            static const QByteArray sessionStoragePrefix = QByteArray(LQD_INSTANCE_MSG_SESSION_STORAGE) + " ";

            while (liquidAppConnection->canReadLine()) {
                const QByteArray message = liquidAppConnection->readLine().trimmed();

                if (message.startsWith(sessionStoragePrefix)) {
                    sessionStorage = QJsonDocument::fromJson(QByteArray::fromBase64(message.mid(sessionStoragePrefix.size()))).object();
                    fillSessionStorageModel();
                }
            }
        });

        // The Liquid app has quit, its session storage is gone
        connect(liquidAppConnection, &QLocalSocket::disconnected, this, [this]() {
            sessionStorage = QJsonObject();
            fillSessionStorageModel();
            sessionStorageTableView->setEnabled(false);
        });
    }

    return liquidAppConnection->state() == QLocalSocket::ConnectedState;
}
