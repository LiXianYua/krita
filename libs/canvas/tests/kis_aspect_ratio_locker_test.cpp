/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <functional>
#include <vector>

#include "kis_aspect_ratio_locker.h"

class FakeValueControl final : public KisAspectRatioValueControl
{
public:
    explicit FakeValueControl(qreal value)
        : m_value(value)
    {
    }

    qreal value() const override { return m_value; }

    void setValue(qreal value) override
    {
        m_value = value;
        if (m_changed) {
            m_changed();
        }
    }

    void setValueChangedCallback(std::function<void()> callback) override
    {
        m_changed = std::move(callback);
    }

private:
    qreal m_value;
    std::function<void()> m_changed;
};

class FakeToggleControl final : public KisAspectRatioToggleControl
{
public:
    explicit FakeToggleControl(bool checked)
        : m_checked(checked)
    {
    }

    bool isChecked() const override { return m_checked; }

    void setToggledCallback(std::function<void(bool)> callback) override
    {
        m_toggled = std::move(callback);
    }

    void setChecked(bool checked)
    {
        m_checked = checked;
        if (m_toggled) {
            m_toggled(checked);
        }
    }

private:
    bool m_checked;
    std::function<void(bool)> m_toggled;
};

class KisAspectRatioLockerTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void preservesRatioWithoutRecursiveNotification();
    void emitsAspectNotificationsInOrder();
};

void KisAspectRatioLockerTest::preservesRatioWithoutRecursiveNotification()
{
    FakeValueControl first(2.0);
    FakeValueControl second(4.0);
    FakeToggleControl toggle(true);
    KisAspectRatioLocker locker;
    int notifications = 0;

    PkObject::connect(&locker, &KisAspectRatioLocker::sliderValueChanged,
                      &locker, [&notifications]() { ++notifications; });
    locker.connectSpinBoxes(&first, &second, &toggle);

    first.setValue(3.0);
    QCOMPARE(second.value(), 6.0);
    QCOMPARE(notifications, 1);

    second.setValue(10.0);
    QCOMPARE(first.value(), 5.0);
    QCOMPARE(notifications, 2);
}

void KisAspectRatioLockerTest::emitsAspectNotificationsInOrder()
{
    FakeValueControl first(2.0);
    FakeValueControl second(4.0);
    FakeToggleControl toggle(true);
    KisAspectRatioLocker locker;
    std::vector<int> notifications;

    PkObject::connect(&locker, &KisAspectRatioLocker::aspectButtonChanged,
                      &locker, [&notifications]() { notifications.push_back(1); });
    PkObject::connect(&locker, &KisAspectRatioLocker::aspectButtonToggled,
                      &locker, [&notifications](bool checked) { notifications.push_back(checked ? 2 : 3); });

    locker.connectSpinBoxes(&first, &second, &toggle);
    QCOMPARE(notifications, std::vector<int>({1, 2}));

    notifications.clear();
    toggle.setChecked(false);
    QCOMPARE(notifications, std::vector<int>({1, 3}));
}

SIMPLE_TEST_MAIN(KisAspectRatioLockerTest)

#include "kis_aspect_ratio_locker_test.moc"
