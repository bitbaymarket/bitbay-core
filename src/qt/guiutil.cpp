#include <QApplication>

#include "guiutil.h"

#include "bitcoinaddressvalidator.h"
#include "bitcoinunits.h"
#include "walletmodel.h"

#include "init.h"
#include "util.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDoubleValidator>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMouseEvent>
#include <QSettings>
#include <QStyleFactory>
#include <QTableView>
#include <QTextDocument>  // For Qt::escape
#include <QThread>
#include <QTreeView>
#include <QUrl>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

#ifdef WIN32
#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT 0x0501
#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#define WIN32_LEAN_AND_MEAN 1
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "shellapi.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

namespace GUIUtil {

int nFontScale = 100;

QString dateTimeStr(const QDateTime& date) {
	return date.date().toString(Qt::SystemLocaleShortDate) + QString(" ") + date.toString("hh:mm");
}

QString dateTimeStr(qint64 nTime) {
	return dateTimeStr(QDateTime::fromTime_t((qint32)nTime));
}

QFont bitcoinAddressFont() {
	QFont font("Roboto Mono");
	font.setStyleHint(QFont::Monospace);
#ifdef Q_OS_UNIX
#ifndef Q_OS_MAC
	{
		QFontMetricsF fm(font);
		qreal         fmh = fm.height();
		if (fmh > 12.) {
			qreal pt = 11. * 12. / fmh;
			font     = QFont("Roboto Mono", pt);
		}
	}
#endif
#endif
	if (nFontScale != 100) {  // nFontScale
		qreal pti = font.pointSizeF();
		qreal pt  = pti * qreal(nFontScale) / 100.;
		font.setPointSizeF(pt);
	}
	return font;
}

QFont appFont() {
#ifdef Q_OS_MAC
	QFont font("Roboto", 15);
#else
	QFont font("Roboto", 11);
#ifdef Q_OS_UNIX
	{
		QFontMetricsF fm(font);
		qreal         fmh = fm.height();
		if (fmh > 12.) {
			qreal pt = 11. * 12. / fmh;
			font     = QFont("Roboto", pt);
		}
	}
#endif
#endif
	if (nFontScale != 100) {  // nFontScale
		qreal pti = font.pointSizeF();
		qreal pt  = pti * qreal(nFontScale) / 100.;
		font.setPointSizeF(pt);
	}
	return font;
}

QFont tabFont() {
#ifdef Q_OS_MAC
	QFont font("Roboto Condensed", 15, QFont::Bold);
#else
	QFont font("Roboto Condensed", 11, QFont::Bold);
#ifdef Q_OS_UNIX
	{
		QFontMetricsF fm(font);
		qreal         fmh = fm.height();
		if (fmh > 12.) {
			qreal pt = 11. * 12. / fmh;
			font     = QFont("Roboto Condensed", pt, QFont::Bold);
		}
	}
#endif
#endif
	if (nFontScale != 100) {  // nFontScale
		qreal pti = font.pointSizeF();
		qreal pt  = pti * qreal(nFontScale) / 100.;
		font.setPointSizeF(pt);
	}
	return font;
}

QFont header1Font() {
#ifdef Q_OS_MAC
	QFont font("Roboto Black", 20, QFont::Bold);
#else
	QFont font("Roboto Black", 15, QFont::Bold);
#ifdef Q_OS_UNIX
	{
		QFontMetricsF fm(font);
		qreal         fmh = fm.height();
		if (fmh > 14.) {
			qreal pt = 15. * 12. / fmh;
			font     = QFont("Roboto Black", pt, QFont::Bold);
		}
	}
#endif
#endif
	if (nFontScale != 100) {  // nFontScale
		qreal pti = font.pointSizeF();
		qreal pt  = pti * qreal(nFontScale) / 100.;
		font.setPointSizeF(pt);
	}
	return font;
}

void setupAddressWidget(QLineEdit* widget, QWidget* parent) {
	widget->setMaxLength(BitcoinAddressValidator::MaxMixAddressLength);
	widget->setValidator(new BitcoinAddressValidator(parent));
	widget->setFont(bitcoinAddressFont());
}

void setupAmountWidget(QLineEdit* widget, QWidget* parent) {
	QDoubleValidator* amountValidator = new QDoubleValidator(parent);
	amountValidator->setDecimals(8);
	amountValidator->setBottom(0.0);
	widget->setValidator(amountValidator);
	widget->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
}

bool parseBitcoinURI(const QUrl& uri, SendCoinsRecipient* out) {
	// NovaCoin: check prefix
	if (uri.scheme() != QString("bitbay"))
		return false;

	SendCoinsRecipient rv;
	rv.address                            = uri.path();
	rv.amount                             = 0;
	QList<QPair<QString, QString> > items = uri.queryItems();
	for (QList<QPair<QString, QString> >::iterator i = items.begin(); i != items.end(); i++) {
		bool fShouldReturnFalse = false;
		if (i->first.startsWith("req-")) {
			i->first.remove(0, 4);
			fShouldReturnFalse = true;
		}

		if (i->first == "label") {
			rv.label           = i->second;
			fShouldReturnFalse = false;
		} else if (i->first == "amount") {
			if (!i->second.isEmpty()) {
				if (!BitcoinUnits::parse(BitcoinUnits::BTC, i->second, &rv.amount)) {
					return false;
				}
			}
			fShouldReturnFalse = false;
		}

		if (fShouldReturnFalse)
			return false;
	}
	if (out) {
		*out = rv;
	}
	return true;
}

bool parseBitcoinURI(QString uri, SendCoinsRecipient* out) {
	// Convert bitbay:// to bitbay:
	//
	//    Cannot handle this later, because bitcoin:// will cause Qt to see the part after // as
	//    host, which will lower-case it (and thus invalidate the address).
	if (uri.startsWith("bitbay://")) {
		uri.replace(0, 12, "bitbay:");
	}
	QUrl uriInstance(uri);
	return parseBitcoinURI(uriInstance, out);
}

QString HtmlEscape(const QString& str, bool fMultiLine) {
	QString escaped = Qt::escape(str);
	if (fMultiLine) {
		escaped = escaped.replace("\n", "<br>\n");
	}
	return escaped;
}

QString HtmlEscape(const std::string& str, bool fMultiLine) {
	return HtmlEscape(QString::fromStdString(str), fMultiLine);
}

void copyEntryData(QAbstractItemView* view, int column, int role) {
	if (!view || !view->selectionModel())
		return;
	QModelIndexList selection = view->selectionModel()->selectedRows(column);

	if (!selection.isEmpty()) {
		// Copy first item
		QApplication::clipboard()->setText(selection.at(0).data(role).toString());
	}
}

QString getSaveFileName(QWidget*       parent,
                        const QString& caption,
                        const QString& dir,
                        const QString& filter,
                        QString*       selectedSuffixOut) {
	QString selectedFilter;
	QString myDir;
	if (dir.isEmpty())  // Default to user documents location
	{
		myDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
	} else {
		myDir = dir;
	}
	QString result = QFileDialog::getSaveFileName(parent, caption, myDir, filter, &selectedFilter);

	/* Extract first suffix from filter pattern "Description (*.foo)" or "Description (*.foo *.bar
	 * ...) */
	QRegExp filter_re(".* \\(\\*\\.(.*)[ \\)]");
	QString selectedSuffix;
	if (filter_re.exactMatch(selectedFilter)) {
		selectedSuffix = filter_re.cap(1);
	}

	/* Add suffix if needed */
	QFileInfo info(result);
	if (!result.isEmpty()) {
		if (info.suffix().isEmpty() && !selectedSuffix.isEmpty()) {
			/* No suffix specified, add selected suffix */
			if (!result.endsWith("."))
				result.append(".");
			result.append(selectedSuffix);
		}
	}

	/* Return selected suffix if asked to */
	if (selectedSuffixOut) {
		*selectedSuffixOut = selectedSuffix;
	}
	return result;
}

Qt::ConnectionType blockingGUIThreadConnection() {
	if (QThread::currentThread() != QCoreApplication::instance()->thread()) {
		return Qt::BlockingQueuedConnection;
	} else {
		return Qt::DirectConnection;
	}
}

bool checkPoint(const QPoint& p, const QWidget* w) {
	QWidget* atW = qApp->widgetAt(w->mapToGlobal(p));
	if (!atW)
		return false;
	return atW->topLevelWidget() == w;
}

bool isObscured(QWidget* w) {
	return !(checkPoint(QPoint(0, 0), w) && checkPoint(QPoint(w->width() - 1, 0), w) &&
	         checkPoint(QPoint(0, w->height() - 1), w) &&
	         checkPoint(QPoint(w->width() - 1, w->height() - 1), w) &&
	         checkPoint(QPoint(w->width() / 2, w->height() / 2), w));
}

void openDebugLogfile() {
	boost::filesystem::path pathDebug = GetDataDir() / "debug.log";

	/* Open debug.log with the associated application */
	if (boost::filesystem::exists(pathDebug))
		QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(pathDebug.string())));
}

