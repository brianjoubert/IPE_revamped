// --------------------------------------------------------------------
// Modern theme support for the Qt frontend of Ipe
// --------------------------------------------------------------------
/*

    This file is part of the extensible drawing editor Ipe.

    Ipe is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

*/

#include "theme_qt.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QStringList>
#include <QGuiApplication>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>

#include <cmath>

namespace ipetheme {

static Mode sMode = Mode::System;
static Colors sColors;
static bool sSystemDarkAtStart = false;
static bool sInitialized = false;

// --------------------------------------------------------------------

static Colors darkColors() {
    Colors c;
    c.window = QColor("#1e1f22");
    c.surface = QColor("#25262a");
    c.base = QColor("#1a1b1e");
    c.raised = QColor("#2f3136");
    c.border = QColor("#3a3c42");
    c.text = QColor("#e3e5e8");
    c.textMuted = QColor("#9ca0a8");
    c.accent = QColor("#4c8dff");
    c.accentText = QColor("#ffffff");
    c.hover = QColor(255, 255, 255, 22);
    c.canvas = QColor("#141517");
    c.dark = true;
    return c;
}

static Colors lightColors() {
    Colors c;
    c.window = QColor("#f3f4f6");
    c.surface = QColor("#fbfbfc");
    c.base = QColor("#ffffff");
    c.raised = QColor("#ffffff");
    c.border = QColor("#d9dce1");
    c.text = QColor("#1d1f23");
    c.textMuted = QColor("#6b7280");
    c.accent = QColor("#2f6fed");
    c.accentText = QColor("#ffffff");
    c.hover = QColor(0, 0, 0, 16);
    c.canvas = QColor("#e4e6ea");
    c.dark = false;
    return c;
}

static bool systemIsDark() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    Qt::ColorScheme cs = QGuiApplication::styleHints()->colorScheme();
    if (cs == Qt::ColorScheme::Dark) return true;
    if (cs == Qt::ColorScheme::Light) return false;
#endif
    return sSystemDarkAtStart;
}

static QPalette makePalette(const Colors & c) {
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.surface);
    p.setColor(QPalette::ToolTipBase, c.raised);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.textMuted);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.raised);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Light, c.raised.lighter(120));
    p.setColor(QPalette::Midlight, c.raised);
    p.setColor(QPalette::Mid, c.border);
    p.setColor(QPalette::Dark, c.border.darker(130));
    p.setColor(QPalette::Shadow, QColor(0, 0, 0, 120));
    p.setColor(QPalette::Highlight, c.accent);
    p.setColor(QPalette::HighlightedText, c.accentText);
    p.setColor(QPalette::Link, c.accent);
    p.setColor(QPalette::LinkVisited, c.accent.darker(120));

    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
	QColor dis = c.textMuted;
	dis.setAlpha(140);
	p.setColor(QPalette::Disabled, role, dis);
    }
    p.setColor(QPalette::Disabled, QPalette::Highlight, c.border);
    return p;
}

// --------------------------------------------------------------------

static QString rgba(const QColor & c) {
    return QString("rgba(%1,%2,%3,%4)")
	.arg(c.red())
	.arg(c.green())
	.arg(c.blue())
	.arg(c.alpha());
}

// Writes small SVG assets used by the style sheet, returns directory
static QString writeAssets(const Colors & c) {
    QString dir = QDir::tempPath() + "/ipe-theme";
    QDir().mkpath(dir);
    QString col = c.textMuted.name();
    QString accent = c.accentText.name();
    auto write = [&](const QString & name, const QString & svg) {
	QFile f(dir + "/" + name);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(svg.toUtf8());
    };
    QString suffix = c.dark ? "-d" : "-l";
    write("down" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' "
		  "viewBox='0 0 10 10'><path d='M2 3.5 L5 6.5 L8 3.5' fill='none' "
		  "stroke='%1' stroke-width='1.5' stroke-linecap='round' "
		  "stroke-linejoin='round'/></svg>")
	      .arg(col));
    write("check" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' "
		  "viewBox='0 0 12 12'><path d='M2.5 6.2 L5 8.5 L9.5 3.5' fill='none' "
		  "stroke='%1' stroke-width='1.8' stroke-linecap='round' "
		  "stroke-linejoin='round'/></svg>")
	      .arg(accent));
    write("checka" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='12' height='12' "
		  "viewBox='0 0 12 12'><path d='M2.5 6.2 L5 8.5 L9.5 3.5' fill='none' "
		  "stroke='%1' stroke-width='1.8' stroke-linecap='round' "
		  "stroke-linejoin='round'/></svg>")
	      .arg(c.accent.name()));
    write("up" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' "
		  "viewBox='0 0 10 10'><path d='M2 6.5 L5 3.5 L8 6.5' fill='none' "
		  "stroke='%1' stroke-width='1.5' stroke-linecap='round' "
		  "stroke-linejoin='round'/></svg>")
	      .arg(col));
    write("close" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' "
		  "viewBox='0 0 10 10'><path d='M2.5 2.5 L7.5 7.5 M7.5 2.5 L2.5 7.5' "
		  "stroke='%1' stroke-width='1.3' stroke-linecap='round'/></svg>")
	      .arg(col));
    write("float" + suffix + ".svg",
	  QString("<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10' "
		  "viewBox='0 0 10 10'><rect x='2' y='2' width='6' height='6' rx='1' "
		  "fill='none' stroke='%1' stroke-width='1.2'/></svg>")
	      .arg(col));
    return dir;
}

