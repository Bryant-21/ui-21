#include "preview/KeybindingsDemo.h"
#include "b21ui/B21UI.h"
#include "b21ui/modern/Theme.h"
#include "client/KeybindingsView.h"
#include <algorithm>

namespace b21ui::preview {
    namespace {
        class Demo final : public Client {
        public:
            Demo() {
                snapshot_.bindings = {
                    {"Forward","Move forward","Fallout 4",17,0,false,{"Gameplay"}},
                    {"Back","Move back","Fallout 4",31,0,false,{"Gameplay"}},
                    {"Jump","Jump","Fallout 4",57,0,false,{"Gameplay"}},
                    {"Activate","Activate","Fallout 4",18,0,false,{"Gameplay"}},
                    {"PrimaryAttack","Attack","Fallout 4",256,0,false,{"Gameplay"}},
                    {"SecondaryAttack","Aim / block","Fallout 4",257,0,false,{"Gameplay"}},
                    {"ReadyWeapon","Ready / reload / holster","Fallout 4",19,0,false,{"Gameplay"}},
                    {"Melee","Melee / grenade","Fallout 4",56,0,false,{"Gameplay"}},
                    {"Pipboy","Open Pip-Boy","Fallout 4",15,0,false,{"Gameplay"}},
                    {"Pause","Pause","Fallout 4",1,0,false,{"Gameplay"}},
                    {"Quicksave","Quick save","Fallout 4",63,0,false,{"Gameplay"}},
                    {"Quickload","Quick load","Fallout 4",67,0,false,{"Gameplay"}},
                    {"Close","Close Pip-Boy","Fallout 4",15,0,false,{"Pip-Boy"}},
                    {"Workbench","Open workbench","B21 DevTools",68,0,false,{"Gameplay"}},
                    {"Probe","LAND probe","B21 DevTools",61,0,false,{"Gameplay"}},
                    {"Player","Toggle music player","B21 MusicPlayer",61,0,true,{"Gameplay"}},
                    {"Quickboy","Switch Pip-Boy view","Tales from Appalachia",87,0,false,{"Pip-Boy"}},
                    {"Overlay","Performance overlay","B21 DevTools",87,1,false,{"Gameplay"}},
                    {"Inventory","Open inventory helper","Inventory helper",207,0,false,{"Gameplay"}},
                    {"Screenshot","Capture screenshot","Photo tools",207,0,false,{"Gameplay"}},
                    {"Unknown","Toggle overlay","MCM / Overlay mod",207,0,true,{},"press","Activation scope not declared"},
                    {"Map","Open map","B21 FullScreenMap",50,0,false,{"Gameplay"},"press","Eligible map; no blocking menus"},
                    {"Jump","Jump","Fallout 4",279,0,false,{"Gameplay"}},
                    {"Activate","Activate","Fallout 4",276,0,false,{"Gameplay"}},
                    {"PrimaryAttack","Attack","Fallout 4",281,0,false,{"Gameplay"}},
                    {"Pipboy","Pip-Boy / light","Fallout 4",277,0,false,{"Gameplay"}},
                    {"ReadyWeapon","Ready / reload / holster","Fallout 4",278,0,false,{"Gameplay"}},
                    {"MapPad","Open map","B21 FullScreenMap",271,0,false,{"Gameplay"}},
                    {"DevPad","Open utility panel","Example hotkey mod",271,0,false,{"Gameplay"},"triple tap","450ms between taps"},
                    {"PausePad","Pause","Fallout 4",270,0,false,{"Gameplay"}},
                    {"MapHold","Open map","B21 FullScreenMap",270,0,false,{"Gameplay"},"hold 1s","Eligible map; short press opens Pause"},
                    {"QuickboyPad","Switch Pip-Boy view","Tales from Appalachia",272,0,false,{"Pip-Boy"}}
                };
                for (auto& binding : snapshot_.bindings) {
                    if (binding.source=="Fallout 4") keys::DescribeGameActivation(binding,binding.contexts.front());
                    else if (binding.trigger=="unknown") binding.trigger="press";
                }
                snapshot_.conflicts=keys::Conflicts(snapshot_.bindings);
                for (const auto& binding : snapshot_.bindings) {
                    auto found=std::ranges::find(snapshot_.sources,binding.source,&keys::Source::label);
                    if (found==snapshot_.sources.end()) snapshot_.sources.push_back({binding.source,binding.source,"Preview fixture",1});
                    else ++found->count;
                }
                snapshot_.undisclosed={"ExamplePrivateHotkeys.dll"};
            }
            void DrawSettingsNavigation(const FrameContext&) override { view_.Navigation(); }
            void Draw(const FrameContext&) override { view_.Draw(snapshot_); }
        private:
            keys::Snapshot snapshot_;
            keys::View view_;
        } demo;
    }
    void RegisterKeybindingsDemo() {
        Register(demo,{.name="UI21_Keybindings",.settings=true,.settingsLabel="Keybindings",.settingsIcon=modern::icon::Keyboard});
        Open(demo);
    }
}
