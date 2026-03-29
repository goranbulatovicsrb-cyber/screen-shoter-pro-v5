#pragma once
#include <QObject>
#include <QTimer>
#include <QRect>
#include <QString>
#include <QUrl>

class QScreenCapture;
class QMediaCaptureSession;
class QMediaRecorder;
class QAudioInput;
class QScreen;

class ScreenRecorder : public QObject {
    Q_OBJECT
public:
    explicit ScreenRecorder(QObject *parent = nullptr);
    ~ScreenRecorder();

    // Settings
    void setFps(int fps);
    void setQuality(const QString &quality);   // "Low","Medium","High","Ultra"
    void setRecordCursor(bool v);
    void setRecordAudio(bool v);
    void setOutputPath(const QString &path);

    bool isRecording() const;
    bool isPaused()    const;
    QString lastOutputFile() const { return m_outputFile; }

public slots:
    void startRecording();
    void stopRecording();
    void pauseRecording();
    void resumeRecording();
    void setFullScreen();
    void selectRegion();
    void selectWindow();

signals:
    void recordingStarted();
    void recordingStopped(const QString &filePath);
    void recordingPaused();
    void recordingResumed();
    void recordingTick(int seconds);
    void recordingError(const QString &msg);
    void frameCapture(const QPixmap &frame);

private slots:
    void onTick();
    void onRecorderError();

private:
    void setupSession();
    QString buildOutputPath();

    QScreenCapture       *m_screenCapture   = nullptr;
    QMediaCaptureSession *m_session         = nullptr;
    QMediaRecorder       *m_recorder        = nullptr;
    QAudioInput          *m_audioInput      = nullptr;

    QTimer   *m_tickTimer;
    int       m_fps         = 30;
    QString   m_quality     = "High";
    bool      m_recordAudio = false;
    bool      m_recording   = false;
    bool      m_paused      = false;
    int       m_elapsed     = 0;
    QString   m_outputPath;
    QString   m_outputFile;
};
