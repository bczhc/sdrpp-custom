#include <imgui.h>
#include <module.h>
#include <gui/gui.h>
#include <config.h>
#include <core.h>
#include <signal_path/signal_path.h>
#include "if_nr.h"

ConfigManager config;

SDRPP_MOD_INFO{
    /* Name:            */ "noise_reduction_logmmse",
    /* Description:     */ "LOGMMSE baseband noise reduction",
    /* Author:          */ "sannysanoff",
    /* Version:         */ 0, 1, 0,
    /* Max instances    */ -1
};

class NRModule : public ModuleManager::Instance {

    dsp::IFNRLogMMSE ifnrProcessor;

public:
    NRModule(std::string name) {
        this->name = name;
        config.acquire();
        if (config.conf.contains("IFNR")) { ifnr = config.conf["IFNR"]; }
        config.release(true);

        gui::menu.registerEntry(name, menuHandler, this, NULL);
        updateBindings();
        actuateIFNR();
    }

    ~NRModule() {
        gui::menu.removeEntry(name);
    }

    void postInit() {}

    void enable() {
        if (!enabled) {
            enabled = true;
            updateBindings();
            actuateIFNR();
        }
    }

    void disable() {
        if (enabled) {
            enabled = false;
            actuateIFNR();
            updateBindings();
        }
    }

    bool isEnabled() {
        return enabled;
    }

private:
    bool ifnr = false;
    bool enabled = true;

    // Registers the NR processor as an IQ front-end preprocessor and wires up
    // the retune handler that resets the noise profile.
    void updateBindings() {
        if (enabled) {
            sigpath::iqFrontEnd.addPreprocessor(&ifnrProcessor, false);

            sigpath::sourceManager.onRetune.bindHandler(&retuneHandler);
            retuneHandler.ctx = this;
            retuneHandler.handler = [](double freq, void* ctx) {
                auto _this = (NRModule*)ctx;
                _this->ifnrProcessor.reset(); // reset noise profile on retune
            };
        }
        else {
            sigpath::iqFrontEnd.removePreprocessor(&ifnrProcessor);
            sigpath::sourceManager.onRetune.unbindHandler(&retuneHandler);
        }
    }

    void actuateIFNR() {
        bool shouldRun = enabled && ifnr;
        if (ifnrProcessor.bypass != !shouldRun) {
            ifnrProcessor.bypass = !shouldRun;
            sigpath::iqFrontEnd.togglePreprocessor(&ifnrProcessor, shouldRun);
        }
    }

    void menuHandler() {
        if (ImGui::Checkbox("Baseband NR##_sdrpp_if_nr", &ifnr)) {
            config.acquire();
            config.conf["IFNR"] = ifnr;
            config.release(true);
            if (ifnr) { // toggled on - clear any previous stop reason.
                ifnrProcessor.stopReason = "";
            }
            actuateIFNR();
        }

        // The processor asks to stop (e.g. CPU too slow) - turn the toggle off.
        if (ifnrProcessor.stopReason != "" && ifnr) {
            ifnr = false;
            config.acquire();
            config.conf["IFNR"] = ifnr;
            config.release(true);
            actuateIFNR();
        }

        if (ifnrProcessor.stopReason != "") {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0, 0, 1.0f));
            ImGui::Text("%s", ifnrProcessor.stopReason.c_str());
            ImGui::PopStyleColor(1);
        }
    }

    static void menuHandler(void* ctx) {
        NRModule* _this = (NRModule*)ctx;
        _this->menuHandler();
    }

    std::string name;
    EventHandler<double> retuneHandler;
};

MOD_EXPORT void _INIT_() {
    std::string root = (std::string)core::args["root"];
    config.setPath(root + "/noise_reduction_logmmse_config.json");
    config.load(json::object());
    config.enableAutoSave();
}

MOD_EXPORT ModuleManager::Instance* _CREATE_INSTANCE_(std::string name) {
    return new NRModule(name);
}

MOD_EXPORT void _DELETE_INSTANCE_(ModuleManager::Instance* instance) {
    delete (NRModule*)instance;
}

MOD_EXPORT void _END_() {
    config.disableAutoSave();
    config.save();
}
