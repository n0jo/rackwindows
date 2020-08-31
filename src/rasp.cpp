/***********************************************************************************************
Rasp
----
VCV Rack module based on Slew/Slew2/Slew3 and Acceleration by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono
- cv inputs for clamp and limit
- clamp and limit outputs normalled to each other
- slew algorithm selectable via menu
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"
#include "rwlib.h"

// quality options
#define ECO 0
#define HIGH 1

// slew types
#define SLEW2 0
#define SLEW 1
#define SLEW3 2

// polyphony
#define MAX_POLY_CHANNELS 16

struct Rasp : Module {
    enum ParamIds {
        CLAMP_PARAM,
        LIMIT_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        CLAMP_CV_INPUT,
        LIMIT_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        CLAMP_OUTPUT,
        LIMIT_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.1;
    const double gainBoost = 10.0;
    bool quality;
    int slewType;

    // control parameters
    float clampParam;
    float limitParam;

    // state variables
    rwlib::Slew slew[MAX_POLY_CHANNELS];
    rwlib::Slew2 slew2[MAX_POLY_CHANNELS];
    rwlib::Slew3 slew3[MAX_POLY_CHANNELS];
    rwlib::Acceleration acceleration[MAX_POLY_CHANNELS];
    long double fpNShapeClamp[MAX_POLY_CHANNELS];
    long double fpNShapeLimit[MAX_POLY_CHANNELS];

    // other
    double overallscale;

    Rasp()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(CLAMP_PARAM, 0.f, 1.f, 0.f, "Clamp");
        configParam(LIMIT_PARAM, 0.f, 1.f, 0.f, "Limit");

        quality = loadQuality();
        slewType = loadSlewType();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        clampParam = 0.f;
        limitParam = 0.f;

        for (int i = 0; i < MAX_POLY_CHANNELS; i++) {
            {
                slew[i] = rwlib::Slew();
                slew2[i] = rwlib::Slew2();
                slew3[i] = rwlib::Slew3();
                acceleration[i] = rwlib::Acceleration();
                fpNShapeClamp[i] = 0.0;
                fpNShapeLimit[i] = 0.0;
            }
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();

        // quality
        json_object_set_new(rootJ, "quality", json_integer(quality));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
        // quality
        json_t* qualityJ = json_object_get(rootJ, "quality");
        if (qualityJ)
            quality = json_integer_value(qualityJ);
    }

    void process(const ProcessArgs& args) override
    {
        // get params
        limitParam = params[LIMIT_PARAM].getValue();
        limitParam += inputs[LIMIT_CV_INPUT].getVoltage() / 5;
        limitParam = clamp(limitParam, 0.f, 1.f);

        clampParam = params[CLAMP_PARAM].getValue();
        clampParam += inputs[CLAMP_CV_INPUT].getVoltage() / 5;
        clampParam = clamp(clampParam, 0.f, 1.f);

        long double inputSample;
        long double clampSample = 0.0;
        long double limitSample = 0.0;

        // for each poly channel
        for (int i = 0, numChannels = std::max(1, inputs[IN_INPUT].getChannels()); i < numChannels; ++i) {

            // get input
            inputSample = inputs[IN_INPUT].getPolyVoltage(i);

            // pad gain
            inputSample *= gainCut;

            if (quality == HIGH) {
                inputSample = rwlib::denormalize(inputSample);
            }

            // work the magic
            if (outputs[CLAMP_OUTPUT].isConnected()) {
                if (outputs[LIMIT_OUTPUT].isConnected()) {
                    switch (slewType) {
                    case SLEW:
                        clampSample = slew[i].process(inputSample, clampParam, overallscale);
                        break;
                    case SLEW2:
                        clampSample = slew2[i].process(inputSample, clampParam, overallscale);
                        break;
                    case SLEW3:
                        clampSample = slew3[i].process(inputSample, clampParam, overallscale);
                    }
                } else {
                    limitSample = acceleration[i].process(inputSample, limitParam, 1.f, overallscale);
                    switch (slewType) {
                    case SLEW:
                        clampSample = slew[i].process(limitSample, clampParam, overallscale);
                        break;
                    case SLEW2:
                        clampSample = slew2[i].process(limitSample, clampParam, overallscale);
                        break;
                    case SLEW3:
                        clampSample = slew3[i].process(limitSample, clampParam, overallscale);
                    }
                }
            }
            if (outputs[LIMIT_OUTPUT].isConnected()) {
                if (outputs[CLAMP_OUTPUT].isConnected()) {
                    limitSample = acceleration[i].process(inputSample, limitParam, 1.f, overallscale);
                } else {
                    switch (slewType) {
                    case SLEW:
                        clampSample = slew[i].process(inputSample, clampParam, overallscale);
                        break;
                    case SLEW2:
                        clampSample = slew2[i].process(inputSample, clampParam, overallscale);
                        break;
                    case SLEW3:
                        clampSample = slew3[i].process(inputSample, clampParam, overallscale);
                    }
                    limitSample = acceleration[i].process(clampSample, limitParam, 1.f, overallscale);
                }
            }

            if (quality == HIGH) {
                // 32 bit dither, made small and tidy.
                int expon;
                frexpf((float)clampSample, &expon);
                long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                clampSample += (dither - fpNShapeClamp[i]);
                fpNShapeClamp[i] = dither;
                frexpf((float)limitSample, &expon);
                dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                limitSample += (dither - fpNShapeLimit[i]);
                fpNShapeLimit[i] = dither;
            }

            // bring levels back up
            clampSample *= gainBoost;
            limitSample *= gainBoost;

            // output
            outputs[CLAMP_OUTPUT].setChannels(numChannels);
            outputs[CLAMP_OUTPUT].setVoltage(clampSample, i);
            outputs[LIMIT_OUTPUT].setChannels(numChannels);
            outputs[LIMIT_OUTPUT].setVoltage(limitSample, i);
        }
    }
};

struct RaspWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Rasp* module;
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

    // slew type item
    struct SlewTypeItem : MenuItem {
        Rasp* module;
        int slewType;

        void onAction(const event::Action& e) override
        {
            module->slewType = slewType;
        }

        void step() override
        {
            rightText = (module->slewType == slewType) ? "✔" : "";
        }
    };

    void appendContextMenu(Menu* menu) override
    {
        Rasp* module = dynamic_cast<Rasp*>(this->module);
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

        MenuLabel* slewLabel = new MenuLabel(); // menu label
        slewLabel->text = "Slew Type";
        menu->addChild(slewLabel);

        SlewTypeItem* slew2 = new SlewTypeItem(); // Slew2
        slew2->text = "Slew2";
        slew2->module = module;
        slew2->slewType = SLEW2;
        menu->addChild(slew2);

        SlewTypeItem* slew3 = new SlewTypeItem(); // Slew3
        slew3->text = "Slew3";
        slew3->module = module;
        slew3->slewType = SLEW3;
        menu->addChild(slew3);

        SlewTypeItem* slew = new SlewTypeItem(); // Slew
        slew->text = "Slew";
        slew->module = module;
        slew->slewType = SLEW;
        menu->addChild(slew);
    }

    RaspWidget(Rasp* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/rasp_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        //params
        addParam(createParamCentered<RwKnobSmallDark>(Vec(30.0, 65.0), module, Rasp::CLAMP_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(30.0, 115.0), module, Rasp::LIMIT_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 165.0), module, Rasp::CLAMP_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Rasp::LIMIT_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Rasp::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 285.0), module, Rasp::CLAMP_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Rasp::LIMIT_OUTPUT));
    }
};

Model* modelRasp = createModel<Rasp, RaspWidget>("rasp");