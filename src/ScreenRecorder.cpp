#include "ScreenRecorder.h"
#include "RecordingCompleteDialog.h"

#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QDateTime>
#include <QStandardPaths>
#include <QDebug>
#include <QUrl>
#include <QFile>

#include <QScreenCapture>
#include <QMediaCaptureSession>
#include <QMediaRecorder>
#include <QMediaFormat>
#include <QAudioInput>
#include <QAudioDevice>
#include <QMediaDevices>

ScreenRecorder::ScreenRecorder(QObject *parent)
    : QObject(parent)
{
    m_outputPath = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                   + "/ScreenMasterPro";
    QDir().mkpath(m_outputPath);

    m_tickTimer = new QTimer(this);
    m_tickTimer->setInterval(1000);
    connect(m_tickTimer, &QTimer::timeout, this, &ScreenRecorder::onTick);

    setupSession();
}

ScreenRecorder::~ScreenRecorder() {
    if (m_recording) stopRecording();
}

void ScreenRecorder::setupSession() {
    // Screen capture source
    m_screenCapture = new QScreenCapture(this);
    m_screenCapture->setScreen(QGuiApplication::primaryScreen());

    // Capture session — connects source to recorder
    m_session = new QMediaCaptureSession(this);
    m_session->setScreenCapture(m_screenCapture);

    // Recorder
    m_recorder = new QMediaRecorder(this);
    m_session->setRecorder(m_recorder);

    // Connect error signal
    connect(m_recorder, &QMediaRecorder::errorOccurred,
            this, &ScreenRecorder::onRecorderError);

    // Connect actual recording state changes
    connect(m_recorder, &QMediaRecorder::recorderStateChanged,
            this, [this](QMediaRecorder::RecorderState state) {
        if (state == QMediaRecorder::StoppedState && m_recording) {
            m_recording = false;
            m_paused    = false;
            m_tickTimer->stop();
            emit recordingStopped(m_outputFile);

            // Show preview dialog
            if (QFile::exists(m_outputFile)) {
                RecordingCompleteDialog *dlg = new RecordingCompleteDialog(m_outputFile);
                dlg->setAttribute(Qt::WA_DeleteOnClose);
                dlg->show();
            }
        }
    });
}

void ScreenRecorder::startRecording() {
    if (m_recording) return;

    m_outputFile = buildOutputPath();
    m_elapsed    = 0;
    m_recording  = true;
    m_paused     = false;

    // ── Format: MP4 + H264 (with fallback) ───────────────────────
    QMediaFormat fmt;
    fmt.setFileFormat(QMediaFormat::MPEG4);

    // Try H264 first, fall back to whatever is available
    auto supportedCodecs = QMediaFormat(QMediaFormat::MPEG4).supportedVideoCodecs(QMediaFormat::Encode);
    if (supportedCodecs.contains(QMediaFormat::VideoCodec::H264))
        fmt.setVideoCodec(QMediaFormat::VideoCodec::H264);
    else if (!supportedCodecs.isEmpty())
        fmt.setVideoCodec(supportedCodecs.first());

    if (m_recordAudio)
        fmt.setAudioCodec(QMediaFormat::AudioCodec::AAC);
    m_recorder->setMediaFormat(fmt);

    // ── Quality ───────────────────────────────────────────────────
    if (m_quality == "Low")
        m_recorder->setQuality(QMediaRecorder::LowQuality);
    else if (m_quality == "Medium")
        m_recorder->setQuality(QMediaRecorder::NormalQuality);
    else if (m_quality == "Ultra")
        m_recorder->setQuality(QMediaRecorder::VeryHighQuality);
    else
        m_recorder->setQuality(QMediaRecorder::HighQuality);

    // ── FPS ───────────────────────────────────────────────────────
    m_recorder->setVideoFrameRate(m_fps);

    // ── Bit rate by quality ───────────────────────────────────────
    int videoBitRate = 8000000; // 8 Mbps default (High)
    if (m_quality == "Low")    videoBitRate = 2000000;
    else if (m_quality == "Medium") videoBitRate = 4000000;
    else if (m_quality == "Ultra")  videoBitRate = 20000000;
    m_recorder->setVideoBitRate(videoBitRate);

    // ── Audio input ───────────────────────────────────────────────
    if (m_recordAudio) {
        m_audioInput = new QAudioInput(this);
        m_session->setAudioInput(m_audioInput);
    } else {
        m_session->setAudioInput(nullptr);
    }

    // ── Output location ───────────────────────────────────────────
    m_recorder->setOutputLocation(QUrl::fromLocalFile(m_outputFile));

    // ── Start ─────────────────────────────────────────────────────
    m_screenCapture->start();
    m_recorder->record();
    m_tickTimer->start();

    emit recordingStarted();
    qDebug() << "Recording started:" << m_outputFile
             << "| FPS:" << m_fps
             << "| Quality:" << m_quality
             << "| Audio:" << m_recordAudio;
}

void ScreenRecorder::stopRecording() {
    if (!m_recording) return;
    m_recorder->stop();
    m_screenCapture->stop();
    m_tickTimer->stop();
    m_recording = false;
    m_paused    = false;
    m_elapsed   = 0;

    if (m_audioInput) {
        delete m_audioInput;
        m_audioInput = nullptr;
    }

    emit recordingStopped(m_outputFile);
}

void ScreenRecorder::pauseRecording() {
    if (!m_recording || m_paused) return;
    m_recorder->pause();
    m_tickTimer->stop();
    m_paused = true;
    emit recordingPaused();
}

void ScreenRecorder::resumeRecording() {
    if (!m_recording || !m_paused) return;
    m_recorder->record();
    m_tickTimer->start();
    m_paused = false;
    emit recordingResumed();
}

void ScreenRecorder::onTick() {
    if (!m_paused) {
        m_elapsed++;
        emit recordingTick(m_elapsed);
    }
}

void ScreenRecorder::onRecorderError() {
    QString err = m_recorder->errorString();
    qWarning() << "Recording error:" << err;
    emit recordingError(err);
    if (m_recording) stopRecording();
}

void ScreenRecorder::setFullScreen() {
    m_screenCapture->setScreen(QGuiApplication::primaryScreen());
}

void ScreenRecorder::selectRegion() {
    // Region recording: capture full screen (region crop needs custom pipeline)
    m_screenCapture->setScreen(QGuiApplication::primaryScreen());
}

void ScreenRecorder::selectWindow() {
    // Window recording: set capture to active window
    // QScreenCapture doesn't support window directly in Qt 6.6
    // Full screen is the fallback
    m_screenCapture->setScreen(QGuiApplication::primaryScreen());
}

void ScreenRecorder::setFps(int fps) {
    m_fps = qBound(1, fps, 60);
}

void ScreenRecorder::setQuality(const QString &quality) {
    m_quality = quality;
}

void ScreenRecorder::setRecordCursor(bool v) {
    Q_UNUSED(v); // QScreenCapture always includes cursor on Windows
}

void ScreenRecorder::setRecordAudio(bool v) {
    m_recordAudio = v;
}

void ScreenRecorder::setOutputPath(const QString &path) {
    m_outputPath = path;
    QDir().mkpath(path);
}

bool ScreenRecorder::isRecording() const { return m_recording; }
bool ScreenRecorder::isPaused()    const { return m_paused; }

QString ScreenRecorder::buildOutputPath() {
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss");
    return m_outputPath + "/Recording_" + timestamp + ".mp4";
}
