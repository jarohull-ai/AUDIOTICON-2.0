#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class StudioBridge;

// Skeletal BridgeObserver designed for M3 (Mock backend) and ready for cxx::bridge in M4.
// Encapsulates progress notifications from the rendering engine to the Qt event loop.
class BridgeObserver : public QObject {
    Q_OBJECT

public:
    explicit BridgeObserver(StudioBridge *target, QObject *parent = nullptr);
    virtual ~BridgeObserver() = default;

    // Direct notification slot / method callable from background worker
    void notifyProgress(int step, int total, double percent, const QString &status);
    void notifyError(const QString &errorMessage);

signals:
    void progressDispatched(int step, int total, double percent, QString status);
    void errorDispatched(QString errorMessage);

private:
    QPointer<StudioBridge> m_target;
};
