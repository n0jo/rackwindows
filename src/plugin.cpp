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
    p->addModel(modelDistance);
    p->addModel(modelElectrohat);
    p->addModel(modelHombre);
    p->addModel(modelMv);
    // p->addModel(modelTape);
    p->addModel(modelTremolo);
    p->addModel(modelVibrato);

    // Any other plugin initialization may go here.
    // As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}

/* Utility Functions */

// ValleyRackFree/src/Common/DSP/NonLinear.hpp
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
