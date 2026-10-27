#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPointer>
#include <QSharedPointer>
#include <QStyle>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineScript>
#include <QWebEngineView>

#include "liquidappfind.hpp"
#include "lqd.h"

LiquidAppFindInput::LiquidAppFindInput(QWidget* parent) : QLineEdit(parent)
{
}

bool LiquidAppFindInput::event(QEvent* event)
{
    // Keys used for going from one match to another must reach the input, rather than trigger Liquid's shortcuts
    if (event->type() == QEvent::ShortcutOverride) {
        const QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

        switch (keyEvent->key()) {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            case Qt::Key_Up:
            case Qt::Key_Down:
            case Qt::Key_F3:
                event->accept();
                return true;

            default:
                break;
        }
    }

    return QLineEdit::event(event);
}

void LiquidAppFindInput::keyPressEvent(QKeyEvent* event)
{
    const bool isShiftPressed = event->modifiers() & Qt::ShiftModifier;

    switch (event->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_F3:
            if (isShiftPressed) {
                emit previousRequested();
            } else {
                emit nextRequested();
            }
            return;

        case Qt::Key_Down:
            emit nextRequested();
            return;

        case Qt::Key_Up:
            emit previousRequested();
            return;

        default:
            QLineEdit::keyPressEvent(event);
    }
}

LiquidAppFindDots::LiquidAppFindDots(QWidget* parent) : QWidget(parent)
{
    // Invisible: it only takes the mouse where the dots are
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus); // Clicking a dot leaves typing in the search box uninterrupted
    setCursor(Qt::PointingHandCursor);
}

void LiquidAppFindDots::setDots(const QList<QPointF>& newDots, const QList<bool>& newHasDot, const QRect& searchBoxGeometry)
{
    dots = newDots;
    hasDot = newHasDot;

    // Wherever the dots are (they're further from the search box when the page is zoomed in)
    QRectF area(searchBoxGeometry);
    for (int i = 0; i < dots.size(); i++) {
        if (hasDot[i]) {
            area = area.united(QRectF(dots[i], QSizeF(0, 0)));
        }
    }
    const int margin = LQD_FIND_DOT_HIT_RADIUS + 1;
    setGeometry(area.toAlignedRect().adjusted(-margin, -margin, margin, margin));

    // Only the dots take clicks (an empty mask would mean no mask at all, i.e. everything would)
    QRegion region;
    for (int i = 0; i < dots.size(); i++) {
        if (hasDot[i]) {
            const QPoint center = (dots[i] - QPointF(pos())).toPoint();
            region += QRegion(center.x() - LQD_FIND_DOT_HIT_RADIUS, center.y() - LQD_FIND_DOT_HIT_RADIUS,
                              2 * LQD_FIND_DOT_HIT_RADIUS, 2 * LQD_FIND_DOT_HIT_RADIUS, QRegion::Ellipse);
        }
    }

    if (region.isEmpty()) {
        setHoveredIndex(-1);
        hide();
    } else {
        setMask(region);
        show();
        raise();
    }

    if (hoveredIndex >= dots.size()) {
        setHoveredIndex(-1);
    }
}

int LiquidAppFindDots::getDotAt(const QPointF& position) const
{
    // The closest one (dots of matches in about the same direction overlap)
    int closestIndex = -1;
    qreal closestDistance = LQD_FIND_DOT_HIT_RADIUS * LQD_FIND_DOT_HIT_RADIUS;

    for (int i = 0; i < dots.size(); i++) {
        if (hasDot[i]) {
            const QPointF delta = dots[i] - QPointF(pos()) - position;
            const qreal distance = delta.x() * delta.x() + delta.y() * delta.y();

            if (distance <= closestDistance) {
                closestDistance = distance;
                closestIndex = i;
            }
        }
    }

    return closestIndex;
}

void LiquidAppFindDots::setHoveredIndex(const int index)
{
    if (index != hoveredIndex) {
        hoveredIndex = index;
        emit hovered(index);
    }
}

void LiquidAppFindDots::mouseMoveEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    setHoveredIndex(getDotAt(event->position()));
#else
    setHoveredIndex(getDotAt(event->localPos()));
#endif
}

void LiquidAppFindDots::mousePressEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const int index = getDotAt(event->position());
#else
    const int index = getDotAt(event->localPos());
#endif

    if (event->button() == Qt::LeftButton && index >= 0) {
        emit clicked(index);
    }
}

void LiquidAppFindDots::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);

    setHoveredIndex(-1);
}

