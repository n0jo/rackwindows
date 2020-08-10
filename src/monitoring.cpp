/***********************************************************************************************
Monitoring
----------
VCV Rack module based on Monitoring by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke

Changes/Additions:
- separate controls for processing modes and cans
- no monolat, monorat, phone

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
    enum ditherModes {
        DITHER_OFF,
        DITHER_24,
        DITHER_16
    };

    // control parameters
    int processingMode;
    int lastProcessingMode;
    int cansMode;
    int ditherMode;

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
        configParam(MODE_PARAM, 0.f, 7.f, 0.f, "Mode");
        configParam(CANS_PARAM, 0.f, 4.f, 0.f, "Cans");
        configParam(DITHER_PARAM, 0.f, 2.f, 0.f, "Dither");

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

        processingMode = 0;
        cansMode = 0;
        lastProcessingMode = 0;
        ditherMode = 0;

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
        ditherMode = params[DITHER_PARAM].getValue();
        lights[DITHER_24_LIGHT].setBrightness(ditherMode == DITHER_24 ? 1.f : 0.f);
        lights[DITHER_16_LIGHT].setBrightness(ditherMode == DITHER_16 ? 1.f : 0.f);

        if (outputs[OUT_L_OUTPUT].isConnected() || outputs[OUT_R_OUTPUT].isConnected()) {

            // get params
            processingMode = params[MODE_PARAM].getValue();
            cansMode = params[CANS_PARAM].getValue();

            if (processingMode != lastProcessingMode) {
                // set up bandpass for vinyl, aurat and phone
                if (processingMode == VINYL) {
                    bandpassL.set(0.0385 / overallscale, 0.0825);
                    bandpassR.set(0.0385 / overallscale, 0.0825);
                }
                if (processingMode == AURAT) {
                    bandpassL.set(0.0375 / overallscale, 0.1575);
                    bandpassR.set(0.0375 / overallscale, 0.1575);
                }
                if (processingMode == PHONE) {
                    bandpassL.set(0.1245 / overallscale, 0.46);
                    bandpassR.set(0.1245 / overallscale, 0.46);
                }
                lastProcessingMode = processingMode;
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
            switch (processingMode) {
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
            switch (ditherMode) {
            case DITHER_OFF:
                break;
            case DITHER_16:
                inputSampleL = darkL.process(inputSampleL, args.sampleRate, false);
                inputSampleR = darkR.process(inputSampleR, args.sampleRate, false);
                break;
            case DITHER_24:
                inputSampleL = darkL.process(inputSampleL, args.sampleRate, true);
                inputSampleR = darkR.process(inputSampleR, args.sampleRate, true);
                break;
            }

            if (quality == HIGH) {
                // 32 bit stereo floating point dither
                int expon;
                frexpf((float)inputSampleL, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                inputSampleL += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
                frexpf((float)inputSampleR, &expon);
                fpd ^= fpd << 13;
                fpd ^= fpd >> 17;
                fpd ^= fpd << 5;
                inputSampleR += ((double(fpd) - uint32_t(0x7fffffff)) * 5.5e-36l * pow(2, expon + 62));
            }

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

    // quality item
    struct QualityItem : MenuItem {
        Monitoring* module;
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
        Monitoring* module = dynamic_cast<Monitoring*>(this->module);
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
        addParam(createParamCentered<RwSwitchThree>(Vec(52.5, 235.0), module, Monitoring::DITHER_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(18.8, 235.0), module, Monitoring::DITHER_24_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(86.3, 235.0), module, Monitoring::DITHER_16_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(33.75, 285.0), module, Monitoring::IN_L_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(71.25, 285.0), module, Monitoring::IN_R_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(33.75, 325.0), module, Monitoring::OUT_L_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(71.25, 325.0), module, Monitoring::OUT_R_OUTPUT));
    }
};

Model* modelMonitoring = createModel<Monitoring, MonitoringWidget>("monitoring");