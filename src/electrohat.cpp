/***********************************************************************************************
ElectroHat
----------
VCV Rack module based on ElectroHat by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- mono

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Electrohat : Module {
    enum ParamIds {
        TRIM_PARAM,
        BRIGHTNESS_PARAM,
        TYPE_PARAM,
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        TRIM_CV_INPUT,
        BRIGHNESS_CV_INPUT,
        TYPE_CV_INPUT,
        DRYWET_CV_INPUT,
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

    long double fpNShapeL;
    long double fpNShapeR;
    //default stuff

    double storedSampleL;
    double storedSampleR;
    double lastSampleL;
    double lastSampleR;
    int tik;
    int lok;
    bool flip;

    float A;
    float B;
    float C;
    float D;
    float E; //parameters. Always 0-1, and we scale/alter them elsewhere.

    Electrohat()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(TRIM_PARAM, 0.f, 1.f, 0.5f, "Trim");
        configParam(BRIGHTNESS_PARAM, 0.f, 1.f, 1.f, "Brightness");
        configParam(TYPE_PARAM, 0.f, 6.f, 0.f, "Type");
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        storedSampleL = 0.0;
        storedSampleR = 0.0;
        lastSampleL = 0.0;
        lastSampleR = 0.0;
        tik = 3746926;
        lok = 0;
        flip = true;

        fpNShapeL = 0.0;
        fpNShapeR = 0.0;
        //this is reset: values being initialized only once. Startup values, whatever they are.
    }

    void process(const ProcessArgs& args) override
    {
        // params
        A = params[TYPE_PARAM].getValue();
        // A += inputs[TYPE_CV_INPUT].getVoltage() / 5;
        // A = clamp(A, 0.01f, 0.99f);

        B = params[TRIM_PARAM].getValue();
        B += inputs[TRIM_CV_INPUT].getVoltage() / 5;
        B = clamp(B, 0.01f, 0.99f);

        C = params[BRIGHTNESS_PARAM].getValue();
        C += inputs[BRIGHNESS_CV_INPUT].getVoltage() / 5;
        C = clamp(C, 0.01f, 0.99f);

        D = params[DRYWET_PARAM].getValue();
        D += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
        D = clamp(D, 0.01f, 0.99f);

        // input
        float in1 = inputs[IN_INPUT].getVoltage();

        double overallscale = 1.0;
        overallscale /= 44100.0;
        overallscale *= args.sampleRate;
        bool highSample = false;
        if (args.sampleRate > 64000)
            highSample = true;
        //we will go to another dither for 88 and 96K

        double drySampleL;
        double drySampleR;
        double tempSampleL;
        double tempSampleR;
        long double inputSampleL;
        long double inputSampleR;

        int deSyn = (A * 5.999) + 1;
        double increment = B;
        double brighten = C;
        double outputlevel = D;
        double wet = E;
        double dry = 1.0 - wet;

        if (deSyn == 4) {
            deSyn = 1;
            increment = 0.411;
            brighten = 0.87;
        }
        //606 preset
        if (deSyn == 5) {
            deSyn = 2;
            increment = 0.111;
            brighten = 1.0;
        }
        //808 preset
        if (deSyn == 6) {
            deSyn = 2;
            increment = 0.299;
            brighten = 0.359;
        }
        //909 preset
        int tok = deSyn + 1;
        increment *= 0.98;
        increment += 0.01;
        increment += (double)tok;
        double fosA = increment;
        double fosB = fosA * increment;
        double fosC = fosB * increment;
        double fosD = fosC * increment;
        double fosE = fosD * increment;
        double fosF = fosE * increment;
        int posA = fosA;
        int posB = fosB;
        int posC = fosC;
        int posD = fosD;
        int posE = fosE;
        int posF = fosF;
        int posG = posF * posE * posD * posC * posB; //factorial

        inputSampleL = in1;
        // inputSampleR = in2;
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        inputSampleL = fabs(inputSampleL) * outputlevel;
        inputSampleR = fabs(inputSampleR) * outputlevel;

        if (flip) { //will always be true unless we have high sample rate
            tik++;
            tik = tik % posG;
            tok = tik * tik;
            tok = tok % posF;
            tok *= tok;
            tok = tok % posE;
            tok *= tok;
            tok = tok % posD;
            tok *= tok;
            tok = tok % posC;
            tok *= tok;
            tok = tok % posB;
            tok *= tok;
            tok = tok % posA;

            inputSampleL = tok * inputSampleL;
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 1)) {
                inputSampleL = -tok * inputSampleL;
            }
            if ((abs(lok - tok) > abs(lok + tok)) && (deSyn == 2)) {
                inputSampleL = -tok * inputSampleL;
            }
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 3)) {
                inputSampleL = -tok * inputSampleL;
            }

            inputSampleR = tok * inputSampleR;
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 1)) {
                inputSampleR = -tok * inputSampleR;
            }
            if ((abs(lok - tok) > abs(lok + tok)) && (deSyn == 2)) {
                inputSampleR = -tok * inputSampleR;
            }
            if ((abs(lok - tok) < abs(lok + tok)) && (deSyn == 3)) {
                inputSampleR = -tok * inputSampleR;
            }

            lok = tok;

            tempSampleL = inputSampleL;
            inputSampleL = inputSampleL - (lastSampleL * brighten);
            lastSampleL = tempSampleL;

            tempSampleR = inputSampleR;
            inputSampleR = inputSampleR - (lastSampleR * brighten);
            lastSampleR = tempSampleR;
        } else { //we have high sample rate and this is an interpolate sample
            inputSampleL = lastSampleL;
            inputSampleR = lastSampleR;
            //not really interpolating, just sample-and-hold
        }

        if (highSample) {
            flip = !flip;
        } else {
            flip = true;
        }

        tempSampleL = inputSampleL;
        inputSampleL += storedSampleL;
        storedSampleL = tempSampleL;

        tempSampleR = inputSampleR;
        inputSampleR += storedSampleR;
        storedSampleR = tempSampleR;

        if (wet != 1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
            inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
        }

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
        outputs[OUT_OUTPUT].setVoltage(inputSampleL);
    }
};

struct ElectrohatWidget : ModuleWidget {
    ElectrohatWidget(Electrohat* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/electrohat_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 65.0), module, Electrohat::TRIM_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 125.0), module, Electrohat::BRIGHTNESS_PARAM));
        addParam(createParamCentered<RwSwitchKnobSmallDark>(Vec(22.5, 185.0), module, Electrohat::TYPE_PARAM));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(67.5, 185.0), module, Electrohat::DRYWET_PARAM));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245.0), module, Electrohat::TRIM_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245.0), module, Electrohat::BRIGHNESS_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285.0), module, Electrohat::TYPE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285.0), module, Electrohat::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 325.0), module, Electrohat::IN_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Electrohat::OUT_OUTPUT));
    }
};

Model* modelElectrohat = createModel<Electrohat, ElectrohatWidget>("electrohat");