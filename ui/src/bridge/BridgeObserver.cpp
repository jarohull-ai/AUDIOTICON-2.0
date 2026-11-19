#include "BridgeObserver.h"
#include "StudioBridge.h"
#include <QMetaObject>

BridgeObserver::BridgeObserver(StudioBridge *target, QObject *parent)
    : QObject(parent), m_target(target) {
}

void BridgeObserver::notifyProgress(int step, int total, double percent, const QString &status) {
    if (!m_target)
        return;

    // Cross-thread safe invocation to Qt main event loop
    QMetaObject::invokeMethod(m_target.data(), [target = m_target, step, total, percent, status]() {
        if (target) {
            target->updateProgressFromWorker(step, total, percent, status);
        }
    }, Qt::QueuedConnection);
}

void BridgeObserver::notifyError(const QString &errorMessage) {
    if (!m_target)
        return;

    QMetaObject::invokeMethod(m_target.data(), [target = m_target, errorMessage]() {
        if (target) {
            target->setErrorFromWorker(errorMessage);
        }
    }, Qt::QueuedConnection);
}
