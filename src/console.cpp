/***********************************************************************************************
Console
------
VCV Rack module based on Console by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- console channel and buss combined into an 8-channel stereo summing mixer
- no gain control
- polyphonic
- console types: Console6, PurestConsole

Additional code heavily inspired by the Fundamental Mixer by Andrew Belt.
Some UI elements based on graphics from the Component Library by Wes Milholen.

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Console : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(IN_L_INPUTS, 9),
        ENUMS(IN_R_INPUTS, 9),
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
        ENUMS(VU_LIGHTS, 9),
        NUM_LIGHTS
    };

    // Constants
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;

    // Need to save, no reset
    // int panelTheme;

    // Need to save, with reset
    bool quality;
    int consoleType;

    // No need to save, with reset

    // No need to save, no reset
    uint32_t fpd[16];

    dsp::VuMeter2 vuMeters[9];
    dsp::ClockDivider lightDivider;

    Console()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        onReset();

        quality = loadQuality();
        consoleType = loadConsoleType();

        // panelTheme = (loadDarkAsDefault() ? 1 : 0);

        for (int i = 0; i < 16; i++) {
            fpd[i] = 17;
        }

        lightDivider.setDivision(512);
    }

    void onReset() override
    {
        resetNonJson(false);
    }

    void resetNonJson(bool recurseNonJson)
    {
    }

    void onRandomize() override
    {
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();

        // quality
        json_object_set_new(rootJ, "quality", json_integer(quality));

        // consoleType
        json_object_set_new(rootJ, "consoleType", json_integer(consoleType));

        // panelTheme
        // json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

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

        // panelTheme
        // json_t* panelThemeJ = json_object_get(rootJ, "panelTheme");
        // if (panelThemeJ)
        //     panelTheme = json_integer_value(panelThemeJ);

        resetNonJson(true);
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

    float consoleChannel(Input& input, long double mix[], int numChannels)
    {
        if (input.isConnected()) {
            float sum = 0.0f;

            // input
            float inputSamples[16] = {};
            input.readVoltages(inputSamples);

            for (int i = 0; i < numChannels; i++) {

                long double inputSample = inputSamples[i];

                // calculate sum for VU meter
                sum += inputSample;

                // pad gain, will be boosted in consoleBuss()
                inputSample *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSample) < 1.18e-37)
                        inputSample = fpd[i] * 1.18e-37;
                }

                // encode
                inputSample = encode(inputSample, consoleType);

                // add to mix
                mix[i] += inputSample;
            }
            return sum;
        }
    }

    void consoleBuss(Output& output, long double mix[], int maxChannels)
    {
        if (output.isConnected()) {
            float out[16] = {};

            for (int i = 0; i < maxChannels; i++) {
                long double inputSample = mix[i];

                // decode
                inputSample = decode(inputSample, consoleType);

                if (quality == HIGH) {
                    //begin 32 bit stereo floating point dither
                    int expon;
                    frexpf((float)inputSample, &expon);
                    fpd[i] ^= fpd[i] << 13;
                    fpd[i] ^= fpd[i] >> 17;
                    fpd[i] ^= fpd[i] << 5;
                    inputSample += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                    //end 32 bit stereo floating point dither
                }

                // bring gain back up
                inputSample *= gainBoost;

                out[i] = (float)inputSample;
            }

            output.setChannels(maxChannels);
            output.writeVoltages(out);
        }
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {
            long double mixL[16] = {};
            long double mixR[16] = {};
            float sumL = 0.0;
            float sumR = 0.0;
            int numChannelsL = 1;
            int numChannelsR = 1;
            int maxChannelsL = 1;
            int maxChannelsR = 1;

            // for each mixer channel
            for (int i = 0; i < 9; i++) {
                numChannelsL = inputs[IN_L_INPUTS + i].getChannels();
                maxChannelsL = std::max(maxChannelsL, numChannelsL);
                sumL = consoleChannel(inputs[IN_L_INPUTS + i], mixL, numChannelsL); // encode L

                numChannelsR = inputs[IN_R_INPUTS + i].getChannels();
                maxChannelsR = std::max(maxChannelsR, numChannelsR);
                sumR = consoleChannel(inputs[IN_R_INPUTS + i], mixR, numChannelsR); // encode R

                // channel VU light
                vuMeters[i].process(args.sampleTime, (sumL + sumR) / 5.f);
                if (lightDivider.process()) {
                    float b = vuMeters[i].getBrightness(-18.f, 0.f);
                    lights[VU_LIGHTS + i].setBrightness(b);
                }
            }

            consoleBuss(outputs[OUT_L_OUTPUT], mixL, maxChannelsL); // decode L
            consoleBuss(outputs[OUT_R_OUTPUT], mixR, maxChannelsR); // decode R
        }
    }
};

struct ConsoleWidget : ModuleWidget {

    SvgPanel* darkPanel;

    // quality item
    struct QualityItem : MenuItem {
        Console* module;
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
        Console* module;
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

    // struct PanelThemeItem : MenuItem {
    //     Console* module;
    //     int theme;

    //     void onAction(const event::Action& e) override
    //     {
    //         module->panelTheme = theme;
    //     }

    //     void step() override
    //     {
    //         rightText = (module->panelTheme == theme) ? "✔" : "";
    //     }
    // };

    void appendContextMenu(Menu* menu) override
    {
        Console* module = dynamic_cast<Console*>(this->module);
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

        // menu->addChild(new MenuSeparator()); // separator

        // MenuLabel* themeLabel = new MenuLabel(); // menu label
        // themeLabel->text = "Theme";
        // menu->addChild(themeLabel);

        // PanelThemeItem* lightItem = new PanelThemeItem(); // light theme
        // lightItem->text = lightPanelID; // plugin.hpp
        // lightItem->module = module;
        // lightItem->theme = 0;
        // menu->addChild(lightItem);

        // PanelThemeItem* darkItem = new PanelThemeItem(); // dark theme
        // darkItem->text = darkPanelID; // plugin.hpp
        // darkItem->module = module;
        // darkItem->theme = 1;
        // menu->addChild(darkItem);

        // menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault()))); // dark theme as default
    }

    ConsoleWidget(Console* module)
    {
        setModule(module);

        // panel
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/console_dark.svg")));
        // if (module) {
        //     darkPanel = new SvgPanel();
        //     darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/console_light.svg")));
        //     darkPanel->visible = false;
        //     addChild(darkPanel);
        // }

        float center = box.size.x / 2.0f;
        // float col1 = center - box.size.x / 4.0f;
        // float col3 = center + box.size.x / 4.0f;

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 55.0), module, Console::VU_LIGHTS + 0));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 85.0), module, Console::VU_LIGHTS + 1));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 115.0), module, Console::VU_LIGHTS + 2));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 145.0), module, Console::VU_LIGHTS + 3));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 175.0), module, Console::VU_LIGHTS + 4));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 205.0), module, Console::VU_LIGHTS + 5));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 235.0), module, Console::VU_LIGHTS + 6));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 265.0), module, Console::VU_LIGHTS + 7));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(center, 295.0), module, Console::VU_LIGHTS + 8));

        // inputs
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
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 295.0), module, Console::IN_L_INPUTS + 8));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 295.0), module, Console::IN_R_INPUTS + 8));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Console::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Console::OUT_R_OUTPUT));
    }

    // void step() override
    // {
    //     if (module) {
    //         panel->visible = ((((Console*)module)->panelTheme) == 0);
    //         darkPanel->visible = ((((Console*)module)->panelTheme) == 1);
    //     }
    //     Widget::step();
    // }
};

Model* modelConsole = createModel<Console, ConsoleWidget>("console");