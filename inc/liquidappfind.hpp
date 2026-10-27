#pragma once

#include <QLineEdit>
#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QWidget>

class QLabel;
class QTimer;
class QWebEngineView;

// Text input of the search, which also takes care of going from one match to another
class LiquidAppFindInput : public QLineEdit
{
    Q_OBJECT

public:
    explicit LiquidAppFindInput(QWidget* parent = Q_NULLPTR);

signals:
    void nextRequested(void);
    void previousRequested(void);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
};

// Dots around the search box, where rays come out of (they're drawn on the page, see find.js): this is what makes them
// clickable. It covers the search box, but only its dots take the mouse, everything else goes through to what's below.
class LiquidAppFindDots : public QWidget
{
    Q_OBJECT

public:
    explicit LiquidAppFindDots(QWidget* parent = Q_NULLPTR);

    // In the web view's coordinates (null ones are of matches that have no ray)
    void setDots(const QList<QPointF>& dots, const QList<bool>& hasDot, const QRect& searchBoxGeometry);

signals:
    void hovered(const int index);
    void clicked(const int index);

protected:
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int getDotAt(const QPointF& position) const;
    void setHoveredIndex(const int index);

    QList<QPointF> dots;
    QList<bool> hasDot;
    int hoveredIndex = -1;
};

// Find in page: a search box floating in the middle of the web view, with every match highlighted, and connected to it
// by a ray, all of which inverts the page underneath, like an X-ray. Keeps on looking for as long as it's open,
// so whatever shows up on the page later (including what gets typed into text inputs) gets found too.
class LiquidAppFind : public QObject
{
    Q_OBJECT

public:
    explicit LiquidAppFind(QWebEngineView* webView);

    // Opens (or focuses) the search box; the text replaces the query, or gets added to it (for find-as-you-type)
    void open(const QString& text = QString(), const bool append = false);
    void close(void);
    bool isOpen(void) const;

    // Snapshots are of the page, not of the search (it stays open meanwhile)
    void hideForSnapshot(void);
    void showAfterSnapshot(void);

    // The web view has been resized
    void updateGeometry(void);

    // Something may have moved (e.g. the page has been scrolled), find out where the matches are now
    void scheduleUpdate(void);

    // The search box itself: the input, and how many matches there are
    static QWidget* createSearchBox(QWidget* parent, LiquidAppFindInput** input, QLabel** countLabel);

    // Whether the search box (drawn by the page, inverting it) is light, and its text has to be dark
    static void setSearchBoxLight(QWidget* searchBox, const bool isLight);

private:
    void update(void);
    void onUpdated(const QString& json, const qint64 duration);
    void updateCountLabel(const int total, const int current);
    void focusPage(void);
    void setSearchBoxLight(const bool isLight);

    QWebEngineView* webView;
    QWidget* searchBox;
    LiquidAppFindInput* input;
    QLabel* countLabel;
    LiquidAppFindDots* dots;
    QTimer* pollTimer;
    QTimer* scheduleTimer;
    QString script;

    bool isOpenBool = false;
    bool isHiddenForSnapshot = false;
    bool hadFocusBeforeSnapshot = false;
    bool isUpdating = false;
    bool isUpdatePending = false;
    int pendingMove = 0;
    int pendingGoTo = -1; // Match whose dot has been clicked
    int hoveredIndex = -1;
};
