#include "Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace audioticon {
namespace paths {
namespace {

QString ensure(const QString &path) {
    QDir().mkpath(path);
    return path;
}

} // namespace

QString dataDir() {
    QString base = qEnvironmentVariable("XDG_DATA_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.local/share");
    return ensure(base + QStringLiteral("/audioticon"));
}

QString cacheDir() {
    QString base = qEnvironmentVariable("XDG_CACHE_HOME");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/.cache");
    return ensure(base + QStringLiteral("/audioticon"));
}

QString voicesDir() { return ensure(dataDir() + QStringLiteral("/voices")); }

QString outputsDir() { return ensure(dataDir() + QStringLiteral("/outputs")); }

} // namespace paths
} // namespace audioticon
