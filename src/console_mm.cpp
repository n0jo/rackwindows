/***********************************************************************************************
Console MM
----------
VCV Rack module based on Console by Chris Johnson from Airwindows <www.airwindows.com>, but specifically designed
to work in conjunction with MindMeld's MixMaster module. It takes the polyphonic direct outs of MixMaster, encodes and sums
the individual channels, then decodes and outputs a single stereo sum.

Ported and designed by Jens Robert Janke

Changes/Additions:
- encodes, sums and decodes the direct outs of MixMaster
- additional direct outputs, unprocessed or summed
- console types: Console6, PurestConsole
- partly polyphonic

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
        ENUMS(DIRECT_OUTPUTS, 3),
        ENUMS(OUT_OUTPUTS, 2),
        OUT_L_OUTPUT,
        OUT_R_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // module variables
    const double gainFactor = 32;
    bool quality;
    int consoleType;
    dsp::VuMeter2 vuMeters[9];
    dsp::ClockDivider lightDivider;
    enum directOutModes {
        UNPROCESSED,
        SUMMED
    };
    int directOutMode;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    uint32_t fpd[16];

    Console_mm()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LEVEL_PARAM, 0.f, 1.f, 1.f, "Level", " dB", -10, 20.0f * 3);

        quality = loadQuality();
        consoleType = loadConsoleType();
        directOutMode = loadDirectOutMode();
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

        // directOutMode
        json_object_set_new(rootJ, "directOutMode", json_integer(directOutMode));

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

        // directOutMode
        json_t* directOutModeJ = json_object_get(rootJ, "directOutMode");
        if (directOutModeJ)
            directOutMode = json_integer_value(directOutModeJ);

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
        long double directOutSum[] = { 0.0, 0.0, 0.0 };
        long double stereoOutSum[] = { 0.0, 0.0 };

        // for each input
        for (int x = 0; x < 3; x++) {

            int numChannels = inputs[IN_INPUTS + x].getChannels();
            outputs[DIRECT_OUTPUTS + x].setChannels(numChannels);

            if (inputs[IN_INPUTS + x].isConnected()) {

                // for each poly channel
                for (int i = 0; i < std::max(1, numChannels); i++) {

                    // get input
                    long double inputSample = inputs[IN_INPUTS + x].getPolyVoltage(i);

                    if (directOutMode == UNPROCESSED) {
                        // send the input directly to the respective output
                        outputs[DIRECT_OUTPUTS + x].setVoltage(inputSample, i);
                    }

                    // only process if there is a signal
                    if (inputSample) {

                        // pad gain, will be boosted before output
                        inputSample /= gainFactor;

                        if (quality == HIGH) {
                            if (fabs(inputSample) < 1.18e-37)
                                inputSample = fpd[i] * 1.18e-37;
                        }

                        // encode
                        inputSample = encode(inputSample, consoleType);

                        // add alternately to the left or right channel of the stereo sum
                        stereoOutSum[i % 2] += inputSample;

                        if (directOutMode == SUMMED) {
                            // add processed sample to respective output sum
                            directOutSum[x] += inputSample;
                        }
                    }
                }
            }
        }

        if (directOutMode == SUMMED) {
            // for each direct output in summing mode
            for (int i = 0; i < 3; i++) {
                if (outputs[DIRECT_OUTPUTS + i].isConnected()) {

                    // decode
                    directOutSum[i] = decode(directOutSum[i], consoleType);

                    if (quality == HIGH) {
                        // 32 bit floating point dither
                        int expon;
                        frexpf((float)directOutSum[i], &expon);
                        fpd[i] ^= fpd[i] << 13;
                        fpd[i] ^= fpd[i] >> 17;
                        fpd[i] ^= fpd[i] << 5;
                        directOutSum[i] += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                    }

                    // bring gain back up
                    directOutSum[i] *= gainFactor;
                }

                // outputs
                outputs[DIRECT_OUTPUTS + i].setVoltage(directOutSum[i]);
            }
        }

        // for each stereo channel
        for (int i = 0; i < 2; i++) {

            if (outputs[OUT_OUTPUTS + i].isConnected()) {

                // decode
                stereoOutSum[i] = decode(stereoOutSum[i], consoleType);

                if (quality == HIGH) {
                    // 32 bit floating point dither
                    int expon;
                    frexpf((float)stereoOutSum[i], &expon);
                    fpd[i] ^= fpd[i] << 13;
                    fpd[i] ^= fpd[i] >> 17;
                    fpd[i] ^= fpd[i] << 5;
                    stereoOutSum[i] += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                }

                // bring gain back up
                stereoOutSum[i] *= gainFactor;
            }

            // outpul level control
            stereoOutSum[i] *= pow(params[LEVEL_PARAM].getValue(), 3);

            // outputs
            outputs[OUT_OUTPUTS + i].setVoltage(stereoOutSum[i]);
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

    // directoutMode item
    struct DirectOutModeItem : MenuItem {
        Console_mm* module;
        int directOutMode;

        void onAction(const event::Action& e) override
        {
            module->directOutMode = directOutMode;
        }

        void step() override
        {
            rightText = (module->directOutMode == directOutMode) ? "✔" : "";
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
        consoleTypeLabel->text = "Console Type";
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

        menu->addChild(new MenuSeparator()); // separator

        MenuLabel* directOutModeLabel = new MenuLabel(); // menu label
        directOutModeLabel->text = "Direct Output Mode";
        menu->addChild(directOutModeLabel);

        DirectOutModeItem* unprocessed = new DirectOutModeItem(); // unprocessed
        unprocessed->text = "Unprocessed";
        unprocessed->module = module;
        unprocessed->directOutMode = 0;
        menu->addChild(unprocessed);

        DirectOutModeItem* summed = new DirectOutModeItem(); // summed
        summed->text = "Summed";
        summed->module = module;
        summed->directOutMode = 1;
        menu->addChild(summed);
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
        addParam(createParamCentered<RwKnobLargeDark>(Vec(45.0, 310.0), module, Console_mm::LEVEL_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 75.0), module, Console_mm::IN_INPUTS + 0));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 115.0), module, Console_mm::IN_INPUTS + 1));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 155.0), module, Console_mm::IN_INPUTS + 2));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 75.0), module, Console_mm::DIRECT_OUTPUTS + 0));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 115.0), module, Console_mm::DIRECT_OUTPUTS + 1));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 155.0), module, Console_mm::DIRECT_OUTPUTS + 2));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 245.0), module, Console_mm::OUT_OUTPUTS + 0));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 245.0), module, Console_mm::OUT_OUTPUTS + 1));
    }
};

Model* modelConsole_mm = createModel<Console_mm, Console_mmWidget>("console_mm");