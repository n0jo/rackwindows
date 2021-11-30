/***********************************************************************************************
Hombre
------
VCV Rack module based on Hombre by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- mono
- CV inputs for voicing and intensity
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Hombre : Module {
    enum ParamIds {
        VOICING_PARAM,
        INTENSITY_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        VOICING_CV_INPUT,
        INTENSITY_CV_INPUT,
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

    // control parameters
    float voicingParam;
    float intensityParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double p[16][4001];
    double slide[16];
    int gcount[16];
    long double fpNShape[16];

    // other variables, which do not need to be updated every cycle
    double overallscale;
    double target;
    int widthA;
    int widthB;
    double wet;
    double dry;

    Hombre()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(VOICING_PARAM, 0.f, 1.f, 0.5f, "Voicing");
        configParam(INTENSITY_PARAM, 0.f, 1.f, 0.5f, "Intensity");

        configInput(VOICING_CV_INPUT, "Voicing CV");
        configInput(INTENSITY_CV_INPUT, "Intensity CV");
        configInput(IN_INPUT, "Signal");
        configOutput(OUT_OUTPUT, "Signal");

        configBypass(IN_INPUT, OUT_OUTPUT);

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        for (int i = 0; i < 16; i++) {
            for (int count = 0; count < 4000; count++) {
                p[i][count] = 0.0;
            }
            gcount[i] = 0;
            slide[i] = 0.5;
            fpNShape[i] = 0.0;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        widthA = (int)(1.0 * overallscale);
        widthB = (int)(7.0 * overallscale); //max 364 at 44.1, 792 at 96K
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
        if (outputs[OUT_OUTPUT].isConnected()) {

            voicingParam = params[VOICING_PARAM].getValue();
            voicingParam += inputs[VOICING_CV_INPUT].getVoltage() / 5;
            voicingParam = clamp(voicingParam, 0.01f, 0.99f);

            intensityParam = params[INTENSITY_PARAM].getValue();
            intensityParam += inputs[INTENSITY_CV_INPUT].getVoltage() / 5;
            intensityParam = clamp(intensityParam, 0.01f, 0.99f);

            target = voicingParam;
            wet = intensityParam;
            dry = 1.0 - wet;

            double offsetA;
            double offsetB;
            double total;
            int count;
            long double inputSample;
            double drySample;

            // input
            int numChannels = std::max(1, inputs[IN_INPUT].getChannels());

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (quality == HIGH) {
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

                slide[i] = (slide[i] * 0.9997) + (target * 0.0003);

                //adjust for sample rate
                offsetA = ((pow(slide[i], 2)) * 77) + 3.2;
                offsetB = (3.85 * offsetA) + 41;
                offsetA *= overallscale;
                offsetB *= overallscale;

                if (gcount[i] < 1 || gcount[i] > 2000) {
                    gcount[i] = 2000;
                }
                count = gcount[i];

                //double buffer
                p[i][count + 2000] = p[i][count] = inputSample;

                count = (int)(gcount[i] + floor(offsetA));

                total = p[i][count] * 0.391; //less as value moves away from .0
                total += p[i][count + widthA]; //we can assume always using this in one way or another?
                total += p[i][count + widthA + widthA] * 0.391; //greater as value moves away from .0

                inputSample += ((total * 0.274));

                count = (int)(gcount[i] + floor(offsetB));

                total = p[i][count] * 0.918; //less as value moves away from .0
                total += p[i][count + widthB]; //we can assume always using this in one way or another?
                total += p[i][count + widthB + widthB] * 0.918; //greater as value moves away from .0

                inputSample -= ((total * 0.629));

                inputSample /= 4;

                //still scrolling through the samples, remember
                gcount[i]--;

                if (wet != 1.0) {
                    inputSample = (inputSample * wet) + (drySample * dry);
                }

                if (quality == HIGH) {
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

struct HombreWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Hombre* module;
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
        Hombre* module = dynamic_cast<Hombre*>(this->module);
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

    HombreWidget(Hombre* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/hombre_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Hombre::VOICING_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Hombre::INTENSITY_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Hombre::VOICING_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Hombre::INTENSITY_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 285.0), module, Hombre::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Hombre::OUT_OUTPUT));
    }
};

Model* modelHombre = createModel<Hombre, HombreWidget>("hombre");
