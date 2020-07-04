/***********************************************************************************************
ResEQ
------
VCV Rack module based on ResEQ by Chris Johnson from Airwindows <www.airwindows.com>

Ported and designed by Jens Robert Janke 

Changes/Additions:
- 4 frequency beams instead of 8
- mono
- CV inputs for frequency beams and drywet
- polyphonic

Some UI elements based on graphics from the Component Library by Wes Milholen. 

See ./LICENSE.md for all licenses
************************************************************************************************/

#include "plugin.hpp"

struct Reseq : Module {
    enum ParamIds {
        ENUMS(RESO_PARAMS, 4),
        DRYWET_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        ENUMS(RESO_CV_INPUTS, 4),
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

    // module variables
    const double gainCut = 0.03125;
    const double gainBoost = 32.0;
    int quality;
    dsp::ClockDivider partTimeJob;

    // control parameters
    float r1;
    float r2;
    float r3;
    float r4;
    float drywet;
    bool isActiveR1;
    bool isActiveR2;
    bool isActiveR3;
    bool isActiveR4;

    // global variables (as arrays in order to handle up to 16 polyphonic channels)
    double b[16][61];
    double f[16][61];
    int framenumber[16];
    uint32_t fpd[16];

    // part-time variables (which do not need to be updated every cycle)
    double overallscale;
    double v1;
    double v2;
    double v3;
    double v4;
    double f1;
    double f2;
    double f3;
    double f4;
    double wet;
    double falloff;

    Reseq()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

        for (int i = 0; i < 4; i++) {
            configParam(RESO_PARAMS + i, 0.f, 1.f, 0.f, string::f("Reso %d", i + 1), "%", 0, 100);
        }
        configParam(DRYWET_PARAM, 0.f, 1.f, 1.f, "Dry/Wet");

        quality = 1;
        quality = loadQuality();

        partTimeJob.setDivision(64);

        onSampleRateChange();

        isActiveR1 = isActiveR2 = isActiveR3 = isActiveR4 = false;
        updateParams();

        for (int i = 0; i < 16; i++) {
            for (int x = 0; x < 61; x++) {
                b[i][x] = 0.0;
                f[i][x] = 0.0;
            }
            framenumber[i] = 1;
            fpd[i] = 17;
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
        r1 = params[RESO_PARAMS + 0].getValue();
        r1 += inputs[RESO_CV_INPUTS + 0].getVoltage() / 5;
        r1 = clamp(r1, 0.01f, 0.99f);

        r2 = params[RESO_PARAMS + 1].getValue();
        r2 += inputs[RESO_CV_INPUTS + 1].getVoltage() / 5;
        r2 = clamp(r2, 0.01f, 0.99f);

        r3 = params[RESO_PARAMS + 2].getValue();
        r3 += inputs[RESO_CV_INPUTS + 2].getVoltage() / 5;
        r3 = clamp(r3, 0.01f, 0.99f);

        r4 = params[RESO_PARAMS + 3].getValue();
        r4 += inputs[RESO_CV_INPUTS + 3].getVoltage() / 5;
        r4 = clamp(r4, 0.01f, 0.99f);

        drywet = params[DRYWET_PARAM].getValue();
        drywet += inputs[DRYWET_CV_INPUT].getVoltage() / 5;
        drywet = clamp(drywet, 0.01f, 0.99f);

        wet = drywet;

        if (r1 > 0.01f) {
            v1 = r1;
            f1 = pow(v1, 2);
            v1 += 0.2;
            v1 /= overallscale;
            isActiveR1 = true;
        } else {
            isActiveR1 = false;
        }

        if (r2 > 0.01f) {
            v2 = r1;
            f2 = pow(v2, 2);
            v2 += 0.2;
            v2 /= overallscale;
            isActiveR2 = true;
        } else {
            isActiveR2 = false;
        }

        if (r3 > 0.01f) {
            v3 = r3;
            f3 = pow(v1, 2);
            v3 += 0.2;
            v3 /= overallscale;
            isActiveR3 = true;
        } else {
            isActiveR3 = false;
        }

        if (r4 > 0.01f) {
            v4 = r4;
            f4 = pow(v1, 2);
            v4 += 0.2;
            v4 /= overallscale;
            isActiveR4 = true;
        } else {
            isActiveR4 = false;
        }
    }

