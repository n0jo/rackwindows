/***********************************************************************************************
Console
------
VCV Rack module based on Console by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- console channel and buss combined into an 8-channel stereo summing mixer
- no gain control
- polyphonic

Additional code heavily inspired by the Fundamental Mixer by Andrew Belt.
Some UI elements based on graphics from the Component Library by Wes Milholen.

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Console : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(IN_L_INPUTS, 8),
        ENUMS(IN_R_INPUTS, 8),
        IN_ST_L_INPUT,
        IN_ST_R_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    const double gainCut = 0.03125;
    const double gainBoost = 32.0;

    uint32_t fpd;

    float A;

    Console()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        A = 1.0;
        fpd = 17;
    }

    void encodeChannel(Input& input, float mix[], int numChannels)
    {
        if (input.isConnected()) {

            // input
            float inputSamples[16] = {};
            input.readVoltages(inputSamples);

            for (int i = 0; i < numChannels; i++) {
                // pad gain
                inputSamples[i] *= gainCut;

                if (fabs(inputSamples[i]) < 1.18e-37)
                    inputSamples[i] = fpd * 1.18e-37;

                // encode
                if (inputSamples[i] > 1.0)
                    inputSamples[i] = 1.0;
                else if (inputSamples[i] > 0.0)
                    inputSamples[i] = 1.0 - pow(1.0 - inputSamples[i], 2.0);

                if (inputSamples[i] < -1.0)
                    inputSamples[i] = -1.0;
                else if (inputSamples[i] < 0.0)
                    inputSamples[i] = -1.0 + pow(1.0 + inputSamples[i], 2.0);

                // bring gain back up
                inputSamples[i] *= gainBoost;

                // add to mix
                mix[i] += inputSamples[i];
            }
        }
    }

    void decodeChannel(Output& output, float mix[], int maxChannels)
    {
        if (output.isConnected()) {
            for (int i = 0; i < maxChannels; i++) {
                long double inputSample = mix[i];

                // pad gain
                inputSample *= gainCut;

                if (fabs(inputSample) < 1.18e-37)
                    inputSample = fpd * 1.18e-37;

                // decode
                if (inputSample > 1.0)
                    inputSample = 1.0;
                else if (inputSample > 0.0)
                    inputSample = 1.0 - pow(1.0 - inputSample, 0.5);

                if (inputSample < -1.0)
                    inputSample = -1.0;
                else if (inputSample < 0.0)
                    inputSample = -1.0 + pow(1.0 + inputSample, 0.5);

                // bring gain back up
                inputSample *= gainBoost;

                //begin 32 bit stereo floating point dither
                int expon;
                frexpf((float)inputSample, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                inputSample += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                //end 32 bit stereo floating point dither

                mix[i] = inputSample;
            }
            output.setChannels(maxChannels);
            output.writeVoltages(mix);
        }
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {
            float mixL[16] = {};
            float mixR[16] = {};
            int numChannelsL = 1;
            int numChannelsR = 1;
            int maxChannelsL = 1;
            int maxChannelsR = 1;

            // for each mixer channel
            for (int i = 0; i < 8; i++) {

                // encode L
                numChannelsL = inputs[IN_L_INPUTS + i].getChannels();
                maxChannelsL = std::max(maxChannelsL, numChannelsL);
                encodeChannel(inputs[IN_L_INPUTS + i], mixL, numChannelsL);

                // encode R
                numChannelsR = inputs[IN_R_INPUTS + i].getChannels();
                maxChannelsR = std::max(maxChannelsR, numChannelsR);
                encodeChannel(inputs[IN_R_INPUTS + i], mixR, numChannelsR);
            }

            // decode L
            decodeChannel(outputs[OUT_L_OUTPUT], mixL, maxChannelsL);

            // decode R
            decodeChannel(outputs[OUT_R_OUTPUT], mixR, maxChannelsR);
        }
    }
};

struct ConsoleWidget : ModuleWidget {
    ConsoleWidget(Console* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/console_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 55.0), module, Console::IN_L_INPUTS + 0));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 55.0), module, Console::IN_R_INPUTS + 0));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 85.0), module, Console::IN_L_INPUTS + 1));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 85.0), module, Console::IN_R_INPUTS + 1));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 115.0), module, Console::IN_L_INPUTS + 2));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 115.0), module, Console::IN_R_INPUTS + 2));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 145.0), module, Console::IN_L_INPUTS + 3));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 145.0), module, Console::IN_R_INPUTS + 3));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 175.0), module, Console::IN_L_INPUTS + 4));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 175.0), module, Console::IN_R_INPUTS + 4));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 205.0), module, Console::IN_L_INPUTS + 5));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 205.0), module, Console::IN_R_INPUTS + 5));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 235.0), module, Console::IN_L_INPUTS + 6));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 235.0), module, Console::IN_R_INPUTS + 6));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 265.0), module, Console::IN_L_INPUTS + 7));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 265.0), module, Console::IN_R_INPUTS + 7));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 295.0), module, Console::IN_ST_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 295.0), module, Console::IN_ST_R_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Console::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Console::OUT_R_OUTPUT));
    }
};

Model* modelConsole = createModel<Console, ConsoleWidget>("console");