ToolTipToRichTextFilter::ToolTipToRichTextFilter(int size_threshold, QObject* parent)
    : QObject(parent), size_threshold(size_threshold) {}

bool ToolTipToRichTextFilter::eventFilter(QObject* obj, QEvent* evt) {
	if (evt->type() == QEvent::ToolTipChange) {
		QWidget* widget  = static_cast<QWidget*>(obj);
		QString  tooltip = widget->toolTip();
		if (tooltip.size() > size_threshold && !tooltip.startsWith("<qt>") &&
		    !Qt::mightBeRichText(tooltip)) {
			// Prefix <qt/> to make sure Qt detects this as rich text
			// Escape the current message as HTML and replace \n by <br>
			tooltip = "<qt>" + HtmlEscape(tooltip, true) + "<qt/>";
			widget->setToolTip(tooltip);
			return true;
		}
	}
	return QObject::eventFilter(obj, evt);
}

#ifdef WIN32
boost::filesystem::path static StartupShortcutPath() {
	return GetSpecialFolderPath(CSIDL_STARTUP) / "BitBay.lnk";
}

bool GetStartOnSystemStartup() {
	// check for Bitcoin.lnk
	return boost::filesystem::exists(StartupShortcutPath());
}

bool SetStartOnSystemStartup(bool fAutoStart) {
	// If the shortcut exists already, remove it for updating
	boost::filesystem::remove(StartupShortcutPath());

	if (fAutoStart) {
		CoInitialize(NULL);

		// Get a pointer to the IShellLink interface.
		IShellLink* psl = NULL;
		HRESULT hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink,
		                                reinterpret_cast<void**>(&psl));

		if (SUCCEEDED(hres)) {
			// Get the current executable path
			TCHAR pszExePath[MAX_PATH];
			GetModuleFileName(NULL, pszExePath, sizeof(pszExePath));

			TCHAR pszArgs[5] = TEXT("-min");

			// Set the path to the shortcut target
			psl->SetPath(pszExePath);
			PathRemoveFileSpec(pszExePath);
			psl->SetWorkingDirectory(pszExePath);
			psl->SetShowCmd(SW_SHOWMINNOACTIVE);
			psl->SetArguments(pszArgs);

			// Query IShellLink for the IPersistFile interface for
			// saving the shortcut in persistent storage.
			IPersistFile* ppf = NULL;
			hres = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&ppf));
			if (SUCCEEDED(hres)) {
				WCHAR pwsz[MAX_PATH];
				// Ensure that the string is ANSI.
				MultiByteToWideChar(CP_ACP, 0, StartupShortcutPath().string().c_str(), -1, pwsz,
				                    MAX_PATH);
				// Save the link by calling IPersistFile::Save.
				hres = ppf->Save(pwsz, TRUE);
				ppf->Release();
				psl->Release();
				CoUninitialize();
				return true;
			}
			psl->Release();
		}
		CoUninitialize();
		return false;
	}
	return true;
}

