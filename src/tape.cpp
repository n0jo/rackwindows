/***********************************************************************************************
Tape
----
VCV Rack module based on Tape by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- cv inputs for slam and bump
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

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

    const double gainCut = 0.1;
    const double gainBoost = 10.0;

    double iirMidRollerAR;
    double iirMidRollerBR;
    double iirHeadBumpAR;
    double iirHeadBumpBR;

    long double biquadAR[9];
    long double biquadBR[9];
    long double biquadCR[9];
    long double biquadDR[9];

    bool flipL;
    bool flipR;

    long double lastSampleL;
    long double lastSampleR;

    uint32_t fpd;
    //default stuff

    float A;
    float B;

    Tape()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SLAM_PARAM, 0.f, 1.f, 0.5f, "Slam");
        configParam(BUMP_PARAM, 0.f, 1.f, 0.5f, "Bump");

        A = 0.5;
        B = 0.5;
        iirMidRollerAR = 0.0;
        iirMidRollerBR = 0.0;
        iirHeadBumpAR = 0.0;
        iirHeadBumpBR = 0.0;
        for (int x = 0; x < 9; x++) {
            biquadAR[x] = 0.0;
            biquadBR[x] = 0.0;
            biquadCR[x] = 0.0;
            biquadDR[x] = 0.0;
        }
        flipL = false;
        flipR = false;
        lastSampleL = 0.0;
        lastSampleR = 0.0;
        fpd = 17;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void processChannel(float sampleRate, bool& flip, long double& lastSample, Param& slam, Param& bump, Input& slamCv, Input& bumpCv, Input& input, Output& output)
    {
        if (output.isConnected()) {

            float in[16] = {};
            float out[16] = {};
            int numChannels = 1;

            // params
            A = slam.getValue();
            A += slamCv.getVoltage() / 10;
            A = clamp(A, 0.01f, 0.99f);

            B = bump.getValue();
            B += bumpCv.getVoltage() / 10;
            B = clamp(B, 0.01f, 0.99f);

            if (input.isConnected()) {
                // get input
                numChannels = input.getChannels();
                input.readVoltages(in);
            }

            for (int i = 0; i < numChannels; i++) {

                double overallscale = 1.0;
                overallscale /= 44100.0;
                overallscale *= sampleRate;

                double inputgain = pow(10.0, ((A - 0.5) * 24.0) / 20.0);
                double bumpgain = B * 0.1;
                double HeadBumpFreq = 0.12 / overallscale;
                double softness = 0.618033988749894848204586;
                double RollAmount = (1.0 - softness) / overallscale;

                //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
                //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
                biquadAR[0] = biquadBR[0] = 0.0072 / overallscale;
                biquadAR[1] = biquadBR[1] = 0.0009;
                double K = tan(M_PI * biquadBR[0]);
                double norm = 1.0 / (1.0 + K / biquadBR[1] + K * K);
                biquadAR[2] = biquadBR[2] = K / biquadBR[1] * norm;
                biquadAR[4] = biquadBR[4] = -biquadBR[2];
                biquadAR[5] = biquadBR[5] = 2.0 * (K * K - 1.0) * norm;
                biquadAR[6] = biquadBR[6] = (1.0 - K / biquadBR[1] + K * K) * norm;

                biquadCR[0] = biquadDR[0] = 0.032 / overallscale;
                biquadCR[1] = biquadDR[1] = 0.0007;
                K = tan(M_PI * biquadDR[0]);
                norm = 1.0 / (1.0 + K / biquadDR[1] + K * K);
                biquadCR[2] = biquadDR[2] = K / biquadDR[1] * norm;
                biquadCR[4] = biquadDR[4] = -biquadDR[2];
                biquadCR[6] = biquadDR[6] = (1.0 - K / biquadDR[1] + K * K) * norm;

                long double inputSample = in[i];

                // pad gain
                inputSample *= gainCut;

                if (fabs(inputSample) < 1.18e-37)
                    inputSample = fpd * 1.18e-37;

                long double drySampleR = inputSample;

                long double HighsSampleR = 0.0;
                long double NonHighsSampleR = 0.0;
                long double tempSample;

                if (flip) {
                    iirMidRollerAR = (iirMidRollerAR * (1.0 - RollAmount)) + (inputSample * RollAmount);
                    HighsSampleR = inputSample - iirMidRollerAR;
                    NonHighsSampleR = iirMidRollerAR;

                    iirHeadBumpAR += (inputSample * 0.05);
                    iirHeadBumpAR -= (iirHeadBumpAR * iirHeadBumpAR * iirHeadBumpAR * HeadBumpFreq);
                    iirHeadBumpAR = sin(iirHeadBumpAR);

                    tempSample = (iirHeadBumpAR * biquadAR[2]) + biquadAR[7];
                    biquadAR[7] = (iirHeadBumpAR * biquadAR[3]) - (tempSample * biquadAR[5]) + biquadAR[8];
                    biquadAR[8] = (iirHeadBumpAR * biquadAR[4]) - (tempSample * biquadAR[6]);
                    iirHeadBumpAR = tempSample; //interleaved biquad
                    if (iirHeadBumpAR > 1.0)
                        iirHeadBumpAR = 1.0;
                    if (iirHeadBumpAR < -1.0)
                        iirHeadBumpAR = -1.0;
                    iirHeadBumpAR = asin(iirHeadBumpAR);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * biquadCR[2]) + biquadCR[7];
                    biquadCR[7] = (inputSample * biquadCR[3]) - (tempSample * biquadCR[5]) + biquadCR[8];
                    biquadCR[8] = (inputSample * biquadCR[4]) - (tempSample * biquadCR[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                } else {
                    iirMidRollerBR = (iirMidRollerBR * (1.0 - RollAmount)) + (inputSample * RollAmount);
                    HighsSampleR = inputSample - iirMidRollerBR;
                    NonHighsSampleR = iirMidRollerBR;

                    iirHeadBumpBR += (inputSample * 0.05);
                    iirHeadBumpBR -= (iirHeadBumpBR * iirHeadBumpBR * iirHeadBumpBR * HeadBumpFreq);
                    iirHeadBumpBR = sin(iirHeadBumpBR);

                    tempSample = (iirHeadBumpBR * biquadBR[2]) + biquadBR[7];
                    biquadBR[7] = (iirHeadBumpBR * biquadBR[3]) - (tempSample * biquadBR[5]) + biquadBR[8];
                    biquadBR[8] = (iirHeadBumpBR * biquadBR[4]) - (tempSample * biquadBR[6]);
                    iirHeadBumpBR = tempSample; //interleaved biquad
                    if (iirHeadBumpBR > 1.0)
                        iirHeadBumpBR = 1.0;
                    if (iirHeadBumpBR < -1.0)
                        iirHeadBumpBR = -1.0;
                    iirHeadBumpBR = asin(iirHeadBumpBR);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * biquadDR[2]) + biquadDR[7];
                    biquadDR[7] = (inputSample * biquadDR[3]) - (tempSample * biquadDR[5]) + biquadDR[8];
                    biquadDR[8] = (inputSample * biquadDR[4]) - (tempSample * biquadDR[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                }
                flip = !flip;

                long double groundSampleR = drySampleR - inputSample; //set up UnBox

                if (inputgain != 1.0) {
                    inputSample *= inputgain;
                } //gain boost inside UnBox: do not boost fringe audio

                long double applySoften = fabs(HighsSampleR) * 1.57079633;
                if (applySoften > 1.57079633)
                    applySoften = 1.57079633;
                applySoften = 1 - cos(applySoften);
                if (HighsSampleR > 0)
                    inputSample -= applySoften;
                if (HighsSampleR < 0)
                    inputSample += applySoften;
                //apply Soften depending on polarity

                if (inputSample > 1.2533141373155)
                    inputSample = 1.2533141373155;
                if (inputSample < -1.2533141373155)
                    inputSample = -1.2533141373155;
                //clip to 1.2533141373155 to reach maximum output
                inputSample = sin(inputSample * fabs(inputSample)) / ((inputSample == 0.0) ? 1 : fabs(inputSample));
                //Spiral, for cleanest most optimal tape effect

                double suppress = (1.0 - fabs(inputSample)) * 0.00013;
                if (iirHeadBumpAR > suppress)
                    iirHeadBumpAR -= suppress;
                if (iirHeadBumpAR < -suppress)
                    iirHeadBumpAR += suppress;
                if (iirHeadBumpBR > suppress)
                    iirHeadBumpBR -= suppress;
                if (iirHeadBumpBR < -suppress)
                    iirHeadBumpBR += suppress;
                //restrain resonant quality of head bump algorithm

                inputSample += groundSampleR; //apply UnBox processing

                inputSample += ((iirHeadBumpAR + iirHeadBumpBR) * bumpgain); //and head bump

                if (lastSample >= 0.99) {
                    if (inputSample < 0.99)
                        lastSample = ((0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        lastSample = 0.99;
                }

                if (lastSample <= -0.99) {
                    if (inputSample > -0.99)
                        lastSample = ((-0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        lastSample = -0.99;
                }

                if (inputSample > 0.99) {
                    if (lastSample < 0.99)
                        inputSample = ((0.99 * softness) + (lastSample * (1.0 - softness)));
                    else
                        inputSample = 0.99;
                }

                if (inputSample < -0.99) {
                    if (lastSample > -0.99)
                        inputSample = ((-0.99 * softness) + (lastSample * (1.0 - softness)));
                    else
                        inputSample = -0.99;
                }
                lastSample = inputSample; //end ADClip R

                if (inputSample > 0.99)
                    inputSample = 0.99;
                if (inputSample < -0.99)
                    inputSample = -0.99;
                //final iron bar

                // bring gain back up
                inputSample *= gainBoost;

                //begin 32 bit stereo floating point dither
                int expon;
                frexpf((float)inputSample, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                inputSample += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                //end 32 bit stereo floating point dither

                out[i] = inputSample;
            }
            // output
            output.setChannels(numChannels);
            output.writeVoltages(out);
        }
    }

    void process(const ProcessArgs& args) override
    {
        // left
        processChannel(args.sampleRate, flipL, lastSampleL, params[SLAM_PARAM], params[BUMP_PARAM], inputs[SLAM_CV_INPUT], inputs[BUMP_CV_INPUT], inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
        // right
        processChannel(args.sampleRate, flipR, lastSampleR, params[SLAM_PARAM], params[BUMP_PARAM], inputs[SLAM_CV_INPUT], inputs[BUMP_CV_INPUT], inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
    }
};

struct TapeWidget : ModuleWidget {
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