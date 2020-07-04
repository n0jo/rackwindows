/***********************************************************************************************
Distance
--------
VCV Rack module based on Distance by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono
- CV inputs for distance and dry/wet
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Distance : Module {
    enum ParamIds {
        DISTANCE_PARAM,
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        DISTANCE_CV_INPUT,
        DRYWET_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    dsp::ClockDivider partTimeJob;

    // control parameters
    float distanceParam;
    float drywetParam;

    // global variables (as arrays in order to handle up to 16 polyphonic channels)
    double lastclamp[16];
    double clasp[16];
    double change[16];
    double thirdresult[16];
    double prevresult[16];
    double last[16];
    long double fpNShape[16];

    // part-time variables (which do not need to be updated every cycle)
    double overallscale;
    double softslew;
    double filtercorrect;
    double thirdfilter;
    double levelcorrect;
    double wet;
    double dry;

    Distance()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(DISTANCE_PARAM, 0.f, 1.f, 0.f, "Distance");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        quality = loadQuality();

        partTimeJob.setDivision(2);

        onSampleRateChange();
        updateParams();

        for (int i = 0; i < 16; i++) {
            thirdresult[i] = prevresult[i] = lastclamp[i] = clasp[i] = change[i] = last[i] = 0.0;
            fpNShape[i] = 0.0;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;
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

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
        // quality
        json_t* qualityJ = json_object_get(rootJ, "quality");
        if (qualityJ)
            quality = json_integer_value(qualityJ);

        resetNonJson(true);
    }

    void updateParams()
    {
        distanceParam = params[DISTANCE_PARAM].getValue();
        distanceParam += inputs[DISTANCE_CV_INPUT].getVoltage() / 5;
        distanceParam = clamp(distanceParam, 0.01f, 0.99f);

        drywetParam = params[DRYWET_PARAM].getValue();
        drywetParam += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
        drywetParam = clamp(drywetParam, 0.01f, 0.99f);

        softslew = (pow(distanceParam * 2.0, 3.0) * 12.0) + 0.6;
        softslew *= overallscale;
        filtercorrect = softslew / 2.0;
        thirdfilter = softslew / 3.0;
        levelcorrect = 1.0 + (softslew / 6.0);
        wet = drywetParam;
        dry = 1.0 - wet;
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            // stuff that doesn't need to be processed every cycle
            if (partTimeJob.process()) {
                updateParams();
            }

            double postfilter;
            double bridgerectifier;
            long double inputSample;
            long double drySampleL;

            // number of polyphonic channels
            int numChannels = inputs[IN_INPUT].getChannels();

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (quality == 1) {
                    if (inputSample < 1.2e-38 && -inputSample < 1.2e-38) {
                        static int noisesource = 0;
                        //this declares a variable before anything else is compiled. It won't keep assigning
                        //it to 0 for every sample, it's as if the declaration doesn't exist in this context,
                        //but it lets me add this denormalization fix in a single place rather than updating
                        //it in three different locations. The variable isn't thread-safe but this is only
                        //a random seed and we can share it with whatever.
                        noisesource = noisesource % 1700021;
                        noisesource++;
                        int residue = noisesource * noisesource;
                        residue = residue % 170003;
                        residue *= residue;
                        residue = residue % 17011;
                        residue *= residue;
                        residue = residue % 1709;
                        residue *= residue;
                        residue = residue % 173;
                        residue *= residue;
                        residue = residue % 17;
                        double applyresidue = residue;
                        applyresidue *= 0.00000001;
                        applyresidue *= 0.00000001;
                        inputSample = applyresidue;
                    }
                }

                drySampleL = inputSample;

                inputSample *= softslew;
                lastclamp[i] = clasp[i];
                clasp[i] = inputSample - last[i];
                postfilter = change[i] = fabs(clasp[i] - lastclamp[i]);
                postfilter += filtercorrect;
                if (change[i] > 1.5707963267949)
                    change[i] = 1.5707963267949;
                bridgerectifier = (1.0 - sin(change[i]));
                if (bridgerectifier < 0.0)
                    bridgerectifier = 0.0;
                inputSample = last[i] + (clasp[i] * bridgerectifier);
                last[i] = inputSample;
                inputSample /= softslew;
                inputSample += (thirdresult[i] * thirdfilter);
                inputSample /= (thirdfilter + 1.0);
                inputSample += (prevresult[i] * postfilter);
                inputSample /= (postfilter + 1.0);
                //do an IIR like thing to further squish superdistant stuff
                thirdresult[i] = prevresult[i];
                prevresult[i] = inputSample;
                inputSample *= levelcorrect;

                if (wet < 1.0) {
                    inputSample = (drySampleL * dry) + (inputSample * wet);
                }

                if (quality == 1) {
                    //stereo 32 bit dither, made small and tidy.
                    int expon;
                    frexpf((float)inputSample, &expon);
                    long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                    inputSample += (dither - fpNShape[i]);
                    fpNShape[i] = dither;
                    //end 32 bit dither
                }

                // bring gain back up
                inputSample *= gainBoost;

                // output
                outputs[OUT_OUTPUT].setChannels(numChannels);
                outputs[OUT_OUTPUT].setVoltage(inputSample, i);
            }
        }
    }
};

struct DistanceWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Distance* module;
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
        Distance* module = dynamic_cast<Distance*>(this->module);
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

    DistanceWidget(Distance* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/distance_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Distance::DISTANCE_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Distance::DRYWET_PARAM));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Distance::DISTANCE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Distance::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Distance::IN_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Distance::OUT_OUTPUT));
    }
};

Model* modelDistance = createModel<Distance, DistanceWidget>("distance");