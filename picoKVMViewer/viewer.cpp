#include "viewer.h"
#include "ui_viewer.h"

#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>

#include <QtWidgets>


Viewer::Viewer() : ui(new Ui::Viewer)
{
    ui->setupUi(this);

    QActionGroup *serialPortsGroup = new QActionGroup(this);
    serialPortsGroup->setExclusive(true);

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QAction *serialPortAction = new QAction(info.portName(), serialPortsGroup);
        serialPortAction->setCheckable(true);
        ui->menuSettings->addAction(serialPortAction);

        qDebug() << "Serial Port " << info.portName();
    }

    connect(serialPortsGroup, &QActionGroup::triggered, this, &Viewer::updateSerialPort);


    QActionGroup *videoDevicesGroup = new QActionGroup(this);
    serialPortsGroup->setExclusive(true);

#ifdef Q_OS_LINUX
    m_videoDevice = "/dev/video0";
    QDir dir("/dev", "video*");
    dir.setFilter(QDir::System);
    QFileInfoList files = dir.entryInfoList();
    for (QFileInfo info : files) {
        QAction *videoDeviceAction = new QAction(info.absoluteFilePath(), videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        if(videoDeviceAction->text() == m_videoDevice) {
            videoDeviceAction->setChecked(true);
        }
        ui->menuSettings->addAction(videoDeviceAction);

        qDebug() << "Video Device: " << info.absoluteFilePath();
    }
#endif

#ifdef Q_OS_WINDOWS
    //TODO find actual camera device paths
    m_videoDevice = "0";
    QStringList deviceIds = { "0", "1", "2", "3" };
    for (QString deviceId : deviceIds) {
        QAction *videoDeviceAction = new QAction(deviceId, videoDevicesGroup);
        videoDeviceAction->setCheckable(true);
        if(videoDeviceAction->text() == m_videoDevice) {
            videoDeviceAction->setChecked(true);
        }
        ui->menuSettings->addAction(videoDeviceAction);

        qDebug() << "Video Device: " << deviceId;
    }
#endif

    connect(videoDevicesGroup, &QActionGroup::triggered, this, &Viewer::updateVideoDevice);

    buildPipelines();

    m_player = new QMediaPlayer;
    m_player->setVideoOutput(ui->videoview);
    connect(m_player, QOverload<QMediaPlayer::Error>::of(&QMediaPlayer::error), this, &Viewer::mediaPlayerError);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, &Viewer::mediaStatusChanged);
    tryNextPipeline();
}

void Viewer::buildPipelines()
{
    m_pipelines.clear();
    if (m_videoDevice.isEmpty()) {
        return;
    }

    QString sink="xvimagesink name=\"qtvideosink\"";
    QString format="width=1920,height=1080";
#ifdef Q_OS_LINUX
    m_pipelines.append(QString("v4l2src device=%1 ! video/x-raw,%2 ! %3").arg(m_videoDevice, format, sink));
    m_pipelines.append(QString("v4l2src device=%1 ! video/x-raw,%2 ! videoconvert ! %3").arg(m_videoDevice, format, sink));
    m_pipelines.append(QString("v4l2src device=%1 ! image/jpeg,%2 ! jpegdec ! %3").arg(m_videoDevice, format, sink));
#endif

#ifdef Q_OS_WIN
    m_pipelines.append(QString("ksvideosrc device-index=%1 ! image/jpeg,%2 ! jpegdec ! %3").arg(m_videoDevice, format, sink));
#endif

    m_nextPipelineIdx = 0;
}

void Viewer::tryNextPipeline()
{
    if (m_nextPipelineIdx < 0) {
        return;
    }

    if (m_nextPipelineIdx >= m_pipelines.count()) {
        m_nextPipelineIdx = -1;
        QMessageBox::warning(this, tr("Image Capture Error"), "Failed to find a suitable video device");
        return;
    }
    QString pipeline = m_pipelines.at(m_nextPipelineIdx++);
    qDebug() << "Trying pipeline: " << pipeline;
    m_player->setMedia(QUrl("gst-pipeline: "+pipeline));
    m_player->play();
}

void Viewer::mediaPlayerError(QMediaPlayer::Error error) {
    qDebug() << "Error: " << error;
    if(error == QMediaPlayer::ResourceError) {
        tryNextPipeline();
    }
}

void Viewer::mediaStatusChanged(QMediaPlayer::MediaStatus status) {
    qDebug() << "Status: " << status;
    if(status == QMediaPlayer::InvalidMedia) {
        tryNextPipeline();
    }
}

void Viewer::updateSerialPort(QAction *action) {
    ui->videoview->setSerialPort(action->text());
    action->setChecked(true);
}

void Viewer::updateVideoDevice(QAction *action)
{
    m_videoDevice = action->text();
    buildPipelines();
    tryNextPipeline();
}


