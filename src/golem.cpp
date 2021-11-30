#include "plugin.hpp"

// quality options
#define ECO 0
#define HIGH 1

// delay modes
#define DI 0
#define MIC 1

// scaling options
#define LINEAR 0
#define EXPONENTIAL 1

// range options
#define BIPOLAR 0
#define UNIPOLAR 1

struct Golem : Module {
    enum ParamIds {
        BALANCE_PARAM,
        BALANCE_TRIM_PARAM,
        OFFSET_PARAM,
        OFFSET_TRIM_PARAM,
        PHASE_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        BALANCE_CV_INPUT,
        OFFSET_CV_INPUT,
        IN_A_INPUT,
        IN_B_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        OUT_POS_OUTPUT,
        OUT_NEG_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        PHASE_A_LIGHT,
        PHASE_B_LIGHT,
        NUM_LIGHTS
    };

    // module variables
    const double gainCut = 0.1;
    const double gainBoost = 10.0;
    int quality;
    int delayMode;
    int balanceTrimRange;
    int offsetTrimRange;
    int offsetScaling;

    // control parameters
    float balanceParam;
    float offsetParam;
    float phaseParam;

    // state variables
    rwlib::GolemBCN golem;
    long double fpNShape;

    Golem()
    {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(BALANCE_PARAM, -1.f, 1.f, 0.f, "Balance");
        configParam(OFFSET_PARAM, -1.f, 1.f, 0.f, "Offset");
        configParam(BALANCE_TRIM_PARAM, -1.f, 1.f, 0.f, "Balance CV");
        configParam(OFFSET_TRIM_PARAM, -1.f, 1.f, 0.f, "Offset CV");
        configSwitch(PHASE_PARAM, 0.f, 2.f, 0.f, "Phase", {"Off", "Flip polarity channel A", "Flip polarity channel B"});

        configInput(BALANCE_CV_INPUT, "Balance CV");
        configInput(OFFSET_CV_INPUT, "Offset CV");
        configInput(IN_A_INPUT, "Channel A");
        configInput(IN_B_INPUT, "Channel B");
        configOutput(OUT_POS_OUTPUT, "Positive Signal");
        configOutput(OUT_NEG_OUTPUT, "Negative Signal");

        configBypass(IN_A_INPUT, OUT_POS_OUTPUT);

        quality = ECO;
        delayMode = DI;
        balanceTrimRange = BIPOLAR;
        offsetTrimRange = BIPOLAR;
        offsetScaling = LINEAR;
        onReset();
    }

    void onReset() override
    {
        balanceParam = 0.f;
        offsetParam = 0.f;
        phaseParam = 0.f;

        golem = rwlib::GolemBCN();
        fpNShape = 0.0;
    }

    json_t* dataToJson() override
    {
        json_t* rootJ = json_object();

        // quality
        json_object_set_new(rootJ, "quality", json_integer(quality));

        // delay mode
        json_object_set_new(rootJ, "delayMode", json_integer(delayMode));

        // balance trim range
        json_object_set_new(rootJ, "balanceTrimRange", json_integer(balanceTrimRange));

        // offset trim range
        json_object_set_new(rootJ, "offsetTrimRange", json_integer(offsetTrimRange));

        // offset sclaing
        json_object_set_new(rootJ, "offsetScaling", json_integer(offsetScaling));

        return rootJ;
    }

    void dataFromJson(json_t* rootJ) override
    {
        // quality
        json_t* qualityJ = json_object_get(rootJ, "quality");
        if (qualityJ)
            quality = json_integer_value(qualityJ);

        // delay mode
        json_t* delayModeJ = json_object_get(rootJ, "delayMode");
        if (delayModeJ)
            delayMode = json_integer_value(delayModeJ);

        // balance trim range
        json_t* balanceTrimRangeJ = json_object_get(rootJ, "balanceTrimRange");
        if (balanceTrimRangeJ)
            balanceTrimRange = json_integer_value(balanceTrimRangeJ);

        // offset trim range
        json_t* offsetTrimRangeJ = json_object_get(rootJ, "offsetTrimRange");
        if (offsetTrimRangeJ)
            offsetTrimRange = json_integer_value(offsetTrimRangeJ);

        // offset scaling
        json_t* offsetScalingJ = json_object_get(rootJ, "offsetScaling");
        if (offsetScalingJ)
            offsetScaling = json_integer_value(offsetScalingJ);
    }

