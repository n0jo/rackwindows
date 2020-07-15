/***********************************************************************************************
Dual BSG
--------
VCV Rack module based on BitshiftGain by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- 2 BSG units
- if no input is connected, the respective output will provide constant voltage selectable in 1V steps from -8V to +8V
- option to link bottom BSG to top BSG -> gain shifts at top BSG are automatically compensated for by the bottom BSG
- if linked, bottom knob acts as an offset
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

// CAUTION: the output is not limited in any way, positive values can produce very high volumes

#include "plugin.hpp"

struct Bitshiftgain : Module {
    enum ParamIds {
        SHIFT_A_PARAM,
        SHIFT_B_PARAM,
        LINK_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        IN_A_INPUT,
        IN_B_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_A_OUTPUT,
        OUT_B_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        LINK_LIGHT,
        NUM_LIGHTS
    };

    int shiftA;
    int shiftB;
    bool isLinked;
    double lastSampleA; // for zero crossing detection
    double lastSampleB; // for zero crossing detection

    Bitshiftgain()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SHIFT_A_PARAM, -8.0, 8.0, 0.0, "Shift");
        configParam(SHIFT_B_PARAM, -8.0, 8.0, 0.0, "Shift/Offset");
        configParam(LINK_PARAM, 0.f, 1.f, 0.0, "Link");

        onReset();
    }

    void onReset() override
    {
        shiftA = 0;
        shiftB = 0;
        isLinked = false;
        lastSampleA = 0.0;
        lastSampleB = 0.0;
    }

    double bitShift(int bitshiftGain)
    {
        double gain = 1.0;

        //we are directly punching in the gain values rather than calculating them
        switch (bitshiftGain) {
        case -16:
            gain = 0.0000152587890625;
            break;
        case -15:
            gain = 0.000030517578125;
            break;
        case -14:
            gain = 0.00006103515625;
            break;
        case -13:
            gain = 0.0001220703125;
            break;
        case -12:
            gain = 0.000244140625;
            break;
        case -11:
            gain = 0.00048828125;
            break;
        case -10:
            gain = 0.0009765625;
            break;
        case -9:
            gain = 0.001953125;
            break;
        case -8:
            gain = 0.00390625;
            break;
        case -7:
            gain = 0.0078125;
            break;
        case -6:
            gain = 0.015625;
            break;
        case -5:
            gain = 0.03125;
            break;
        case -4:
            gain = 0.0625;
            break;
        case -3:
            gain = 0.125;
            break;
        case -2:
            gain = 0.25;
            break;
        case -1:
            gain = 0.5;
            break;
        case 0:
            gain = 1.0;
            break;
        case 1:
            gain = 2.0;
            break;
        case 2:
            gain = 4.0;
            break;
        case 3:
            gain = 8.0;
            break;
        case 4:
            gain = 16.0;
            break;
        case 5:
            gain = 32.0;
            break;
        case 6:
            gain = 64.0;
            break;
        case 7:
            gain = 128.0;
            break;
        case 8:
            gain = 256.0;
            break;
        case 9:
            gain = 512.0;
            break;
        case 10:
            gain = 1024.0;
            break;
        case 11:
            gain = 2048.0;
            break;
        case 12:
            gain = 4096.0;
            break;
        case 13:
            gain = 8192.0;
            break;
        case 14:
            gain = 16384.0;
            break;
        case 15:
            gain = 32768.0;
            break;
        case 16:
            gain = 65536.0;
            break;
        }

        return gain;
    }

    void process(const ProcessArgs& args) override
    {
        // link
        isLinked = params[LINK_PARAM].getValue() ? true : false;
        lights[LINK_LIGHT].setBrightness(isLinked);

        /* section A
        =============================================================================== */

        if (inputs[IN_A_INPUT].isConnected()) {

            // get number of polyphonic channels
            int numChannelsA = inputs[IN_A_INPUT].getChannels();

            // set number of output channels
            outputs[OUT_A_OUTPUT].setChannels(numChannelsA);

            // update shiftA only at zero crossings (of first channel) to reduce clicks on parameter changes
            // reasonably effective on most sources, but will not happen across multiple channels, therefore monophonic only
            bool isZero = (inputs[IN_A_INPUT].getVoltage() * lastSampleA < 0.0);
            shiftA = isZero ? params[SHIFT_A_PARAM].getValue() : shiftA;
            lastSampleA = inputs[IN_A_INPUT].getVoltage();

            // for each poly channel
            for (int i = 0; i < numChannelsA; i++) {
                // shift signal in 6db steps
                outputs[OUT_A_OUTPUT].setVoltage(inputs[IN_A_INPUT].getPolyVoltage(i) * bitShift(shiftA), i);
            }
        } else {
            // output -8 to 8 in 1V steps if no input is connected
            outputs[OUT_A_OUTPUT].setVoltage(params[SHIFT_A_PARAM].getValue());
        }

        /* section B
        =============================================================================== */

        if (inputs[IN_B_INPUT].isConnected()) {

            // get number of polyphonic channels
            int numChannelsB = inputs[IN_B_INPUT].getChannels();

            // set number of output channels
            outputs[OUT_B_OUTPUT].setChannels(numChannelsB);

            // update shiftB only at zero crossings (of first channel) to reduce clicks on parameter changes
            // reasonably effective on most sources, but will not happen across multiple channels, therefore monophonic only
            bool isZero = (inputs[IN_B_INPUT].getVoltage() * lastSampleB < 0.0);
            shiftB = isZero ? params[SHIFT_B_PARAM].getValue() : shiftB;
            lastSampleB = inputs[IN_B_INPUT].getVoltage();

            // for each poly channel
            for (int i = 0; i < numChannelsB; i++) {
                if (isLinked) {
                    if (inputs[IN_A_INPUT].isConnected()) {
                        // offset signal in 6db steps
                        outputs[OUT_B_OUTPUT].setVoltage(inputs[IN_B_INPUT].getPolyVoltage(i) * bitShift(-shiftA + shiftB), i);
                    } else {
                        // offset signal in 1V steps
                        outputs[OUT_B_OUTPUT].setVoltage(inputs[IN_B_INPUT].getPolyVoltage(i) + params[SHIFT_B_PARAM].getValue(), i);
                    }
                } else {
                    // shift signal in 6db steps
                    outputs[OUT_B_OUTPUT].setVoltage(inputs[IN_B_INPUT].getPolyVoltage(i) * bitShift(shiftB), i);
                }
            }
        } else {
            // output -8 to 8 in 1V steps if no input is connected
            outputs[OUT_B_OUTPUT].setVoltage(params[SHIFT_B_PARAM].getValue());
        }
    }
};

struct BitshiftgainWidget : ModuleWidget {
    BitshiftgainWidget(Bitshiftgain* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/bitshiftgain_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(30.0, 65.0), module, Bitshiftgain::SHIFT_A_PARAM));
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(30.0, 235.0), module, Bitshiftgain::SHIFT_B_PARAM));

        // switches
        addParam(createParamCentered<RwCKSSRot>(Vec(30.0, 195.0), module, Bitshiftgain::LINK_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(48, 195), module, Bitshiftgain::LINK_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 115.0), module, Bitshiftgain::IN_A_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Bitshiftgain::IN_B_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 155.0), module, Bitshiftgain::OUT_A_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Bitshiftgain::OUT_B_OUTPUT));
    }
};

Model* modelBitshiftgain = createModel<Bitshiftgain, BitshiftgainWidget>("bitshiftgain");