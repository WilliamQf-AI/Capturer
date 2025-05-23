#include "capturer.h"

#include "clipboard.h"
#include "color-window.h"
#include "config.h"
#include "image-window.h"
#include "libcap/devices.h"
#include "logging.h"
#include "message.h"
#include "pdf-viewer.h"
#include "settingdialog.h"
#include "video-player.h"

#include <probe/system.h>
#include <QApplication>
#include <QFileInfo>
#include <QKeyEvent>
#include <QScreen>
#include <QStyleFactory>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QUrl>

#define SET_HOTKEY(X, Y)                                                                                   \
    if (!X->setShortcut(Y, true)) {                                                                        \
        logw("Failed to register hotkey : {}", Y.toString().toStdString());                                \
        error += tr("Failed to register hotkey : <%1>\n").arg(Y.toString());                               \
    }

Capturer::Capturer(int& argc, char **argv)
    : QApplication(argc, argv)
{
    setWindowIcon(QIcon(":/icons/capturer"));

    setStyle(QStyleFactory::create("Fusion"));

    snip_hotkey_        = new QHotkey(this);
    repeat_snip_hotkey_ = new QHotkey(this);
    preview_hotkey_     = new QHotkey(this);
    toggle_hotkey_      = new QHotkey(this);
    video_hotkey_       = new QHotkey(this);
    gif_hotkey_         = new QHotkey(this);
    quicklook_hotkey_   = new QHotkey(this);
    transparent_input_  = new QHotkey(this);

    sniper_.reset(new ScreenShoter());

    connect(snip_hotkey_, &QHotkey::activated, sniper_.get(), &ScreenShoter::start);
    connect(repeat_snip_hotkey_, &QHotkey::activated, sniper_.get(), &ScreenShoter::repeat);
    connect(preview_hotkey_, &QHotkey::activated, this, &Capturer::PreviewClipboard);
    connect(toggle_hotkey_, &QHotkey::activated, this, &Capturer::TogglePreviews);
    connect(video_hotkey_, &QHotkey::activated, this, &Capturer::RecordVideo);
    connect(gif_hotkey_, &QHotkey::activated, this, &Capturer::RecordGIF);
    connect(quicklook_hotkey_, &QHotkey::activated, this, &Capturer::QuickLook);
    connect(transparent_input_, &QHotkey::activated, this, &Capturer::TransparentPreviewInput);
    connect(sniper_.get(), &ScreenShoter::pinData, this, &Capturer::PreviewMimeData);
}

void Capturer::RecordVideo()
{
    if (!recorder_) {
        recorder_ = new ScreenRecorder(ScreenRecorder::VIDEO);
        recorder_->setAttribute(Qt::WA_DeleteOnClose);
        recorder_->setStyle(config::recording::video::style);
    }
    recorder_->record();
}

void Capturer::RecordGIF()
{
    if (!gifcptr_) {
        gifcptr_ = new ScreenRecorder(ScreenRecorder::GIF);
        gifcptr_->setAttribute(Qt::WA_DeleteOnClose);
        gifcptr_->setStyle(config::recording::gif::style);
    }
    gifcptr_->record();
}

void Capturer::Init()
{
    clipboard::init();

    SystemTrayInit();

    UpdateHotkeys();

    UpdateScreenshotStyle();

    SetTheme(config::definite_theme());

    config::on_theme_changed = [this](const QString& theme) { SetTheme(theme); };

    config::set_autorun(config::autorun);

    //
    // microphones: default or null
    const auto asrc      = av::default_audio_source();
    config::devices::mic = asrc.value_or(av::device_t{}).id;

    // speakers: default or null
    const auto asink         = av::default_audio_sink();
    config::devices::speaker = asink.value_or(av::device_t{}).id;

    // cameras
    if (const auto& cameras = av::cameras(); !cameras.empty()) {
        config::devices::camera = cameras[0].id;
    }

    logi("initialized.");
}

