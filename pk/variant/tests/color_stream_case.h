#pragma once

#include <QObject>

class ColorStreamCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void colorsMatchQt46WireBytes();
    void shortReadsRetainQt46DecodedState();
    void rawSpecsArePreservedLikeQt46();
};
