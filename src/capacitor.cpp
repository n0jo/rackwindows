/***********************************************************************************************
Capacitor
---------
VCV Rack module based on Capacitor by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- mono
- no Dry/Wet
- CV inputs for Lowpass and Highpass
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Capacitor : Module {
    enum ParamIds {
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        LOWPASS_CV_INPUT,
        HIGHPASS_CV_INPUT,
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
    float lowpassParam;
    float highpassParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double iirHighpassA[16];
    double iirHighpassB[16];
    double iirHighpassC[16];
    double iirHighpassD[16];
    double iirHighpassE[16];
    double iirHighpassF[16];
    double iirLowpassA[16];
    double iirLowpassB[16];
    double iirLowpassC[16];
    double iirLowpassD[16];
    double iirLowpassE[16];
    double iirLowpassF[16];
    double lowpassChase[16];
    double highpassChase[16];
    double lowpassAmount[16];
    double highpassAmount[16];
    double lastLowpass[16];
    double lastHighpass[16];
    int count[16];
    long double fpNShape[16];

    // other
    double overallscale;

    Capacitor()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Lowpass");
        configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "Highpass");

        configInput(LOWPASS_CV_INPUT, "Lowpass CV");
        configInput(HIGHPASS_CV_INPUT, "Highpass CV");
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
            iirHighpassA[i] = 0.0;
            iirHighpassB[i] = 0.0;
            iirHighpassC[i] = 0.0;
            iirHighpassD[i] = 0.0;
            iirHighpassE[i] = 0.0;
            iirHighpassF[i] = 0.0;
            iirLowpassA[i] = 0.0;
            iirLowpassB[i] = 0.0;
            iirLowpassC[i] = 0.0;
            iirLowpassD[i] = 0.0;
            iirLowpassE[i] = 0.0;
            iirLowpassF[i] = 0.0;
            lowpassChase[i] = 0.0;
            highpassChase[i] = 0.0;
            lowpassAmount[i] = 1.0;
            highpassAmount[i] = 0.0;
            lastLowpass[i] = 1000.0;
            lastHighpass[i] = 1000.0;
            count[i] = 0;
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

            lowpassParam = params[LOWPASS_PARAM].getValue();
            lowpassParam += inputs[LOWPASS_CV_INPUT].getVoltage() / 5;
            lowpassParam = clamp(lowpassParam, 0.01f, 0.99f);

            highpassParam = params[HIGHPASS_PARAM].getValue();
            highpassParam += inputs[HIGHPASS_CV_INPUT].getVoltage() / 5;
            highpassParam = clamp(highpassParam, 0.01f, 0.99f);

            double lowpassSpeed;
            double highpassSpeed;
            double invLowpass;
            double invHighpass;
            long double inputSample;

            // for each poly channel
            for (int i = 0, numChannels = std::max(1, inputs[IN_INPUT].getChannels()); i < numChannels; ++i) {

                lowpassChase[i] = pow(lowpassParam, 2);
                highpassChase[i] = pow(highpassParam, 2);
                //should not scale with sample rate, because values reaching 1 are important
                //to its ability to bypass when set to max
                lowpassSpeed = 300 / (fabs(lastLowpass[i] - lowpassChase[i]) + 1.0);
                highpassSpeed = 300 / (fabs(lastHighpass[i] - highpassChase[i]) + 1.0);
                lastLowpass[i] = lowpassChase[i];
                lastHighpass[i] = highpassChase[i];

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

                lowpassAmount[i] = (((lowpassAmount[i] * lowpassSpeed) + lowpassChase[i]) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - lowpassAmount[i];
                highpassAmount[i] = (((highpassAmount[i] * highpassSpeed) + highpassChase[i]) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - highpassAmount[i];

                //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
                //steepens the filter after minimizing artifacts.
                count[i]++;
                if (count[i] > 5)
                    count[i] = 0;
                switch (count[i]) {
                case 0:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassB[i] = (iirHighpassB[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassB[i];
                    iirLowpassB[i] = (iirLowpassB[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassB[i];
                    iirHighpassD[i] = (iirHighpassD[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassD[i];
                    iirLowpassD[i] = (iirLowpassD[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassD[i];
                    break;
                case 1:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassC[i] = (iirHighpassC[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassC[i];
                    iirLowpassC[i] = (iirLowpassC[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassC[i];
                    iirHighpassE[i] = (iirHighpassE[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassE[i];
                    iirLowpassE[i] = (iirLowpassE[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassE[i];
                    break;
                case 2:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassB[i] = (iirHighpassB[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassB[i];
                    iirLowpassB[i] = (iirLowpassB[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassB[i];
                    iirHighpassF[i] = (iirHighpassF[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassF[i];
                    iirLowpassF[i] = (iirLowpassF[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassF[i];
                    break;
                case 3:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassC[i] = (iirHighpassC[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassC[i];
                    iirLowpassC[i] = (iirLowpassC[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassC[i];
                    iirHighpassD[i] = (iirHighpassD[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassD[i];
                    iirLowpassD[i] = (iirLowpassD[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassD[i];
                    break;
                case 4:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassB[i] = (iirHighpassB[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassB[i];
                    iirLowpassB[i] = (iirLowpassB[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassB[i];
                    iirHighpassE[i] = (iirHighpassE[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassE[i];
                    iirLowpassE[i] = (iirLowpassE[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassE[i];
                    break;
                case 5:
                    iirHighpassA[i] = (iirHighpassA[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassA[i];
                    iirLowpassA[i] = (iirLowpassA[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassA[i];
                    iirHighpassC[i] = (iirHighpassC[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassC[i];
                    iirLowpassC[i] = (iirLowpassC[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassC[i];
                    iirHighpassF[i] = (iirHighpassF[i] * invHighpass) + (inputSample * highpassAmount[i]);
                    inputSample -= iirHighpassF[i];
                    iirLowpassF[i] = (iirLowpassF[i] * invLowpass) + (inputSample * lowpassAmount[i]);
                    inputSample = iirLowpassF[i];
                    break;
                }

                //stereo 32 bit dither, made small and tidy.
                if (quality == HIGH) {
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

struct CapacitorWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Capacitor* module;
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
        Capacitor* module = dynamic_cast<Capacitor*>(this->module);
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

    CapacitorWidget(Capacitor* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/capacitor_mono_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 65.0), module, Capacitor::LOWPASS_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(30.0, 125.0), module, Capacitor::HIGHPASS_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30, 205.0), module, Capacitor::LOWPASS_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30, 245.0), module, Capacitor::HIGHPASS_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30, 285.0), module, Capacitor::IN_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30, 325.0), module, Capacitor::OUT_OUTPUT));
    }
};

Model* modelCapacitor = createModel<Capacitor, CapacitorWidget>("capacitor");
