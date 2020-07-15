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
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    dsp::ClockDivider processDivider;

    // control parameters
    float slamParam;
    float bumpParam;

    // state variables (as arrays in order to handle up to 16 polyphonic channels)
    double iirMidRollerAL[16];
    double iirMidRollerBL[16];
    double iirHeadBumpAL[16];
    double iirHeadBumpBL[16];
    double iirMidRollerAR[16];
    double iirMidRollerBR[16];
    double iirHeadBumpAR[16];
    double iirHeadBumpBR[16];

    long double biquadAL[16][9];
    long double biquadBL[16][9];
    long double biquadCL[16][9];
    long double biquadDL[16][9];
    long double biquadAR[16][9];
    long double biquadBR[16][9];
    long double biquadCR[16][9];
    long double biquadDR[16][9];

    long double lastSampleL[16];
    long double lastSampleR[16];
    bool flipL[16];
    bool flipR[16];
    uint32_t fpdL[16];
    uint32_t fpdR[16];

    // other variables, which do not need to be updated every cycle
    double overallscale;
    double inputgain;
    double bumpgain;
    double headBumpFreq;
    double rollAmount;
    float lastSlamParam;
    float lastBumpParam;

    // constants
    const double softness = 0.618033988749894848204586;

    Tape()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SLAM_PARAM, 0.f, 1.f, 0.5f, "Slam");
        configParam(BUMP_PARAM, 0.f, 1.f, 0.5f, "Bump");

        quality = loadQuality();
        processDivider.setDivision(1024);
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        lastSlamParam = 0.0;
        lastBumpParam = 0.0;

        for (int i = 0; i < 16; i++) {
            iirMidRollerAL[i] = iirMidRollerBL[i] = iirHeadBumpAL[i] = iirHeadBumpBL[i] = 0.0;
            iirMidRollerAR[i] = iirMidRollerBR[i] = iirHeadBumpAR[i] = iirHeadBumpBR[i] = 0.0;

            for (int x = 0; x < 9; x++) {
                biquadAL[i][x] = biquadBL[i][x] = biquadCL[i][x] = biquadDL[i][x] = 0.0;
                biquadAR[i][x] = biquadBR[i][x] = biquadCR[i][x] = biquadDR[i][x] = 0.0;
            }

            flipL[i] = flipR[i] = false;
            lastSampleL[i] = lastSampleR[i] = 0.0;
            fpdL[i] = fpdR[i] = 17;
        }
    }

    void onSampleRateChange() override
    {
        float sampleRate = APP->engine->getSampleRate();

        overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= sampleRate;

        headBumpFreq = 0.12 / overallscale;
        rollAmount = (1.0 - softness) / overallscale;
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

    void processChannel(Input& input, Output& output, double iirMidRollerA[], double iirMidRollerB[], double iirHeadBumpA[], double iirHeadBumpB[], long double biquadA[][9], long double biquadB[][9], long double biquadC[][9], long double biquadD[][9], long double lastSample[], bool flip[], uint32_t fpd[])
    {
        if (output.isConnected()) {

            slamParam = params[SLAM_PARAM].getValue();
            slamParam += inputs[SLAM_CV_INPUT].getVoltage() / 10;
            slamParam = clamp(slamParam, 0.01f, 0.99f);

            bumpParam = params[BUMP_PARAM].getValue();
            bumpParam += inputs[BUMP_CV_INPUT].getVoltage() / 10;
            bumpParam = clamp(bumpParam, 0.01f, 0.99f);

            if (slamParam != lastSlamParam) {
                inputgain = pow(10.0, ((slamParam - 0.5) * 24.0) / 20.0);
                lastSlamParam = slamParam;
            }

            if (bumpParam != lastBumpParam) {
                bumpgain = bumpParam * 0.1;
                lastBumpParam = bumpParam;
            }

            float in[16] = {};
            float out[16] = {};
            int numChannels = 1;

            double suppress;
            long double drySample;
            long double highsSample;
            long double nonHighsSample;
            long double tempSample;
            long double groundSample;
            long double applySoften;
            long double inputSample;
            double K;
            double norm;

            if (input.isConnected()) {
                // get input
                numChannels = input.getChannels();
                input.readVoltages(in);
            }

            for (int i = 0; i < numChannels; i++) {

                if (processDivider.process()) {
                    //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
                    //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
                    biquadA[i][0] = biquadB[i][0] = 0.0072 / overallscale;
                    biquadA[i][1] = biquadB[i][1] = 0.0009;
                    K = tan(M_PI * biquadB[i][0]);
                    norm = 1.0 / (1.0 + K / biquadB[i][1] + K * K);
                    biquadA[i][2] = biquadB[i][2] = K / biquadB[i][1] * norm;
                    biquadA[i][4] = biquadB[i][4] = -biquadB[i][2];
                    biquadA[i][5] = biquadB[i][5] = 2.0 * (K * K - 1.0) * norm;
                    biquadA[i][6] = biquadB[i][6] = (1.0 - K / biquadB[i][1] + K * K) * norm;

                    biquadC[i][0] = biquadD[i][0] = 0.032 / overallscale;
                    biquadC[i][1] = biquadD[i][1] = 0.0007;
                    K = tan(M_PI * biquadD[i][0]);
                    norm = 1.0 / (1.0 + K / biquadD[i][1] + K * K);
                    biquadC[i][2] = biquadD[i][2] = K / biquadD[i][1] * norm;
                    biquadC[i][4] = biquadD[i][4] = -biquadD[i][2];
                    biquadC[i][6] = biquadD[i][6] = (1.0 - K / biquadD[i][1] + K * K) * norm;
                }

                inputSample = in[i];

                // pad gain
                inputSample *= gainCut;

                if (quality == HIGH) {
                    if (fabs(inputSample) < 1.18e-37)
                        inputSample = fpd[i] * 1.18e-37;
                }

                drySample = inputSample;

                highsSample = 0.0;
                nonHighsSample = 0.0;

                if (flip[i]) {
                    iirMidRollerA[i] = (iirMidRollerA[i] * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - iirMidRollerA[i];
                    nonHighsSample = iirMidRollerA[i];

                    iirHeadBumpA[i] += (inputSample * 0.05);
                    iirHeadBumpA[i] -= (iirHeadBumpA[i] * iirHeadBumpA[i] * iirHeadBumpA[i] * headBumpFreq);
                    iirHeadBumpA[i] = sin(iirHeadBumpA[i]);

                    tempSample = (iirHeadBumpA[i] * biquadA[i][2]) + biquadA[i][7];
                    biquadA[i][7] = (iirHeadBumpA[i] * biquadA[i][3]) - (tempSample * biquadA[i][5]) + biquadA[i][8];
                    biquadA[i][8] = (iirHeadBumpA[i] * biquadA[i][4]) - (tempSample * biquadA[i][6]);
                    iirHeadBumpA[i] = tempSample; //interleaved biquad
                    if (iirHeadBumpA[i] > 1.0)
                        iirHeadBumpA[i] = 1.0;
                    if (iirHeadBumpA[i] < -1.0)
                        iirHeadBumpA[i] = -1.0;
                    iirHeadBumpA[i] = asin(iirHeadBumpA[i]);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * biquadC[i][2]) + biquadC[i][7];
                    biquadC[i][7] = (inputSample * biquadC[i][3]) - (tempSample * biquadC[i][5]) + biquadC[i][8];
                    biquadC[i][8] = (inputSample * biquadC[i][4]) - (tempSample * biquadC[i][6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                } else {
                    iirMidRollerB[i] = (iirMidRollerB[i] * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - iirMidRollerB[i];
                    nonHighsSample = iirMidRollerB[i];

                    iirHeadBumpB[i] += (inputSample * 0.05);
                    iirHeadBumpB[i] -= (iirHeadBumpB[i] * iirHeadBumpB[i] * iirHeadBumpB[i] * headBumpFreq);
                    iirHeadBumpB[i] = sin(iirHeadBumpB[i]);

                    tempSample = (iirHeadBumpB[i] * biquadB[i][2]) + biquadB[i][7];
                    biquadB[i][7] = (iirHeadBumpB[i] * biquadB[i][3]) - (tempSample * biquadB[i][5]) + biquadB[i][8];
                    biquadB[i][8] = (iirHeadBumpB[i] * biquadB[i][4]) - (tempSample * biquadB[i][6]);
                    iirHeadBumpB[i] = tempSample; //interleaved biquad
                    if (iirHeadBumpB[i] > 1.0)
                        iirHeadBumpB[i] = 1.0;
                    if (iirHeadBumpB[i] < -1.0)
                        iirHeadBumpB[i] = -1.0;
                    iirHeadBumpB[i] = asin(iirHeadBumpB[i]);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * biquadD[i][2]) + biquadD[i][7];
                    biquadD[i][7] = (inputSample * biquadD[i][3]) - (tempSample * biquadD[i][5]) + biquadD[i][8];
                    biquadD[i][8] = (inputSample * biquadD[i][4]) - (tempSample * biquadD[i][6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                }
                flip[i] = !flip[i];

                groundSample = drySample - inputSample; //set up UnBox

                if (inputgain != 1.0) {
                    inputSample *= inputgain;
                } //gain boost inside UnBox: do not boost fringe audio

                applySoften = fabs(highsSample) * 1.57079633;
                if (applySoften > 1.57079633)
                    applySoften = 1.57079633;
                applySoften = 1 - cos(applySoften);
                if (highsSample > 0)
                    inputSample -= applySoften;
                if (highsSample < 0)
                    inputSample += applySoften;
                //apply Soften depending on polarity

                if (inputSample > 1.2533141373155)
                    inputSample = 1.2533141373155;
                if (inputSample < -1.2533141373155)
                    inputSample = -1.2533141373155;
                //clip to 1.2533141373155 to reach maximum output
                inputSample = sin(inputSample * fabs(inputSample)) / ((inputSample == 0.0) ? 1 : fabs(inputSample));
                //Spiral, for cleanest most optimal tape effect

                suppress = (1.0 - fabs(inputSample)) * 0.00013;
                if (iirHeadBumpA[i] > suppress)
                    iirHeadBumpA[i] -= suppress;
                if (iirHeadBumpA[i] < -suppress)
                    iirHeadBumpA[i] += suppress;
                if (iirHeadBumpB[i] > suppress)
                    iirHeadBumpB[i] -= suppress;
                if (iirHeadBumpB[i] < -suppress)
                    iirHeadBumpB[i] += suppress;
                //restrain resonant quality of head bump algorithm

                inputSample += groundSample; //apply UnBox processing

                inputSample += ((iirHeadBumpA[i] + iirHeadBumpB[i]) * bumpgain); //and head bump

                if (lastSample[i] >= 0.99) {
                    if (inputSample < 0.99)
                        lastSample[i] = ((0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        lastSample[i] = 0.99;
                }

                if (lastSample[i] <= -0.99) {
                    if (inputSample > -0.99)
                        lastSample[i] = ((-0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        lastSample[i] = -0.99;
                }

                if (inputSample > 0.99) {
                    if (lastSample[i] < 0.99)
                        inputSample = ((0.99 * softness) + (lastSample[i] * (1.0 - softness)));
                    else
                        inputSample = 0.99;
                }

                if (inputSample < -0.99) {
                    if (lastSample[i] > -0.99)
                        inputSample = ((-0.99 * softness) + (lastSample[i] * (1.0 - softness)));
                    else
                        inputSample = -0.99;
                }
                lastSample[i] = inputSample; //end ADClip R

                if (inputSample > 0.99)
                    inputSample = 0.99;
                if (inputSample < -0.99)
                    inputSample = -0.99;
                //final iron bar

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

                out[i] = inputSample;
            }
            // output
            output.setChannels(numChannels);
            output.writeVoltages(out);
        }
    }

    void process(const ProcessArgs& args) override
    {
        processChannel(inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT], iirMidRollerAL, iirMidRollerBL, iirHeadBumpAL, iirHeadBumpBL, biquadAL, biquadBL, biquadCL, biquadDL, lastSampleL, flipL, fpdL);
        processChannel(inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT], iirMidRollerAR, iirMidRollerBR, iirHeadBumpAR, iirHeadBumpBR, biquadAR, biquadBR, biquadCR, biquadDR, lastSampleR, flipR, fpdR);
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