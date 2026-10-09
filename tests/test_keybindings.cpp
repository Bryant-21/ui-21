#include <doctest/doctest.h>
#include "core/Keybindings.h"
using namespace b21ui::keys;

TEST_CASE("hotkey collisions respect scope, modifiers and gestures") {
    Binding a{"one","One","First",207,0,false,{"Gameplay"},"press"};
    Binding b{"two","Two","Second",207,0,false,{"Gameplay"},"press"};
    CHECK(Compare(a,b)==Collision::Overlap);
    b.contexts={"Pip-Boy"}; CHECK(Compare(a,b)==Collision::None);
    b.contexts={}; CHECK(Compare(a,b)==Collision::Possible);
    b.contexts={"*"}; CHECK(Compare(a,b)==Collision::Overlap);
    b.conditions="Only in combat"; CHECK(Compare(a,b)==Collision::Possible);
    b.conditions.clear(); b.trigger="hold 1s"; CHECK(Compare(a,b)==Collision::Possible);
    b.trigger="press"; a.exactModifiers=true; b.exactModifiers=true; b.modifiers=1;
    CHECK(Compare(a,b)==Collision::None);
    a.exactModifiers=false; CHECK(Compare(a,b)==Collision::Overlap);
    a.modifiers=2; CHECK(Compare(a,b)==Collision::None);
    b.exactModifiers=false; CHECK(Compare(a,b)==Collision::Overlap);
    b.key=0; CHECK(Compare(a,b)==Collision::None);
    b=a; CHECK(Compare(a,b)==Collision::None);
}
TEST_CASE("unknown activation is never reported as a confirmed press collision") {
    Binding a{"one","One","First",207,0,false,{"Gameplay"}};
    Binding b{"two","Two","Second",207,0,false,{"Gameplay"}};
    CHECK(a.trigger=="unknown"); CHECK(Compare(a,b)==Collision::Possible);
    b.trigger="press"; CHECK(Compare(a,b)==Collision::Possible);
    const auto json=nlohmann::json::parse(R"([{"id":"one","label":"Action","key":207,"trigger":""}])");
    CHECK(Decode(json,"Source").front().trigger=="unknown");
    auto missing=json; missing[0].erase("trigger");
    CHECK(Decode(missing,"Source").front().trigger=="unknown");
}
TEST_CASE("game dual actions retain hold and release semantics only in gameplay") {
    Binding binding{"ReadyWeapon","ReadyWeapon","Fallout 4",19};
    DescribeGameActivation(binding,"Gameplay");
    CHECK(binding.trigger=="tap / hold"); CHECK(binding.conditions.find("holster")!=std::string::npos);
    DescribeGameActivation(binding,"Workshop"); CHECK(binding.trigger=="unknown");
    binding.id="Melee"; DescribeGameActivation(binding,"Gameplay");
    CHECK(binding.trigger=="tap / hold + release"); CHECK(binding.conditions.find("grenade")!=std::string::npos);
    binding.id="Pipboy"; DescribeGameActivation(binding,"Gameplay");
    CHECK(binding.trigger=="tap / hold"); CHECK(binding.conditions.find("light")!=std::string::npos);
    binding.id="StrafeLeft"; DescribeGameActivation(binding,"Gameplay"); CHECK(binding.trigger=="hold");
    binding.id="PrivateEvent"; DescribeGameActivation(binding,"Gameplay"); CHECK(binding.trigger=="unknown");
}
TEST_CASE("Xbox and mouse conflicts use separate macro codes") {
    Binding a{"a","Attack","Game",281,0,false,{"Gameplay"}};
    Binding b{"b","Ability","Mod",281,0,false,{"Gameplay"}};
    REQUIRE(Conflicts({a,b}).size()==1);
    b.key=256; CHECK(Conflicts({a,b}).empty());
    CHECK(KeyName(207)=="End"); CHECK(KeyName(61)=="F3"); CHECK(KeyName(271)=="View");
    CHECK(KeyName(280)=="LT"); CHECK(KeyName(265)=="Wheel down");
    a.key=87; a.modifiers=3; CHECK(ChordName(a)=="Shift + Ctrl + F11");
}
TEST_CASE("provider data validates atomically and uses a source fallback") {
    Binding a{"one","Action","",61,0,true,{"Gameplay"},"press","Only when enabled"};
    auto json=Encode({a}); auto decoded=Decode(json,"Provider");
    REQUIRE(decoded.size()==1); CHECK(decoded[0].source=="Provider");
    CHECK(decoded[0].conditions==a.conditions); CHECK(decoded[0].exactModifiers);
    json[0]["key"]=-1; CHECK_THROWS(Decode(json,"Provider"));
    CHECK_THROWS(Decode(nlohmann::json::object(),"Provider"));
}
TEST_CASE("failed providers remain visible in discovery coverage") {
    Providers providers;
    const B21UI_KeybindingProvider good{sizeof(good),"good","Good",nullptr,[](void*) -> const char* {
        return R"([{"id":"end","label":"Action","key":207,"contexts":["Gameplay"]}])";
    }};
    CHECK(providers.Add(good)); CHECK_FALSE(providers.Add(good));
    const B21UI_KeybindingProvider bad{sizeof(bad),"bad","Bad",nullptr,[](void*) -> const char* { return "null"; }};
    CHECK(providers.Add(bad));
    auto read=providers.Read(); REQUIRE(read.bindings.size()==1); REQUIRE(read.sources.size()==2);
    CHECK(read.sources[0].count==1); CHECK(read.sources[1].status.starts_with("Unavailable:"));
    CHECK(InContext(read.bindings[0],"Gameplay")); CHECK_FALSE(InContext(read.bindings[0],"Pip-Boy"));
    CHECK(InContext(read.bindings[0],""));
}
TEST_CASE("gamepad masks keep unknown inputs visible without guessing their names") {
    CHECK(FromGamepadMask(0x10020)==271);
    CHECK(FromGamepadMask(0x1000)==276);
    CHECK(FromGamepadMask(9)==280);
    CHECK(FromGamepadMask(0)==0);
    CHECK(FromGamepadMask(11)==1035);
    CHECK(KeyName(FromGamepadMask(11))=="Xbox input B");
}
TEST_CASE("virtual key normalization preserves extended End and function keys") {
    CHECK(FromVirtualKey(0x23)==207);
    CHECK(FromVirtualKey(0x24)==199);
    CHECK(FromVirtualKey(0x72)==61);
    CHECK(FromVirtualKey(0x7A)==87);
    CHECK(FromVirtualKey(0x48)==35);
    CHECK(FromVirtualKey(0)==0);
}