    void process(const ProcessArgs& args) override
    {
        // set trimpot range according to settings
        float balanceTrimParam = balanceTrimRange == UNIPOLAR ? (params[BALANCE_TRIM_PARAM].getValue() + 1) * 0.5 : params[BALANCE_TRIM_PARAM].getValue();
        float offsetTrimParam = offsetTrimRange == UNIPOLAR ? (params[OFFSET_TRIM_PARAM].getValue() + 1) * 0.5 : params[OFFSET_TRIM_PARAM].getValue();

        // get params
        balanceParam = params[BALANCE_PARAM].getValue();
        balanceParam += inputs[BALANCE_CV_INPUT].getVoltage() * balanceTrimParam / 5;
        balanceParam = clamp(balanceParam, -1.f, 1.f);

        offsetParam = params[OFFSET_PARAM].getValue();
        offsetParam += inputs[OFFSET_CV_INPUT].getVoltage() * offsetTrimParam / 5;
        offsetParam = clamp(offsetParam, -1.f, 1.f);

        phaseParam = params[PHASE_PARAM].getValue();

        // phase lights
        lights[PHASE_A_LIGHT].setBrightness(phaseParam == 1 ? 1.f : 0.f);
        lights[PHASE_B_LIGHT].setBrightness(phaseParam == 2 ? 1.f : 0.f);

        // set phase parameter according to delay mode settings
        if (phaseParam) {
            phaseParam += delayMode ? 0.f : 2.f;
        }

        // get input
        long double inputSampleA = inputs[IN_A_INPUT].getVoltage();
        long double inputSampleB = inputs[IN_B_INPUT].getVoltage();

        // pad gain
        inputSampleA *= gainCut;
        inputSampleB *= gainCut;

        if (quality == HIGH) {
            inputSampleA = rwlib::denormalize(inputSampleA);
            inputSampleB = rwlib::denormalize(inputSampleB);
        }

        // work the magic
        long double outputSample = golem.process(inputSampleA, inputSampleB, balanceParam, offsetParam, phaseParam, offsetScaling);

        if (quality == HIGH) {
            //stereo 32 bit dither, made small and tidy.
            int expon;
            frexpf((float)outputSample, &expon);
            long double dither = (rand() / (RAND_MAX * 7.737125245533627e+25)) * pow(2, expon + 62);
            outputSample += (dither - fpNShape);
            fpNShape = dither;
        }

        // bring levels back up
        outputSample *= gainBoost;

        // output
        outputs[OUT_POS_OUTPUT].setVoltage(outputSample);
        outputs[OUT_NEG_OUTPUT].setVoltage(-outputSample);
    }
};

struct GolemWidget : ModuleWidget {

    // quality item
    struct QualityItem : MenuItem {
        struct QualitySubItem : MenuItem {
            Golem* module;
            int quality;

            void onAction(const event::Action& e) override
            {
                module->quality = quality;
            }
        };

        Golem* module;
        Menu* createChildMenu() override
        {
            Menu* menu = new Menu;

            QualitySubItem* eco = createMenuItem<QualitySubItem>("Eco", CHECKMARK(module->quality == ECO));
            eco->module = this->module;
            eco->quality = ECO;
            menu->addChild(eco);

            QualitySubItem* high = createMenuItem<QualitySubItem>("High", CHECKMARK(module->quality == HIGH));
            high->module = this->module;
            high->quality = HIGH;
            menu->addChild(high);

            return menu;
        }
    };

    struct DelayModeItem : MenuItem {
        struct DelayModeSubItem : MenuItem {
            Golem* module;
            int delayMode;
            void onAction(const event::Action& e) override
            {
                module->delayMode = delayMode;
            }
        };

        Golem* module;
        Menu* createChildMenu() override
        {
            Menu* menu = new Menu;

            DelayModeSubItem* di = createMenuItem<DelayModeSubItem>("DI", CHECKMARK(module->delayMode == DI));
            di->module = this->module;
            di->delayMode = DI;
            menu->addChild(di);

            DelayModeSubItem* mic = createMenuItem<DelayModeSubItem>("MIC", CHECKMARK(module->delayMode == MIC));
            mic->module = this->module;
            mic->delayMode = MIC;
            menu->addChild(mic);

            return menu;
        }
    };

    struct BalanceTrimRangeItem : MenuItem {
        struct BalanceTrimRangeSubItem : MenuItem {
            Golem* module;
            int balanceTrimRange;
            void onAction(const event::Action& e) override
            {
                module->balanceTrimRange = balanceTrimRange;
            }
        };

        Golem* module;
        Menu* createChildMenu() override
        {
            Menu* menu = new Menu;

            BalanceTrimRangeSubItem* bipolar = createMenuItem<BalanceTrimRangeSubItem>("Bipolar", CHECKMARK(module->balanceTrimRange == BIPOLAR));
            bipolar->module = this->module;
            bipolar->balanceTrimRange = BIPOLAR;
            menu->addChild(bipolar);

            BalanceTrimRangeSubItem* unipolar = createMenuItem<BalanceTrimRangeSubItem>("Unipolar", CHECKMARK(module->balanceTrimRange == UNIPOLAR));
            unipolar->module = this->module;
            unipolar->balanceTrimRange = UNIPOLAR;
            menu->addChild(unipolar);

            return menu;
        }
    };

    struct OffsetTrimRangeItem : MenuItem {
        struct OffsetTrimRangeSubItem : MenuItem {
            Golem* module;
            int offsetTrimRange;
            void onAction(const event::Action& e) override
            {
                module->offsetTrimRange = offsetTrimRange;
            }
        };