#elif defined(Q_OS_LINUX)

// Follow the Desktop Application Autostart Spec:
//  http://standards.freedesktop.org/autostart-spec/autostart-spec-latest.html

boost::filesystem::path static GetAutostartDir() {
	namespace fs = boost::filesystem;

	char* pszConfigHome = getenv("XDG_CONFIG_HOME");
	if (pszConfigHome)
		return fs::path(pszConfigHome) / "autostart";
	char* pszHome = getenv("HOME");
	if (pszHome)
		return fs::path(pszHome) / ".config" / "autostart";
	return fs::path();
}

boost::filesystem::path static GetAutostartFilePath() {
	return GetAutostartDir() / "bitbay.desktop";
}

bool GetStartOnSystemStartup() {
	boost::filesystem::ifstream optionFile(GetAutostartFilePath());
	if (!optionFile.good())
		return false;
	// Scan through file for "Hidden=true":
	std::string line;
	while (!optionFile.eof()) {
		getline(optionFile, line);
		if (line.find("Hidden") != std::string::npos && line.find("true") != std::string::npos)
			return false;
	}
	optionFile.close();

	return true;
}

bool SetStartOnSystemStartup(bool fAutoStart) {
	if (!fAutoStart)
		boost::filesystem::remove(GetAutostartFilePath());
	else {
		char pszExePath[MAX_PATH + 1];
		memset(pszExePath, 0, sizeof(pszExePath));
		if (readlink("/proc/self/exe", pszExePath, sizeof(pszExePath) - 1) == -1)
			return false;

		boost::filesystem::create_directories(GetAutostartDir());

		boost::filesystem::ofstream optionFile(GetAutostartFilePath(),
		                                       std::ios_base::out | std::ios_base::trunc);
		if (!optionFile.good())
			return false;
		// Write a bitcoin.desktop file to the autostart directory:
		optionFile << "[Desktop Entry]\n";
		optionFile << "Type=Application\n";
		optionFile << "Name=BitBay\n";
		optionFile << "Exec=" << pszExePath << " -min\n";
		optionFile << "Terminal=false\n";
		optionFile << "Hidden=false\n";
		optionFile.close();
	}
	return true;
}
#else