QWidget* LiquidAppFind::createSearchBox(QWidget* parent, LiquidAppFindInput** input, QLabel** countLabel)
{
    // See-through: the page draws it, blurring and inverting what's underneath (see find.js); text is white or black,
    // whichever stands out from the page there, once inverted
    QWidget* searchBox = new QWidget(parent);
    searchBox->setObjectName("liquidFindBox");
    searchBox->setAttribute(Qt::WA_TranslucentBackground);
    searchBox->setStyleSheet(
        "#liquidFindBox { background: transparent; border: none; }"
        "#liquidFindBox QLineEdit { background: transparent; border: none; color: white; font-size: 16px; font-weight: 600;"
        "                           selection-background-color: rgba(255, 255, 255, 90); selection-color: white; }"
        "#liquidFindBox QLabel { background: transparent; color: rgba(255, 255, 255, 220); font-size: 13px; font-weight: 600; }"
        "#liquidFindBox QLabel[noMatches=\"true\"] { color: rgb(255, 150, 130); }"
        // Dark pages make for a light search box
        "#liquidFindBox[light=\"true\"] QLineEdit { color: black; selection-background-color: rgba(0, 0, 0, 70); selection-color: black; }"
        "#liquidFindBox[light=\"true\"] QLabel { color: rgba(0, 0, 0, 210); }"
        "#liquidFindBox[light=\"true\"] QLabel[noMatches=\"true\"] { color: rgb(170, 30, 0); }"
    );

    QHBoxLayout* layout = new QHBoxLayout(searchBox);
    layout->setContentsMargins(18, 0, 16, 0);
    layout->setSpacing(10);

    *input = new LiquidAppFindInput(searchBox);
    (*input)->setPlaceholderText(tr("Find on page"));
    (*input)->setAttribute(Qt::WA_MacShowFocusRect, false);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
    QPalette palette = (*input)->palette();
    palette.setColor(QPalette::PlaceholderText, QColor(255, 255, 255, 150));
    (*input)->setPalette(palette);
#endif
    layout->addWidget(*input, 1);

    *countLabel = new QLabel(searchBox);
    layout->addWidget(*countLabel);

    // The count gets a halo, for it to stand out (the input doesn't: graphics effects keep its caret from being shown,
    // and what's behind it is blurred anyway)
    QGraphicsDropShadowEffect* halo = new QGraphicsDropShadowEffect(*countLabel);
    halo->setBlurRadius(6);
    halo->setOffset(0, 0);
    halo->setColor(QColor(0, 0, 0, 230));
    (*countLabel)->setGraphicsEffect(halo);

    return searchBox;
}

LiquidAppFind::LiquidAppFind(QWebEngineView* webView) : QObject(webView), webView(webView)
{
    // The search engine itself (see find.js)
    QFile scriptFile(":/scripts/find.js");
    if (scriptFile.open(QFile::ReadOnly | QFile::Text)) {
        script = QString::fromUtf8(scriptFile.readAll());
    }

    searchBox = createSearchBox(webView, &input, &countLabel);
    searchBox->hide();

    dots = new LiquidAppFindDots(webView);
    dots->hide();
    connect(dots, &LiquidAppFindDots::hovered, this, [this](const int index) {
        // Only changes how things are drawn
        hoveredIndex = index;
        this->webView->page()->runJavaScript(QString("window.__liquidFind && __liquidFind.hover(%1);").arg(index),
                                             QWebEngineScript::ApplicationWorld);
    });
    connect(dots, &LiquidAppFindDots::clicked, this, [this](const int index) {
        pendingGoTo = index;
        scheduleUpdate();
    });

    // Matches get looked up again whenever the query changes, the page scrolls, and every now and then
    // (pages change all the time, and whatever shows up on them gets found)
    scheduleTimer = new QTimer(this);
    scheduleTimer->setSingleShot(true);
    scheduleTimer->setInterval(0);
    connect(scheduleTimer, &QTimer::timeout, this, &LiquidAppFind::update);

    pollTimer = new QTimer(this);
    pollTimer->setInterval(LQD_FIND_POLL_INTERVAL);
    connect(pollTimer, &QTimer::timeout, this, &LiquidAppFind::scheduleUpdate);

    connect(input, &QLineEdit::textChanged, this, &LiquidAppFind::scheduleUpdate);
    connect(input, &LiquidAppFindInput::nextRequested, this, [this]() {
        pendingMove++;
        scheduleUpdate();
    });
    connect(input, &LiquidAppFindInput::previousRequested, this, [this]() {
        pendingMove--;
        scheduleUpdate();
    });

    connect(webView->page(), &QWebEnginePage::scrollPositionChanged, this, &LiquidAppFind::scheduleUpdate);
    connect(webView->page(), &QWebEnginePage::contentsSizeChanged, this, &LiquidAppFind::scheduleUpdate);
}

