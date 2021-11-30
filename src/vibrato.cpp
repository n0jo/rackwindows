/***********************************************************************************************
Vibrato
-------
VCV Rack module based on Vibrato by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- CV inputs for speed, depth, fmspeed, fmdepth and inv/wet
- trigger outputs (EOC) for speed and fmspeed
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Vibrato : Module {

    enum ParamIds {
        SPEED_PARAM,
        FMSPEED_PARAM,
        DEPTH_PARAM,
        FMDEPTH_PARAM,
        INVWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        SPEED_CV_INPUT,
        DEPTH_CV_INPUT,
        FMSPEED_CV_INPUT,
        FMDEPTH_CV_INPUT,
        INVWET_CV_INPUT,
        IN_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        EOC_OUTPUT,
        OUT_OUTPUT,
        EOC_FM_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        SPEED_LIGHT,
        SPEED_FM_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    dsp::PulseGenerator eocPulse, eocFmPulse;

    // control parameters
    float speedParam;
    float depthParam;
    float fmSpeedParam;
    float fmDepthParam;
    float invwetParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double p[16][16386]; //this is processed, not raw incoming samples
    double sweep[16];
    double sweepB[16];
    int gcount[16];
    double airPrev[16];
    double airEven[16];
    double airOdd[16];
    double airFactor[16];
    bool flip[16];
    uint32_t fpd[16];

    // other variables, which do not need to be updated every cycle
    double overallscale;
    double speed;
    double depth;
    double speedB;
    double depthB;
    double wet;
    float lastSpeedParam;
    float lastDepthParam;
    float lastFmSpeedParam;
    float lastFmDepthParam;
    float lastInvwetParam;

    // constants
    const double tupi = 3.141592653589793238 * 2.0;

    Vibrato()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SPEED_PARAM, 0.f, 1.f, 0.f, "Speed");
        configParam(FMSPEED_PARAM, 0.f, 1.f, 0.f, "FM Speed");
        configParam(DEPTH_PARAM, 0.f, 1.f, 0.f, "Depth");
        configParam(FMDEPTH_PARAM, 0.f, 1.f, 0.f, "FM Depth");
        configParam(INVWET_PARAM, 0.f, 1.f, 0.5f, "Inv/Wet");

        configInput(SPEED_CV_INPUT, "Speed CV");
        configInput(DEPTH_CV_INPUT, "Depth CV");
        configInput(FMSPEED_CV_INPUT, "FM Speed CV");
        configInput(FMDEPTH_CV_INPUT, "FM Depth CV");
        configInput(INVWET_CV_INPUT, "Inv/Wet CV");
        configInput(IN_INPUT, "Signal");
        configOutput(OUT_OUTPUT, "Signal");
        configOutput(EOC_OUTPUT, "EOC");
        configOutput(EOC_FM_OUTPUT, "FM EOC");

        configBypass(IN_INPUT, OUT_OUTPUT);

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        lastSpeedParam = 0.0;
        lastDepthParam = 0.0;
        lastFmSpeedParam = 0.0;
        lastFmDepthParam = 0.0;
        lastInvwetParam = 0.0;

        for (int i = 0; i < 16; i++) {
            for (int count = 0; count < 16385; count++) {
                p[i][count] = 0.0;
            }
            sweep[i] = sweepB[i] = 3.141592653589793238 / 2.0;
            gcount[i] = 0;

            airPrev[i] = 0.0;
            airEven[i] = 0.0;
            airOdd[i] = 0.0;
            airFactor[i] = 0.0;

            flip[i] = false;

            fpd[i] = 17;
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
        if (outputs[OUT_OUTPUT].isConnected() || outputs[EOC_OUTPUT].isConnected() || outputs[EOC_FM_OUTPUT].isConnected()) {

            speedParam = params[SPEED_PARAM].getValue();
            speedParam += inputs[SPEED_CV_INPUT].getVoltage() / 5;
            speedParam = clamp(speedParam, 0.01f, 0.99f);

            depthParam = params[DEPTH_PARAM].getValue();
            depthParam += inputs[DEPTH_CV_INPUT].getVoltage() / 5;
            depthParam = clamp(depthParam, 0.01f, 0.99f);

            fmSpeedParam = params[FMSPEED_PARAM].getValue();
            fmSpeedParam += inputs[FMSPEED_CV_INPUT].getVoltage() / 5;
            fmSpeedParam = clamp(fmSpeedParam, 0.01f, 0.99f);

            fmDepthParam = params[FMDEPTH_PARAM].getValue();
            fmDepthParam += inputs[FMDEPTH_CV_INPUT].getVoltage() / 5;
            fmDepthParam = clamp(fmDepthParam, 0.01f, 0.99f);

            invwetParam = params[INVWET_PARAM].getValue();
            invwetParam += inputs[INVWET_CV_INPUT].getVoltage() / 5;
            invwetParam = clamp(invwetParam, 0.01f, 0.99f);

            if (speedParam != lastSpeedParam) {
                speed = pow(0.1 + speedParam, 6);
            }

            if (depthParam != lastDepthParam) {
                depth = (pow(depthParam, 3) / sqrt(speed)) * 4.0;
            }

            if (fmSpeedParam != lastFmSpeedParam) {
                speedB = pow(0.1 + fmSpeedParam, 6);
            }

            if (fmDepthParam != lastFmDepthParam) {
                depthB = pow(fmDepthParam, 3) / sqrt(speedB);
            }

            if (invwetParam != lastInvwetParam) {
                wet = (invwetParam * 2.0) - 1.0; //note: inv/dry/wet
            }

            // number of polyphonic channels
            int numChannels = std::max(1, inputs[IN_INPUT].getChannels());

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                long double inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSample) < 1.18e-37)
                        inputSample = fpd[i] * 1.18e-37;
                }

                double drySample = inputSample;

                airFactor[i] = airPrev[i] - inputSample;

                if (flip[i]) {
                    airEven[i] += airFactor[i];
                    airOdd[i] -= airFactor[i];
                    airFactor[i] = airEven[i];
                } else {
                    airOdd[i] += airFactor[i];
                    airEven[i] -= airFactor[i];
                    airFactor[i] = airOdd[i];
                }

                //air, compensates for loss of highs in the interpolation
                airOdd[i] = (airOdd[i] - ((airOdd[i] - airEven[i]) / 256.0)) / 1.0001;
                airEven[i] = (airEven[i] - ((airEven[i] - airOdd[i]) / 256.0)) / 1.0001;
                airPrev[i] = inputSample;
                inputSample += airFactor[i];

                flip[i] = !flip[i];

                if (gcount[i] < 1 || gcount[i] > 8192) {
                    gcount[i] = 8192;
                }
                int count = gcount[i];
                p[i][count + 8192] = p[i][count] = inputSample;

                double offset = depth + (depth * sin(sweep[i]));
                count += (int)floor(offset);

                inputSample = p[i][count] * (1.0 - (offset - floor(offset))); //less as value moves away from .0
                inputSample += p[i][count + 1]; //we can assume always using this in one way or another?
                inputSample += p[i][count + 2] * (offset - floor(offset)); //greater as value moves away from .0
                inputSample -= ((p[i][count] - p[i][count + 1]) - (p[i][count + 1] - p[i][count + 2])) / 50.0; //interpolation hacks 'r us
                inputSample *= 0.5; // gain trim

                //still scrolling through the samples, remember
                sweep[i] += (speed + (speedB * sin(sweepB[i]) * depthB));
                sweepB[i] += speedB;
                if (sweep[i] > tupi) {
                    sweep[i] -= tupi;
                }
                if (sweep[i] < 0.0) {
                    sweep[i] += tupi;
                } //through zero FM
                if (sweepB[i] > tupi) {
                    sweepB[i] -= tupi;
                }
                gcount[i]--;

                //Inv/Dry/Wet control
                if (wet != 1.0) {
                    inputSample = (inputSample * wet) + (drySample * (1.0 - fabs(wet)));
                }

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

                // audio output
                outputs[OUT_OUTPUT].setChannels(numChannels);
                outputs[OUT_OUTPUT].setVoltage(inputSample, i);
            }

            // triggers
            if (sweep[0] < 0.1) {
                eocPulse.trigger(1e-3);
            }
            if (sweepB[0] < 0.1) {
                eocFmPulse.trigger(1e-3);
            }

            // lights
            lights[SPEED_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweep[0] / 5) + 1), args.sampleTime);
            lights[SPEED_FM_LIGHT].setSmoothBrightness(fmaxf(0.0, (-sweepB[0] / 5) + 1), args.sampleTime);

            // trigger outputs
            outputs[EOC_OUTPUT].setVoltage((eocPulse.process(args.sampleTime) ? 10.0 : 0.0));
            outputs[EOC_FM_OUTPUT].setVoltage((eocFmPulse.process(args.sampleTime) ? 10.0 : 0.0));
        }
    }
};

struct VibratoWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Vibrato* module;
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
        Vibrato* module = dynamic_cast<Vibrato*>(this->module);
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

    VibratoWidget(Vibrato* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/vibrato_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Vibrato::SPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 65.0), module, Vibrato::FMSPEED_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Vibrato::DEPTH_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 125.0), module, Vibrato::FMDEPTH_PARAM));
        addParam(createParamCentered<RwKnobLargeDark>(Vec(60.0, 190.0), module, Vibrato::INVWET_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(13, 37), module, Vibrato::SPEED_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(107, 37), module, Vibrato::SPEED_FM_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 245.0), module, Vibrato::SPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 285.0), module, Vibrato::DEPTH_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 245.0), module, Vibrato::FMSPEED_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 285.0), module, Vibrato::FMDEPTH_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(60.0, 245.0), module, Vibrato::INVWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(60.0, 285.0), module, Vibrato::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(22.5, 325.0), module, Vibrato::EOC_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(60.0, 325.0), module, Vibrato::OUT_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(97.5, 325.0), module, Vibrato::EOC_FM_OUTPUT));
    }
};

Model* modelVibrato = createModel<Vibrato, VibratoWidget>("vibrato");
