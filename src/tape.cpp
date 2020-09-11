/***********************************************************************************************
Tape
----
VCV Rack module based on Tape by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- cv inputs for slam and bump
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

// polyphony
#define MAX_POLY_CHANNELS 16

struct Tape : Module {
    enum ParamIds {
        SLAM_PARAM,
        BUMP_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SLAM_CV_INPUT,
        BUMP_CV_INPUT,
        IN_L_INPUT,
        IN_R_INPUT,
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

    // module variables
    const double gainCut = 0.1;
    const double gainBoost = 10.0;
    int quality;

    // control parameters
    float slamParam;
    float bumpParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    rwlib::Tape tapeL[MAX_POLY_CHANNELS];
    rwlib::Tape tapeR[MAX_POLY_CHANNELS];
    uint32_t fpdL[MAX_POLY_CHANNELS];
    uint32_t fpdR[MAX_POLY_CHANNELS];

    // other
    double overallscale;

    Tape()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SLAM_PARAM, 0.f, 1.f, 0.5f, "Slam", "%", 0, 100);
        configParam(BUMP_PARAM, 0.f, 1.f, 0.5f, "Bump", "%", 0, 100);

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        for (int i = 0; i < MAX_POLY_CHANNELS; i++) {
            tapeL[i] = rwlib::Tape();
            tapeR[i] = rwlib::Tape();

            fpdL[i] = fpdR[i] = 17;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        for (int i = 0; i < MAX_POLY_CHANNELS; i++) {
            tapeL[i].onSampleRateChange(overallscale);
            tapeR[i].onSampleRateChange(overallscale);
        }
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
        slamParam = params[SLAM_PARAM].getValue();
        slamParam += inputs[SLAM_CV_INPUT].getVoltage() / 10;
        slamParam = clamp(slamParam, 0.01f, 0.99f);

        bumpParam = params[BUMP_PARAM].getValue();
        bumpParam += inputs[BUMP_CV_INPUT].getVoltage() / 10;
        bumpParam = clamp(bumpParam, 0.01f, 0.99f);

        // number of polyphonic channels
        int numChannelsL = std::max(1, inputs[IN_L_INPUT].getChannels());
        int numChannelsR = std::max(1, inputs[IN_R_INPUT].getChannels());

        // process left channel
        if (outputs[OUT_L_OUTPUT].isConnected()) {

            // for each poly channel left
            for (int i = 0; i < numChannelsL; i++) {

                // input
                long double inputSampleL = inputs[IN_L_INPUT].getPolyVoltage(i);

                // pad gain
                inputSampleL *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSampleL) < 1.18e-37)
                        inputSampleL = fpdL[i] * 1.18e-37;
                }

                // work the magic
                inputSampleL = tapeL[i].process(inputSampleL, slamParam, bumpParam, overallscale);

                if (quality == HIGH) {
                    //32 bit stereo floating point dither
                    int expon;
                    frexpf((float)inputSampleL, &expon);
                    fpdL[i] ^= fpdL[i] << 13;
                    fpdL[i] ^= fpdL[i] >> 17;
                    fpdL[i] ^= fpdL[i] << 5;
                    inputSampleL += ((double(fpdL[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                }

                // bring gain back up
                inputSampleL *= gainBoost;

                // output
                outputs[OUT_L_OUTPUT].setChannels(numChannelsL);
                outputs[OUT_L_OUTPUT].setVoltage(inputSampleL, i);
            }
        }

        // process right channel
        if (outputs[OUT_R_OUTPUT].isConnected()) {

            // for each poly channel right
            for (int i = 0; i < numChannelsR; i++) {

                // input
                long double inputSampleR = inputs[IN_R_INPUT].getPolyVoltage(i);

                // pad gain
                inputSampleR *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSampleR) < 1.18e-37)
                        inputSampleR = fpdR[i] * 1.18e-37;
                }

                // work the magic
                inputSampleR = tapeR[i].process(inputSampleR, slamParam, bumpParam, overallscale);

                if (quality == HIGH) {
                    //32 bit stereo floating point dither
                    int expon;
                    frexpf((float)inputSampleR, &expon);
                    fpdR[i] ^= fpdR[i] << 13;
                    fpdR[i] ^= fpdR[i] >> 17;
                    fpdR[i] ^= fpdR[i] << 5;
                    inputSampleR += ((double(fpdR[i]) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                }

                // bring gain back up
                inputSampleR *= gainBoost;

                // output
                outputs[OUT_R_OUTPUT].setChannels(numChannelsR);
                outputs[OUT_R_OUTPUT].setVoltage(inputSampleR, i);
            }
        }
    }
};

struct TapeWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Tape* module;
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
        Tape* module = dynamic_cast<Tape*>(this->module);
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

    TapeWidget(Tape* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/tape_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobLargeDark>(Vec(45.0, 75.0), module, Tape::SLAM_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 145.0), module, Tape::BUMP_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245.0), module, Tape::SLAM_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245.0), module, Tape::BUMP_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285.0), module, Tape::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285.0), module, Tape::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Tape::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Tape::OUT_R_OUTPUT));
    }
};

Model* modelTape = createModel<Tape, TapeWidget>("tape");