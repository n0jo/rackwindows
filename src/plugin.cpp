#include "plugin.hpp"

Plugin* pluginInstance;

void init(Plugin* p)
{
    pluginInstance = p;

    // Add modules here
    p->addModel(modelBitshiftgain);
    p->addModel(modelCapacitor);
    p->addModel(modelCapacitor_stereo);
    p->addModel(modelChorus);
    p->addModel(modelConsole);
    p->addModel(modelDistance);
    p->addModel(modelHombre);
    p->addModel(modelMv);
    p->addModel(modelTape);
    p->addModel(modelTremolo);
    p->addModel(modelVibrato);

    // Any other plugin initialization may go here.
    // As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}

/* Other stuff */

// processing quality
void saveQuality(bool quality)
{
    json_t* settingsJ = json_object();
    json_object_set_new(settingsJ, "quality", json_boolean(quality));
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "w");
    if (file) {
        json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
        fclose(file);
    }
    json_decref(settingsJ);
}

bool loadQuality()
{
    bool ret = false;
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "r");
    if (!file) {
        saveQuality(false);
        return ret;
    }
    json_error_t error;
    json_t* settingsJ = json_loadf(file, 0, &error);
    if (!settingsJ) {
        // invalid setting json file
        fclose(file);
        saveQuality(false);
        return ret;
    }
    json_t* qualityJ = json_object_get(settingsJ, "quality");
    if (qualityJ)
        ret = json_boolean_value(qualityJ);

    fclose(file);
    json_decref(settingsJ);
    return ret;
}

// console type
void saveConsoleType(int consoleType)
{
    json_t* settingsJ = json_object();
    json_object_set_new(settingsJ, "consoleType", json_boolean(consoleType));
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "w");
    if (file) {
        json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
        fclose(file);
    }
    json_decref(settingsJ);
}

int loadConsoleType()
{
    bool ret = false;
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "r");
    if (!file) {
        saveConsoleType(false);
        return ret;
    }
    json_error_t error;
    json_t* settingsJ = json_loadf(file, 0, &error);
    if (!settingsJ) {
        // invalid setting json file
        fclose(file);
        saveConsoleType(false);
        return ret;
    }
    json_t* consoleTypeJ = json_object_get(settingsJ, "consoleType");
    if (consoleTypeJ)
        ret = json_boolean_value(consoleTypeJ);

    fclose(file);
    json_decref(settingsJ);
    return ret;
}

// https://github.com/MarcBoule/Geodesics/blob/master/src/Geodesics.cpp
void saveDarkAsDefault(bool darkAsDefault)
{
    json_t* settingsJ = json_object();
    json_object_set_new(settingsJ, "darkAsDefault", json_boolean(darkAsDefault));
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "w");
    if (file) {
        json_dumpf(settingsJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
        fclose(file);
    }
    json_decref(settingsJ);
}

// https://github.com/MarcBoule/Geodesics/blob/master/src/Geodesics.cpp
bool loadDarkAsDefault()
{
    bool ret = false;
    std::string settingsFilename = asset::user("Rackwindows.json");
    FILE* file = fopen(settingsFilename.c_str(), "r");
    if (!file) {
        saveDarkAsDefault(false);
        return ret;
    }
    json_error_t error;
    json_t* settingsJ = json_loadf(file, 0, &error);
    if (!settingsJ) {
        // invalid setting json file
        fclose(file);
        saveDarkAsDefault(false);
        return ret;
    }
    json_t* darkAsDefaultJ = json_object_get(settingsJ, "darkAsDefault");
    if (darkAsDefaultJ)
        ret = json_boolean_value(darkAsDefaultJ);

    fclose(file);
    json_decref(settingsJ);
    return ret;
}

// https://github.com/ValleyAudio/ValleyRackFree/blob/v1.0/src/Common/DSP/NonLinear.hpp
inline float tanhDriveSignal(float x, float drive)
{
    x *= drive;

    if (x < -1.3f) {
        return -1.f;
    } else if (x < -0.75f) {
        return (x * x + 2.6f * x + 1.69f) * 0.833333f - 1.f;
    } else if (x > 1.3f) {
        return 1.f;
    } else if (x > 0.75f) {
        return 1.f - (x * x - 2.6f * x + 1.69f) * 0.833333f;
    }
    return x;
}