void LiquidAppFind::open(const QString& text, const bool append)
{
    if (!text.isEmpty()) {
        if (append && isOpen()) {
            input->setText(input->text() + text);
        } else {
            input->setText(text);
        }
        input->deselect();
        input->end(false);
    } else {
        // Ctrl+F brings back the previous query, selected (typing replaces it)
        input->selectAll();
    }

    if (!isOpen()) {
        isOpenBool = true;
        updateGeometry();
        searchBox->show();
        pollTimer->start();
    }

    searchBox->raise();
    dots->raise();

    // Not the way shortcuts do it: that'd select all of the text, and the next key typed would replace it
    input->setFocus(Qt::OtherFocusReason);
    scheduleUpdate();
}

void LiquidAppFind::focusPage(void)
{
    // Not the way tabbing does it: that'd make the page focus its first focusable element
    webView->setFocus(Qt::OtherFocusReason);
}

void LiquidAppFind::close(void)
{
    if (!isOpen()) {
        return;
    }

    isOpenBool = false;
    pollTimer->stop();
    scheduleTimer->stop();
    pendingMove = 0;
    pendingGoTo = -1;
    hoveredIndex = -1;

    // Focus has to be handed back to the page before hiding the search box: hiding the focused widget would make Qt
    // pass focus on as if by tabbing
    if (input->hasFocus()) {
        focusPage();
    }
    searchBox->hide();
    dots->hide();

    webView->page()->runJavaScript("window.__liquidFind && __liquidFind.close();", QWebEngineScript::ApplicationWorld);
}

bool LiquidAppFind::isOpen(void) const
{
    return isOpenBool;
}

void LiquidAppFind::hideForSnapshot(void)
{
    if (!isOpen() || isHiddenForSnapshot) {
        return;
    }

    isHiddenForSnapshot = true;

    hadFocusBeforeSnapshot = input->hasFocus();
    if (hadFocusBeforeSnapshot) {
        focusPage();
    }
    searchBox->hide();
    dots->hide();

    // What's drawn on the page has to be gone (and the page repainted) before the snapshot gets taken
    QEventLoop loop;
    QPointer<QEventLoop> loopPointer(&loop);
    const QSharedPointer<bool> isDone(new bool(false));
    QTimer::singleShot(LQD_FIND_HIDE_TIMEOUT, &loop, &QEventLoop::quit);
    webView->page()->runJavaScript("window.__liquidFind && __liquidFind.setHidden(true);", QWebEngineScript::ApplicationWorld,
                                   [loopPointer, isDone](const QVariant&) {
        *isDone = true;
        if (loopPointer) {
            loopPointer->quit();
        }
    });
    if (!*isDone) {
        loop.exec();
    }

    QEventLoop repaintLoop;
    QTimer::singleShot(LQD_FIND_REPAINT_DELAY, &repaintLoop, &QEventLoop::quit);
    repaintLoop.exec();
}

void LiquidAppFind::showAfterSnapshot(void)
{
    if (!isHiddenForSnapshot) {
        return;
    }

    isHiddenForSnapshot = false;

    if (isOpen()) {
        webView->page()->runJavaScript("window.__liquidFind && __liquidFind.setHidden(false);", QWebEngineScript::ApplicationWorld);
        searchBox->show();
        searchBox->raise();
        if (hadFocusBeforeSnapshot) {
            input->setFocus(Qt::OtherFocusReason);
        }
        scheduleUpdate();
    }
}

void LiquidAppFind::updateGeometry(void)
{
    // In the very middle of the viewport (dots follow once matches get looked up again)
    const QRect viewport = webView->rect();
    const int width = qMin(LQD_FIND_BOX_WIDTH, viewport.width() - 2 * LQD_FIND_BOX_MARGIN);
    searchBox->setGeometry(viewport.center().x() - width / 2, viewport.center().y() - LQD_FIND_BOX_HEIGHT / 2, width, LQD_FIND_BOX_HEIGHT);

    if (isOpen()) {
        scheduleUpdate();
    }
}

void LiquidAppFind::scheduleUpdate(void)
{
    if (isOpen() && !scheduleTimer->isActive()) {
        scheduleTimer->start();
    }
}

