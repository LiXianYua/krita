/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisExiv2IODevice.h"
#include "kis_exiv2_common.h"

#include <dlfcn.h>

#include <exiv2/exif.hpp>
#include <exiv2/xmp_exiv2.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <PkList.h>
#include <PkMap.h>
#include <PkStream.h>
#include <PkStringHash.h>
#include <PkVariant.h>
#include <kis_debug.h>
#include <kis_meta_data_backend_registry.h>
#include <kis_meta_data_entry.h>
#include <kis_meta_data_schema.h>
#include <kis_meta_data_schema_registry.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_value.h>

namespace {

int fail(const char *message)
{
    std::cerr << message << '\n';
    return 1;
}

void printHex(const std::string &bytes)
{
    for (const unsigned char byte : bytes) {
        std::cerr << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned int>(byte) << ' ';
    }
    std::cerr << std::dec << '\n';
}

class MemoryStream : public PkStream
{
public:
    explicit MemoryStream(std::string data = {})
        : m_data(std::move(data))
    {
    }

    pk_int64 size() const override
    {
        return static_cast<pk_int64>(m_data.size());
    }

    const std::string &data() const
    {
        return m_data;
    }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 available = static_cast<pk_int64>(m_data.size()) - pos();
        if (available <= 0) {
            return 0;
        }
        const pk_int64 count = std::min(available, maxSize);
        std::memcpy(data, m_data.data() + pos(), static_cast<std::size_t>(count));
        return count;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        const std::size_t offset = static_cast<std::size_t>(pos());
        const std::size_t count = static_cast<std::size_t>(maxSize);
        if (offset + count > m_data.size()) {
            m_data.resize(offset + count);
        }
        std::memcpy(m_data.data() + offset, data, count);
        return maxSize;
    }

private:
    std::string m_data;
};

bool loadModule(const char *module)
{
    // Initialize logging before dlopen creates the backend registry singleton,
    // so registry destruction can log before the logging singleton is torn down.
    dbgRegistry << "Loading metadata regression module";
    if (!dlopen(module, RTLD_NOW | RTLD_GLOBAL)) {
        std::cerr << "failed to load metadata module " << module << ": "
                  << dlerror() << '\n';
        return false;
    }
    return true;
}

bool loadModules()
{
    const char *modules[] = {METADATA_EXIF_MODULE,
                             METADATA_IPTC_MODULE,
                             METADATA_XMP_MODULE};
    for (const char *module : modules) {
        if (!loadModule(module)) {
            return false;
        }
    }
    return true;
}

const KisMetaData::Schema *schemaFor(const PkString &uri, const PkString &prefix)
{
    KisMetaData::SchemaRegistry *registry = KisMetaData::SchemaRegistry::instance();
    const KisMetaData::Schema *schema = registry->schemaFromUri(uri);
    return schema ? schema : registry->create(uri, prefix);
}

std::string exifValueBytes(const std::string &serialized, const char *key)
{
    Exiv2::ExifData data;
    const Exiv2::ByteOrder order = Exiv2::ExifParser::decode(
        data,
        reinterpret_cast<const Exiv2::byte *>(serialized.data()),
        serialized.size());
    if (order == Exiv2::invalidByteOrder) {
        throw std::runtime_error("production EXIF output did not decode");
    }
    const auto it = data.findKey(Exiv2::ExifKey(key));
    if (it == data.end()) {
        throw std::runtime_error("production EXIF output omitted OECF");
    }
    const auto value = it->getValue();
    std::string bytes(value->size(), '\0');
    value->copy(reinterpret_cast<Exiv2::byte *>(bytes.data()), order);
    return bytes;
}

int testStablePathReference()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "krita-metadata-path-lifetime";
    std::filesystem::create_directories(directory);
    const std::filesystem::path filePath = directory / "metadata.exv";
    std::ofstream(filePath).close();

    KisExiv2IODevice device(PkString(filePath.string().c_str()));
    const std::string *first = &device.path();
    const std::string firstValue = *first;
    const std::string *second = &device.path();

    std::vector<std::string> churn;
    for (int i = 0; i < 4096; ++i) {
        churn.emplace_back(256, static_cast<char>('a' + (i % 26)));
    }

    const bool stableAddress = first == second;
    const bool stableContent = *first == firstValue && *second == firstValue;
    std::filesystem::remove_all(directory);

    if (!stableAddress) {
        return fail("KisExiv2IODevice::path() did not return a stable reference");
    }
    if (!stableContent) {
        return fail("KisExiv2IODevice::path() reference content did not survive churn");
    }
    return 0;
}

