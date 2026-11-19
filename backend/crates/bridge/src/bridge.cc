#include "BridgeObserver.h"
// The umbrella header for this crate. `ffi.rs.h` is NOT produced on a clean
// build, even though it can linger in an incremental target/ directory.
#include "audioticon-bridge/src/lib.rs.h"

namespace audioticon::bridge {

void BridgeObserver::on_progress(RenderStatusDto status) const {
    (void)status;
}

} // namespace audioticon::bridge