// TODO: OSX startup stuff; see:
// https://developer.apple.com/library/mac/#documentation/MacOSX/Conceptual/BPSystemStartup/Articles/CustomLogin.html

bool GetStartOnSystemStartup() {
	return false;
}
bool SetStartOnSystemStartup(bool fAutoStart) {
	return false;
}

#endif

HelpMessageBox::HelpMessageBox(QWidget* parent) : QMessageBox(parent) {
	header = tr("BitBay-Qt") + " " + tr("version") + " " +
	         QString::fromStdString(FormatFullVersion()) + "\n\n" + tr("Usage:") + "\n" +
	         "  bitbay-qt [" + tr("command-line options") + "]                     " + "\n";

	coreOptions = QString::fromStdString(HelpMessage());

	uiOptions = tr("UI options") + ":\n" + "  -lang=<lang>           " +
	            tr("Set language, for example \"de_DE\" (default: system locale)") + "\n" +
	            "  -min                   " + tr("Start minimized") + "\n" +
	            "  -splash                " + tr("Show splash screen on startup (default: 1)") +
	            "\n";

	setWindowTitle(tr("BitBay-Qt"));
	setTextFormat(Qt::PlainText);
	// setMinimumWidth is ignored for QMessageBox so put in non-breaking spaces to make it wider.
	setText(header + QString(QChar(0x2003)).repeated(50));
	setDetailedText(coreOptions + "\n" + uiOptions);
}

