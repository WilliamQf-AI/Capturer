#include "recording-menu.h"

#include "probe/graphics.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QScreen>
#include <QTime>

#ifdef _WIN32
#include "libcap/win-wgc/win-wgc.h"
#endif

RecordingMenu::RecordingMenu(bool mm, bool sm, uint8_t buttons, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::ToolTip)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);

    // frameless: background & border
    auto backgroud_layout = new QHBoxLayout(this);
    backgroud_layout->setSpacing(0);
    backgroud_layout->setContentsMargins({});

    auto background = new QWidget(this);
    background->setObjectName("recording-menu");
    backgroud_layout->addWidget(background);

    auto layout = new QHBoxLayout(background);
    layout->setSpacing(0);
    layout->setContentsMargins({});

    if (buttons & RecordingMenu::AUDIO) {
        // microphone button
        mic_btn_ = new QCheckBox();
        mic_btn_->setChecked(mm);
        mic_btn_->setObjectName("mic-btn");
        connect(mic_btn_, &QCheckBox::clicked, [this](bool checked) { emit muted(1, checked); });
        layout->addWidget(mic_btn_);

        // speaker button
        speaker_btn_ = new QCheckBox();
        speaker_btn_->setChecked(sm);
        speaker_btn_->setObjectName("speaker-btn");
        connect(speaker_btn_, &QCheckBox::clicked, [this](bool checked) { emit muted(2, checked); });
        layout->addWidget(speaker_btn_);
    }

    if (buttons & RecordingMenu::CAMERA) {
        camera_btn_ = new QCheckBox();
        camera_btn_->setChecked(false);
        camera_btn_->setObjectName("camera-btn");
        connect(camera_btn_, &QCheckBox::clicked, [this](bool checked) { emit opened(checked); });
        layout->addWidget(camera_btn_);
    }

    // time
    time_label_ = new QLabel("--:--:--");
    time_label_->setObjectName("time");
    time_label_->setAlignment(Qt::AlignCenter);
    layout->addWidget(time_label_);

    // pause button
    pause_btn_ = new QCheckBox();
    pause_btn_->setObjectName("pause-btn");
    connect(pause_btn_, &QCheckBox::clicked,
            [this](bool checked) { checked ? emit paused() : emit resumed(); });
    layout->addWidget(pause_btn_);

    // close / stop button
    close_btn_ = new QCheckBox();
    close_btn_->setObjectName("stop-btn");
    connect(close_btn_, &QCheckBox::clicked, [this]() {
        emit stopped();
        close();
    });
    layout->addWidget(close_btn_);

#ifdef _WIN32
    // exclude the recording menu
    wgc::exclude(reinterpret_cast<HWND>(winId()));
#endif
}

// in ms
void RecordingMenu::time(int64_t time)
{
    time_label_->setText(QTime(0, 0, 0).addMSecs(time).toString("hh:mm:ss"));
}

void RecordingMenu::start()
{
    time_label_->setText("00:00:00");
    if (pause_btn_) pause_btn_->setChecked(false);

    emit started();

    show();
    // global position, primary display monitor
    move(probe::graphics::displays()[0].geometry.right() - width() - 12, 100);
}

void RecordingMenu::mousePressEvent(QMouseEvent *event)
{
    begin_pos_ = event->globalPos();
    moving_    = true;
}

void RecordingMenu::mouseMoveEvent(QMouseEvent *event)
{
    if (moving_) {
        move(event->globalPos() - begin_pos_ + pos());
        begin_pos_ = event->globalPos();
    }
}

void RecordingMenu::mouseReleaseEvent(QMouseEvent *) { moving_ = false; }

void RecordingMenu::mute(int type, bool muted)
{
    if (type == 0 && mic_btn_) {
        mic_btn_->setChecked(muted);
    }
    else if (speaker_btn_) {
        speaker_btn_->setChecked(muted);
    }
}

void RecordingMenu::camera_checked(bool v)
{
    if (camera_btn_) camera_btn_->setChecked(v);
}

void RecordingMenu::disable_cam(bool v)
{
    if (camera_btn_) {
        camera_btn_->setDisabled(v);
    }
}

void RecordingMenu::disable_mic(bool v)
{
    if (mic_btn_) {
        mic_btn_->setDisabled(v);
    }
}

void RecordingMenu::disable_speaker(bool v)
{
    if (speaker_btn_) {
        speaker_btn_->setDisabled(v);
    }
}