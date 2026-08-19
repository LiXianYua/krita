#pragma once

#include <QObject>

class ImageCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void defaultConstruction();
    void constructArgb32();
    void constructIndexed8();
    void constructMono();
    void isNullThreeWays();
    void rectAndSize();
    void colorCount();
};