void HelpMessageBox::printToConsole() {
	// On other operating systems, the expected action is to print the message to the console.
	QString strUsage = header + "\n" + coreOptions + "\n" + uiOptions;
	fprintf(stdout, "%s", strUsage.toStdString().c_str());
}

void HelpMessageBox::showOrPrint() {
#if defined(WIN32)
	// On Windows, show a message box, as there is no stderr/stdout in windowed applications
	exec();
#else
	// On other operating systems, print help text to console
	printToConsole();
#endif
}

void SetBitBayThemeQSS(QApplication& app) {
#ifdef Q_OS_UNIX
	app.setAttribute(Qt::AA_Use96Dpi);
	app.setAttribute(Qt::AA_UseHighDpiPixmaps);
	app.setAttribute(Qt::AA_DisableHighDpiScaling);
#endif
#ifdef Q_OS_MAC
	app.setAttribute(Qt::AA_UseHighDpiPixmaps);
	app.setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION >= 0x051400
	app.setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif
#ifdef Q_OS_WIN
	app.setAttribute(Qt::AA_UseHighDpiPixmaps);
	app.setAttribute(Qt::AA_EnableHighDpiScaling);
#if QT_VERSION >= 0x051400
	app.setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif
#endif

	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Black.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-BlackItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Bold.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-BoldItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Italic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Light.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-LightItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Medium.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-MediumItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Regular.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-Thin.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/Roboto-ThinItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-Bold.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-BoldItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-Italic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-Light.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-LightItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoCondensed-Regular.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoMono-BoldItalic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoMono-Bold.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoMono-Italic.ttf");
	QFontDatabase::addApplicationFont(":/fonts/res/fonts/RobotoMono-Regular.ttf");

	// load fontscale
	QSettings settings;
	nFontScale = settings.value("nFontScale", 100).toInt();
	if (nFontScale < 50 || nFontScale > 200) {
		nFontScale = 100;
	}

	QFont font = appFont();
	QApplication::setFont(font);
	//    qDebug() << font.toString();
	//    QFontDatabase database;
	//    for (QString f : database.families()) {
	//        cout << f.toStdString() << endl;
	//    }

	app.setStyle(QStyleFactory::create("fusion"));

	app.setStyleSheet(
	    R"(
        QWidget { background: rgb(221,222,237); }
        QLineEdit {
            background: rgb(255,255,255);
            color: rgb(0,0,0);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
            min-height: 25px;
        }
        QDoubleSpinBox {
            padding-right: 15px;
            background: rgb(255,255,255);
            color: rgb(0,0,0);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
            min-height: 25px;
        }
        QComboBox {
            padding-right: 15px;
            background: rgb(204,203,227);
            color: rgb(90,80,165);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
            min-height: 25px;
            padding-left: 10px;
        }
        QComboBox::drop-down {
            border: 0px solid #6c6c6c;
        }
        QComboBox::down-arrow {
            image: url(:/icons/spinbox_down_arrow);
            width: 8px;
            height: 5px;
        }
        QToolButton {
            border: 1px solid rgb(255,255,255);
            border-right: 1px solid rgb(135,135,135);
            border-bottom: 1px solid rgb(135,135,135);
            min-height: 25px;
            min-width: 25px;
        }
        QToolButton:pressed {
            border: 1px solid rgb(135,135,135);
            border-right: 1px solid rgb(255,255,255);
            border-bottom: 1px solid rgb(255,255,255);
        }
        QPushButton {
            background: rgb(255,215,31);
            color: rgb(59,65,145);
            border: 1px solid rgb(255,255,255);
            border-right: 2px solid rgb(255,255,255);
            border-right-color: qlineargradient(
                x1: 0, y1: 0,
                x2: 1, y2: 0,
                stop: 0      #ffffff,
                stop: 0.5    #ffffff,
                stop: 0.5001 #b6bdca,
                stop: 1      #b6bdca
            );
            border-bottom: 2px solid rgb(255,255,255);
            border-bottom-color: qlineargradient(
                x1: 0, y1: 0,
                x2: 0, y2: 1,
                stop: 0      #ffffff,
                stop: 0.5    #ffffff,
                stop: 0.5001 #b6bdca,
                stop: 1      #b6bdca
            );
            min-height: 25px;
            min-width: 120px;
            margin-top: 2px;
            margin-left: 2px;
            margin-right: 2px;
            margin-bottom: 2px;
            padding-left: 15px;
            padding-right: 15px;
        }
        QPushButton:hover {
            background: orange;
        }
        QPushButton:disabled {
            background: rgb(226,226,226);
            color: rgb(206,206,206);
        }
        QPushButton:pressed {
            margin-top: 4px;
            margin-left: 4px;
            margin-right: 0px;
            margin-bottom: 0px;
        }
        QLineEdit {
			padding-left: 6px;
			padding-right: 6px;
        }
        QHeaderView::section {
            background-color: rgb(71,58,148);
            color: white;
            padding-left: 4px;
            border: 0px solid #6c6c6c;
            border-right: 1px solid #6c6c6c;
            min-height: 25px;
            text-align: center;
        }
        QPlainTextEdit {
            background-color: rgb(255,255,255);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
        }
        QTableView {
            background-color: rgb(255,255,255);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
        }
        QTableView::item:selected {
            color: rgb(0,0,0);
            background-color: rgb(237,238,246);
        }
        QTableView::item:selected:active {
            color: rgb(0,0,0);
            border: 0px solid #6c6c6c;
            background-color: rgb(227,228,236);
        }
        QTreeView {
            background-color: rgb(255,255,255);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
        }
        QTreeView::item:selected {
            color: rgb(0,0,0);
            background-color: rgb(237,238,246);
        }
        QTreeView::item:selected:active {
            color: rgb(0,0,0);
            border: 0px solid #6c6c6c;
            background-color: rgb(227,228,236);
        }
        QTabBar::tab:selected {
            color: rgb(71,58,148);
        }
        QDoubleSpinBox {
            padding-left: 10px;
            padding-right: 10px;
        }
        QDoubleSpinBox::up-button {
            border: 0px solid #6c6c6c;
        }
        QDoubleSpinBox::down-button {
            border: 0px solid #6c6c6c;
        }
        QDoubleSpinBox::up-arrow {
            image: url(:/icons/spinbox_up_arrow);
            width: 8px;
            height: 5px;
        }
        QDoubleSpinBox::down-arrow {
            image: url(:/icons/spinbox_down_arrow);
            width: 8px;
            height: 5px;
        }
        QMenu { 
            background: rgb(255,255,255); 
            selection-color: rgb(204,203,227);
            border-color: rgb(135,135,135);
            border-width: 1.2px;
            border-style: solid;
        }
        QMenu::item:selected {
            color: rgb(0,0,0);
            background: rgb(204,203,227);
        }
        QBalloonTip {
            min-width: 16px;
            min-height: 16px;
            padding-left: 0px;
            padding-right: 0px;
        }

    )");
}

void SetBitBayFonts(QWidget* w) {
	QFont font = appFont();
	for (auto l : w->findChildren<QLabel*>()) {
		l->setFont(font);
	}
	for (auto tv : w->findChildren<QTableView*>()) {
		tv->horizontalHeader()->setFont(font);
		tv->verticalHeader()->setFont(font);
		tv->setFont(font);
	}
	for (auto tv : w->findChildren<QTreeView*>()) {
		tv->header()->setFont(font);
		tv->setFont(font);
	}
	for (auto lv : w->findChildren<QListView*>()) {
		lv->setFont(font);
	}
	for (auto b : w->findChildren<QAbstractButton*>()) {
		b->setFont(font);
	}
}

void ClickableLabel::mouseReleaseEvent(QMouseEvent* me) {
	me->accept();
	emit clicked();
}

}  // namespace GUIUtil