void LiquidAppFind::update(void)
{
    if (!isOpen() || isHiddenForSnapshot) {
        return;
    }

    // One at a time (the page may be busy), whatever's been asked for in the meantime gets done right after
    if (isUpdating) {
        isUpdatePending = true;
        return;
    }

    const int move = qBound(-1, pendingMove, 1);
    pendingMove -= move;
    const int goTo = pendingGoTo;
    pendingGoTo = -1;

    // The page draws the search box too (in CSS pixels)
    const qreal zoom = webView->zoomFactor();
    const QRect box = searchBox->geometry();
    const QString quotedQuery = QString::fromUtf8(QJsonDocument(QJsonArray() << input->text()).toJson(QJsonDocument::Compact)).mid(1).chopped(1);
    const QString call = script + QString("\n;__liquidFind.update(%1, %2, %3, [%4, %5, %6, %7], %8);")
                                      .arg(quotedQuery).arg(move).arg(goTo)
                                      .arg(box.x() / zoom).arg(box.y() / zoom).arg(box.width() / zoom).arg(box.height() / zoom)
                                      .arg(hoveredIndex);

    isUpdating = true;
    QPointer<LiquidAppFind> self(this);
    QElapsedTimer timer;
    timer.start();

    // Liquid's own JS world: works even if JavaScript is disabled for the page, and the page can't interfere
    webView->page()->runJavaScript(call, QWebEngineScript::ApplicationWorld, [self, timer](const QVariant& result) {
        if (self) {
            self->onUpdated(result.toString(), timer.elapsed());
        }
    });
}

void LiquidAppFind::onUpdated(const QString& json, const qint64 duration)
{
    isUpdating = false;

    // Pages with lots of matches take longer to look through, they get looked through less often (keeps them responsive)
    pollTimer->setInterval(qMax<qint64>(LQD_FIND_POLL_INTERVAL, duration * 3));

    if (isOpen() && !isHiddenForSnapshot) {
        const QJsonObject result = QJsonDocument::fromJson(json.toUtf8()).object();
        const qreal zoom = webView->zoomFactor();

        // Where the dots are (in CSS pixels), for them to be clickable
        QList<QPointF> dotPositions;
        QList<bool> hasDot;
        for (const QJsonValue& value : result.value("dots").toArray()) {
            const QJsonArray dot = value.toArray();
            hasDot.append(dot.size() == 2);
            dotPositions.append((dot.size() == 2) ? QPointF(dot[0].toDouble() * zoom, dot[1].toDouble() * zoom) : QPointF());
        }
        dots->setDots(dotPositions, hasDot, searchBox->geometry());

        // The page may have put something over them in the meantime (e.g. upon going full-screen)
        searchBox->raise();
        dots->raise();

        setSearchBoxLight(!result.value("isSearchBoxDark").toBool(true));

        if (input->text().trimmed().isEmpty()) {
            updateCountLabel(-1, 0);
        } else {
            updateCountLabel(result.value("total").toInt(), result.value("current").toInt());
        }
    }

    if (isUpdatePending || pendingMove != 0 || pendingGoTo >= 0) {
        isUpdatePending = false;
        scheduleUpdate();
    }
}

void LiquidAppFind::updateCountLabel(const int total, const int current)
{
    if (total < 0) {
        countLabel->clear();
    } else if (total == 0) {
        countLabel->setText(tr("No matches"));
    } else {
        countLabel->setText(tr("%1 of %2").arg(current).arg(total));
    }

    // Repaint according to the stylesheet
    const bool hasNoMatches = total == 0;
    if (countLabel->property("noMatches").toBool() != hasNoMatches) {
        countLabel->setProperty("noMatches", hasNoMatches);
        countLabel->style()->unpolish(countLabel);
        countLabel->style()->polish(countLabel);
    }
}

void LiquidAppFind::setSearchBoxLight(const bool isLight)
{
    if (searchBox->property("light").toBool() == isLight) {
        return;
    }

    setSearchBoxLight(searchBox, isLight);
}

void LiquidAppFind::setSearchBoxLight(QWidget* searchBox, const bool isLight)
{
    searchBox->setProperty("light", isLight);

    // Text and its halo
    QPalette palette;
    for (QWidget* widget : searchBox->findChildren<QWidget*>()) {
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);

        if (QGraphicsDropShadowEffect* halo = qobject_cast<QGraphicsDropShadowEffect*>(widget->graphicsEffect())) {
            halo->setColor(isLight ? QColor(255, 255, 255, 230) : QColor(0, 0, 0, 230));
        }

#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        if (QLineEdit* lineEdit = qobject_cast<QLineEdit*>(widget)) {
            palette = lineEdit->palette();
            palette.setColor(QPalette::PlaceholderText, isLight ? QColor(0, 0, 0, 140) : QColor(255, 255, 255, 150));
            lineEdit->setPalette(palette);
        }
#endif
    }
}
