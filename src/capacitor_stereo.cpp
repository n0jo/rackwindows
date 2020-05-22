/***********************************************************************************************
Capacitor Stereo
----------------
VCV Rack module based on Capacitor by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- separate controls for left and right channels
- CV inputs for lowpass, highpass and dry/wet
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

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

    const double gainCut = 0.03125;
    const double gainBoost = 32.0;

    bool isLinked;
    bool quality;

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
        float wetChase;

        float lowpassAmount;
        float highpassAmount;
        float wet;

        float lastLowpass;
        float lastHighpass;
        float lastWet;
    } v32L[16], v32R[16];

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
        double wetChase;

        double lowpassAmount;
        double highpassAmount;
        double wet;

        double lastLowpass;
        double lastHighpass;
        double lastWet;
    } v64L[16], v64R[16];

    int countL[16];
    int countR[16];

    float lastLowpass;
    float lastHighpass;

    long double fpNShapeL[16];
    long double fpNShapeR[16];

    float A;
    float B;
    float C;

    Capacitor_stereo()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_L_PARAM, 0.f, 1.f, 1.f, "Lowpass L");
        configParam(LOWPASS_R_PARAM, 0.f, 1.f, 1.f, "Lowpass R");
        configParam(HIGHPASS_L_PARAM, 0.f, 1.f, 0.f, "Highpass L");
        configParam(HIGHPASS_R_PARAM, 0.f, 1.f, 0.f, "Highpass R");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");
        configParam(LINK_PARAM, 0.f, 1.f, 1.f, "Link");

        isLinked = true;
        quality = loadQuality();

        for (int i = 0; i < 16; i++) {
            v32L[i].iirHighpassA = v64L[i].iirHighpassA = v32R[i].iirHighpassA = v64R[i].iirHighpassA = 0.0;
            v32L[i].iirHighpassB = v64L[i].iirHighpassB = v32R[i].iirHighpassB = v64R[i].iirHighpassB = 0.0;
            v32L[i].iirHighpassC = v64L[i].iirHighpassC = v32R[i].iirHighpassC = v64R[i].iirHighpassC = 0.0;
            v32L[i].iirHighpassD = v64L[i].iirHighpassD = v32R[i].iirHighpassD = v64R[i].iirHighpassD = 0.0;
            v32L[i].iirHighpassE = v64L[i].iirHighpassE = v32R[i].iirHighpassE = v64R[i].iirHighpassE = 0.0;
            v32L[i].iirHighpassF = v64L[i].iirHighpassF = v32R[i].iirHighpassF = v64R[i].iirHighpassF = 0.0;
            v32L[i].iirLowpassA = v64L[i].iirLowpassA = v32R[i].iirLowpassA = v64R[i].iirLowpassA = 0.0;
            v32L[i].iirLowpassB = v64L[i].iirLowpassB = v32R[i].iirLowpassB = v64R[i].iirLowpassB = 0.0;
            v32L[i].iirLowpassC = v64L[i].iirLowpassC = v32R[i].iirLowpassC = v64R[i].iirLowpassC = 0.0;
            v32L[i].iirLowpassD = v64L[i].iirLowpassD = v32R[i].iirLowpassD = v64R[i].iirLowpassD = 0.0;
            v32L[i].iirLowpassE = v64L[i].iirLowpassE = v32R[i].iirLowpassE = v64R[i].iirLowpassE = 0.0;
            v32L[i].iirLowpassF = v64L[i].iirLowpassF = v32R[i].iirLowpassF = v64R[i].iirLowpassF = 0.0;
            v32L[i].lowpassChase = v64L[i].lowpassChase = v32R[i].lowpassChase = v64R[i].lowpassChase = 0.0;
            v32L[i].highpassChase = v64L[i].highpassChase = v32R[i].highpassChase = v64R[i].highpassChase = 0.0;
            v32L[i].wetChase = v64L[i].wetChase = v32R[i].wetChase = v64R[i].wetChase = 0.0;
            v32L[i].lowpassAmount = v64L[i].lowpassAmount = v32R[i].lowpassAmount = v64R[i].lowpassAmount = 1.0;
            v32L[i].highpassAmount = v64L[i].highpassAmount = v32R[i].highpassAmount = v64R[i].highpassAmount = 0.0;
            v32L[i].wet = v64L[i].wet = v32R[i].wet = v64R[i].wet = 1.0;
            v32L[i].lastLowpass = v64L[i].lastLowpass = v32R[i].lastLowpass = v64R[i].lastLowpass = 1000.0;
            v32L[i].lastHighpass = v64L[i].lastHighpass = v32R[i].lastHighpass = v64R[i].lastHighpass = 1000.0;
            v32L[i].lastWet = v64L[i].lastWet = v32R[i].lastWet = v64R[i].lastWet = 1000.0;

            countL[i] = countR[i] = 0;

            lastLowpass = lastHighpass = 0.0;

            fpNShapeL[i] = fpNShapeR[i] = 0.0;
        }
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

    void processChannel32(vars32 v[], int count[], Param& lowpass, Param& highpass, Param& drywet, Input& lowpassCv, Input& highpassCv, Input& drywetCv, Input& input, Output& output)
    {
        if (output.isConnected()) {

            // params
            A = lowpass.getValue();
            A += lowpassCv.getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = highpass.getValue();
            B += highpassCv.getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            C = drywet.getValue();
            C += drywetCv.getVoltage() / 5;
            C = clamp(C, 0.01f, 0.99f);

            float lowpassSpeed;
            float highpassSpeed;
            float wetSpeed;
            float invLowpass;
            float invHighpass;
            float dry;

            float inputSample;
            float drySample;

            // for each poly channel
            for (int i = 0, numChannels = std::max(1, input.getChannels()); i < numChannels; ++i) {

                v[i].lowpassChase = pow(A, 2);
                v[i].highpassChase = pow(B, 2);
                v[i].wetChase = C;
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

                drySample = inputSample;

                v[i].lowpassAmount = (((v[i].lowpassAmount * lowpassSpeed) + v[i].lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v[i].lowpassAmount;
                v[i].highpassAmount = (((v[i].highpassAmount * highpassSpeed) + v[i].highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v[i].highpassAmount;
                v[i].wet = (((v[i].wet * wetSpeed) + v[i].wetChase) / (wetSpeed + 1.0));
                dry = 1.0 - v[i].wet;

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

                inputSample = (drySample * dry) + (inputSample * v[i].wet);

                // bring gain back up
                inputSample *= gainBoost;

                // output
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }

    void processChannel64(vars64 v[], int count[], long double fpNShape[], Param& lowpass, Param& highpass, Param& drywet, Input& lowpassCv, Input& highpassCv, Input& drywetCv, Input& input, Output& output)
    {
        if (output.isConnected()) {

            // params
            A = lowpass.getValue();
            A += lowpassCv.getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = highpass.getValue();
            B += highpassCv.getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            C = drywet.getValue();
            C += drywetCv.getVoltage() / 5;
            C = clamp(C, 0.01f, 0.99f);

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

                v[i].lowpassChase = pow(A, 2);
                v[i].highpassChase = pow(B, 2);
                v[i].wetChase = C;
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

                drySample = inputSample;

                v[i].lowpassAmount = (((v[i].lowpassAmount * lowpassSpeed) + v[i].lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v[i].lowpassAmount;
                v[i].highpassAmount = (((v[i].highpassAmount * highpassSpeed) + v[i].highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v[i].highpassAmount;
                v[i].wet = (((v[i].wet * wetSpeed) + v[i].wetChase) / (wetSpeed + 1.0));
                dry = 1.0 - v[i].wet;

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

                inputSample = (drySample * dry) + (inputSample * v[i].wet);

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
        // link
        isLinked = params[LINK_PARAM].getValue() ? true : false;

        // if (isLinked = params[LINK_PARAM].getValue() ? true : false) {
        if (isLinked) {
            if (params[LOWPASS_L_PARAM].getValue() != lastLowpass) {
                params[LOWPASS_R_PARAM] = params[LOWPASS_L_PARAM];
            } else if (params[LOWPASS_R_PARAM].getValue() != lastLowpass) {
                params[LOWPASS_L_PARAM] = params[LOWPASS_R_PARAM];
            }
            if (params[HIGHPASS_L_PARAM].getValue() != lastHighpass) {
                params[HIGHPASS_R_PARAM] = params[HIGHPASS_L_PARAM];
            } else if (params[HIGHPASS_R_PARAM].getValue() != lastHighpass) {
                params[HIGHPASS_L_PARAM] = params[HIGHPASS_R_PARAM];
            }
        }

        lastLowpass = params[LOWPASS_R_PARAM].getValue();
        lastHighpass = params[HIGHPASS_R_PARAM].getValue();

        switch (quality) {
        case 1:
            processChannel32(v32L, countL, params[LOWPASS_L_PARAM], params[HIGHPASS_L_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_L_INPUT], inputs[HIGHPASS_CV_L_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
            processChannel32(v32R, countR, params[LOWPASS_R_PARAM], params[HIGHPASS_R_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_R_INPUT], inputs[HIGHPASS_CV_R_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
            break;
        default:
            processChannel64(v64L, countL, fpNShapeL, params[LOWPASS_L_PARAM], params[HIGHPASS_L_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_L_INPUT], inputs[HIGHPASS_CV_L_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_L_INPUT], outputs[OUT_L_OUTPUT]);
            processChannel64(v64R, countR, fpNShapeR, params[LOWPASS_R_PARAM], params[HIGHPASS_R_PARAM], params[DRYWET_PARAM], inputs[LOWPASS_CV_R_INPUT], inputs[HIGHPASS_CV_R_INPUT], inputs[DRYWET_CV_INPUT], inputs[IN_R_INPUT], outputs[OUT_R_OUTPUT]);
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

        QualityItem* high = new QualityItem(); // high quality
        high->text = "High";
        high->module = module;
        high->quality = 0;
        menu->addChild(high);

        QualityItem* low = new QualityItem(); // low quality
        low->text = "Low";
        low->module = module;
        low->quality = 1;
        menu->addChild(low);
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