static QString styleSheet(const Colors & c) {
    QString dir = writeAssets(c);
    QString suffix = c.dark ? "-d" : "-l";
    QColor pressed = c.accent;
    pressed.setAlpha(c.dark ? 70 : 45);
    QColor checkedBorder = c.accent;
    checkedBorder.setAlpha(170);
    QColor scrollHandle = c.textMuted;
    scrollHandle.setAlpha(110);
    QColor scrollHandleHover = c.textMuted;
    scrollHandleHover.setAlpha(170);

    QString qss = R"(
* { outline: none; }
QMainWindow { background: @window; }
QMainWindow::separator { background: @border; width: 1px; height: 1px; }

QMenuBar { background: @surface; color: @text; border-bottom: 1px solid @border;
           padding: 2px 4px; }
QMenuBar::item { background: transparent; padding: 4px 9px; border-radius: 5px; }
QMenuBar::item:selected { background: @hover; }
QMenuBar::item:pressed { background: @pressed; }

QMenu { background: @raised; color: @text; border: 1px solid @border; padding: 5px; }
QMenu::item { padding: 5px 26px 5px 22px; border-radius: 5px; }
QMenu::item:selected { background: @accent; color: @accentText; }
QMenu::item:disabled { color: @mutedDisabled; }
QMenu::separator { height: 1px; background: @border; margin: 5px 6px; }
QMenu::indicator { width: 14px; height: 14px; left: 4px; }
QMenu::indicator:checked { image: url(@dir/checka@suffix.svg); }
QMenu::indicator:checked:selected { image: url(@dir/check@suffix.svg); }

QToolBar { background: @surface; border: none; spacing: 2px; padding: 4px 6px; }
QToolBar#ObjectTools { border-right: 1px solid @border; padding: 6px 4px; }
QToolBar#EditTools, QToolBar#SnapTools, QToolBar#VariantTools, QToolBar#AppTools {
    border-bottom: 1px solid @border; }
QToolBar::separator { background: @border; width: 1px; height: 1px; margin: 4px 5px; }
QToolBar::handle { width: 0px; height: 0px; image: none; }

QToolButton { background: transparent; color: @text; border: 1px solid transparent;
              border-radius: 6px; padding: 3px; }
QToolButton:hover { background: @hover; }
QToolButton:pressed { background: @pressed; }
QToolButton:checked { background: @pressed; border: 1px solid @checkedBorder; }
QToolButton:disabled { color: @mutedDisabled; }
QToolButton#qt_toolbar_ext_button { padding: 0px; }
QToolButton#GradientButton { background: @raised; border: 1px solid @border;
    border-radius: 6px; padding: 4px 8px; }
QToolButton#GradientButton:hover { border-color: @accent; }
QGroupBox { border: 1px solid @border; border-radius: 8px; margin-top: 14px;
            padding: 10px 8px 8px 8px; color: @textMuted; }
QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
QSlider::groove:horizontal { height: 4px; background: @border; border-radius: 2px; }
QSlider::handle:horizontal { background: @accent; width: 14px; height: 14px;
    margin: -5px 0; border-radius: 7px; }
QRadioButton::indicator { width: 14px; height: 14px; border-radius: 7px;
    border: 1px solid @border; background: @base; }
QRadioButton::indicator:checked { background: @accent; border: 3px solid @base; }
QToolButton#PaletteButton { background: @base; border: 1px solid @border;
    color: @textMuted; padding: 3px 10px 3px 6px; border-radius: 7px; }
QToolButton#PaletteButton:hover { border-color: @accent; color: @text; }

QComboBox { background: @raised; color: @text; border: 1px solid @border;
            border-radius: 6px; padding: 3px 8px; min-height: 18px; }