    void processChannel(Input& input, Output& output)
    {
        // number of polyphonic channels
        int numChannels = std::max(1, inputs[IN_INPUT].getChannels());

        // for each poly channel
        for (int i = 0; i < numChannels; ++i) {

            // input
            long double inputSample = input.getPolyVoltage(i);

            // pad gain
            inputSample *= gainCut;

            // each process frame we'll update some of the kernel frames. That way we don't have to crunch the whole thing at once,
            // and we can load a LOT more resonant peaks into the kernel.
            framenumber[i] += 1;
            if (framenumber[i] > 59)
                framenumber[i] = 1;
            falloff = sin(framenumber[i] / 19.098992);
            f[i][framenumber[i]] = 0.0;

            if (isActiveR1) {
                if ((framenumber[i] * f1) < 1.57079633)
                    f[i][framenumber[i]] += (sin((framenumber[i] * f1) * 2.0) * falloff * v1);
                else
                    f[i][framenumber[i]] += (cos(framenumber[i] * f1) * falloff * v1);
            }
            if (isActiveR2) {
                if ((framenumber[i] * f2) < 1.57079633)
                    f[i][framenumber[i]] += (sin((framenumber[i] * f2) * 2.0) * falloff * v2);
                else
                    f[i][framenumber[i]] += (cos(framenumber[i] * f2) * falloff * v2);
            }
            if (isActiveR3) {
                if ((framenumber[i] * f3) < 1.57079633)
                    f[i][framenumber[i]] += (sin((framenumber[i] * f3) * 2.0) * falloff * v3);
                else
                    f[i][framenumber[i]] += (cos(framenumber[i] * f3) * falloff * v3);
            }
            if (isActiveR4) {
                if ((framenumber[i] * f4) < 1.57079633)
                    f[i][framenumber[i]] += (sin((framenumber[i] * f4) * 2.0) * falloff * v4);
                else
                    f[i][framenumber[i]] += (cos(framenumber[i] * f4) * falloff * v4);
            }
            //done updating the kernel for this go-round

            if (quality == 1) {
                if (fabs(inputSample) < 1.18e-43)
                    inputSample = fpd[i] * 1.18e-43;
            }

            long double drySample = inputSample;

            // EQ kernel
            b[i][59] = b[i][58];
            b[i][58] = b[i][57];
            b[i][57] = b[i][56];
            b[i][56] = b[i][55];
            b[i][55] = b[i][54];
            b[i][54] = b[i][53];
            b[i][53] = b[i][52];
            b[i][52] = b[i][51];
            b[i][51] = b[i][50];
            b[i][50] = b[i][49];
            b[i][49] = b[i][48];
            b[i][48] = b[i][47];
            b[i][47] = b[i][46];
            b[i][46] = b[i][45];
            b[i][45] = b[i][44];
            b[i][44] = b[i][43];
            b[i][43] = b[i][42];
            b[i][42] = b[i][41];
            b[i][41] = b[i][40];
            b[i][40] = b[i][39];
            b[i][39] = b[i][38];
            b[i][38] = b[i][37];
            b[i][37] = b[i][36];
            b[i][36] = b[i][35];
            b[i][35] = b[i][34];
            b[i][34] = b[i][33];
            b[i][33] = b[i][32];
            b[i][32] = b[i][31];
            b[i][31] = b[i][30];
            b[i][30] = b[i][29];
            b[i][29] = b[i][28];
            b[i][28] = b[i][27];
            b[i][27] = b[i][26];
            b[i][26] = b[i][25];
            b[i][25] = b[i][24];
            b[i][24] = b[i][23];
            b[i][23] = b[i][22];
            b[i][22] = b[i][21];
            b[i][21] = b[i][20];
            b[i][20] = b[i][19];
            b[i][19] = b[i][18];
            b[i][18] = b[i][17];
            b[i][17] = b[i][16];
            b[i][16] = b[i][15];
            b[i][15] = b[i][14];
            b[i][14] = b[i][13];
            b[i][13] = b[i][12];
            b[i][12] = b[i][11];
            b[i][11] = b[i][10];
            b[i][10] = b[i][9];
            b[i][9] = b[i][8];
            b[i][8] = b[i][7];
            b[i][7] = b[i][6];
            b[i][6] = b[i][5];
            b[i][5] = b[i][4];
            b[i][4] = b[i][3];
            b[i][3] = b[i][2];
            b[i][2] = b[i][1];
            b[i][1] = b[i][0];
            b[i][0] = inputSample;

            inputSample = (b[i][1] * f[i][1]);
            inputSample += (b[i][2] * f[i][2]);
            inputSample += (b[i][3] * f[i][3]);
            inputSample += (b[i][4] * f[i][4]);
            inputSample += (b[i][5] * f[i][5]);
            inputSample += (b[i][6] * f[i][6]);
            inputSample += (b[i][7] * f[i][7]);
            inputSample += (b[i][8] * f[i][8]);
            inputSample += (b[i][9] * f[i][9]);
            inputSample += (b[i][10] * f[i][10]);
            inputSample += (b[i][11] * f[i][11]);
            inputSample += (b[i][12] * f[i][12]);
            inputSample += (b[i][13] * f[i][13]);
            inputSample += (b[i][14] * f[i][14]);
            inputSample += (b[i][15] * f[i][15]);
            inputSample += (b[i][16] * f[i][16]);
            inputSample += (b[i][17] * f[i][17]);
            inputSample += (b[i][18] * f[i][18]);
            inputSample += (b[i][19] * f[i][19]);
            inputSample += (b[i][20] * f[i][20]);
            inputSample += (b[i][21] * f[i][21]);
            inputSample += (b[i][22] * f[i][22]);
            inputSample += (b[i][23] * f[i][23]);
            inputSample += (b[i][24] * f[i][24]);
            inputSample += (b[i][25] * f[i][25]);
            inputSample += (b[i][26] * f[i][26]);
            inputSample += (b[i][27] * f[i][27]);
            inputSample += (b[i][28] * f[i][28]);
            inputSample += (b[i][29] * f[i][29]);
            inputSample += (b[i][30] * f[i][30]);
            inputSample += (b[i][31] * f[i][31]);
            inputSample += (b[i][32] * f[i][32]);
            inputSample += (b[i][33] * f[i][33]);
            inputSample += (b[i][34] * f[i][34]);
            inputSample += (b[i][35] * f[i][35]);
            inputSample += (b[i][36] * f[i][36]);
            inputSample += (b[i][37] * f[i][37]);
            inputSample += (b[i][38] * f[i][38]);
            inputSample += (b[i][39] * f[i][39]);
            inputSample += (b[i][40] * f[i][40]);
            inputSample += (b[i][41] * f[i][41]);
            inputSample += (b[i][42] * f[i][42]);
            inputSample += (b[i][43] * f[i][43]);
            inputSample += (b[i][44] * f[i][44]);
            inputSample += (b[i][45] * f[i][45]);
            inputSample += (b[i][46] * f[i][46]);
            inputSample += (b[i][47] * f[i][47]);
            inputSample += (b[i][48] * f[i][48]);
            inputSample += (b[i][49] * f[i][49]);
            inputSample += (b[i][50] * f[i][50]);
            inputSample += (b[i][51] * f[i][51]);
            inputSample += (b[i][52] * f[i][52]);
            inputSample += (b[i][53] * f[i][53]);
            inputSample += (b[i][54] * f[i][54]);
            inputSample += (b[i][55] * f[i][55]);
            inputSample += (b[i][56] * f[i][56]);
            inputSample += (b[i][57] * f[i][57]);
            inputSample += (b[i][58] * f[i][58]);
            inputSample += (b[i][59] * f[i][59]);
            inputSample /= 12.0;
            //inlined- this is our little EQ kernel. Longer will give better tightness on bass frequencies.
            //Note that normal programmers will make this a loop, which isn't much slower if at all, on modern CPUs.
            //It was unrolled more or less to show how much is done when you define a loop like that: it's easy to specify stuff where a lot of grinding is done.
            //end EQ kernel

            if (wet != 1.0) {
                inputSample = (inputSample * wet) + (drySample * (1.0 - wet));
            }

            if (quality == 1) {
                //begin 64 bit stereo floating point dither
                int expon;
                frexp((double)inputSample, &expon);
                fpd[i] ^= fpd[i] << 13;
                fpd[i] ^= fpd[i] >> 17;
                fpd[i] ^= fpd[i] << 5;
                inputSample += ((double(fpd[i]) - uint32_t(0x7fffffff)) * 1.1e-44l * pow(2, expon + 62));
                //end 64 bit stereo floating point dither
            }

            // bring gain back up
            inputSample *= gainBoost;

            // output
            outputs[OUT_OUTPUT].setChannels(numChannels);
            outputs[OUT_OUTPUT].setVoltage(inputSample, i);

        } // end poly channel loop
    }

