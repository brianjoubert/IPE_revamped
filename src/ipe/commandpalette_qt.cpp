// --------------------------------------------------------------------
// Command palette for the Qt frontend of Ipe
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

*/

#include "commandpalette_qt.h"
#include "theme_qt.h"

#include <QApplication>
#include <QGraphicsDropShadowEffect>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

enum { RoleTitle = Qt::UserRole + 1, RolePath, RoleShortcut, RoleIndex };

// --------------------------------------------------------------------

namespace {

class PaletteDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QSize sizeHint(const QStyleOptionViewItem & opt, const QModelIndex &) const override {
	return QSize(opt.rect.width(), opt.fontMetrics.height() + 14);
    }

    void paint(QPainter * p, const QStyleOptionViewItem & opt,
	       const QModelIndex & idx) const override {
	const ipetheme::Colors & c = ipetheme::colors();
	p->save();
	p->setRenderHint(QPainter::Antialiasing);
	bool sel = opt.state & QStyle::State_Selected;
	QRect r = opt.rect.adjusted(2, 1, -2, -1);
	if (sel) {
	    p->setPen(Qt::NoPen);
	    p->setBrush(c.accent);
	    p->drawRoundedRect(r, 6, 6);
	} else if (opt.state & QStyle::State_MouseOver) {
	    p->setPen(Qt::NoPen);
	    p->setBrush(c.hover);
	    p->drawRoundedRect(r, 6, 6);
	}
	QRect tr = r.adjusted(10, 0, -10, 0);
	QString title = idx.data(RoleTitle).toString();
	QString path = idx.data(RolePath).toString();
	QString sc = idx.data(RoleShortcut).toString();

	QFont f = opt.font;
	p->setFont(f);
	p->setPen(sel ? c.accentText : c.text);
	QFontMetrics fm(f);
	int tw = fm.horizontalAdvance(title);
	p->drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, title);

	QColor muted = sel ? c.accentText : c.textMuted;
	if (sel) muted.setAlpha(200);
	p->setPen(muted);
	int scw = 0;
	if (!sc.isEmpty()) {
	    QFont sf = f;
	    sf.setPointSizeF(f.pointSizeF() * 0.9);
	    p->setFont(sf);
	    QFontMetrics sfm(sf);
	    scw = sfm.horizontalAdvance(sc) + 12;
	    QRect kr(tr.right() - scw + 4, tr.top() + 5, scw - 4, tr.height() - 10);
	    QColor kb = sel ? QColor(255, 255, 255, 40) : c.hover;
	    p->setPen(Qt::NoPen);
	    p->setBrush(kb);
	    p->drawRoundedRect(kr, 4, 4);
	    p->setPen(muted);
	    p->drawText(kr, Qt::AlignCenter, sc);
	    p->setFont(f);
	}
	if (!path.isEmpty()) {
	    QRect pr = tr.adjusted(tw + 12, 0, -scw - 8, 0);
	    QString el = fm.elidedText(path, Qt::ElideLeft, pr.width());
	    p->setPen(muted);
	    p->drawText(pr, Qt::AlignVCenter | Qt::AlignLeft, el);
	}
	p->restore();
    }
};

// Fuzzy subsequence match; returns score < 0 if no match.
int fuzzyScore(const QString & needle, const QString & hay) {
    if (needle.isEmpty()) return 0;
    QString h = hay.toLower();
    QString n = needle.toLower();
    int direct = h.indexOf(n);
    if (direct >= 0) {
	// contiguous match; prefer start of string / word
	int s = 1000 - direct;
	if (direct == 0 || h[direct - 1] == ' ') s += 500;
	return s;
    }
    int score = 0, hi = 0, run = 0;
    for (QChar ch : n) {
	if (ch == ' ') continue;
	int found = h.indexOf(ch, hi);
	if (found < 0) return -1;
	run = (found == hi) ? run + 1 : 0;
	score += 10 + 5 * run - (found - hi);
	if (found == 0 || h[found - 1] == ' ') score += 15;
	hi = found + 1;
    }
    return score > 0 ? score : -1;
}

QString stripMnemonic(QString s) {
    s.replace("&&", "\x01");
    s.remove('&');
    s.replace("\x01", "&");
    return s;
}

} // namespace

// --------------------------------------------------------------------

