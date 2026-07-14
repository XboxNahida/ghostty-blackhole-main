#include "portal_capture.h"
#include "wayland_surface_export.h"
#include <QtTest>
#include <QSignalSpy>

class PortalCaptureTests : public QObject {
    Q_OBJECT
private slots:
    void tightAndPaddedFrames() {
        QByteArray tight(16, '\0'); for (int i = 0; i < tight.size(); ++i) tight[i] = char(i);
        QByteArray output;
        QVERIFY(PortalCapture::copyCompleteBgraFrame(
            reinterpret_cast<const uchar *>(tight.constData()), tight.size(), 8, 2, 2, output));
        QCOMPARE(output, tight);
        QByteArray padded(24, char(99)); memcpy(padded.data(), tight.data(), 8); memcpy(padded.data()+12, tight.data()+8, 8);
        QVERIFY(PortalCapture::copyCompleteBgraFrame(
            reinterpret_cast<const uchar *>(padded.constData()), padded.size(), 12, 2, 2, output));
        QCOMPARE(output, tight);
    }
    void rejectsTruncatedAndInvalidFrames() {
        QByteArray bytes(15, '\0'), output("unchanged");
        QVERIFY(!PortalCapture::copyCompleteBgraFrame(reinterpret_cast<const uchar *>(bytes.constData()), bytes.size(), 8, 2, 2, output));
        QCOMPARE(output, QByteArray("unchanged"));
        QVERIFY(!PortalCapture::copyCompleteBgraFrame(nullptr, 0, 0, 0, 0, output));
        QVERIFY(!PortalCapture::copyCompleteBgraFrame(reinterpret_cast<const uchar *>(bytes.constData()), bytes.size(), 4, 2, 2, output));
    }
    void noNewFrameAndIdempotentStop() {
        PortalCapture capture; PortalCapture::Frame frame;
        QVERIFY(!capture.takeLatestFrame(frame));
        QSignalSpy stopped(&capture, &PortalCapture::captureStopped);
        capture.stopCapture(); capture.stopCapture();
        QCOMPARE(stopped.count(), 0);
    }
    void invalidStreamVariants() {
        PortalCapture::StreamInfo stream; QString error;
        QVERIFY(!PortalCapture::parseFirstStream({}, stream, &error)); QVERIFY(!error.isEmpty());
        QVERIFY(!PortalCapture::parseFirstStream(QVariantList{}, stream, &error));
    }
    void capabilityValidation() {
        QVERIFY(PortalCapture::capabilityError(0, 7).contains("AvailableSourceTypes=0"));
        QVERIFY(PortalCapture::capabilityError(7, 0).contains("AvailableCursorModes=0"));
        QVERIFY(!PortalCapture::capabilityError(7, 7).size());
    }
    void startCarriesParentIdentifier() {
        const QVariantMap options{{QStringLiteral("handle_token"), QStringLiteral("request")}};
        const QVariantList args = PortalCapture::startArguments(
            QStringLiteral("/org/freedesktop/portal/desktop/session/test/session"),
            QStringLiteral("wayland:opaque-handle"), options);
        QCOMPARE(args.size(), 3);
        QCOMPARE(args.at(1).toString(), QStringLiteral("wayland:opaque-handle"));
        QCOMPARE(args.at(2).toMap(), options);
    }
    void exportedHandleCallbackBoundary() {
        QCOMPARE(WaylandSurfaceExport::portalParentIdentifier("opaque-token"),
                 QStringLiteral("wayland:opaque-token"));
        QVERIFY(WaylandSurfaceExport::portalParentIdentifier(nullptr).isEmpty());
        QVERIFY(WaylandSurfaceExport::portalParentIdentifier("").isEmpty());
    }
};
QTEST_GUILESS_MAIN(PortalCaptureTests)
#include "portal_capture_tests.moc"
