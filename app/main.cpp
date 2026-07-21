// TropoLink entry point: QML engine bootstrap, theme, i18n (Polish-first).

#include "ui/models/AppController.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QUrl>

namespace {
// GUI-subsystem apps have no console: mirror Qt messages into a diagnostics log.
void fileMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message) {
    static QFile* logFile = [] {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        auto* f = new QFile(dir + QStringLiteral("/tropolink.log"));
        f->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
        return f;
    }();
    if (logFile->isOpen()) {
        QTextStream out(logFile);
        const char* level = type == QtDebugMsg      ? "DBG"
                            : type == QtInfoMsg     ? "INF"
                            : type == QtWarningMsg  ? "WRN"
                            : type == QtCriticalMsg ? "CRT"
                                                    : "FTL";
        out << level << ' ' << message;
        if (context.file != nullptr) {
            out << "  (" << context.file << ':' << context.line << ')';
        }
        out << '\n';
        out.flush();
    }
}
} // namespace

int main(int argc, char* argv[]) {
    qInstallMessageHandler(fileMessageHandler);
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName("TropoLink");
    QGuiApplication::setApplicationName("TropoLink");
    QGuiApplication::setApplicationVersion(TROPOLINK_VERSION);
    QQuickStyle::setStyle("Basic"); // fully custom-drawn controls, dark theme default

    // Headless report mode: TropoLink --report out.pdf [--lang pl|en]
    // Loads the reference project, computes, writes the PDF and prints the content
    // hash — the reproducibility check of the acceptance walkthrough.
    const QStringList args = QGuiApplication::arguments();
    const int reportIdx = static_cast<int>(args.indexOf(QStringLiteral("--report")));
    if (reportIdx >= 0 && reportIdx + 1 < args.size()) {
        AppController controller;
        const int langIdx = static_cast<int>(args.indexOf(QStringLiteral("--lang")));
        // Transient: generating a report must not rewrite the operator's UI language.
        controller.setLanguageCodeTransient(langIdx >= 0 && langIdx + 1 < args.size() ? args.at(langIdx + 1)
                                                                                      : QStringLiteral("pl"));
        controller.loadReferenceProject();
        // Wait for the compute pipeline to settle, then render.
        QObject::connect(
            &controller, &AppController::reportGenerated, &app, [&controller](const QString& path) {
                fileMessageHandler(
                    QtInfoMsg, QMessageLogContext(),
                    QStringLiteral("report %1 sha256 %2").arg(path, controller.lastReportHash()));
                QCoreApplication::exit(0);
            });
        QTimer::singleShot(4000, &controller, [&controller, args, reportIdx] {
            controller.generateReport(QUrl::fromLocalFile(args.at(reportIdx + 1)), QImage());
        });
        QTimer::singleShot(60000, &app, [] { QCoreApplication::exit(2); });
        return QGuiApplication::exec();
    }

    // Polish-first: default to pl unless the operator has chosen otherwise.
    QSettings settings;
    const QString language = settings.value("ui/language", "pl").toString();
    auto* translator = new QTranslator(&app);
    if (language == "pl") {
        if (translator->load(QStringLiteral(":/i18n/tropolink_pl.qm"))) {
            QGuiApplication::installTranslator(translator);
        }
    }

    AppController controller;
    controller.setLanguageCode(language);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.loadFromModule("TropoLink", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }
    return QGuiApplication::exec();
}