void Capturer::SystemTrayInit()
{
    tray_.reset(new QSystemTrayIcon(QIcon(":/icons/capturer"), this));
    tray_->setToolTip("Capturer Settings");

    tray_menu_.reset(new Menu());
    tray_menu_->setObjectName("tray-menu");

    tray_snip_         = tray_menu_->addAction(tr("Screenshot"), sniper_.data(), &ScreenShoter::start);
    tray_record_video_ = tray_menu_->addAction(tr("Record Video"), this, &Capturer::RecordVideo);
    tray_record_gif_   = tray_menu_->addAction(tr("Record GIF"), this, &Capturer::RecordGIF);
    tray_menu_->addSeparator();
    tray_open_camera_ = tray_menu_->addAction(tr("Open Camera"), this, &Capturer::ToggleCamera);
    tray_menu_->addSeparator();
    tray_settings_ = tray_menu_->addAction(tr("Settings"), this, &Capturer::OpenSettingsDialog);
    tray_menu_->addSeparator();
    tray_exit_ = tray_menu_->addAction(tr("Quit"), qApp, &QCoreApplication::exit);

    tray_->setContextMenu(tray_menu_.data());
    tray_->show();

    connect(tray_.data(), &QSystemTrayIcon::activated, this, &Capturer::TrayActivated);
}

void Capturer::PreviewClipboard() { PreviewMimeData(clipboard::back(true)); }

void Capturer::PreviewMimeData(const std::shared_ptr<QMimeData>& mimedata)
{
    if (mimedata != nullptr) {

        FramelessWindow *preview{};

        if (mimedata->hasUrls() && mimedata->urls().size() == 1 && mimedata->urls()[0].isLocalFile() &&
            QFileInfo(mimedata->urls()[0].toLocalFile()).isFile() &&
            QString("gif;mp4;mkv;m2ts;mts;avi;wmv;ts;mov;flv;webm")
                .split(';')
                .contains(QFileInfo(mimedata->urls()[0].fileName()).suffix(), Qt::CaseInsensitive)) {

            preview = new VideoPlayer();
            dynamic_cast<VideoPlayer *>(preview)->open(mimedata->urls()[0].toLocalFile().toStdString());
            dynamic_cast<VideoPlayer *>(preview)->start();

            mimedata->setData(clipboard::MIME_TYPE_STATUS, "P");
        }
        else if (mimedata->hasUrls() && mimedata->urls().size() == 1 && mimedata->urls()[0].isLocalFile() &&
                 QFileInfo(mimedata->urls()[0].toLocalFile()).isFile() &&
                 QFileInfo(mimedata->urls()[0].fileName()).suffix().toLower() == "pdf") {

            preview = new PdfViewer(mimedata);
            mimedata->setData(clipboard::MIME_TYPE_STATUS, "P");
        }
        else if (mimedata->hasColor()) {
            preview = new ColorWindow(mimedata);
        }
        else {
            preview = new ImageWindow(mimedata);
        }

        preview->setAttribute(Qt::WA_DeleteOnClose);
        preview->show();
        if (!mimedata->hasFormat(clipboard::MIME_TYPE_POINT)) {
            const auto screen = screenAt(QCursor::pos()) ? screenAt(QCursor::pos()) : primaryScreen();
            const auto pos    = screen->geometry().center() - preview->rect().center();
            preview->move(std::max(screen->geometry().left(), pos.x()),
                          std::max(screen->geometry().top(), pos.y()));
        }
        previews_.emplace_back(preview);

        connect(preview, &FramelessWindow::closed, [this] {
            previews_.erase(std::ranges::remove_if(previews_, [](auto ptr) { return !ptr; }).begin(),
                            previews_.end());
        });
    }
}

void Capturer::ToggleCamera()
{
    // close
    if (camera_) {
        camera_->close();
        return;
    }

    // open
    camera_ = new CameraPlayer();

    if (av::cameras().empty()) {
        logw("No camera found");
        Message::error(tr("No camera found"));
        return;
    }

    if (camera_->open(config::devices::camera, { { "format", "webcam" }, { "filters", "hflip" } }) < 0) {
        loge("failed to open camera");
        return;
    }
    camera_->start();
}

void Capturer::OpenSettingsDialog()
{
    if (settings_window_) {
        settings_window_->activateWindow();
        settings_window_->raise();
        return;
    }

    settings_window_ = new SettingWindow();
    settings_window_->show();
    settings_window_->move(primaryScreen()->geometry().center() -
                           QPoint{ settings_window_->width() / 2, settings_window_->height() / 2 });
}