        Golem* module;
        Menu* createChildMenu() override
        {
            Menu* menu = new Menu;

            OffsetTrimRangeSubItem* bipolar = createMenuItem<OffsetTrimRangeSubItem>("Bipolar", CHECKMARK(module->offsetTrimRange == BIPOLAR));
            bipolar->module = this->module;
            bipolar->offsetTrimRange = BIPOLAR;
            menu->addChild(bipolar);

            OffsetTrimRangeSubItem* unipolar = createMenuItem<OffsetTrimRangeSubItem>("Unipolar", CHECKMARK(module->offsetTrimRange == UNIPOLAR));
            unipolar->module = this->module;
            unipolar->offsetTrimRange = UNIPOLAR;
            menu->addChild(unipolar);

            return menu;
        }
    };

    struct OffsetScalingItem : MenuItem {
        struct OffsetScalingSubItem : MenuItem {
            Golem* module;
            int offsetScaling;
            void onAction(const event::Action& e) override
            {
                module->offsetScaling = offsetScaling;
            }
        };

        Golem* module;
        Menu* createChildMenu() override
        {
            Menu* menu = new Menu;

            OffsetScalingSubItem* lin = createMenuItem<OffsetScalingSubItem>("Linear", CHECKMARK(module->offsetScaling == LINEAR));
            lin->module = this->module;
            lin->offsetScaling = LINEAR;
            menu->addChild(lin);

            OffsetScalingSubItem* exp = createMenuItem<OffsetScalingSubItem>("Exponential", CHECKMARK(module->offsetScaling == EXPONENTIAL));
            exp->module = this->module;
            exp->offsetScaling = EXPONENTIAL;
            menu->addChild(exp);

            return menu;
        }
    };

    void appendContextMenu(Menu* menu) override
    {
        Golem* module = dynamic_cast<Golem*>(this->module);
        assert(module);

        menu->addChild(new MenuSeparator()); // separator

        MenuLabel* settingsLabel = new MenuLabel();
        settingsLabel->text = "Settings";
        menu->addChild(settingsLabel);

        QualityItem* qualityItem = createMenuItem<QualityItem>("Quality", RIGHT_ARROW);
        qualityItem->module = module;
        menu->addChild(qualityItem);

        DelayModeItem* delayModeItem = createMenuItem<DelayModeItem>("Delay Mode", RIGHT_ARROW);
        delayModeItem->module = module;
        menu->addChild(delayModeItem);

        BalanceTrimRangeItem* balanceTrimRangeItem = createMenuItem<BalanceTrimRangeItem>("Balance Trim Range", RIGHT_ARROW);
        balanceTrimRangeItem->module = module;
        menu->addChild(balanceTrimRangeItem);

        OffsetTrimRangeItem* offsetTrimRangeItem = createMenuItem<OffsetTrimRangeItem>("Offset Trim Range", RIGHT_ARROW);
        offsetTrimRangeItem->module = module;
        menu->addChild(offsetTrimRangeItem);

        OffsetScalingItem* offsetScalingItem = createMenuItem<OffsetScalingItem>("Offset Scaling", RIGHT_ARROW);
        offsetScalingItem->module = module;
        menu->addChild(offsetScalingItem);
    }

    GolemWidget(Golem* module)
    {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/golem_dark.svg")));

        //screws
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewBlack>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // params
        addParam(createParamCentered<RwKnobLargeDark>(Vec(52.5, 155.0), module, Golem::BALANCE_PARAM));
        addParam(createParamCentered<RwKnobTrimpot>(Vec(86.3, 190.0), module, Golem::BALANCE_TRIM_PARAM));
        addParam(createParamCentered<RwKnobMediumDark>(Vec(52.5, 225.0), module, Golem::OFFSET_PARAM));
        addParam(createParamCentered<RwKnobTrimpot>(Vec(18.7, 260.0), module, Golem::OFFSET_TRIM_PARAM));
        addParam(createParamCentered<RwSwitchThreeVert>(Vec(52.5, 80.0), module, Golem::PHASE_PARAM));

        // lights
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(52.5, 46.8), module, Golem::PHASE_A_LIGHT));
        addChild(createLightCentered<SmallLight<GreenLight>>(Vec(52.5, 113.3), module, Golem::PHASE_B_LIGHT));

        // inputs
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(18.8, 190.0), module, Golem::BALANCE_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(86.3, 260.0), module, Golem::OFFSET_CV_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(18.8, 55.0), module, Golem::IN_A_INPUT));
        addInput(createInputCentered<RwPJ301MPortSilver>(Vec(86.3, 55.0), module, Golem::IN_B_INPUT));

        // outputs
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(52.5, 285.0), module, Golem::OUT_POS_OUTPUT));
        addOutput(createOutputCentered<RwPJ301MPort>(Vec(52.5, 325.0), module, Golem::OUT_NEG_OUTPUT));
    }
};

Model* modelGolem = createModel<Golem, GolemWidget>("golem");