CommandPalette::CommandPalette(QWidget * parent)
    : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setObjectName("CommandPalette");
    setAttribute(Qt::WA_TranslucentBackground, false);
    QVBoxLayout * lo = new QVBoxLayout(this);
    lo->setContentsMargins(0, 0, 0, 0);
    lo->setSpacing(0);
    iEdit = new QLineEdit;
    iEdit->setPlaceholderText("Search commands and ipelets…");
    iEdit->setClearButtonEnabled(true);
    iList = new QListWidget;
    iList->setItemDelegate(new PaletteDelegate(iList));
    iList->setMouseTracking(true);
    iList->setUniformItemSizes(true);
    iList->setFocusPolicy(Qt::NoFocus);
    iList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    lo->addWidget(iEdit);
    lo->addWidget(iList);

    iEdit->installEventFilter(this);
    connect(iEdit, &QLineEdit::textChanged, this, [this]() { refilter(); });
    connect(iList, &QListWidget::itemClicked, this, [this]() { runCurrent(); });
}

void CommandPalette::collect(QMenu * menu, const QString & path) {
    for (QAction * a : menu->actions()) {
	if (a->isSeparator()) continue;
	QString t = stripMnemonic(a->text());
	if (a->menu()) {
	    collect(a->menu(), path.isEmpty() ? t : path + "  ›  " + t);
	    continue;
	}
	if (t.isEmpty() || !a->isEnabled()) continue;
	Entry e;
	e.action = a;
	e.title = t;
	e.path = path;
	e.shortcut = a->shortcut().toString(QKeySequence::NativeText);
	iEntries.push_back(e);
    }
}

void CommandPalette::open(QMenuBar * menuBar) {
    iEntries.clear();
    for (QAction * a : menuBar->actions()) {
	if (a->menu()) collect(a->menu(), stripMnemonic(a->text()));
    }
    QWidget * w = parentWidget();
    int width = std::min(640, std::max(420, w->width() * 5 / 10));
    int height = std::min(440, std::max(260, w->height() * 6 / 10));
    resize(width, height);
    QPoint topLeft = w->mapToGlobal(QPoint((w->width() - width) / 2, w->height() / 8));
    move(topLeft);
    iEdit->clear();
    refilter();
    show();
    raise();
    activateWindow();
    iEdit->setFocus();
}

void CommandPalette::refilter() {
    QString q = iEdit->text().trimmed();
    struct Hit {
	int score;
	int index;
    };
    std::vector<Hit> hits;
    for (int i = 0; i < iEntries.size(); ++i) {
	const Entry & e = iEntries[i];
	if (!e.action) continue;
	int s = fuzzyScore(q, e.title);
	int sp = fuzzyScore(q, e.path + " " + e.title);
	if (sp >= 0) sp = sp / 3;
	int best = std::max(s, sp);
	if (best >= 0) hits.push_back({best, i});
    }
    if (!q.isEmpty())
	std::stable_sort(hits.begin(), hits.end(),
			 [](const Hit & a, const Hit & b) { return a.score > b.score; });
    iList->clear();
    for (const Hit & h : hits) {
	const Entry & e = iEntries[h.index];
	QListWidgetItem * it = new QListWidgetItem(iList);
	it->setData(RoleTitle, e.title);
	it->setData(RolePath, e.path);
	it->setData(RoleShortcut, e.shortcut);
	it->setData(RoleIndex, h.index);
	it->setToolTip(e.action->toolTip());
    }
    if (iList->count() > 0) iList->setCurrentRow(0);
}

void CommandPalette::runCurrent() {
    QListWidgetItem * it = iList->currentItem();
    if (!it) return;
    int idx = it->data(RoleIndex).toInt();
    QPointer<QAction> a = iEntries[idx].action;
    hide();
    // trigger after the popup has closed, so dialogs get the focus
    if (a) QTimer::singleShot(0, a, [a]() {
	    if (a && a->isEnabled()) a->trigger();
	});
}

bool CommandPalette::eventFilter(QObject * obj, QEvent * ev) {
    if (obj == iEdit && ev->type() == QEvent::KeyPress) {
	QKeyEvent * ke = static_cast<QKeyEvent *>(ev);
	int row = iList->currentRow();
	int n = iList->count();
	switch (ke->key()) {
	case Qt::Key_Down:
	    if (n) iList->setCurrentRow((row + 1) % n);
	    return true;
	case Qt::Key_Up:
	    if (n) iList->setCurrentRow((row - 1 + n) % n);
	    return true;
	case Qt::Key_PageDown:
	    if (n) iList->setCurrentRow(std::min(n - 1, row + 8));
	    return true;
	case Qt::Key_PageUp:
	    if (n) iList->setCurrentRow(std::max(0, row - 8));
	    return true;
	case Qt::Key_Return:
	case Qt::Key_Enter: runCurrent(); return true;
	case Qt::Key_Escape: hide(); return true;
	default: break;
	}
    }
    return QFrame::eventFilter(obj, ev);
}

// --------------------------------------------------------------------