void Capturer::QuickLook()
{
#if _WIN32
    const auto active = probe::graphics::active_window();
    if (!active || active->pname != "explorer.exe") return;

    const auto file = (active->classname == "Progman" || active->classname == "WorkerW")
                          ? probe::graphics::desktop_focused()
                          : probe::graphics::explorer_focused(active->handle);

    if (file.empty()) return;

    auto data = std::make_shared<QMimeData>();
    data->setUrls({ QUrl::fromLocalFile(QString::fromUtf8(file.c_str())) });

    PreviewMimeData(data);
#endif
}

void Capturer::TogglePreviews()
{
    if (previews_.empty()) return;

    bool visible = false; // current state
    for (const auto& win : previews_) {
        if (win && win->isVisible()) {
            visible = true;
            break;
        }
    }

    for (const auto& win : previews_) {
        if (win) {
            win->setVisible(!visible);
        }
    }
}

void Capturer::TransparentPreviewInput()
{
    for (const auto& win : previews_) {
        if (win && win->geometry().contains(QCursor::pos())) {
            win->toggleTransparentInput();
#if _WIN32
            if (win->windowOpacity() == 1.0) win->setWindowOpacity(0.99);
#endif
        }
    }
}

void Capturer::UpdateHotkeys()
{
    QString error = "";
    // clang-format off
    SET_HOTKEY(snip_hotkey_,        config::hotkeys::screenshot);
    SET_HOTKEY(repeat_snip_hotkey_, config::hotkeys::repeat_last_screenshot);
    SET_HOTKEY(preview_hotkey_,     config::hotkeys::preview);
    SET_HOTKEY(toggle_hotkey_,      config::hotkeys::toggle_previews);
    SET_HOTKEY(video_hotkey_,       config::hotkeys::record_video);
    SET_HOTKEY(gif_hotkey_,         config::hotkeys::record_gif);
#if _WIN32
    SET_HOTKEY(quicklook_hotkey_,   config::hotkeys::quick_look);
#endif
    SET_HOTKEY(transparent_input_,  config::hotkeys::transparent_input);
    // clang-format on
    if (!error.isEmpty()) ShowMessage("Capturer", error, QSystemTrayIcon::Critical);
}

void Capturer::TrayActivated(const QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick && sniper_) sniper_->start();
}

void Capturer::ShowMessage(const QString& title, const QString& msg,
                           const QSystemTrayIcon::MessageIcon icon, const int msecs)
{
    tray_->showMessage(title, msg, icon, msecs);
}

void Capturer::SetTheme(const QString& theme)
{
    if (theme_ == theme || (theme != "dark" && theme != "light")) return;

    theme_ = theme;

    QIcon::setThemeName(theme);

    std::vector<QString> files{
        ":/stylesheets/capturer",       ":/stylesheets/capturer-" + theme,
        ":/stylesheets/menu",           ":/stylesheets/menu-" + theme,
        ":/stylesheets/settingswindow", ":/stylesheets/settingswindow-" + theme,
        ":/stylesheets/player",
    };

    QString style{};
    for (auto& qss : files) {

        QFile file(qss);
        file.open(QFile::ReadOnly);

        if (file.isOpen()) {
            style += file.readAll();
            file.close();
        }
    }
    setStyleSheet(style);

    // system tray menu icons
    QString color = (config::definite_theme() == "dark") ? "light" : "dark";
    if (probe::util::tolower(probe::system::name()).starts_with("ubuntu")) {
        color = (probe::system::theme() == probe::system::theme_t::dark) ? "light" : "dark";
    }

    tray_snip_->setIcon(QIcon(":/icons/screenshot-" + color));
    tray_record_video_->setIcon(QIcon(":/icons/capture-" + color));
    tray_record_gif_->setIcon(QIcon(":/icons/gif-" + color));
    tray_open_camera_->setIcon(QIcon(":/icons/camera-" + color));
    tray_settings_->setIcon(QIcon(":/icons/setting-" + color));
    tray_exit_->setIcon(QIcon(":/icons/exit-" + color));

    styleHints()->setColorScheme(theme_ == "dark" ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
}

void Capturer::UpdateScreenshotStyle()
{
    if (sniper_) sniper_->setStyle(config::snip::style);
}
