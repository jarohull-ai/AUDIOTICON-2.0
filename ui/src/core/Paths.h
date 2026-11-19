#pragma once

#include <QString>

namespace audioticon {

// Everything the app writes lives under one XDG directory, so an uninstall is
// "remove the folder" and tests can redirect it with XDG_DATA_HOME.
namespace paths {

// $XDG_DATA_HOME/audioticon, or ~/.local/share/audioticon. Created on demand.
QString dataDir();
// $XDG_CACHE_HOME/audioticon (rendered segments, downloaded models).
QString cacheDir();
// <dataDir>/voices — installed Piper voices and their manifests.
QString voicesDir();
// <dataDir>/outputs — rendered recordings.
QString outputsDir();

} // namespace paths
} // namespace audioticon
