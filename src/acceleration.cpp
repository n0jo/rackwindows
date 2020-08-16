#include "plugin.hpp"
#include "rwlib.h"

// quality options
#define ECO 0
#define HIGH 1

// polyphony
#define MAX_POLY_CHANNELS 16

struct Acceleration : Module {
    enum ParamIds {
        LIMIT_PARAM,
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        LIMIT_CV_INPUT,
        DRYWET_CV_INPUT,
        ENUMS(IN_INPUTS, 2), // stereo in
        NUM_INPUTS
    };
    enum OutputIds {
        ENUMS(OUT_OUTPUTS, 2), // stereo out
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.1;
    const double gainBoost = 10.0;
    bool quality;

    // control parameters
    float limitParam;
    float drywetParam;

    // state variables
    rwlib::Acceleration acceleration[2][MAX_POLY_CHANNELS];
    long double fpNShape[2][MAX_POLY_CHANNELS];

    // other
    double overallscale;

    Acceleration()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LIMIT_PARAM, 0.f, 1.f, 0.f, "Limit", " %", 0, 100);
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet", " %", 0, 100);

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        limitParam = 0.f;
        drywetParam = 1.f;

        for (int i = 0; i < 2; i++) {
            for (int j = 0; j < MAX_POLY_CHANNELS; j++) {
                {
                    acceleration[i][j] = rwlib::Acceleration();
                    fpNShape[i][j] = 0.0;
                }
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

        drywetParam = params[DRYWET_PARAM].getValue();
        drywetParam += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
        drywetParam = clamp(drywetParam, 0.f, 1.f);

        long double inputSample;

        // for each audio channel
        for (int x = 0; x < 2; x++) {

            if (outputs[OUT_OUTPUTS + x].isConnected()) {

                // for each poly channel
                for (int i = 0, numChannels = std::max(1, inputs[IN_INPUTS + x].getChannels()); i < numChannels; ++i) {

                    // get input
                    inputSample = inputs[IN_INPUTS + x].getPolyVoltage(i);

                    // pad gain
                    inputSample *= gainCut;

                    if (quality == HIGH) {
                        inputSample = rwlib::denormalize(inputSample);
                    }

                    // work the magic
                    inputSample = acceleration[x][i].process(inputSample, limitParam, drywetParam, overallscale);

                    if (quality == HIGH) {
                        // 32 bit dither, made small and tidy.
                        int expon;
                        frexpf((float)inputSample, &expon);
                        long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                        inputSample += (dither - fpNShape[x][i]);
                        fpNShape[x][i] = dither;
                    }

                    // bring gain back up
                    inputSample *= gainBoost;

                    // output
                    outputs[OUT_OUTPUTS + x].setChannels(numChannels);
                    outputs[OUT_OUTPUTS + x].setVoltage(inputSample, i);
                }
            }
        }
    }
};

struct AccelerationWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Acceleration* module;
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

    void appendContextMenu(Menu* menu) override
    {
        Acceleration* module = dynamic_cast<Acceleration*>(this->module);
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
    }

    AccelerationWidget(Acceleration* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/acceleration_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // params
        addParam(createParamCentered<RwKnobLargeDark>(Vec(45.0, 75.0), module, Acceleration::LIMIT_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(45.0, 140.0), module, Acceleration::DRYWET_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.3, 245.0), module, Acceleration::LIMIT_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.8, 245.0), module, Acceleration::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.3, 285.0), module, Acceleration::IN_INPUTS + 0)); // left
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.8, 285.0), module, Acceleration::IN_INPUTS + 1)); // right

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.3, 325.0), module, Acceleration::OUT_OUTPUTS + 0)); // left
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.8, 325.0), module, Acceleration::OUT_OUTPUTS + 1)); // right
    }
};

Model* modelAcceleration = createModel<Acceleration, AccelerationWidget>("acceleration");