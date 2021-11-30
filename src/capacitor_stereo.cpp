/***********************************************************************************************
Capacitor Stereo
----------------
VCV Rack module based on Capacitor by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- separate controls for left and right channels
- controls can be linked
- CV inputs for lowpass, highpass and dry/wet
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Capacitor_stereo : Module {
    enum ParamIds {
        LOWPASS_L_PARAM,
        LOWPASS_R_PARAM,
        HIGHPASS_L_PARAM,
        HIGHPASS_R_PARAM,
        DRYWET_PARAM,
        LINK_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        LOWPASS_CV_L_INPUT,
        LOWPASS_CV_R_INPUT,
        HIGHPASS_CV_L_INPUT,
        HIGHPASS_CV_R_INPUT,
        DRYWET_CV_INPUT,
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
        LINK_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    bool isLinked;
    bool quality;
    float lastLowpassParam;
    float lastHighpassParam;

    // control parameters
    float lowpassParam;
    float highpassParam;
    float drywetParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    struct stateVars {
        double iirHighpassA;
        double iirHighpassB;
        double iirHighpassC;
        double iirHighpassD;
        double iirHighpassE;
        double iirHighpassF;
        double iirLowpassA;
        double iirLowpassB;
        double iirLowpassC;
        double iirLowpassD;
        double iirLowpassE;
        double iirLowpassF;
        double lowpassChase;
        double highpassChase;
        double wetChase;
        double lowpassAmount;
        double highpassAmount;
        double wet;
        double lastLowpass;
        double lastHighpass;
        double lastWet;
        int count;
        long double fpNShape;
    } stateL[16], stateR[16];

    // other
    double overallscale;

    Capacitor_stereo()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_L_PARAM, 0.f, 1.f, 1.f, "Lowpass L");
        configParam(LOWPASS_R_PARAM, 0.f, 1.f, 1.f, "Lowpass R");
        configParam(HIGHPASS_L_PARAM, 0.f, 1.f, 0.f, "Highpass L");
        configParam(HIGHPASS_R_PARAM, 0.f, 1.f, 0.f, "Highpass R");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");
        configSwitch(LINK_PARAM, 0.f, 1.f, 1.f, "Link", {"Not linked", "Linked"});

        configInput(LOWPASS_CV_L_INPUT, "Lowpass L CV");
        configInput(LOWPASS_CV_R_INPUT, "Lowpass R CV");
        configInput(HIGHPASS_CV_L_INPUT, "Highpass L CV");
        configInput(HIGHPASS_CV_R_INPUT, "Highpass R CV");
        configInput(DRYWET_CV_INPUT, "Dry/wet CV");
        configInput(IN_L_INPUT, "Signal L");
        configInput(IN_R_INPUT, "Signal R");
        configOutput(OUT_L_OUTPUT, "Signal L");
        configOutput(OUT_R_OUTPUT, "Signal R");

        configBypass(IN_L_INPUT, OUT_L_OUTPUT);
        configBypass(IN_R_INPUT, OUT_R_OUTPUT);

        isLinked = true;
        quality = loadQuality();
        onReset();
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
        onSampleRateChange();

        for (int i = 0; i < 16; i++) {
            stateL[i].iirHighpassA = stateR[i].iirHighpassA = 0.0;
            stateL[i].iirHighpassB = stateR[i].iirHighpassB = 0.0;
            stateL[i].iirHighpassC = stateR[i].iirHighpassC = 0.0;
            stateL[i].iirHighpassD = stateR[i].iirHighpassD = 0.0;
            stateL[i].iirHighpassE = stateR[i].iirHighpassE = 0.0;
            stateL[i].iirHighpassF = stateR[i].iirHighpassF = 0.0;
            stateL[i].iirLowpassA = stateR[i].iirLowpassA = 0.0;
            stateL[i].iirLowpassB = stateR[i].iirLowpassB = 0.0;
            stateL[i].iirLowpassC = stateR[i].iirLowpassC = 0.0;
            stateL[i].iirLowpassD = stateR[i].iirLowpassD = 0.0;
            stateL[i].iirLowpassE = stateR[i].iirLowpassE = 0.0;
            stateL[i].iirLowpassF = stateR[i].iirLowpassF = 0.0;
            stateL[i].lowpassChase = stateR[i].lowpassChase = 0.0;
            stateL[i].highpassChase = stateR[i].highpassChase = 0.0;
            stateL[i].wetChase = stateR[i].wetChase = 0.0;
            stateL[i].lowpassAmount = stateR[i].lowpassAmount = 1.0;
            stateL[i].highpassAmount = stateR[i].highpassAmount = 0.0;
            stateL[i].wet = stateR[i].wet = 1.0;
            stateL[i].lastLowpass = stateR[i].lastLowpass = 1000.0;
            stateL[i].lastHighpass = stateR[i].lastHighpass = 1000.0;
            stateL[i].lastWet = stateR[i].lastWet = 1000.0;
            stateL[i].count = stateR[i].count = 0;
            stateL[i].fpNShape = stateR[i].fpNShape = 0.0;

            lastLowpassParam = lastHighpassParam = 0.0f;
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

    void processChannel(stateVars v[], Param& lowpass, Param& highpass, Param& drywet, Input& lowpassCv, Input& highpassCv, Input& drywetCv, Input& input, Output& output)
    {
        // params
        lowpassParam = lowpass.getValue();
        lowpassParam += lowpassCv.getVoltage() / 5;
        lowpassParam = clamp(lowpassParam, 0.01f, 0.99f);

        highpassParam = highpass.getValue();
        highpassParam += highpassCv.getVoltage() / 5;
        highpassParam = clamp(highpassParam, 0.01f, 0.99f);

        drywetParam = drywet.getValue();
        drywetParam += drywetCv.getVoltage() / 5;
        drywetParam = clamp(drywetParam, 0.00f, 1.00f);

        double lowpassSpeed;
        double highpassSpeed;
        double wetSpeed;
        double invLowpass;
        double invHighpass;
        double dry;

        long double inputSample;
        long double drySample;

        // for each poly channel
        for (int i = 0, numChannels = std::max(1, input.getChannels()); i < numChannels; ++i) {

            v[i].lowpassChase = pow(lowpassParam, 2);
            v[i].highpassChase = pow(highpassParam, 2);
            v[i].wetChase = drywetParam;
            //should not scale with sample rate, because values reaching 1 are important
            //to its ability to bypass when set to max
            lowpassSpeed = 300 / (fabs(v[i].lastLowpass - v[i].lowpassChase) + 1.0);
            highpassSpeed = 300 / (fabs(v[i].lastHighpass - v[i].highpassChase) + 1.0);
            wetSpeed = 300 / (fabs(v[i].lastWet - v[i].wetChase) + 1.0);
            v[i].lastLowpass = v[i].lowpassChase;
            v[i].lastHighpass = v[i].highpassChase;
            v[i].lastWet = v[i].wetChase;

            // input
            inputSample = input.getPolyVoltage(i);

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

            v[i].lowpassAmount = (((v[i].lowpassAmount * lowpassSpeed) + v[i].lowpassChase) / (lowpassSpeed + 1.0));
            invLowpass = 1.0 - v[i].lowpassAmount;
            v[i].highpassAmount = (((v[i].highpassAmount * highpassSpeed) + v[i].highpassChase) / (highpassSpeed + 1.0));
            invHighpass = 1.0 - v[i].highpassAmount;
            v[i].wet = (((v[i].wet * wetSpeed) + v[i].wetChase) / (wetSpeed + 1.0));
            dry = 1.0 - v[i].wet;

            //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
            //steepens the filter after minimizing artifacts.
            v[i].count++;
            if (v[i].count > 5)
                v[i].count = 0;
            switch (v[i].count) {
            case 0:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassB = (v[i].iirHighpassB * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassB;
                v[i].iirLowpassB = (v[i].iirLowpassB * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassB;
                v[i].iirHighpassD = (v[i].iirHighpassD * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassD;
                v[i].iirLowpassD = (v[i].iirLowpassD * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassD;
                break;
            case 1:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassC = (v[i].iirHighpassC * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassC;
                v[i].iirLowpassC = (v[i].iirLowpassC * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassC;
                v[i].iirHighpassE = (v[i].iirHighpassE * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassE;
                v[i].iirLowpassE = (v[i].iirLowpassE * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassE;
                break;
            case 2:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassB = (v[i].iirHighpassB * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassB;
                v[i].iirLowpassB = (v[i].iirLowpassB * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassB;
                v[i].iirHighpassF = (v[i].iirHighpassF * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassF;
                v[i].iirLowpassF = (v[i].iirLowpassF * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassF;
                break;
            case 3:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassC = (v[i].iirHighpassC * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassC;
                v[i].iirLowpassC = (v[i].iirLowpassC * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassC;
                v[i].iirHighpassD = (v[i].iirHighpassD * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassD;
                v[i].iirLowpassD = (v[i].iirLowpassD * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassD;
                break;
            case 4:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassB = (v[i].iirHighpassB * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassB;
                v[i].iirLowpassB = (v[i].iirLowpassB * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassB;
                v[i].iirHighpassE = (v[i].iirHighpassE * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassE;
                v[i].iirLowpassE = (v[i].iirLowpassE * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassE;
                break;
            case 5:
                v[i].iirHighpassA = (v[i].iirHighpassA * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassA;
                v[i].iirLowpassA = (v[i].iirLowpassA * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassA;
                v[i].iirHighpassC = (v[i].iirHighpassC * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassC;
                v[i].iirLowpassC = (v[i].iirLowpassC * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassC;
                v[i].iirHighpassF = (v[i].iirHighpassF * invHighpass) + (inputSample * v[i].highpassAmount);
                inputSample -= v[i].iirHighpassF;
                v[i].iirLowpassF = (v[i].iirLowpassF * invLowpass) + (inputSample * v[i].lowpassAmount);
                inputSample = v[i].iirLowpassF;
                break;
            }

            inputSample = (drySample * dry) + (inputSample * v[i].wet);

            if (quality == HIGH) {
                //stereo 32 bit dither, made small and tidy.
                int expon;
                frexpf((float)inputSample, &expon);
                long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                inputSample += (dither - v[i].fpNShape);
                v[i].fpNShape = dither;
                //end 32 bit dither
            }

            // bring gain back up
            inputSample *= gainBoost;

            // output
            output.setChannels(numChannels);
            output.setVoltage(inputSample, i);
        }
    }

    void process(const ProcessArgs& args) override
    {
        // link
        isLinked = params[LINK_PARAM].getValue() ? true : false;

        if (isLinked) {
            if (params[LOWPASS_L_PARAM].getValue() != lastLowpassParam) {
                params[LOWPASS_R_PARAM] = params[LOWPASS_L_PARAM];
            } else if (params[LOWPASS_R_PARAM].getValue() != lastLowpassParam) {
                params[LOWPASS_L_PARAM] = params[LOWPASS_R_PARAM];
            }
            if (params[HIGHPASS_L_PARAM].getValue() != lastHighpassParam) {
                params[HIGHPASS_R_PARAM] = params[HIGHPASS_L_PARAM];
            } else if (params[HIGHPASS_R_PARAM].getValue() != lastHighpassParam) {
                params[HIGHPASS_L_PARAM] = params[HIGHPASS_R_PARAM];
            }
        }

        lastLowpassParam = params[LOWPASS_R_PARAM].getValue();
        lastHighpassParam = params[HIGHPASS_R_PARAM].getValue();

        if (outputs[OUT_L_OUTPUT].isConnected()) {
            processChannel(stateL, params[LOWPASS_L_PARAM], params[HIGHPASS_L_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_L_INPUT], inputs[HIGHPASS_CV_L_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
        }
        if (outputs[OUT_R_OUTPUT].isConnected()) {
            processChannel(stateR, params[LOWPASS_R_PARAM], params[HIGHPASS_R_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_R_INPUT], inputs[HIGHPASS_CV_R_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
        }

        // link light
        lights[LINK_LIGHT].setBrightness(isLinked);
    }
};

struct Capacitor_stereoWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Capacitor_stereo* module;
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
        Capacitor_stereo* module = dynamic_cast<Capacitor_stereo*>(this->module);
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

    Capacitor_stereoWidget(Capacitor_stereo* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/capacitor_st_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Capacitor_stereo::LOWPASS_L_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 65.0), module, Capacitor_stereo::LOWPASS_R_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Capacitor_stereo::HIGHPASS_L_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(90.0, 125.0), module, Capacitor_stereo::HIGHPASS_R_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(60.0, 175.0), module, Capacitor_stereo::DRYWET_PARAM));

        // switches
        addParam(createParamCentered<RwCKSS>(Vec(60.0, 305.0), module, Capacitor_stereo::LINK_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(60, 285), module, Capacitor_stereo::LINK_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 205.0), module, Capacitor_stereo::LOWPASS_CV_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 205.0), module, Capacitor_stereo::LOWPASS_CV_R_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 245.0), module, Capacitor_stereo::HIGHPASS_CV_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 245.0), module, Capacitor_stereo::HIGHPASS_CV_R_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(60.0, 225.0), module, Capacitor_stereo::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(22.5, 285.0), module, Capacitor_stereo::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(97.5, 285.0), module, Capacitor_stereo::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(22.5, 325.0), module, Capacitor_stereo::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(97.5, 325.0), module, Capacitor_stereo::OUT_R_OUTPUT));
    }
};

Model* modelCapacitor_stereo = createModel<Capacitor_stereo, Capacitor_stereoWidget>("capacitor_stereo");
