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

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

// CAUTION: as of now, output is not limited in any way, positive values can produce very high volumes

#include "plugin.hpp"

struct Bitshiftgain : Module {
    enum ParamIds {
        SHIFT1_PARAM,
        SHIFT2_PARAM,
        LINK_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        IN1_INPUT,
        IN2_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT1_OUTPUT,
        OUT2_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        LINK_LIGHT,
        NUM_LIGHTS
    };

    double gain1;
    double gain2;
    int shift1;
    int shift2;
    bool isLinked;

    Bitshiftgain()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SHIFT1_PARAM, -8.0, 8.0, 0.0, "Shift");
        configParam(SHIFT2_PARAM, -8.0, 8.0, 0.0, "Shift");
        configParam(LINK_PARAM, 0.f, 1.f, 0.0, "Link");

        gain1 = 1.0;
        gain2 = 1.0;
        shift1 = 0;
        shift2 = 0;
        isLinked = false;
    }

    double bitShift(int bitshiftGain)
    {
        double gain = 1.0;

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
        //we are directly punching in the gain values rather than calculating them

        return gain;
    }

    void process(const ProcessArgs& args) override
    {
        // knob values
        shift1 = (int)trunc(params[SHIFT1_PARAM].getValue());
        shift2 = (int)trunc(params[SHIFT2_PARAM].getValue());

        // link light
        isLinked = params[LINK_PARAM].getValue() ? true : false;
        lights[LINK_LIGHT].setBrightness(isLinked);

        // gain
        gain1 = bitShift(shift1);
        if (isLinked) {
            gain2 = bitShift(-shift1 + shift2);
        } else {
            gain2 = bitShift(shift2);
        }

        // output1
        if (inputs[IN1_INPUT].isConnected()) {
            outputs[OUT1_OUTPUT].setVoltage(inputs[IN1_INPUT].getVoltage() * gain1);
        } else {
            // output -8 to 8 if no input is connected
            outputs[OUT1_OUTPUT].setVoltage(shift1);
        }
        //output2
        if (inputs[IN2_INPUT].isConnected()) {
            outputs[OUT2_OUTPUT].setVoltage(inputs[IN2_INPUT].getVoltage() * gain2);
        } else {
            // output -8 to 8 if no input is connected
            outputs[OUT2_OUTPUT].setVoltage(shift2);
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
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(30.0, 65.0), module, Bitshiftgain::SHIFT1_PARAM));
        addParam(createParamCentered<RwSwitchKnobMediumDark>(Vec(30.0, 235.0), module, Bitshiftgain::SHIFT2_PARAM));

        // switches
        addParam(createParamCentered<CKSSRot>(Vec(30.0, 195.0), module, Bitshiftgain::LINK_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(48, 195), module, Bitshiftgain::LINK_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 115.0), module, Bitshiftgain::IN1_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Bitshiftgain::IN2_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 155.0), module, Bitshiftgain::OUT1_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Bitshiftgain::OUT2_OUTPUT));
    }
};

Model* modelBitshiftgain = createModel<Bitshiftgain, BitshiftgainWidget>("bitshiftgain");