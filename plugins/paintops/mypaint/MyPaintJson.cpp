/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintJson.h"

#include <json-c/json.h>

#include <cctype>
#include <memory>

namespace MyPaintJson
{
namespace
{

using JsonPtr = std::unique_ptr<json_object, decltype(&json_object_put)>;

JsonPtr parseObject(const std::string &source)
{
    json_tokener *tokener = json_tokener_new();
    if (!tokener) {
        return JsonPtr(json_object_new_object(), &json_object_put);
    }

    json_object *parsed = json_tokener_parse_ex(tokener, source.data(), static_cast<int>(source.size()));
    const json_tokener_error error = json_tokener_get_error(tokener);
    std::size_t parseEnd = json_tokener_get_parse_end(tokener);
    json_tokener_free(tokener);

    while (parseEnd < source.size() && std::isspace(static_cast<unsigned char>(source[parseEnd]))) {
        ++parseEnd;
    }

    if (error != json_tokener_success || !parsed || parseEnd != source.size()
        || json_object_get_type(parsed) != json_type_object) {
        if (parsed) {
            json_object_put(parsed);
        }
        parsed = json_object_new_object();
    }

    return JsonPtr(parsed, &json_object_put);
}

json_object *ensureObjectMember(json_object *parent, const char *name)
{
    json_object *member = nullptr;
    if (json_object_object_get_ex(parent, name, &member)
        && json_object_get_type(member) == json_type_object) {
        return member;
    }

    member = json_object_new_object();
    json_object_object_add(parent, name, member);
    return member;
}

json_object *curveObject(const std::vector<CurvePoint> &points)
{
    json_object *curve = json_object_new_array_ext(static_cast<int>(points.size()));
    for (const CurvePoint &point : points) {
        json_object *pair = json_object_new_array_ext(2);
        json_object_array_add(pair, json_object_new_double(point.first));
        json_object_array_add(pair, json_object_new_double(point.second));
        json_object_array_add(curve, pair);
    }
    return curve;
}

} // namespace

std::string updateSetting(const std::string &source,
                          const std::string &settingName,
                          const std::vector<InputMapping> &inputs,
                          double baseValue)
{
    JsonPtr root = parseObject(source);
    json_object *settings = ensureObjectMember(root.get(), "settings");
    json_object *setting = ensureObjectMember(settings, settingName.c_str());
    json_object *inputObject = ensureObjectMember(setting, "inputs");

    for (const InputMapping &input : inputs) {
        if (input.active) {
            json_object_object_add(inputObject, input.name.c_str(), curveObject(input.points));
        } else {
            json_object_object_del(inputObject, input.name.c_str());
        }
    }

    json_object_object_add(setting, "base_value", json_object_new_double(baseValue));
    return json_object_to_json_string_ext(root.get(), JSON_C_TO_STRING_PRETTY);
}

} // namespace MyPaintJson