QComboBox:hover { border-color: @accent; }
QComboBox:on { border-color: @accent; }
QComboBox:disabled { color: @mutedDisabled; }
QComboBox::drop-down { border: none; width: 18px; subcontrol-origin: padding;
                       subcontrol-position: center right; }
QComboBox::down-arrow { image: url(@dir/down@suffix.svg); width: 10px; height: 10px; }
QComboBox QAbstractItemView { background: @raised; color: @text; border: 1px solid @border;
    selection-background-color: @accent; selection-color: @accentText; padding: 4px; }

QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox {
    background: @base; color: @text; border: 1px solid @border; border-radius: 6px;
    padding: 4px 6px; selection-background-color: @accent; selection-color: @accentText; }
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QSpinBox:focus,
QDoubleSpinBox:focus { border-color: @accent; }

QSpinBox, QDoubleSpinBox { padding-right: 18px; }
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border;
    subcontrol-position: top right; width: 16px; border: none; background: transparent; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border;
    subcontrol-position: bottom right; width: 16px; border: none; background: transparent; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(@dir/up@suffix.svg);
    width: 8px; height: 8px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(@dir/down@suffix.svg);
    width: 8px; height: 8px; }
QPushButton { background: @raised; color: @text; border: 1px solid @border;
              border-radius: 6px; padding: 5px 14px; min-width: 60px; }
QPushButton:hover { border-color: @accent; }
QPushButton:pressed { background: @pressed; }
QPushButton:default { background: @accent; color: @accentText; border-color: @accent; }
QPushButton:disabled { color: @mutedDisabled; }

QCheckBox, QRadioButton { color: @text; spacing: 6px; }
QCheckBox::indicator, QListView::indicator { width: 14px; height: 14px; border-radius: 4px;
    border: 1px solid @border; background: @base; }
QCheckBox::indicator:hover, QListView::indicator:hover { border-color: @accent; }
QCheckBox::indicator:checked, QListView::indicator:checked {
    background: @accent; border-color: @accent; image: url(@dir/check@suffix.svg); }

QDockWidget { color: @textMuted; titlebar-close-icon: url(@dir/close@suffix.svg);
              titlebar-normal-icon: url(@dir/float@suffix.svg); }
QDockWidget::title { background: @surface; padding: 7px 10px 5px 10px; text-align: left;
                     border-bottom: 1px solid @border; }
QDockWidget::close-button, QDockWidget::float-button { border: none; background: transparent;
    border-radius: 4px; padding: 2px; icon-size: 10px; }
QDockWidget::close-button:hover, QDockWidget::float-button:hover { background: @hover; }
QDockWidget > QWidget { background: @surface; }

QListWidget, QListView, QTreeView, QTableView { background: @base; color: @text;
    border: none; selection-background-color: @accent; selection-color: @accentText; }

QStatusBar { background: @surface; color: @textMuted; border-top: 1px solid @border; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @textMuted; padding: 0px 6px; }
QSizeGrip { width: 0px; height: 0px; }

QToolTip { background: @raised; color: @text; border: 1px solid @border;
           border-radius: 5px; padding: 4px 7px; }

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle { background: @scrollHandle; border-radius: 3px; }
QScrollBar::handle:hover { background: @scrollHandleHover; }
QScrollBar::handle:vertical { min-height: 24px; }
QScrollBar::handle:horizontal { min-width: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

QTabWidget::pane { border: 1px solid @border; border-radius: 6px; top: -1px; }
QTabBar::tab { background: transparent; color: @textMuted; padding: 6px 12px;
               border-bottom: 2px solid transparent; }
QTabBar::tab:selected { color: @text; border-bottom-color: @accent; }
QTabBar::tab:hover { color: @text; }

QDialog, QMessageBox { background: @window; }
QLabel { color: @text; background: transparent; }
QFrame#PropertiesPanel { background: @surface; }
QLabel#SectionLabel { color: @textMuted; font-size: 8pt; font-weight: 600;
                      padding: 6px 2px 0px 2px; }

QFrame#CommandPalette { background: @raised; border: 1px solid @border;
                        border-radius: 10px; }
QFrame#CommandPalette QLineEdit { border: none; border-bottom: 1px solid @border;
    border-radius: 0px; background: transparent; padding: 10px 12px; font-size: 11pt; }
