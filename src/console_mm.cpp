/***********************************************************************************************
Console MM
----------
VCV Rack module based on Console by Chris Johnson from Airwindows <www.airwindows.com>, but specifically designed
to work in conjunction with MindMeld's MixMaster. It takes the polyphonic direct outs of MixMaster, encodes and sums
the individual channels, then decodes and outputs the stereo sum.

Ported and designed by Jens Robert Janke

Changes/Additions:
- encodes, sums and decodes the direct outs of MixMaster
- console types: Console6, PurestConsole

Additional code inspired by the Fundamental Mixer by Andrew Belt.

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Console_mm : Module {
    enum ParamIds {
        LEVEL_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(IN_INPUTS, 3),
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(THRU_OUTPUTS, 3),
        ENUMS(OUT_OUTPUTS, 2),
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    bool quality;
    int consoleType;
    dsp::VuMeter2 vuMeters[9];
    dsp::ClockDivider lightDivider;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    uint32_t fpd[16];

    Console_mm()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LEVEL_PARAM, 0.f, 1.f, 0.5f, "Level", "%");

        quality = loadQuality();
        consoleType = loadConsoleType();
        lightDivider.setDivision(512);
        onReset();
    }

    void onReset() override
    {
        for (int i = 0; i < 16; i++) {
            fpd[i] = 17;
        }
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();

        // quality
        json_object_set_new(rootJ, "quality", json_integer(quality));

        // consoleType
        json_object_set_new(rootJ, "consoleType", json_integer(consoleType));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
        // quality
        json_t* qualityJ = json_object_get(rootJ, "quality");
        if (qualityJ)
            quality = json_integer_value(qualityJ);

        // consoleType
        json_t* consoleTypeJ = json_object_get(rootJ, "consoleType");
        if (consoleTypeJ)
            consoleType = json_integer_value(consoleTypeJ);
    }

    long double encode(long double inputSample, int consoleType = 0)
    {
        switch (consoleType) {
        case 0: // Console6Channel
            if (inputSample > 1.0)
                inputSample = 1.0;
            else if (inputSample > 0.0)
                inputSample = 1.0 - pow(1.0 - inputSample, 2.0);

            if (inputSample < -1.0)
                inputSample = -1.0;
            else if (inputSample < 0.0)
                inputSample = -1.0 + pow(1.0 + inputSample, 2.0);
            break;
        case 1: // PurestConsoleChannel
            inputSample = sin(inputSample);
            break;
        }
        return inputSample;
    }

    long double decode(long double inputSample, int consoleType = 0)
    {
        switch (consoleType) {
        case 0: // Console6Buss
            if (inputSample > 1.0)
                inputSample = 1.0;
            else if (inputSample > 0.0)
                inputSample = 1.0 - pow(1.0 - inputSample, 0.5);

            if (inputSample < -1.0)
                inputSample = -1.0;
            else if (inputSample < 0.0)
                inputSample = -1.0 + pow(1.0 + inputSample, 0.5);
            break;
        case 1: // PurestConsoleBuss
            inputSample = sin(inputSample);
            break;
        }
        return inputSample;
    }

    void process(const ProcessArgs& args) override
    {
        long double sum[] = { 0.0, 0.0 };

        // for each input
        for (int x = 0; x < 3; x++) {

            int numChannels = inputs[IN_INPUTS + x].getChannels();
            outputs[THRU_OUTPUTS + x].setChannels(numChannels);

            if (inputs[IN_INPUTS + x].isConnected()) {

                // for each poly channel
                for (int i = 0; i < std::max(1, numChannels); i++) {

                    // get input
                    long double inputSample = inputs[IN_INPUTS + x].getPolyVoltage(i);

                    // first we send the input to the respective Thru output
                    outputs[THRU_OUTPUTS + x].setVoltage(inputSample, i);

                    // only process if there is a signal
                    if (inputSample) {

                        // pad gain, will be boosted before output
                        inputSample *= gainCut;

                        if (quality == HIGH) {
                            if (fabs(inputSample) < 1.18e-37)
                                inputSample = fpd[i] * 1.18e-37;
                        }

                        // encode
                        inputSample = encode(inputSample, consoleType);

                        // add alternately to left or right sum
                        sum[i % 2] += inputSample;
                    }
                }
            }
        }

        // for each output
        for (int i = 0; i < 2; i++) {

            if (outputs[OUT_OUTPUTS + i].isConnected()) {

                // decode
                sum[i] = decode(sum[i], consoleType);

                if (quality == HIGH) {
                    // 32 bit floating point dither
                    int expon;
                    frexpf((float)sum[i], &expon);
                    fpd[i] ^= fpd[i] << 13;
                    fpd[i] ^= fpd[i] >> 17;
                    fpd[i] ^= fpd[i] << 5;
                    sum[i] += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                }

                // bring gain back up
                sum[i] *= gainBoost;
            }

            // outpul level control
            sum[i] *= params[LEVEL_PARAM].getValue() * 2;

            // outputs
            outputs[OUT_OUTPUTS + i].setVoltage(sum[i]);
        }
    }
};

struct Console_mmWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Console_mm* module;
        int quality;

        void onAction(const event::Action& e) override
        {
            module->quality = quality;
        }

        void step() override
        {
            rightText = (module->quality == quality) ? "✔" : "";
        }
    };

    // console type item
    struct ConsoleTypeItem : MenuItem {
        Console_mm* module;
        int consoleType;

        void onAction(const event::Action& e) override
        {
            module->consoleType = consoleType;
        }

        void step() override
        {
            rightText = (module->consoleType == consoleType) ? "✔" : "";
        }
    };

    void appendContextMenu(Menu* menu) override
    {
        Console_mm* module = dynamic_cast<Console_mm*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator()); // separator

        MenuLabel* qualityLabel = new MenuLabel(); // menu label
        qualityLabel->text = "Quality";
        menu->addChild(qualityLabel);

        QualityItem* low = new QualityItem(); // low quality
        low->text = "Eco";
        low->module = module;
        low->quality = 0;
        menu->addChild(low);

        QualityItem* high = new QualityItem(); // high quality
        high->text = "High";
        high->module = module;
        high->quality = 1;
        menu->addChild(high);

        menu->addChild(new MenuSeparator()); // separator

        MenuLabel* consoleTypeLabel = new MenuLabel(); // menu label
        consoleTypeLabel->text = "Type";
        menu->addChild(consoleTypeLabel);

        ConsoleTypeItem* console6 = new ConsoleTypeItem(); // Console6
        console6->text = "Console6";
        console6->module = module;
        console6->consoleType = 0;
        menu->addChild(console6);

        ConsoleTypeItem* purestConsole = new ConsoleTypeItem(); // PurestConsole
        purestConsole->text = "PurestConsole";
        purestConsole->module = module;
        purestConsole->consoleType = 1;
        menu->addChild(purestConsole);
    }

    Console_mmWidget(Console_mm* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/console_mm_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // params
        addParam(createParamCentered<RwKnobLargeDark>(Vec(45.0, 260.0), module, Console_mm::LEVEL_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 55.0), module, Console_mm::IN_INPUTS + 0));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 95.0), module, Console_mm::IN_INPUTS + 1));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 135.0), module, Console_mm::IN_INPUTS + 2));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.695, 55.0), module, Console_mm::THRU_OUTPUTS + 0));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.695, 95.0), module, Console_mm::THRU_OUTPUTS + 1));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 135.0), module, Console_mm::THRU_OUTPUTS + 2));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Console_mm::OUT_OUTPUTS + 0));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Console_mm::OUT_OUTPUTS + 1));
    }
};

Model* modelConsole_mm = createModel<Console_mm, Console_mmWidget>("console_mm");