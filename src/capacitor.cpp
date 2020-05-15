/***********************************************************************************************
Capacitor
---------
VCV Rack module based on Capacitor by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono
- no Dry/Wet
- CV inputs for Lowpass and Highpass

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Capacitor : Module {
    enum ParamIds {
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        DRYWET_PARAM,
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

    // 32 bit variables
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
    } v32;

    // 64 bit variables
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
    } v64;

    long double fpNShape;

    int count;

    float A;
    float B;
    float C;

    Capacitor()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Lowpass");
        configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "Highpass");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        v32.iirHighpassA = v64.iirHighpassA = 0.0;
        v32.iirHighpassB = v64.iirHighpassB = 0.0;
        v32.iirHighpassC = v64.iirHighpassC = 0.0;
        v32.iirHighpassD = v64.iirHighpassD = 0.0;
        v32.iirHighpassE = v64.iirHighpassE = 0.0;
        v32.iirHighpassF = v64.iirHighpassF = 0.0;
        v32.iirLowpassA = v64.iirLowpassA = 0.0;
        v32.iirLowpassB = v64.iirLowpassB = 0.0;
        v32.iirLowpassC = v64.iirLowpassC = 0.0;
        v32.iirLowpassD = v64.iirLowpassD = 0.0;
        v32.iirLowpassE = v64.iirLowpassE = 0.0;
        v32.iirLowpassF = v64.iirLowpassF = 0.0;
        v32.lowpassChase = v64.lowpassChase = 0.0;
        v32.highpassChase = v64.highpassChase = 0.0;
        v32.wetChase = v64.wetChase = 0.0;
        v32.lowpassAmount = v64.lowpassAmount = 1.0;
        v32.highpassAmount = v64.highpassAmount = 0.0;
        v32.wet = v64.wet = 1.0;
        v32.lastLowpass = v64.lastLowpass = 1000.0;
        v32.lastHighpass = v64.lastHighpass = 1000.0;
        v32.lastWet = v64.lastWet = 1000.0;

        count = 0;

        fpNShape = 0.0;
    }

    void processChannel32(vars32 v, Input& input, Output& output)
    {
        if (output.isConnected()) {

            // poly
            for (int i = 0, numChannels = std::max(1, inputs[IN_INPUT].getChannels()); i < numChannels; ++i) {

                // params
                A = params[LOWPASS_PARAM].getValue();
                A += inputs[LOWPASS_CV_INPUT].getVoltage() / 5;
                A = clamp(A, 0.01f, 0.99f);

                B = params[HIGHPASS_PARAM].getValue();
                B += inputs[HIGHPASS_CV_INPUT].getVoltage() / 5;
                B = clamp(B, 0.01f, 0.99f);

                // C = 1.0;

                v.lowpassChase = pow(A, 2);
                v.highpassChase = pow(B, 2);
                // v.wetChase = C;
                //should not scale with sample rate, because values reaching 1 are important
                //to its ability to bypass when set to max
                float lowpassSpeed = 300 / (fabs(v.lastLowpass - v.lowpassChase) + 1.0);
                float highpassSpeed = 300 / (fabs(v.lastHighpass - v.highpassChase) + 1.0);
                // float wetSpeed = 300 / (fabs(v.lastWet - v.wetChase) + 1.0);
                v.lastLowpass = v.lowpassChase;
                v.lastHighpass = v.highpassChase;
                // v.lastWet = v.wetChase;

                float invLowpass;
                float invHighpass;
                // float dry;

                // float drySample;

                // input
                float inputSample = inputs[IN_INPUT].getPolyVoltage(i);

                // if (inputSample < 1.2e-38 && -inputSample < 1.2e-38) {
                //     static int noisesource = 0;
                //     //this declares a variable before anything else is compiled. It won't keep assigning
                //     //it to 0 for every sample, it's as if the declaration doesn't exist in this context,
                //     //but it lets me add this denormalization fix in a single place rather than updating
                //     //it in three different locations. The variable isn't thread-safe but this is only
                //     //a random seed and we can share it with whatever.
                //     noisesource = noisesource % 1700021;
                //     noisesource++;
                //     int residue = noisesource * noisesource;
                //     residue = residue % 170003;
                //     residue *= residue;
                //     residue = residue % 17011;
                //     residue *= residue;
                //     residue = residue % 1709;
                //     residue *= residue;
                //     residue = residue % 173;
                //     residue *= residue;
                //     residue = residue % 17;
                //     double applyresidue = residue;
                //     applyresidue *= 0.00000001;
                //     applyresidue *= 0.00000001;
                //     inputSample = applyresidue;
                // }

                // drySample = inputSample;

                v.lowpassAmount = (((v.lowpassAmount * lowpassSpeed) + v.lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v.lowpassAmount;
                v.highpassAmount = (((v.highpassAmount * highpassSpeed) + v.highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v.highpassAmount;
                // v.wet = (((v.wet * wetSpeed) + v.wetChase) / (wetSpeed + 1.0));
                // dry = 1.0 - v.wet;

                count++;
                if (count > 5)
                    count = 0;
                switch (count) {
                case 0:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassD = (v.iirHighpassD * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassD;
                    v.iirLowpassD = (v.iirLowpassD * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassD;
                    break;
                case 1:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassE = (v.iirHighpassE * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassE;
                    v.iirLowpassE = (v.iirLowpassE * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassE;
                    break;
                case 2:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassF = (v.iirHighpassF * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassF;
                    v.iirLowpassF = (v.iirLowpassF * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassF;
                    break;
                case 3:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassD = (v.iirHighpassD * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassD;
                    v.iirLowpassD = (v.iirLowpassD * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassD;
                    break;
                case 4:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassE = (v.iirHighpassE * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassE;
                    v.iirLowpassE = (v.iirLowpassE * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassE;
                    break;
                case 5:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassF = (v.iirHighpassF * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassF;
                    v.iirLowpassF = (v.iirLowpassF * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassF;
                    break;
                }
                //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
                //steepens the filter after minimizing artifacts.

                // inputSample = (drySample * dry) + (inputSample * v.wet);

                //stereo 32 bit dither, made small and tidy.
                // int expon;
                // frexpf((float)inputSample, &expon);
                // long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                // inputSample += (dither - fpNShape);
                // fpNShape = dither;
                //end 32 bit dither

                // output
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }

    void processChannel64(vars64 v, Input& input, Output& output)
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            // poly
            for (int i = 0, numChannels = std::max(1, inputs[IN_INPUT].getChannels()); i < numChannels; ++i) {

                // params
                A = params[LOWPASS_PARAM].getValue();
                A += inputs[LOWPASS_CV_INPUT].getVoltage() / 5;
                A = clamp(A, 0.01f, 0.99f);

                B = params[HIGHPASS_PARAM].getValue();
                B += inputs[HIGHPASS_CV_INPUT].getVoltage() / 5;
                B = clamp(B, 0.01f, 0.99f);

                C = 1.0;

                // input
                float in1 = inputs[IN_INPUT].getPolyVoltage(i);

                v.lowpassChase = pow(A, 2);
                v.highpassChase = pow(B, 2);
                // v.wetChase = C;
                //should not scale with sample rate, because values reaching 1 are important
                //to its ability to bypass when set to max
                double lowpassSpeed = 300 / (fabs(v.lastLowpass - v.lowpassChase) + 1.0);
                double highpassSpeed = 300 / (fabs(v.lastHighpass - v.highpassChase) + 1.0);
                double wetSpeed = 300 / (fabs(v.lastWet - v.wetChase) + 1.0);
                v.lastLowpass = v.lowpassChase;
                v.lastHighpass = v.highpassChase;
                v.lastWet = v.wetChase;

                double invLowpass;
                double invHighpass;
                double dry;

                long double inputSample;
                float drySampleL;

                inputSample = in1;
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
                drySampleL = inputSample;

                v.lowpassAmount = (((v.lowpassAmount * lowpassSpeed) + v.lowpassChase) / (lowpassSpeed + 1.0));
                invLowpass = 1.0 - v.lowpassAmount;
                v.highpassAmount = (((v.highpassAmount * highpassSpeed) + v.highpassChase) / (highpassSpeed + 1.0));
                invHighpass = 1.0 - v.highpassAmount;
                v.wet = (((v.wet * wetSpeed) + v.wetChase) / (wetSpeed + 1.0));
                dry = 1.0 - v.wet;

                count++;
                if (count > 5)
                    count = 0;
                switch (count) {
                case 0:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassD = (v.iirHighpassD * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassD;
                    v.iirLowpassD = (v.iirLowpassD * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassD;
                    break;
                case 1:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassE = (v.iirHighpassE * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassE;
                    v.iirLowpassE = (v.iirLowpassE * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassE;
                    break;
                case 2:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassF = (v.iirLowpassF * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassF;
                    break;
                case 3:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassD = (v.iirHighpassD * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassD;
                    v.iirLowpassD = (v.iirLowpassD * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassD;
                    break;
                case 4:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassB = (v.iirLowpassB * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassB;
                    v.iirHighpassE = (v.iirHighpassE * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassE;
                    v.iirLowpassE = (v.iirLowpassE * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassE;
                    break;
                case 5:
                    v.iirHighpassA = (v.iirHighpassA * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassA;
                    v.iirLowpassA = (v.iirLowpassA * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassA;
                    v.iirHighpassC = (v.iirHighpassC * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassC;
                    v.iirLowpassC = (v.iirLowpassC * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassC;
                    v.iirHighpassB = (v.iirHighpassB * invHighpass) + (inputSample * v.highpassAmount);
                    inputSample -= v.iirHighpassB;
                    v.iirLowpassF = (v.iirLowpassF * invLowpass) + (inputSample * v.lowpassAmount);
                    inputSample = v.iirLowpassF;
                    break;
                }
                //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
                //steepens the filter after minimizing artifacts.

                inputSample = (drySampleL * dry) + (inputSample * v.wet);

                //stereo 32 bit dither, made small and tidy.
                int expon;
                frexpf((float)inputSample, &expon);
                long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
                inputSample += (dither - fpNShape);
                fpNShape = dither;
                //end 32 bit dither

                // output
                output.setChannels(numChannels);
                output.setVoltage(inputSample, i);
            }
        }
    }

    void process(const ProcessArgs& args) override
    {
        processChannel32(v32, inputs[IN_INPUT], outputs[OUT_OUTPUT]);
        // processChannel64(v64, inputs[IN_INPUT], outputs[OUT_OUTPUT]);
    }
};

struct CapacitorWidget : ModuleWidget {
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