QFrame#CommandPalette QListWidget { background: transparent; padding: 4px; }
QFrame#CommandPalette QListWidget::item { padding: 0px; }
QFrame#CommandPalette QListWidget::item:selected { background: @accent; }
)";
    QColor mutedDisabled = c.textMuted;
    mutedDisabled.setAlpha(120);
    qss.replace("@window", c.window.name());
    qss.replace("@surface", c.surface.name());
    qss.replace("@raised", c.raised.name());
    qss.replace("@base", c.base.name());
    qss.replace("@border", c.border.name());
    qss.replace("@textMuted", c.textMuted.name());
    qss.replace("@mutedDisabled", rgba(mutedDisabled));
    qss.replace("@text", c.text.name());
    qss.replace("@accentText", c.accentText.name());
    qss.replace("@accent", c.accent.name());
    qss.replace("@hover", rgba(c.hover));
    qss.replace("@pressed", rgba(pressed));
    qss.replace("@checkedBorder", rgba(checkedBorder));
    qss.replace("@scrollHandleHover", rgba(scrollHandleHover));
    qss.replace("@scrollHandle", rgba(scrollHandle));
    qss.replace("@dir", QDir::fromNativeSeparators(dir));
    qss.replace("@suffix", suffix);
    return qss;
}

// --------------------------------------------------------------------

static void apply() {
    bool dark = (sMode == Mode::Dark) || (sMode == Mode::System && systemIsDark());
    sColors = dark ? darkColors() : lightColors();
    qApp->setStyleSheet(styleSheet(sColors));
    qApp->setPalette(makePalette(sColors));
}

void initialize() {
    if (sInitialized) return;
    sInitialized = true;

    // Remember whether the platform was dark before we override the palette
    QPalette sys = QGuiApplication::palette();
    sSystemDarkAtStart = sys.color(QPalette::Window).lightness() < 128;

    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Prefer a modern UI font when available (falls back to the default)
    QFont font = QApplication::font();
    QStringList families = {"Segoe UI Variable Text", "Segoe UI", "Inter",
			    "Cantarell", "Noto Sans"};
    families.append(font.families());
    font.setFamilies(families);
    QApplication::setFont(font);

    QSettings settings("ipe", "ipe-modern-ui");
    QString m = settings.value("theme", "system").toString();
    if (m == "light")
	sMode = Mode::Light;
    else if (m == "dark")
	sMode = Mode::Dark;
    else
	sMode = Mode::System;

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
		     qApp, [](Qt::ColorScheme) {
			 if (sMode == Mode::System) apply();
		     });
#endif
    apply();
}

Mode mode() { return sMode; }

QString modeName(Mode m) {
    switch (m) {
    case Mode::Light: return "light";
    case Mode::Dark: return "dark";
    default: return "system";
    }
}

void setMode(Mode m) {
    sMode = m;
    QSettings settings("ipe", "ipe-modern-ui");
    settings.setValue("theme", modeName(m));
    apply();
}

bool isDark() { return sColors.dark; }

const Colors & colors() { return sColors; }

// --------------------------------------------------------------------

QIcon glyph(const QString & name) {
    const int S = 64; // drawn at 64px, scaled by QIcon
    QPixmap pm(S, S);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(sColors.text, 5.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    if (name == "search") {
	p.drawEllipse(QPointF(27, 27), 15, 15);
	p.drawLine(QPointF(38, 38), QPointF(52, 52));
    } else if (name == "sun") {
	p.drawEllipse(QPointF(32, 32), 11, 11);
	for (int i = 0; i < 8; ++i) {
	    double a = i * 3.14159265358979 / 4.0;
	    p.drawLine(QPointF(32 + 18 * std::cos(a), 32 + 18 * std::sin(a)),
		       QPointF(32 + 25 * std::cos(a), 32 + 25 * std::sin(a)));
	}
    } else if (name == "moon") {
	QPainterPath outer;
	outer.addEllipse(QPointF(32, 32), 21, 21);
	QPainterPath cut;
	cut.addEllipse(QPointF(44, 22), 17, 17);
	p.drawPath(outer.subtracted(cut));
    } else if (name == "gradient") {
	QLinearGradient lg(10, 0, 54, 0);
	lg.setColorAt(0, QColor("#ff5e62"));
	lg.setColorAt(0.5, QColor("#ff9966"));
	lg.setColorAt(1, QColor("#4c8dff"));
	p.setPen(QPen(sColors.text, 3.0));
	p.setBrush(lg);
	p.drawRoundedRect(QRectF(8, 14, 48, 36), 8, 8);
    } else if (name == "system") {
	p.drawEllipse(QPointF(32, 32), 21, 21);
	QPainterPath half;
	half.moveTo(32, 11);
	half.arcTo(QRectF(11, 11, 42, 42), 90, 180);
	half.closeSubpath();
	p.setBrush(sColors.text);
	p.drawPath(half);
    }
    p.end();
    return QIcon(pm);
}

} // namespace ipetheme

// --------------------------------------------------------------------
