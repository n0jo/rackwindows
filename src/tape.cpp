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

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    dsp::ClockDivider partTimeJob;

    // control parameters
    float slamParam;
    float bumpParam;

    // global variables (as arrays in order to handle up to 16 polyphonic channels)
    struct vars32 {
        float iirMidRollerA;
        float iirMidRollerB;
        float iirHeadBumpA;
        float iirHeadBumpB;

        float biquadA[9];
        float biquadB[9];
        float biquadC[9];
        float biquadD[9];

        float lastSample;
        bool flip;
        uint32_t fpd;
    } v32L[16], v32R[16];

    struct vars64 {
        double iirMidRollerA;
        double iirMidRollerB;
        double iirHeadBumpA;
        double iirHeadBumpB;

        long double biquadA[9];
        long double biquadB[9];
        long double biquadC[9];
        long double biquadD[9];

        long double lastSample;
        bool flip;
        uint32_t fpd;
    } v64L[16], v64R[16];

    // part-time variables (which do not need to be updated every cycle)
    double overallscale;
    double inputgain;
    double bumpgain;
    double HeadBumpFreq;
    double rollAmount;

    // constants
    const double softness = 0.618033988749894848204586;

    Tape()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(SLAM_PARAM, 0.f, 1.f, 0.5f, "Slam");
        configParam(BUMP_PARAM, 0.f, 1.f, 0.5f, "Bump");

        quality = loadQuality();

        partTimeJob.setDivision(2);

        onSampleRateChange();
        updateParams();

        for (int i = 0; i < 16; i++) {
            v32L[i].iirMidRollerA = v32L[i].iirMidRollerB = v32L[i].iirHeadBumpA = v32L[i].iirHeadBumpB = 0.0;
            v32R[i].iirMidRollerA = v32R[i].iirMidRollerB = v32R[i].iirHeadBumpA = v32R[i].iirHeadBumpB = 0.0;
            v64L[i].iirMidRollerA = v64L[i].iirMidRollerB = v64L[i].iirHeadBumpA = v64L[i].iirHeadBumpB = 0.0;
            v64R[i].iirMidRollerA = v64R[i].iirMidRollerB = v64R[i].iirHeadBumpA = v64R[i].iirHeadBumpB = 0.0;

            for (int x = 0; x < 9; x++) {
                v32L[i].biquadA[x] = v32L[i].biquadB[x] = v32L[i].biquadC[x] = v32L[i].biquadD[x] = 0.0;
                v32R[i].biquadA[x] = v32R[i].biquadB[x] = v32R[i].biquadC[x] = v32R[i].biquadD[x] = 0.0;
                v64L[i].biquadA[x] = v64L[i].biquadB[x] = v64L[i].biquadC[x] = v64L[i].biquadD[x] = 0.0;
                v64R[i].biquadA[x] = v64R[i].biquadB[x] = v64R[i].biquadC[x] = v64R[i].biquadD[x] = 0.0;
            }

            v32L[i].flip = v32R[i].flip = false;
            v64L[i].flip = v64R[i].flip = false;

            v32L[i].lastSample = v32R[i].lastSample = 0.0;
            v64L[i].lastSample = v64R[i].lastSample = 0.0;

            v32L[i].fpd = v32R[i].fpd = 17;
            v64L[i].fpd = v64R[i].fpd = 17;
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
        slamParam = params[SLAM_PARAM].getValue();
        slamParam += inputs[SLAM_CV_INPUT].getVoltage() / 10;
        slamParam = clamp(slamParam, 0.01f, 0.99f);

        bumpParam = params[BUMP_PARAM].getValue();
        bumpParam += inputs[BUMP_CV_INPUT].getVoltage() / 10;
        bumpParam = clamp(bumpParam, 0.01f, 0.99f);

        inputgain = pow(10.0, ((slamParam - 0.5) * 24.0) / 20.0);
        bumpgain = bumpParam * 0.1;
        HeadBumpFreq = 0.12 / overallscale;
        rollAmount = (1.0 - softness) / overallscale;
    }

    void processChannel32(vars32 v[], Input& input, Output& output)
    {
        if (output.isConnected()) {

            // stuff that doesn't need to be processed every cycle
            if (partTimeJob.process()) {
                updateParams();
            }

            float in[16] = {};
            float out[16] = {};
            int numChannels = 1;

            float suppress;
            float drySample;
            float highsSample;
            float nonHighsSample;
            float tempSample;
            float groundSample;
            float applySoften;
            float inputSample;
            float K;
            float norm;

            if (input.isConnected()) {
                // get input
                numChannels = input.getChannels();
                input.readVoltages(in);
            }

            for (int i = 0; i < numChannels; i++) {

                //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
                //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
                v[i].biquadA[0] = v[i].biquadB[0] = 0.0072 / overallscale;
                v[i].biquadA[1] = v[i].biquadB[1] = 0.0009;
                K = tan(M_PI * v[i].biquadB[0]);
                norm = 1.0 / (1.0 + K / v[i].biquadB[1] + K * K);
                v[i].biquadA[2] = v[i].biquadB[2] = K / v[i].biquadB[1] * norm;
                v[i].biquadA[4] = v[i].biquadB[4] = -v[i].biquadB[2];
                v[i].biquadA[5] = v[i].biquadB[5] = 2.0 * (K * K - 1.0) * norm;
                v[i].biquadA[6] = v[i].biquadB[6] = (1.0 - K / v[i].biquadB[1] + K * K) * norm;

                v[i].biquadC[0] = v[i].biquadD[0] = 0.032 / overallscale;
                v[i].biquadC[1] = v[i].biquadD[1] = 0.0007;
                K = tan(M_PI * v[i].biquadD[0]);
                norm = 1.0 / (1.0 + K / v[i].biquadD[1] + K * K);
                v[i].biquadC[2] = v[i].biquadD[2] = K / v[i].biquadD[1] * norm;
                v[i].biquadC[4] = v[i].biquadD[4] = -v[i].biquadD[2];
                v[i].biquadC[6] = v[i].biquadD[6] = (1.0 - K / v[i].biquadD[1] + K * K) * norm;

                inputSample = in[i];

                // pad gain
                inputSample *= gainCut;

                drySample = inputSample;

                highsSample = 0.0;
                nonHighsSample = 0.0;

                if (v[i].flip) {
                    v[i].iirMidRollerA = (v[i].iirMidRollerA * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - v[i].iirMidRollerA;
                    nonHighsSample = v[i].iirMidRollerA;

                    v[i].iirHeadBumpA += (inputSample * 0.05);
                    v[i].iirHeadBumpA -= (v[i].iirHeadBumpA * v[i].iirHeadBumpA * v[i].iirHeadBumpA * HeadBumpFreq);
                    v[i].iirHeadBumpA = sin(v[i].iirHeadBumpA);

                    tempSample = (v[i].iirHeadBumpA * v[i].biquadA[2]) + v[i].biquadA[7];
                    v[i].biquadA[7] = (v[i].iirHeadBumpA * v[i].biquadA[3]) - (tempSample * v[i].biquadA[5]) + v[i].biquadA[8];
                    v[i].biquadA[8] = (v[i].iirHeadBumpA * v[i].biquadA[4]) - (tempSample * v[i].biquadA[6]);
                    v[i].iirHeadBumpA = tempSample; //interleaved biquad
                    if (v[i].iirHeadBumpA > 1.0)
                        v[i].iirHeadBumpA = 1.0;
                    if (v[i].iirHeadBumpA < -1.0)
                        v[i].iirHeadBumpA = -1.0;
                    v[i].iirHeadBumpA = asin(v[i].iirHeadBumpA);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * v[i].biquadC[2]) + v[i].biquadC[7];
                    v[i].biquadC[7] = (inputSample * v[i].biquadC[3]) - (tempSample * v[i].biquadC[5]) + v[i].biquadC[8];
                    v[i].biquadC[8] = (inputSample * v[i].biquadC[4]) - (tempSample * v[i].biquadC[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                } else {
                    v[i].iirMidRollerB = (v[i].iirMidRollerB * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - v[i].iirMidRollerB;
                    nonHighsSample = v[i].iirMidRollerB;

                    v[i].iirHeadBumpB += (inputSample * 0.05);
                    v[i].iirHeadBumpB -= (v[i].iirHeadBumpB * v[i].iirHeadBumpB * v[i].iirHeadBumpB * HeadBumpFreq);
                    v[i].iirHeadBumpB = sin(v[i].iirHeadBumpB);

                    tempSample = (v[i].iirHeadBumpB * v[i].biquadB[2]) + v[i].biquadB[7];
                    v[i].biquadB[7] = (v[i].iirHeadBumpB * v[i].biquadB[3]) - (tempSample * v[i].biquadB[5]) + v[i].biquadB[8];
                    v[i].biquadB[8] = (v[i].iirHeadBumpB * v[i].biquadB[4]) - (tempSample * v[i].biquadB[6]);
                    v[i].iirHeadBumpB = tempSample; //interleaved biquad
                    if (v[i].iirHeadBumpB > 1.0)
                        v[i].iirHeadBumpB = 1.0;
                    if (v[i].iirHeadBumpB < -1.0)
                        v[i].iirHeadBumpB = -1.0;
                    v[i].iirHeadBumpB = asin(v[i].iirHeadBumpB);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * v[i].biquadD[2]) + v[i].biquadD[7];
                    v[i].biquadD[7] = (inputSample * v[i].biquadD[3]) - (tempSample * v[i].biquadD[5]) + v[i].biquadD[8];
                    v[i].biquadD[8] = (inputSample * v[i].biquadD[4]) - (tempSample * v[i].biquadD[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                }
                v[i].flip = !v[i].flip;

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
                if (v[i].iirHeadBumpA > suppress)
                    v[i].iirHeadBumpA -= suppress;
                if (v[i].iirHeadBumpA < -suppress)
                    v[i].iirHeadBumpA += suppress;
                if (v[i].iirHeadBumpB > suppress)
                    v[i].iirHeadBumpB -= suppress;
                if (v[i].iirHeadBumpB < -suppress)
                    v[i].iirHeadBumpB += suppress;
                //restrain resonant quality of head bump algorithm

                inputSample += groundSample; //apply UnBox processing

                inputSample += ((v[i].iirHeadBumpA + v[i].iirHeadBumpB) * bumpgain); //and head bump

                if (v[i].lastSample >= 0.99) {
                    if (inputSample < 0.99)
                        v[i].lastSample = ((0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        v[i].lastSample = 0.99;
                }

                if (v[i].lastSample <= -0.99) {
                    if (inputSample > -0.99)
                        v[i].lastSample = ((-0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        v[i].lastSample = -0.99;
                }

                if (inputSample > 0.99) {
                    if (v[i].lastSample < 0.99)
                        inputSample = ((0.99 * softness) + (v[i].lastSample * (1.0 - softness)));
                    else
                        inputSample = 0.99;
                }

                if (inputSample < -0.99) {
                    if (v[i].lastSample > -0.99)
                        inputSample = ((-0.99 * softness) + (v[i].lastSample * (1.0 - softness)));
                    else
                        inputSample = -0.99;
                }
                v[i].lastSample = inputSample; //end ADClip R

                if (inputSample > 0.99)
                    inputSample = 0.99;
                if (inputSample < -0.99)
                    inputSample = -0.99;
                //final iron bar

                // bring gain back up
                inputSample *= gainBoost;

                out[i] = inputSample;
            }
            // output
            output.setChannels(numChannels);
            output.writeVoltages(out);
        }
    }

    void processChannel64(vars64 v[], Input& input, Output& output)
    {
        if (output.isConnected()) {

            // stuff that doesn't need to be processed every cycle
            if (partTimeJob.process()) {
                updateParams();
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

                //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
                //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
                v[i].biquadA[0] = v[i].biquadB[0] = 0.0072 / overallscale;
                v[i].biquadA[1] = v[i].biquadB[1] = 0.0009;
                K = tan(M_PI * v[i].biquadB[0]);
                norm = 1.0 / (1.0 + K / v[i].biquadB[1] + K * K);
                v[i].biquadA[2] = v[i].biquadB[2] = K / v[i].biquadB[1] * norm;
                v[i].biquadA[4] = v[i].biquadB[4] = -v[i].biquadB[2];
                v[i].biquadA[5] = v[i].biquadB[5] = 2.0 * (K * K - 1.0) * norm;
                v[i].biquadA[6] = v[i].biquadB[6] = (1.0 - K / v[i].biquadB[1] + K * K) * norm;

                v[i].biquadC[0] = v[i].biquadD[0] = 0.032 / overallscale;
                v[i].biquadC[1] = v[i].biquadD[1] = 0.0007;
                K = tan(M_PI * v[i].biquadD[0]);
                norm = 1.0 / (1.0 + K / v[i].biquadD[1] + K * K);
                v[i].biquadC[2] = v[i].biquadD[2] = K / v[i].biquadD[1] * norm;
                v[i].biquadC[4] = v[i].biquadD[4] = -v[i].biquadD[2];
                v[i].biquadC[6] = v[i].biquadD[6] = (1.0 - K / v[i].biquadD[1] + K * K) * norm;

                inputSample = in[i];

                // pad gain
                inputSample *= gainCut;

                if (fabs(inputSample) < 1.18e-37)
                    inputSample = v[i].fpd * 1.18e-37;

                drySample = inputSample;

                highsSample = 0.0;
                nonHighsSample = 0.0;

                if (v[i].flip) {
                    v[i].iirMidRollerA = (v[i].iirMidRollerA * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - v[i].iirMidRollerA;
                    nonHighsSample = v[i].iirMidRollerA;

                    v[i].iirHeadBumpA += (inputSample * 0.05);
                    v[i].iirHeadBumpA -= (v[i].iirHeadBumpA * v[i].iirHeadBumpA * v[i].iirHeadBumpA * HeadBumpFreq);
                    v[i].iirHeadBumpA = sin(v[i].iirHeadBumpA);

                    tempSample = (v[i].iirHeadBumpA * v[i].biquadA[2]) + v[i].biquadA[7];
                    v[i].biquadA[7] = (v[i].iirHeadBumpA * v[i].biquadA[3]) - (tempSample * v[i].biquadA[5]) + v[i].biquadA[8];
                    v[i].biquadA[8] = (v[i].iirHeadBumpA * v[i].biquadA[4]) - (tempSample * v[i].biquadA[6]);
                    v[i].iirHeadBumpA = tempSample; //interleaved biquad
                    if (v[i].iirHeadBumpA > 1.0)
                        v[i].iirHeadBumpA = 1.0;
                    if (v[i].iirHeadBumpA < -1.0)
                        v[i].iirHeadBumpA = -1.0;
                    v[i].iirHeadBumpA = asin(v[i].iirHeadBumpA);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * v[i].biquadC[2]) + v[i].biquadC[7];
                    v[i].biquadC[7] = (inputSample * v[i].biquadC[3]) - (tempSample * v[i].biquadC[5]) + v[i].biquadC[8];
                    v[i].biquadC[8] = (inputSample * v[i].biquadC[4]) - (tempSample * v[i].biquadC[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                } else {
                    v[i].iirMidRollerB = (v[i].iirMidRollerB * (1.0 - rollAmount)) + (inputSample * rollAmount);
                    highsSample = inputSample - v[i].iirMidRollerB;
                    nonHighsSample = v[i].iirMidRollerB;

                    v[i].iirHeadBumpB += (inputSample * 0.05);
                    v[i].iirHeadBumpB -= (v[i].iirHeadBumpB * v[i].iirHeadBumpB * v[i].iirHeadBumpB * HeadBumpFreq);
                    v[i].iirHeadBumpB = sin(v[i].iirHeadBumpB);

                    tempSample = (v[i].iirHeadBumpB * v[i].biquadB[2]) + v[i].biquadB[7];
                    v[i].biquadB[7] = (v[i].iirHeadBumpB * v[i].biquadB[3]) - (tempSample * v[i].biquadB[5]) + v[i].biquadB[8];
                    v[i].biquadB[8] = (v[i].iirHeadBumpB * v[i].biquadB[4]) - (tempSample * v[i].biquadB[6]);
                    v[i].iirHeadBumpB = tempSample; //interleaved biquad
                    if (v[i].iirHeadBumpB > 1.0)
                        v[i].iirHeadBumpB = 1.0;
                    if (v[i].iirHeadBumpB < -1.0)
                        v[i].iirHeadBumpB = -1.0;
                    v[i].iirHeadBumpB = asin(v[i].iirHeadBumpB);

                    inputSample = sin(inputSample);
                    tempSample = (inputSample * v[i].biquadD[2]) + v[i].biquadD[7];
                    v[i].biquadD[7] = (inputSample * v[i].biquadD[3]) - (tempSample * v[i].biquadD[5]) + v[i].biquadD[8];
                    v[i].biquadD[8] = (inputSample * v[i].biquadD[4]) - (tempSample * v[i].biquadD[6]);
                    inputSample = tempSample; //interleaved biquad
                    if (inputSample > 1.0)
                        inputSample = 1.0;
                    if (inputSample < -1.0)
                        inputSample = -1.0;
                    inputSample = asin(inputSample);
                }
                v[i].flip = !v[i].flip;

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
                if (v[i].iirHeadBumpA > suppress)
                    v[i].iirHeadBumpA -= suppress;
                if (v[i].iirHeadBumpA < -suppress)
                    v[i].iirHeadBumpA += suppress;
                if (v[i].iirHeadBumpB > suppress)
                    v[i].iirHeadBumpB -= suppress;
                if (v[i].iirHeadBumpB < -suppress)
                    v[i].iirHeadBumpB += suppress;
                //restrain resonant quality of head bump algorithm

                inputSample += groundSample; //apply UnBox processing

                inputSample += ((v[i].iirHeadBumpA + v[i].iirHeadBumpB) * bumpgain); //and head bump

                if (v[i].lastSample >= 0.99) {
                    if (inputSample < 0.99)
                        v[i].lastSample = ((0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        v[i].lastSample = 0.99;
                }

                if (v[i].lastSample <= -0.99) {
                    if (inputSample > -0.99)
                        v[i].lastSample = ((-0.99 * softness) + (inputSample * (1.0 - softness)));
                    else
                        v[i].lastSample = -0.99;
                }

                if (inputSample > 0.99) {
                    if (v[i].lastSample < 0.99)
                        inputSample = ((0.99 * softness) + (v[i].lastSample * (1.0 - softness)));
                    else
                        inputSample = 0.99;
                }

                if (inputSample < -0.99) {
                    if (v[i].lastSample > -0.99)
                        inputSample = ((-0.99 * softness) + (v[i].lastSample * (1.0 - softness)));
                    else
                        inputSample = -0.99;
                }
                v[i].lastSample = inputSample; //end ADClip R

                if (inputSample > 0.99)
                    inputSample = 0.99;
                if (inputSample < -0.99)
                    inputSample = -0.99;
                //final iron bar

                //begin 32 bit stereo floating point dither
                int expon;
                frexpf((float)inputSample, &expon);
                v[i].fpd ^= v[i].fpd << 13;
                v[i].fpd ^= v[i].fpd >> 17;
                v[i].fpd ^= v[i].fpd << 5;
                inputSample += ((double(v[i].fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                //end 32 bit stereo floating point dither

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
        switch (quality) {
        case 1:
            processChannel64(v64L, inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
            processChannel64(v64R, inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
            break;
        default:
            processChannel32(v32L, inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
            processChannel32(v32R, inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
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