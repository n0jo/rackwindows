/***********************************************************************************************
Capacitor Stereo
----------------
VCV Rack module based on Capacitor by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- CV inputs for lowpass, highpass and dry/wet

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Capacitor_stereo : Module {
    enum ParamIds {
        LOWPASS_PARAM,
        HIGHPASS_PARAM,
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        DRYWET_CV_INPUT,
        LOWPASS_CV_INPUT,
        HIGHPASS_CV_INPUT,
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

    double iirHighpassAL;
    double iirHighpassBL;
    double iirHighpassCL;
    double iirHighpassDL;
    double iirHighpassEL;
    double iirHighpassFL;
    double iirLowpassAL;
    double iirLowpassBL;
    double iirLowpassCL;
    double iirLowpassDL;
    double iirLowpassEL;
    double iirLowpassFL;

    double iirHighpassAR;
    double iirHighpassBR;
    double iirHighpassCR;
    double iirHighpassDR;
    double iirHighpassER;
    double iirHighpassFR;
    double iirLowpassAR;
    double iirLowpassBR;
    double iirLowpassCR;
    double iirLowpassDR;
    double iirLowpassER;
    double iirLowpassFR;

    int count;

    double lowpassChase;
    double highpassChase;
    double wetChase;

    double lowpassAmount;
    double highpassAmount;
    double wet;

    double lastLowpass;
    double lastHighpass;
    double lastWet;

    long double fpNShapeL;
    long double fpNShapeR;
    //default stuff

    float A;
    float B;
    float C;

    Capacitor_stereo()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(LOWPASS_PARAM, 0.f, 1.f, 1.f, "Lowpass");
        configParam(HIGHPASS_PARAM, 0.f, 1.f, 0.f, "Highpass");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        iirHighpassAL = 0.0;
        iirHighpassBL = 0.0;
        iirHighpassCL = 0.0;
        iirHighpassDL = 0.0;
        iirHighpassEL = 0.0;
        iirHighpassFL = 0.0;
        iirLowpassAL = 0.0;
        iirLowpassBL = 0.0;
        iirLowpassCL = 0.0;
        iirLowpassDL = 0.0;
        iirLowpassEL = 0.0;
        iirLowpassFL = 0.0;

        iirHighpassAR = 0.0;
        iirHighpassBR = 0.0;
        iirHighpassCR = 0.0;
        iirHighpassDR = 0.0;
        iirHighpassER = 0.0;
        iirHighpassFR = 0.0;
        iirLowpassAR = 0.0;
        iirLowpassBR = 0.0;
        iirLowpassCR = 0.0;
        iirLowpassDR = 0.0;
        iirLowpassER = 0.0;
        iirLowpassFR = 0.0;

        count = 0;
        lowpassChase = 0.0;
        highpassChase = 0.0;
        wetChase = 0.0;
        lowpassAmount = 1.0;
        highpassAmount = 0.0;
        wet = 1.0;
        lastLowpass = 1000.0;
        lastHighpass = 1000.0;
        lastWet = 1000.0;

        fpNShapeL = 0.0;
        fpNShapeR = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {

            // params
            A = params[LOWPASS_PARAM].getValue();
            A += inputs[LOWPASS_CV_INPUT].getVoltage() / 5;
            A = clamp(A, 0.01f, 0.99f);

            B = params[HIGHPASS_PARAM].getValue();
            B += inputs[HIGHPASS_CV_INPUT].getVoltage() / 5;
            B = clamp(B, 0.01f, 0.99f);

            C = params[DRYWET_PARAM].getValue();
            C += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
            C = clamp(C, 0.01f, 0.99f);

            // input
            float in1 = inputs[IN_L_INPUT].getVoltage();
            float in2 = inputs[IN_R_INPUT].getVoltage();

            lowpassChase = pow(A, 2);
            highpassChase = pow(B, 2);
            wetChase = C;
            //should not scale with sample rate, because values reaching 1 are important
            //to its ability to bypass when set to max
            double lowpassSpeed = 300 / (fabs(lastLowpass - lowpassChase) + 1.0);
            double highpassSpeed = 300 / (fabs(lastHighpass - highpassChase) + 1.0);
            double wetSpeed = 300 / (fabs(lastWet - wetChase) + 1.0);
            lastLowpass = lowpassChase;
            lastHighpass = highpassChase;
            lastWet = wetChase;

            double invLowpass;
            double invHighpass;
            double dry;

            long double inputSampleL;
            long double inputSampleR;
            float drySampleL;
            float drySampleR;

            inputSampleL = in1;
            inputSampleR = in2;
            if (inputSampleL < 1.2e-38 && -inputSampleL < 1.2e-38) {
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
                inputSampleL = applyresidue;
            }
            if (inputSampleR < 1.2e-38 && -inputSampleR < 1.2e-38) {
                static int noisesource = 0;
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
                inputSampleR = applyresidue;
                //this denormalization routine produces a white noise at -300 dB which the noise
                //shaping will interact with to produce a bipolar output, but the noise is actually
                //all positive. That should stop any variables from going denormal, and the routine
                //only kicks in if digital black is input. As a final touch, if you save to 24-bit
                //the silence will return to being digital black again.
            }
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;

            lowpassAmount = (((lowpassAmount * lowpassSpeed) + lowpassChase) / (lowpassSpeed + 1.0));
            invLowpass = 1.0 - lowpassAmount;
            highpassAmount = (((highpassAmount * highpassSpeed) + highpassChase) / (highpassSpeed + 1.0));
            invHighpass = 1.0 - highpassAmount;
            wet = (((wet * wetSpeed) + wetChase) / (wetSpeed + 1.0));
            dry = 1.0 - wet;

            count++;
            if (count > 5)
                count = 0;
            switch (count) {
            case 0:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassBL;
                iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassBL;
                iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassDL;
                iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassDL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassBR;
                iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassBR;
                iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassDR;
                iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassDR;
                break;
            case 1:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassCL;
                iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassCL;
                iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassEL;
                iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassEL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassCR;
                iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassCR;
                iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassER;
                iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassER;
                break;
            case 2:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassBL;
                iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassBL;
                iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassFL;
                iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassFL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassBR;
                iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassBR;
                iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassFR;
                iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassFR;
                break;
            case 3:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassCL;
                iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassCL;
                iirHighpassDL = (iirHighpassDL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassDL;
                iirLowpassDL = (iirLowpassDL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassDL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassCR;
                iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassCR;
                iirHighpassDR = (iirHighpassDR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassDR;
                iirLowpassDR = (iirLowpassDR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassDR;
                break;
            case 4:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassBL = (iirHighpassBL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassBL;
                iirLowpassBL = (iirLowpassBL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassBL;
                iirHighpassEL = (iirHighpassEL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassEL;
                iirLowpassEL = (iirLowpassEL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassEL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassBR = (iirHighpassBR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassBR;
                iirLowpassBR = (iirLowpassBR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassBR;
                iirHighpassER = (iirHighpassER * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassER;
                iirLowpassER = (iirLowpassER * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassER;
                break;
            case 5:
                iirHighpassAL = (iirHighpassAL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassAL;
                iirLowpassAL = (iirLowpassAL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassAL;
                iirHighpassCL = (iirHighpassCL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassCL;
                iirLowpassCL = (iirLowpassCL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassCL;
                iirHighpassFL = (iirHighpassFL * invHighpass) + (inputSampleL * highpassAmount);
                inputSampleL -= iirHighpassFL;
                iirLowpassFL = (iirLowpassFL * invLowpass) + (inputSampleL * lowpassAmount);
                inputSampleL = iirLowpassFL;
                iirHighpassAR = (iirHighpassAR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassAR;
                iirLowpassAR = (iirLowpassAR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassAR;
                iirHighpassCR = (iirHighpassCR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassCR;
                iirLowpassCR = (iirLowpassCR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassCR;
                iirHighpassFR = (iirHighpassFR * invHighpass) + (inputSampleR * highpassAmount);
                inputSampleR -= iirHighpassFR;
                iirLowpassFR = (iirLowpassFR * invLowpass) + (inputSampleR * lowpassAmount);
                inputSampleR = iirLowpassFR;
                break;
            }
            //Highpass Filter chunk. This is three poles of IIR highpass, with a 'gearbox' that progressively
            //steepens the filter after minimizing artifacts.

            inputSampleL = (drySampleL * dry) + (inputSampleL * wet);
            inputSampleR = (drySampleR * dry) + (inputSampleR * wet);

            //stereo 32 bit dither, made small and tidy.
            int expon;
            frexpf((float)inputSampleL, &expon);
            long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
            inputSampleL += (dither - fpNShapeL);
            fpNShapeL = dither;
            frexpf((float)inputSampleR, &expon);
            dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
            inputSampleR += (dither - fpNShapeR);
            fpNShapeR = dither;
            //end 32 bit dither

            // output
            outputs[OUT_L_OUTPUT].setVoltage(inputSampleL);
            outputs[OUT_R_OUTPUT].setVoltage(inputSampleR);
        }
    }
};

struct Capacitor_stereoWidget : ModuleWidget {
    Capacitor_stereoWidget(Capacitor_stereo* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/capacitor_stereo_dark.svg")));

        // screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 65.0), module, Capacitor_stereo::LOWPASS_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 125.0), module, Capacitor_stereo::HIGHPASS_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(63.75, 185.0), module, Capacitor_stereo::DRYWET_PARAM));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 205.0), module, Capacitor_stereo::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245.0), module, Capacitor_stereo::LOWPASS_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245.0), module, Capacitor_stereo::HIGHPASS_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285.0), module, Capacitor_stereo::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285.0), module, Capacitor_stereo::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(26.25, 325.0), module, Capacitor_stereo::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Capacitor_stereo::OUT_R_OUTPUT));
    }
};

Model* modelCapacitor_stereo = createModel<Capacitor_stereo, Capacitor_stereoWidget>("capacitor_stereo");