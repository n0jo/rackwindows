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

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Capacitor : Module {
    enum ParamIds {
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        // DRYWET_PARAM,
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
    dsp::ClockDivider partTimeJob;

    // control parameters
    float lowpassParam;
    float highpassParam;

    // global variables (as arrays in order to handle up to 16 polyphonic channels)
    struct vars32 {
        float iirHighpassA;
        float iirHighpassB;
        float iirHighpassC;
        float iirHighpassD;
        float iirHighpassE;
        float iirHighpassF;
        float iirLowpassA;
        float iirLowpassB;
        float iirLowpassC;
        float iirLowpassD;
        float iirLowpassE;
        float iirLowpassF;
        float lowpassChase;
        float highpassChase;
        float lowpassAmount;
        float highpassAmount;
        float lastLowpass;
        float lastHighpass;
    } v32[16];

    struct vars64 {
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
        double lowpassAmount;
        double highpassAmount;
        double lastLowpass;
        double lastHighpass;
    } v64[16];

    int count[16];
    long double fpNShape[16];

    // part-time variables (which do not need to be updated every cycle)
    double overallscale;

    Capacitor()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Lowpass");
        configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "Highpass");

        quality = loadQuality();

        partTimeJob.setDivision(2);

        onSampleRateChange();
        updateParams();

        for (int i = 0; i < 16; i++) {
            v32[i].iirHighpassA = v64[i].iirHighpassA = 0.0;
            v32[i].iirHighpassB = v64[i].iirHighpassB = 0.0;
            v32[i].iirHighpassC = v64[i].iirHighpassC = 0.0;
            v32[i].iirHighpassD = v64[i].iirHighpassD = 0.0;
            v32[i].iirHighpassE = v64[i].iirHighpassE = 0.0;
            v32[i].iirHighpassF = v64[i].iirHighpassF = 0.0;
            v32[i].iirLowpassA = v64[i].iirLowpassA = 0.0;
            v32[i].iirLowpassB = v64[i].iirLowpassB = 0.0;
            v32[i].iirLowpassC = v64[i].iirLowpassC = 0.0;
            v32[i].iirLowpassD = v64[i].iirLowpassD = 0.0;
            v32[i].iirLowpassE = v64[i].iirLowpassE = 0.0;
            v32[i].iirLowpassF = v64[i].iirLowpassF = 0.0;
            v32[i].lowpassChase = v64[i].lowpassChase = 0.0;
            v32[i].highpassChase = v64[i].highpassChase = 0.0;
            // v32[i].wetChase = v64[i].wetChase = 0.0;
            v32[i].lowpassAmount = v64[i].lowpassAmount = 1.0;
            v32[i].highpassAmount = v64[i].highpassAmount = 0.0;
            // v32[i].wet = v64[i].wet = 1.0;
            v32[i].lastLowpass = v64[i].lastLowpass = 1000.0;
            v32[i].lastHighpass = v64[i].lastHighpass = 1000.0;
            // v32[i].lastWet = v64[i].lastWet = 1000.0;

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
        lowpassParam = params[LOWPASS_PARAM].getValue();
        lowpassParam += inputs[LOWPASS_CV_INPUT].getVoltage() / 5;
        lowpassParam = clamp(lowpassParam, 0.01f, 0.99f);

        highpassParam = params[HIGHPASS_PARAM].getValue();
        highpassParam += inputs[HIGHPASS_CV_INPUT].getVoltage() / 5;
        highpassParam = clamp(highpassParam, 0.01f, 0.99f);
    }

    void processChannel32(vars32 v[], Input& input, Output& output)
    {
        if (output.isConnected()) {

            // stuff that doesn't need to be processed every cycle
            if (partTimeJob.process()) {
                updateParams();
            }

            float lowpassSpeed;
            float highpassSpeed;
            float invLowpass;
            float invHighpass;
            float inputSample;

            // for each poly channel
            for (int i = 0, numChannels = std::max(1, input.getChannels()); i < numChannels; ++i) {

                v[i].lowpassChase = pow(lowpassParam, 2);
                v[i].highpassChase = pow(highpassParam, 2);
                //should not scale with sample rate, because values reaching 1 are important
                //to its ability to bypass when set to max
                lowpassSpeed = 300 / (fabs(v[i].lastLowpass - v[i].lowpassChase) + 1.0);
                highpassSpeed = 300 / (fabs(v[i].lastHighpass - v[i].highpassChase) + 1.0);
                v[i].lastLowpass = v[i].lowpassChase;
                v[i].lastHighpass = v[i].highpassChase;

                // input
                inputSample = input.getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

                v[i].lowpassAmount = (((v[i].lowpassAmount * lowpassSpeed) + v[i].lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v[i].lowpassAmount;
                v[i].highpassAmount = (((v[i].highpassAmount * highpassSpeed) + v[i].highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v[i].highpassAmount;

                //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
                //steepens the filter after minimizing artifacts.
                count[i]++;
                if (count[i] > 5)
                    count[i] = 0;
                switch (count[i]) {
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

                // bring gain back up
                inputSample *= gainBoost;

                // output
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }

    void processChannel64(vars64 v[], Input& input, Output& output)
    {
        if (output.isConnected()) {

            // stuff that doesn't need to be processed every cycle
            if (partTimeJob.process()) {
                updateParams();
            }

            double lowpassSpeed;
            double highpassSpeed;
            double invLowpass;
            double invHighpass;
            long double inputSample;

            // for each poly channel
            for (int i = 0, numChannels = std::max(1, inputs[IN_INPUT].getChannels()); i < numChannels; ++i) {

                v[i].lowpassChase = pow(lowpassParam, 2);
                v[i].highpassChase = pow(highpassParam, 2);
                //should not scale with sample rate, because values reaching 1 are important
                //to its ability to bypass when set to max
                lowpassSpeed = 300 / (fabs(v[i].lastLowpass - v[i].lowpassChase) + 1.0);
                highpassSpeed = 300 / (fabs(v[i].lastHighpass - v[i].highpassChase) + 1.0);
                v[i].lastLowpass = v[i].lowpassChase;
                v[i].lastHighpass = v[i].highpassChase;

                // input
                inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // pad gain
                inputSample *= gainCut;

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

                v[i].lowpassAmount = (((v[i].lowpassAmount * lowpassSpeed) + v[i].lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v[i].lowpassAmount;
                v[i].highpassAmount = (((v[i].highpassAmount * highpassSpeed) + v[i].highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v[i].highpassAmount;

                //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
                //steepens the filter after minimizing artifacts.
                count[i]++;
                if (count[i] > 5)
                    count[i] = 0;
                switch (count[i]) {
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

                //stereo 32 bit dither, made small and tidy.
                int expon;
                frexpf((float)inputSample, &expon);
                long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                inputSample += (dither - fpNShape[i]);
                fpNShape[i] = dither;
                //end 32 bit dither

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
        switch (quality) {
        case 1:
            processChannel64(v64, inputs[IN_INPUT], outputs[OUT_OUTPUT]);
            break;
        default:
            processChannel32(v32, inputs[IN_INPUT], outputs[OUT_OUTPUT]);
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