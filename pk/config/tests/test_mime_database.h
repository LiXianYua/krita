#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class TestMimeDatabase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void boundaryEntriesFirstLastMultiSuffix();
    void allSuffixesRoundTripToMimeType();
    void allDescriptionsMatchKritaOriginalEnglishText();
    void suffixesForMimeTypeReturnsAllWithPreferredFirst();
    void unknownExtensionReturnsEmpty();
    void descriptionForUnknownMimeTypeReturnsMimeTypeItself();
    void suffixesForUnknownMimeTypeReturnsEmptyList();
    void mimeTypeForFileUsesLowercasedSuffix();
    void mimeTypeForFileHandlesNoExtensionAndDotfile();
    void mimeTypeForFileHandlesPathWithDotsInDirectory();
};

// mimeTypeForData 没有单独的测试槽：它的形参类型 PkByteArray 归 R-02（pk/port）
// 交付，本仓库当前只有前置声明（见 PkMimeDatabase.h 顶部注释），没有完整定义，
// 无法在这个任务里构造出一个 PkByteArray 实例传进去。函数体本身只有一行
// `return PkString();`——不碰形参——已经在 PkMimeDatabase.cpp 里可读地自证；
// 等 R-02 交付 PkByteArray 后应当补一条 mimeTypeForDataAlwaysReturnsEmpty。
