/***********************************************************************************************
Interstage
--------
VCV Rack module based on Interstage by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- polyphonic

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

struct Interstage : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
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
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double iirSampleAL[16];
    double iirSampleBL[16];
    double iirSampleCL[16];
    double iirSampleDL[16];
    double iirSampleEL[16];
    double iirSampleFL[16];
    long double lastSampleL[16];
    bool flipL[16];
    uint32_t fpdL[16];
    double iirSampleAR[16];
    double iirSampleBR[16];
    double iirSampleCR[16];
    double iirSampleDR[16];
    double iirSampleER[16];
    double iirSampleFR[16];
    long double lastSampleR[16];
    bool flipR[16];
    uint32_t fpdR[16];

    // other variables, which do not need to be updated every cycle
    double overallscale;
    double firstStage;
    double iirAmount;

    // constants
    const double threshold = 0.381966011250105;

    Interstage()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        for (int i = 0; i < 16; i++) {
            iirSampleAL[i] = iirSampleBL[i] = iirSampleCL[i] = iirSampleDL[i] = iirSampleEL[i] = iirSampleFL[i] = lastSampleL[i] = 0.0;
            flipL[i] = true;
            fpdR[i] = 17;
            iirSampleAR[i] = iirSampleBR[i] = iirSampleCR[i] = iirSampleDR[i] = iirSampleER[i] = iirSampleFR[i] = lastSampleR[i] = 0.0;
            flipR[i] = true;
            fpdR[i] = 17;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        firstStage = 0.381966011250105 / overallscale;
        iirAmount = 0.00295 / overallscale;
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

    void processChannel(Input& input, Output& output, double iirSampleA[], double iirSampleB[], double iirSampleC[], double iirSampleD[], double iirSampleE[], double iirSampleF[], long double lastSample[], bool flip[], uint32_t fpd[])
    {
        if (output.isConnected()) {

            long double inputSample;
            long double drySample;

            // number of polyphonic channels
            int numChannels = std::max(1, input.getChannels());

            // for each poly channel
            for (int i = 0; i < numChannels; i++) {

                // input
                inputSample = input.getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSample) < 1.18e-37)
                        inputSample = fpd[i] * 1.18e-37;
                }

                drySample = inputSample;

                inputSample = (inputSample + lastSample[i]) * 0.5; //start the lowpassing with an average

                if (flip[i]) {
                    //make highpass
                    iirSampleA[i] = (iirSampleA[i] * (1 - firstStage)) + (inputSample * firstStage);
                    inputSample = iirSampleA[i];
                    iirSampleC[i] = (iirSampleC[i] * (1 - iirAmount)) + (inputSample * iirAmount);
                    inputSample = iirSampleC[i];
                    iirSampleE[i] = (iirSampleE[i] * (1 - iirAmount)) + (inputSample * iirAmount);
                    inputSample = iirSampleE[i];
                    inputSample = drySample - inputSample;

                    //slew limit against lowpassed reference point
                    if (inputSample - iirSampleA[i] > threshold)
                        inputSample = iirSampleA[i] + threshold;
                    if (inputSample - iirSampleA[i] < -threshold)
                        inputSample = iirSampleA[i] - threshold;

                } else {
                    //make highpass
                    iirSampleB[i] = (iirSampleB[i] * (1 - firstStage)) + (inputSample * firstStage);
                    inputSample = iirSampleB[i];
                    iirSampleD[i] = (iirSampleD[i] * (1 - iirAmount)) + (inputSample * iirAmount);
                    inputSample = iirSampleD[i];
                    iirSampleF[i] = (iirSampleF[i] * (1 - iirAmount)) + (inputSample * iirAmount);
                    inputSample = iirSampleF[i];
                    inputSample = drySample - inputSample;

                    //slew limit against lowpassed reference point
                    if (inputSample - iirSampleB[i] > threshold)
                        inputSample = iirSampleB[i] + threshold;
                    if (inputSample - iirSampleB[i] < -threshold)
                        inputSample = iirSampleB[i] - threshold;
                }

                flip[i] = !flip[i];

                lastSample[i] = inputSample;

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

                // output
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }

    void process(const ProcessArgs& args) override
    {
        processChannel(inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT], iirSampleAL, iirSampleBL, iirSampleCL, iirSampleDL, iirSampleEL, iirSampleFL, lastSampleL, flipL, fpdL);
        processChannel(inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT], iirSampleAR, iirSampleBR, iirSampleCR, iirSampleDR, iirSampleER, iirSampleFR, lastSampleR, flipR, fpdR);
    }
};

struct InterstageWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Interstage* module;
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
        Interstage* module = dynamic_cast<Interstage*>(this->module);
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

    InterstageWidget(Interstage* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/interstage_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH * 1.5, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 205.0), module, Interstage::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(30.0, 245.0), module, Interstage::IN_R_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 285.0), module, Interstage::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(30.0, 325.0), module, Interstage::OUT_R_OUTPUT));
    }
};

Model* modelInterstage = createModel<Interstage, InterstageWidget>("interstage");