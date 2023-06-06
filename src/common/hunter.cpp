#include "hunter.h"

#include <algorithm>

using namespace probe::graphics;

namespace hunter
{
    static std::deque<prey_t> __preys{};
    static window_filter_t __scope{ window_filter_t::visible };

    prey_t prey_t::from(const QRect& rect)
    {
        return prey_t{
            .type = hunter::prey_type_t::rectangle,
            .geometry =
                probe::geometry_t{
                    .x      = rect.left(),
                    .y      = rect.top(),
                    .width  = static_cast<uint32_t>(rect.width()),
                    .height = static_cast<uint32_t>(rect.height()),
                },
        };
    }

    prey_t prey_t::from(const probe::geometry_t& geometry)
    {
        return prey_t{
            .type     = hunter::prey_type_t::rectangle,
            .geometry = geometry,
        };
    }

    prey_t prey_t::from(const probe::graphics::window_t& win)
    {
        return prey_t{
            .type     = !win.parent ? prey_type_t::window : prey_type_t::widget,
            .geometry = win.geometry,
            .handle   = win.handle,
            .name     = win.name,
            .codename = win.classname,
        };
    }

    prey_t prey_t::from(const probe::graphics::display_t& dis)
    {
        return prey_t{
            .type     = prey_type_t::display,
            .geometry = dis.geometry,
            .handle   = dis.handle,
            .name     = dis.name,
            .codename = dis.id,
        };
    }

    prey_t hunt(const QPoint& pos)
    {
        auto hunted = __preys.back();
#ifdef _WIN32
        if (any(__scope & window_filter_t::capturable)) {
            hunted = prey_t::from(display_contains(pos).value());
        }
#endif
        for (auto& prey : __preys) {
            if (prey.geometry.contains(pos.x(), pos.y())) {
                prey.geometry = hunted.geometry.intersected(prey.geometry);
                return prey;
            }
        }

        return hunted;
    }

    void ready(window_filter_t flags)
    {
        __scope = flags;
        __preys.clear();

        std::ranges::for_each(probe::graphics::windows(flags),
                              [&](const auto& win) { __preys.emplace_back(prey_t::from(win)); });

        std::ranges::for_each(probe::graphics::displays(),
                              [&](const auto& dis) { __preys.emplace_back(prey_t::from(dis)); });

        if (!any(flags & window_filter_t::capturable)) {
            auto desktop = probe::graphics::virtual_screen();

            __preys.emplace_back(prey_t{
                .type     = prey_type_t::desktop,
                .geometry = desktop.geometry,
                .handle   = desktop.handle,
                .name     = desktop.name,
                .codename = desktop.classname,
            });
        }
    }

    void clear()
    {
        __scope = window_filter_t::visible;
        __preys.clear();
    }

    std::string to_string(prey_type_t type)
    {
        switch (type) {
        case hunter::prey_type_t::rectangle: return "rectangle";
        case hunter::prey_type_t::widget: return "widget";
        case hunter::prey_type_t::window: return "window";
        case hunter::prey_type_t::display: return "display";
        case hunter::prey_type_t::desktop: return "desktop";
        default: return "unknown";
        }
    }
} // namespace hunter