int testExifDateFormatting()
{
    if (!formatExifDateTime(PkDateTime()).empty()) {
        std::cerr << "invalid EXIF date formatted as: "
                  << formatExifDateTime(PkDateTime()) << '\n';
        return 1;
    }

    const PkDateTime valid(PkDate(2024, 2, 3), PkTime(4, 5, 6));
    if (formatExifDateTime(valid) != "2024:02:03 04:05:06") {
        std::cerr << "valid EXIF date formatted as: "
                  << formatExifDateTime(valid) << '\n';
        return 1;
    }
    return 0;
}

int testBackendRegistration()
{
    if (!loadModules()) {
        return 1;
    }

    KisMetadataBackendRegistry *registry = KisMetadataBackendRegistry::instance();
    if (!registry->get(PkString("exif")) ||
        !registry->get(PkString("iptc")) ||
        !registry->get(PkString("xmp"))) {
        return fail("metadata modules did not register all three backends");
    }
    return 0;
}


int testExifOecfLatin1WireRoundTrip()
{
    if (!loadModule(METADATA_EXIF_MODULE)) {
        return 1;
    }
    KisMetaData::IOBackend *backend =
        KisMetadataBackendRegistry::instance()->get(PkString("exif"));
    if (!backend) {
        return fail("EXIF backend is not registered");
    }

    const KisMetaData::Schema *schema = schemaFor(KisMetaData::Schema::EXIFSchemaUri,
                                                   PkString("exif"));
    PkMap<PkString, KisMetaData::Value> structure;
    structure[PkString("Columns")] = KisMetaData::Value(PkVariant(1));
    structure[PkString("Rows")] = KisMetaData::Value(PkVariant(1));
    PkList<KisMetaData::Value> names;
    names.append(KisMetaData::Value(PkVariant(PkString(u8"café"))));
    structure[PkString("Names")] =
        KisMetaData::Value(names, KisMetaData::Value::OrderedArray);
    PkList<KisMetaData::Value> values;
    values.append(KisMetaData::Value(KisMetaData::Rational(3, 2)));
    structure[PkString("Values")] =
        KisMetaData::Value(values, KisMetaData::Value::OrderedArray);

    KisMetaData::Store source;
    source.addEntry(KisMetaData::Entry(schema,
                                       PkString("OECF"),
                                       KisMetaData::Value(structure)));
    MemoryStream firstSerialized;
    if (!backend->saveTo(&source, &firstSerialized)) {
        return fail("production EXIF backend failed to save OECF");
    }

    const std::string expected("\x01\x00\x01\x00"
                               "caf\xe9\x00"
                               "\x03\x00\x00\x00\x02\x00\x00\x00",
                               17);
    const std::string firstBytes = exifValueBytes(firstSerialized.data(),
                                                   "Exif.Photo.OECF");
    if (firstBytes != expected) {
        std::cerr << "OECF exact Latin-1 bytes differ; got " << firstBytes.size()
                  << " bytes\n";
        return 1;
    }

    MemoryStream loadStream(firstSerialized.data());
    KisMetaData::Store loaded;
    if (!backend->loadFrom(&loaded, &loadStream)) {
        return fail("production EXIF backend failed to reload saved OECF");
    }
    const KisMetaData::Value loadedOecf = loaded.getEntry(schema, PkString("OECF")).value();
    const PkMap<PkString, KisMetaData::Value> loadedStructure = loadedOecf.asStructure();
    if (loadedStructure[PkString("Names")].asArray()[0].asVariant().toString()
        != PkString(u8"café")) {
        return fail("OECF Latin-1 name did not round trip through production load");
    }

    MemoryStream secondSerialized;
    if (!backend->saveTo(&loaded, &secondSerialized)) {
        return fail("production EXIF backend failed to resave loaded OECF");
    }
    const std::string secondBytes = exifValueBytes(secondSerialized.data(), "Exif.Photo.OECF");
    if (secondBytes != expected) {
        std::cerr << "expected: ";
        printHex(expected);
        std::cerr << "actual:   ";
        printHex(secondBytes);
        return fail("OECF load/save round trip changed exact wire bytes");
    }
    return 0;
}

