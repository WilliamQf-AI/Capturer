#ifndef CAPTURER_WIDTH_BUTTON_H
#define CAPTURER_WIDTH_BUTTON_H

#include <QCheckBox>

class WidthButton : public QCheckBox
{
    Q_OBJECT

    Q_PROPERTY(int width READ __r_attr_width WRITE __w_attr_width)

public:
    explicit WidthButton(bool checkable = false, QWidget *parent = nullptr)
        : QCheckBox(parent)
    {
        setCheckable(checkable);
    }

    [[nodiscard]] int value() const { return width_; }

    void setMaxValue(int max) { max_ = max; }

    void setMinValue(int min) { min_ = min; }

signals:
    void changed(int);

public slots:
    void setValue(int width, bool silence = true);

protected:
    void wheelEvent(QWheelEvent *) override;

    int __r_attr_width() const { return __attr_width; };

    void __w_attr_width(int w) { __attr_width = w; }

private:
    int width_{ 3 };

    int max_{ 71 };
    int min_{ 1 };

    int __attr_width{ 2 };
};

#endif //! CAPTURER_WIDTH_BUTTON_H