/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../MyPaintJson.h"

#include <json-c/json.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <limits>
#include <string>
#include <vector>

namespace {

using JsonPtr = std::unique_ptr<json_object, decltype(&json_object_put)>;

[[noreturn]] void fail(const char *message)
{
    std::fprintf(stderr, "MyPaintJsonTest: %s\n", message);
    std::exit(1);
}

void require(bool condition, const char *message)
{
    if (!condition) {
        fail(message);
    }
}

JsonPtr parse(const std::string &text)
{
    json_tokener *tokener = json_tokener_new();
    if (!tokener) {
        fail("json_tokener_new failed");
    }
    json_object *object = json_tokener_parse_ex(tokener, text.data(), static_cast<int>(text.size()));
    const json_tokener_error error = json_tokener_get_error(tokener);
    json_tokener_free(tokener);
    require(error == json_tokener_success && object, "output is not valid JSON");
    return JsonPtr(object, &json_object_put);
}

json_object *member(json_object *object, const char *name)
{
    json_object *value = nullptr;
    require(json_object_object_get_ex(object, name, &value), "required member missing");
    return value;
}

json_object *updatedSetting(const std::string &source,
                            const std::vector<MyPaintJson::InputMapping> &inputs,
                            double baseValue,
                            JsonPtr &owner)
{
    owner = parse(MyPaintJson::updateSetting(source, "opaque", inputs, baseValue));
    return member(member(member(owner.get(), "settings"), "opaque"), "inputs");
}

void testNestedCurvesAndUnknownFieldsSurvive()
{
    const std::string source = R"({
        "version": 3,
        "unknown_top": {"keep": true},
        "settings": {
            "opaque": {
                "unknown_setting": "survive",
                "base_value": 1,
                "inputs": {"pressure": [[0, 0], [1, 1]], "random": [[0, 7]]}
            },
            "sibling": {"base_value": 4.25}
        }
    })";

    const std::vector<MyPaintJson::InputMapping> inputs {
        {"pressure", true, {{-1.5, 0.125}, {0.0, -2.75}, {8.25, 3.5}}},
        {"random", false, {}}
    };

    JsonPtr root(nullptr, &json_object_put);
    json_object *inputObject = updatedSetting(source, inputs, -1234.125, root);
    require(json_object_object_get_ex(root.get(), "unknown_top", nullptr), "unknown top-level field was lost");
    json_object *settings = member(root.get(), "settings");
    require(json_object_object_get_ex(settings, "sibling", nullptr), "sibling setting was lost");
    json_object *opaque = member(settings, "opaque");
    require(std::string(json_object_get_string(member(opaque, "unknown_setting"))) == "survive",
            "unknown setting field was lost");
    require(std::fabs(json_object_get_double(member(opaque, "base_value")) + 1234.125) < 1e-12,
            "non-integral base value was truncated");
    require(!json_object_object_get_ex(inputObject, "random", nullptr), "inactive input was not removed");

    json_object *curve = member(inputObject, "pressure");
    require(json_object_get_type(curve) == json_type_array && json_object_array_length(curve) == 3,
            "curve point count/order was not retained");
    const double expected[][2] = {{-1.5, 0.125}, {0.0, -2.75}, {8.25, 3.5}};
    for (std::size_t index = 0; index < 3; ++index) {
        json_object *point = json_object_array_get_idx(curve, index);
        require(json_object_get_type(point) == json_type_array && json_object_array_length(point) == 2,
                "curve point is not a two-element array");
        require(std::fabs(json_object_get_double(json_object_array_get_idx(point, 0)) - expected[index][0]) < 1e-12,
                "curve x value changed");
        require(std::fabs(json_object_get_double(json_object_array_get_idx(point, 1)) - expected[index][1]) < 1e-12,
                "curve y value changed");
    }
}

void testEachMissingContainerLevelIsBuilt()
{
    const std::vector<std::string> sources {
        R"({})",
        R"({"settings":{}})",
        R"({"settings":{"opaque":{}}})"
    };
    const std::vector<MyPaintJson::InputMapping> inputs {{"pressure", true, {{0.25, 0.75}}}};

    for (const std::string &source : sources) {
        JsonPtr root(nullptr, &json_object_put);
        json_object *inputObject = updatedSetting(source, inputs, 0.5, root);
        require(json_object_get_type(inputObject) == json_type_object, "inputs container was not rebuilt");
        require(json_object_get_type(member(inputObject, "pressure")) == json_type_array,
                "active input curve was not written after a missing container");
    }
}

void testWrongAndInvalidContainersAreRebuilt()
{
    const std::vector<std::string> sources {
        "",
        "not json",
        "[]",
        R"({"settings":7})",
        R"({"settings":{"opaque":false}})",
        R"({"settings":{"opaque":{"inputs":"wrong"}}})"
    };
    const std::vector<MyPaintJson::InputMapping> inputs {{"pressure", true, {{0.25, 0.75}}}};

    for (const std::string &source : sources) {
        JsonPtr root(nullptr, &json_object_put);
        json_object *inputObject = updatedSetting(source, inputs, 0.5, root);
        require(json_object_get_type(inputObject) == json_type_object, "inputs container was not rebuilt");
        require(json_object_get_type(member(inputObject, "pressure")) == json_type_array,
                "active input curve was not written after malformed input");
    }
}

void testPositiveAndNegativeDoubleExtremesSurvive()
{
    const std::vector<MyPaintJson::InputMapping> inputs;
    const double extremes[] = {
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::lowest()
    };

    for (const double value : extremes) {
        JsonPtr root(nullptr, &json_object_put);
        updatedSetting(R"({"settings":{"opaque":{"inputs":{}}}})", inputs, value, root);
        json_object *opaque = member(member(root.get(), "settings"), "opaque");
        require(json_object_get_double(member(opaque, "base_value")) == value,
                "positive or negative double extreme changed");
    }
}

} // namespace

int main()
{
    testNestedCurvesAndUnknownFieldsSurvive();
    testEachMissingContainerLevelIsBuilt();
    testWrongAndInvalidContainersAreRebuilt();
    testPositiveAndNegativeDoubleExtremesSurvive();
    return 0;
}