    void process(const ProcessArgs& args) override
    {
        if (outputs[OUT_OUTPUT].isConnected()) {

            if (partTimeJob.process()) {
                // stuff that doesn't need to be processed every cycle
                updateParams();
            }

            processChannel(inputs[IN_INPUT], outputs[OUT_OUTPUT]);
        }
    }
};

struct ReseqWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        Reseq* module;
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
        Reseq* module = dynamic_cast<Reseq*>(this->module);
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

    ReseqWidget(Reseq* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/reseq_dark.svg")));

        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        addParam(createParamCentered<RwKnobSmallDark>(Vec(22.5, 55.0), module, Reseq::RESO_PARAMS + 0));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(67.5, 55.0), module, Reseq::RESO_PARAMS + 1));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(22.5, 105.0), module, Reseq::RESO_PARAMS + 2));
        addParam(createParamCentered<RwKnobSmallDark>(Vec(67.5, 105.0), module, Reseq::RESO_PARAMS + 3));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(45.0, 155.0), module, Reseq::DRYWET_PARAM));

        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 245.0), module, Reseq::RESO_CV_INPUTS + 0));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 245.0), module, Reseq::RESO_CV_INPUTS + 1));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 285.0), module, Reseq::RESO_CV_INPUTS + 2));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(63.75, 285.0), module, Reseq::RESO_CV_INPUTS + 3));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(45.0, 205.0), module, Reseq::DRYWET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(26.25, 325.0), module, Reseq::IN_INPUT));

        addOutput(createOutputCentered<RwPJ301MPort>(Vec(63.75, 325.0), module, Reseq::OUT_OUTPUT));
    }
};

Model* modelReseq = createModel<Reseq, ReseqWidget>("reseq");