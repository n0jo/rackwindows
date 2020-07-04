/***********************************************************************************************
Chorus
------
VCV Rack module based on Chorus by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- ensemble switch: changes behaviour to ChorusEnsemble
- CV inputs for speed and range
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Chorus : Module {
    enum ParamIds {
        SPEED_PARAM,
        RANGE_PARAM,
        DRYWET_PARAM,
        ENSEMBLE_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SPEED_CV_INPUT,
        RANGE_CV_INPUT,
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
        ENSEMBLE_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    bool isEnsemble;
    dsp::ClockDivider partTimeJob;

    // control parameters
    float speedParam;
    float rangeParam;
    float drywetParam;

    // global variables (as arrays in order to handle up to 16 polyphonic channels)
    const static int totalsamples = 16386;
    float d[16][totalsamples];
    double sweepL[16];
    double sweepR[16];
    int gcountL[16];
    int gcountR[16];
    double airPrevL[16];
    double airEvenL[16];
    double airOddL[16];
    double airFactorL[16];
    double airPrevR[16];
    double airEvenR[16];
    double airOddR[16];
    double airFactorR[16];
    bool fpFlipL[16];
    bool fpFlipR[16];
    long double fpNShapeL[16];
    long double fpNShapeR[16];

    // part-time variables (which do not need to be updated every cycle)
    double overallscale;

    Chorus()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.5f, "Speed");
        configParam(RANGE_PARAM, 0.f, 1.f, 0.f, "Range");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");
        configParam(ENSEMBLE_PARAM, 0.f, 1.f, 0.f, "Ensemble");

        quality = 1;
        quality = loadQuality();
        isEnsemble = false;

        partTimeJob.setDivision(2);

        onSampleRateChange();
        updateParams();

        for (int i = 0; i < 16; i++) {
            for (int count = 0; count < totalsamples - 1; count++) {
                d[i][count] = 0;
            }
            sweepL[i] = 3.141592653589793238 / 2.0;
            sweepR[i] = 3.141592653589793238 / 2.0;
            gcountL[i] = 0;
            gcountR[i] = 0;
            airPrevL[i] = 0.0;
            airPrevR[i] = 0.0;
            airEvenL[i] = 0.0;
            airEvenR[i] = 0.0;
            airOddL[i] = 0.0;
            airOddR[i] = 0.0;
            airFactorL[i] = 0.0;
            airFactorR[i] = 0.0;
            fpFlipL[i] = true;
            fpFlipR[i] = true;
            fpNShapeL[i] = 0.0;
            fpNShapeR[i] = 0.0;
        }
        //this is reset: values being initialized only once. Startup values, whatever they are.
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
        speedParam = params[SPEED_PARAM].getValue();
        speedParam += inputs[SPEED_CV_INPUT].getVoltage() / 5;
        speedParam = clamp(speedParam, 0.01f, 0.99f);

        rangeParam = params[RANGE_PARAM].getValue();
        rangeParam += inputs[RANGE_CV_INPUT].getVoltage() / 5;
        rangeParam = clamp(rangeParam, 0.01f, 0.99f);

        drywetParam = params[DRYWET_PARAM].getValue();
    }

    void processChannel(Input& input, Output& output, double sweep[], int gcount[], double airPrev[], double airEven[], double airOdd[], double airFactor[], bool fpFlip[], long double fpNShape[])
    {
        if (input.isConnected()) {

            double speed = 0.0;
            double range = 0.0;
            double start[4];
            int loopLimit = (int)(totalsamples * 0.499);

            if (isEnsemble) {
                speed = pow(speedParam, 3) * 0.001;
                range = pow(rangeParam, 3) * loopLimit * 0.12;
                // start[4];

                //now we'll precalculate some stuff that needn't be in every sample
                start[0] = range;
                start[1] = range * 2;
                start[2] = range * 3;
                start[3] = range * 4;
            } else {
                speed = pow(speedParam, 4) * 0.001;
                range = pow(rangeParam, 4) * loopLimit * 0.499;
            }

            int count;
            double wet = drywetParam;
            double modulation = range * wet;
            double dry = 1.0 - wet;
            double tupi = 3.141592653589793238 * 2.0;
            double offset;
            //this is a double buffer so we will be splitting it in two

            double drySample;

            speed *= overallscale;

            // input
            int numChannels = input.getChannels();

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                long double inputSample = input.getPolyVoltage(i);

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

                drySample = inputSample;

                airFactor[i] = airPrev[i] - inputSample;
                if (fpFlip[i]) {
                    airEven[i] += airFactor[i];
                    airOdd[i] -= airFactor[i];
                    airFactor[i] = airEven[i];
                } else {
                    airOdd[i] += airFactor[i];
                    airEven[i] -= airFactor[i];
                    airFactor[i] = airOdd[i];
                }
                airOdd[i] = (airOdd[i] - ((airOdd[i] - airEven[i]) / 256.0)) / 1.0001;
                airEven[i] = (airEven[i] - ((airEven[i] - airOdd[i]) / 256.0)) / 1.0001;
                airPrev[i] = inputSample;
                inputSample += (airFactor[i] * wet);
                //air, compensates for loss of highs in flanger's interpolation

                if (gcount[i] < 1 || gcount[i] > loopLimit) {
                    gcount[i] = loopLimit;
                }
                count = gcount[i];
                d[i][count + loopLimit] = d[i][count] = inputSample;
                gcount[i]--;
                //double buffer

                if (isEnsemble) {
                    offset = start[0] + (modulation * sin(sweep[i]));
                    count = gcount[i] + (int)floor(offset);

                    inputSample = d[i][count] * (1 - (offset - floor(offset))); //less as value moves away from .0
                    inputSample += d[i][count + 1]; //we can assume always using this in one way or another?
                    inputSample += (d[i][count + 2] * (offset - floor(offset))); //greater as value moves away from .0
                    inputSample -= (((d[i][count] - d[i][count + 1]) - (d[i][count + 1] - d[i][count + 2])) / 50); //interpolation hacks 'r us

                    offset = start[1] + (modulation * sin(sweep[i] + 1.0));
                    count = gcount[i] + (int)floor(offset);
                    inputSample += d[i][count] * (1 - (offset - floor(offset))); //less as value moves away from .0
                    inputSample += d[i][count + 1]; //we can assume always using this in one way or another?
                    inputSample += (d[i][count + 2] * (offset - floor(offset))); //greater as value moves away from .0
                    inputSample -= (((d[i][count] - d[i][count + 1]) - (d[i][count + 1] - d[i][count + 2])) / 50); //interpolation hacks 'r us

                    offset = start[2] + (modulation * sin(sweep[i] + 2.0));
                    count = gcount[i] + (int)floor(offset);
                    inputSample += d[i][count] * (1 - (offset - floor(offset))); //less as value moves away from .0
                    inputSample += d[i][count + 1]; //we can assume always using this in one way or another?
                    inputSample += (d[i][count + 2] * (offset - floor(offset))); //greater as value moves away from .0
                    inputSample -= (((d[i][count] - d[i][count + 1]) - (d[i][count + 1] - d[i][count + 2])) / 50); //interpolation hacks 'r us

                    offset = start[3] + (modulation * sin(sweep[i] + 3.0));
                    count = gcount[i] + (int)floor(offset);
                    inputSample += d[i][count] * (1 - (offset - floor(offset))); //less as value moves away from .0
                    inputSample += d[i][count + 1]; //we can assume always using this in one way or another?
                    inputSample += (d[i][count + 2] * (offset - floor(offset))); //greater as value moves away from .0
                    inputSample -= (((d[i][count] - d[i][count + 1]) - (d[i][count + 1] - d[i][count + 2])) / 50); //interpolation hacks 'r us

                    inputSample *= 0.25; // to get a comparable level

                } else {

                    offset = range + (modulation * sin(sweep[i]));
                    count += (int)floor(offset);

                    inputSample = d[i][count] * (1 - (offset - floor(offset))); //less as value moves away from .0
                    inputSample += d[i][count + 1]; //we can assume always using this in one way or another?
                    inputSample += (d[i][count + 2] * (offset - floor(offset))); //greater as value moves away from .0
                    inputSample -= (((d[i][count] - d[i][count + 1]) - (d[i][count + 1] - d[i][count + 2])) / 50); //interpolation hacks 'r us

                    inputSample *= 0.5; // to get a comparable level
                    //sliding
                }

                sweep[i] += speed;
                if (sweep[i] > tupi) {
                    sweep[i] -= tupi;
                }
                //still scrolling through the samples, remember

                if (wet != 1.0) {
                    inputSample = (inputSample * wet) + (drySample * dry);
                }
                fpFlip[i] = !fpFlip[i];

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
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }
    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {

            if (partTimeJob.process()) {
                // stuff that doesn't need to be processed every cycle
                updateParams();
            }

            // process L
            processChannel(inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT], sweepL, gcountL, airPrevL, airEvenL, airOddL, airFactorL, fpFlipL, fpNShapeL);
            // process R
            processChannel(inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT], sweepR, gcountR, airPrevR, airEvenR, airOddR, airFactorR, fpFlipR, fpNShapeR);
        }

        // ensemble light
        isEnsemble = params[ENSEMBLE_PARAM].getValue() ? true : false;
        lights[ENSEMBLE_LIGHT].setBrightness(isEnsemble);
    }
};

struct ChorusWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Chorus* module;
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
        Chorus* module = dynamic_cast<Chorus*>(this->module);
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

    ChorusWidget(Chorus* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/chorus_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 65.0), module, Chorus::SPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 125.0), module, Chorus::RANGE_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 185.0), module, Chorus::DRYWET_PARAM));

        // switches
        addParam(createParamCentered<RwCKSS>(Vec(75.0, 155.0), module, Chorus::ENSEMBLE_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(75, 136.8), module, Chorus::ENSEMBLE_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245), module, Chorus::SPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245), module, Chorus::RANGE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285), module, Chorus::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285), module, Chorus::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325), module, Chorus::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325), module, Chorus::OUT_R_OUTPUT));
    }
};

Model* modelChorus = createModel<Chorus, ChorusWidget>("chorus");