int testXmpUnicodeStructuredLoad()
{
    if (!loadModule(METADATA_XMP_MODULE)) {
        return 1;
    }
    KisMetaData::IOBackend *backend =
        KisMetadataBackendRegistry::instance()->get(PkString("xmp"));
    if (!backend) {
        return fail("XMP backend is not registered");
    }

    const std::string packet = u8R"XMP(<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>
<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:stRef="http://ns.adobe.com/xap/1.0/sType/ResourceRef#"
    xmlns:stEvt="http://ns.adobe.com/xap/1.0/sType/ResourceEvent#">
   <xmpMM:DerivedFrom rdf:parseType="Resource">
    <stRef:instanceIDé>structured</stRef:instanceIDé>
   </xmpMM:DerivedFrom>
   <xmpMM:History>
    <rdf:Seq>
     <rdf:li rdf:parseType="Resource">
      <stEvt:actioné>created</stEvt:actioné>
     </rdf:li>
    </rdf:Seq>
   </xmpMM:History>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>
<?xpacket end="w"?>)XMP";
    MemoryStream input(packet);
    KisMetaData::Store store;
    if (!backend->loadFrom(&store, &input)) {
        return fail("production XMP backend rejected Unicode structured packet");
    }

    const KisMetaData::Schema *schema =
        schemaFor(PkString("http://ns.adobe.com/xap/1.0/mm/"), PkString("xmpMM"));
    if (!store.containsEntry(schema, PkString("DerivedFrom"))) {
        return fail("Unicode continuation in structured XMP field was dropped");
    }
    const auto derived = store.getEntry(schema, PkString("DerivedFrom")).value().asStructure();
    if (derived[PkString(u8"instanceIDé")].asVariant().toString()
        != PkString("structured")) {
        return fail("Unicode continuation in structured XMP field was dropped");
    }
    if (!store.containsEntry(schema, PkString("History"))) {
        return fail("Unicode continuation in array-of-structure XMP field was dropped");
    }
    const auto history = store.getEntry(schema, PkString("History")).value().asArray();
    if (history.size() != 1
        || history[0].asStructure()[PkString(u8"actioné")].asVariant().toString()
            != PkString("created")) {
        return fail("Unicode continuation in array-of-structure XMP field was dropped");
    }
    return 0;
}

int testXmpOversizedArrayIndexLoad()
{
    if (!loadModule(METADATA_XMP_MODULE)) {
        return 1;
    }
    KisMetaData::IOBackend *backend =
        KisMetadataBackendRegistry::instance()->get(PkString("xmp"));
    if (!backend) {
        return fail("XMP backend is not registered");
    }

    std::string packet = R"XMP(<x:xmpmeta xmlns:x="adobe:ns:meta/">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:xmpMM="http://ns.adobe.com/xap/1.0/mm/"
    xmlns:stEvt="http://ns.adobe.com/xap/1.0/sType/ResourceEvent#">
   <xmpMM:History>
    <rdf:Seq>)XMP";
    for (int i = 0; i < 1025; ++i) {
        packet += R"XMP(<rdf:li rdf:parseType="Resource"><stEvt:action>created</stEvt:action></rdf:li>)XMP";
    }
    packet += R"XMP(</rdf:Seq></xmpMM:History>
  </rdf:Description></rdf:RDF></x:xmpmeta>)XMP";

    MemoryStream input(packet);
    KisMetaData::Store store;
    try {
        if (!backend->loadFrom(&store, &input)) {
            return fail("production XMP backend rejected otherwise-decodable packet");
        }
    } catch (const std::exception &error) {
        std::cerr << "production XMP load threw for oversized array index: "
                  << error.what() << '\n';
        return 1;
    }
    const KisMetaData::Schema *schema =
        schemaFor(PkString("http://ns.adobe.com/xap/1.0/mm/"), PkString("xmpMM"));
    if (!store.containsEntry(schema, PkString("History"))) {
        return fail("bounded XMP array prefix was not preserved");
    }
    if (store.getEntry(schema, PkString("History")).value().asArray().size() != 1024) {
        return fail("oversized XMP array index was not dropped at the checked bound");
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        return fail("usage: kis_metadata_regression_test path|date|registry|exif-oecf|xmp-unicode|xmp-array-index");
    }
    const std::string testName(argv[1]);
    if (testName == "path") {
        return testStablePathReference();
    }
    if (testName == "date") {
        return testExifDateFormatting();
    }
    if (testName == "registry") {
        return testBackendRegistration();
    }
    if (testName == "exif-oecf") {
        return testExifOecfLatin1WireRoundTrip();
    }
    if (testName == "xmp-unicode") {
        return testXmpUnicodeStructuredLoad();
    }
    if (testName == "xmp-array-index") {
        return testXmpOversizedArrayIndexLoad();
    }
    return fail("unknown metadata regression test");
}
