/***********************************************************************************************
Monitoring
----------
VCV Rack module based on Monitoring by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- separate controls for processing modes and cans
- no monolat, monorat

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"
#include "rwlib.h"

using namespace rwlib;

struct Monitoring : Module {
    enum ParamIds {
        MODE_PARAM,
        CANS_PARAM,
        DITHER_PARAM,
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
        DITHER_16_LIGHT,
        DITHER_24_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainFactor = 32.0;
    int quality;
    enum qualityOptions {
        ECO,
        HIGH
    };
    enum processingModes {
        OFF,
        SUBS,
        SLEW,
        PEAKS,
        MID,
        SIDE,
        VINYL,
        AURAT,
        PHONE
    };
    enum cansModes {
        CANS_OFF,
        CANS_A,
        CANS_B,
        CANS_C,
        CANS_D
    };
    bool isDither16;

    // control parameters
    int mode;
    int cansMode;
    int lastMode;
    int lastCansMode;

    // state variables
    SubsOnly subsL, subsR;
    SlewOnly slewL, slewR;
    PeaksOnly peaksL, peaksR;
    BiquadBandpass bandpassL, bandpassR;
    Cans cans;
    Dark darkL, darkR;
    uint32_t fpd;

    //other
    double overallscale;

    Monitoring()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(MODE_PARAM, 0.f, 8.f, 0.f, "Mode");
        configParam(CANS_PARAM, 0.f, 4.f, 0.f, "Cans");
        configParam(DITHER_PARAM, 0.f, 1.f, 0.f, "Dither");

        quality = loadQuality();
        onReset();
    }

    void onReset() override
    {
        onSampleRateChange();

        subsL = SubsOnly();
        subsR = SubsOnly();
        slewL = SlewOnly();
        slewR = SlewOnly();
        peaksL = PeaksOnly();
        peaksR = PeaksOnly();
        bandpassL = BiquadBandpass();
        bandpassR = BiquadBandpass();
        cans = Cans();
        darkL = Dark();
        darkR = Dark();

        mode = 0;
        cansMode = 0;
        lastMode = 0;
        lastCansMode = 0;

        isDither16 = false;
        fpd = 17;
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
        // dither light
        isDither16 = params[DITHER_PARAM].getValue() ? true : false;
        lights[DITHER_24_LIGHT].setBrightness(!isDither16);
        lights[DITHER_16_LIGHT].setBrightness(isDither16);

        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {

            // get params
            mode = params[MODE_PARAM].getValue();
            cansMode = params[CANS_PARAM].getValue();

            if (mode != lastMode) {
                // set up bandpass for vinyl, aurat and phone
                if (mode == VINYL) {
                    bandpassL.set(0.0385 / overallscale, 0.0825);
                    bandpassR.set(0.0385 / overallscale, 0.0825);
                }
                if (mode == AURAT) {
                    bandpassL.set(0.0375 / overallscale, 0.1575);
                    bandpassR.set(0.0375 / overallscale, 0.1575);
                }
                if (mode == PHONE) {
                    bandpassL.set(0.1245 / overallscale, 0.46);
                    bandpassR.set(0.1245 / overallscale, 0.46);
                }
                lastMode = mode;
            }

            // get input
            long double inputSampleL = inputs[IN_L_INPUT].getVoltage();
            long double inputSampleR = inputs[IN_R_INPUT].getVoltage();

            // pad gain
            inputSampleL /= gainFactor;
            inputSampleR /= gainFactor;

            // prepare mid and side
            long double mid = inputSampleL + inputSampleR;
            long double side = inputSampleL - inputSampleR;

            if (quality == HIGH) {
                if (fabs(inputSampleL) < 1.18e-37)
                    inputSampleL = fpd * 1.18e-37;
                if (fabs(inputSampleR) < 1.18e-37)
                    inputSampleR = fpd * 1.18e-37;
            }

            // processing modes
            switch (mode) {

            case OFF:
                break;

            case SUBS:
                inputSampleL = subsL.process(inputSampleL, args.sampleRate);
                inputSampleR = subsR.process(inputSampleR, args.sampleRate);
                break;

            case SLEW:
                inputSampleL = slewL.process(inputSampleL);
                inputSampleR = slewR.process(inputSampleR);
                break;

            case PEAKS:
                inputSampleL = peaksL.process(inputSampleL, args.sampleRate);
                inputSampleR = peaksR.process(inputSampleR, args.sampleRate);
                break;

            case MID:
                inputSampleL = mid * 0.5;
                inputSampleR = mid * 0.5;
                break;

            case SIDE:
                inputSampleL = side * 0.5;
                inputSampleR = -side * 0.5;
                break;

            case VINYL:
                inputSampleL = bandpassL.process(inputSampleL);
                inputSampleR = bandpassR.process(inputSampleR);
                break;

            case AURAT:
                inputSampleL = bandpassL.process(inputSampleL);
                inputSampleR = bandpassR.process(inputSampleR);
                break;

            case PHONE:
                inputSampleL = bandpassL.process(mid * 0.5);
                inputSampleR = bandpassR.process(mid * 0.5);
                break;
            }

            // cans
            if (cansMode) {
                cans.setMode(cansMode);
                cans.process(args.sampleRate, inputSampleL, inputSampleR);
            }

            // dither
            inputSampleL = darkL.process(inputSampleL, args.sampleRate, !isDither16);
            inputSampleR = darkR.process(inputSampleR, args.sampleRate, !isDither16);

            // bring gain back up
            inputSampleL *= gainFactor;
            inputSampleR *= gainFactor;

            // output
            outputs[OUT_L_OUTPUT].setVoltage(inputSampleL);
            outputs[OUT_R_OUTPUT].setVoltage(inputSampleR);
        }
    }
};

struct MonitoringWidget : ModuleWidget {
    MonitoringWidget(Monitoring* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/monitoring_dark.svg")));

        //screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<RwSwitchKnobMediumDarkTwoThirds>(Vec(52.5, 85.0), module, Monitoring::MODE_PARAM));
        addParam(createParamCentered<RwSwitchKnobMediumDarkOneThird>(Vec(52.5, 165.0), module, Monitoring::CANS_PARAM));

        // switch
        addParam(createParamCentered<RwCKSSRot>(Vec(52.5, 230.0), module, Monitoring::DITHER_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(22.5, 230.0), module, Monitoring::DITHER_24_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(82.5, 230.0), module, Monitoring::DITHER_16_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(33.75, 285.0), module, Monitoring::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(71.25, 285.0), module, Monitoring::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(33.75, 325.0), module, Monitoring::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(71.25, 325.0), module, Monitoring::OUT_R_OUTPUT));
    }
};

Model* modelMonitoring = createModel<Monitoring, MonitoringWidget>("monitoring");