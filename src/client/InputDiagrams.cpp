#include "client/KeybindingsView.h"
#include "client/ControllerTrace.h"
#include "b21ui/modern/Widgets.h"
#include <algorithm>

namespace b21ui::keys {
    namespace {
        namespace t=modern::theme;
        std::vector<ImVec2> Bezier(std::initializer_list<ImVec2> points) {
            std::vector<ImVec2> result{*points.begin()};
            for (std::size_t segment=1; segment+2<points.size(); segment+=3) {
                const auto* p=points.begin()+segment;
                const auto start=result.back();
                for (int i=1; i<=12; ++i) {
                    const float u=static_cast<float>(i)/12, v=1-u;
                    result.push_back({v*v*v*start.x+3*v*v*u*p[0].x+3*v*u*u*p[1].x+u*u*u*p[2].x,
                        v*v*v*start.y+3*v*v*u*p[0].y+3*v*u*u*p[1].y+u*u*u*p[2].y});
                }
            }
            return result;
        }
        std::vector<ImVec2> Transform(std::vector<ImVec2> points,ImVec2 origin,float scale) {
            for (auto& p : points) { p.x=origin.x+p.x*scale; p.y=origin.y+p.y*scale; } return points;
        }
        void DrawShape(std::vector<ImVec2> points,ImU32 fill,ImU32 border,float thickness=1.0F) {
            const auto window=ImGui::GetWindowPos();
            for (auto& p : points) { p.x+=window.x; p.y+=window.y; }
            auto* draw=ImGui::GetWindowDrawList();
            draw->AddConcavePolyFilled(points.data(),static_cast<int>(points.size()),fill);
            draw->AddPolyline(points.data(),static_cast<int>(points.size()),border,ImDrawFlags_Closed,thickness);
        }
        std::vector<ImVec2> Box(float x,float y,float width,float height) {
            return {{x,y},{x+width,y},{x+width,y+height},{x,y+height}};
        }
    }
    void View::DiagramButton(const Snapshot& snapshot,const std::vector<std::size_t>& visible,int code,
                             const std::vector<ImVec2>& shape,const char* label,ImU32 ink) {
        auto a=shape.front(), b=a;
        for (const auto p : shape) { a.x=std::min(a.x,p.x); a.y=std::min(a.y,p.y); b.x=std::max(b.x,p.x); b.y=std::max(b.y,p.y); }
        ImGui::SetCursorPos(a); ImGui::PushID(code);
        const bool clicked=ImGui::InvisibleButton("##shape",{b.x-a.x,b.y-a.y},ImGuiButtonFlags_EnableNav);
        if (clicked) { selectedKey_=code; diagramClicked_=true; }
        auto level=Collision::None; std::vector<std::size_t> rows;
        for (const auto i : visible) if (snapshot.bindings[i].key==code) { rows.push_back(i); level=std::max(level,levels_[i]); }
        const auto edge=level==Collision::Overlap ? t::Error : level==Collision::Possible ? t::Warn : rows.empty() ? t::Border : t::Accent;
        const auto fill=level==Collision::Overlap ? t::ErrorFill : level==Collision::Possible ? t::WarnFill : rows.empty() ? t::Background : t::AccentSoft;
        DrawShape(shape,fill,selectedKey_==code || ImGui::IsItemHovered() || ImGui::IsItemFocused() ? t::Text : edge,selectedKey_==code ? 2.0F : 1.3F);
        const auto window=ImGui::GetWindowPos();
        const float fontSize=std::min(t::Px(14),(b.y-a.y)*0.44F);
        ImGui::PushFont(t::CurrentFonts().body,fontSize);
        const auto size=ImGui::CalcTextSize(label);
        auto* draw=ImGui::GetWindowDrawList();
        const float margin=std::min(3.0F,(b.x-a.x)*0.04F);
        draw->PushClipRect({window.x+a.x+margin,window.y+a.y},{window.x+b.x-margin,window.y+b.y},true);
        draw->AddText({window.x+a.x+std::max(3.0F,(b.x-a.x-size.x)/2),window.y+a.y+(b.y-a.y-size.y)/2},ink ? ink : rows.empty() ? t::Muted : t::Text,label);
        draw->PopClipRect(); ImGui::PopFont();
        if (rows.size()>1) draw->AddCircleFilled({window.x+b.x-5,window.y+a.y+5},2.5F,edge);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort)) {
            ImGui::BeginTooltip(); ImGui::TextUnformatted(KeyName(code).c_str());
            if (rows.empty()) ImGui::TextDisabled("No discovered binding in this filter");
            for (const auto row : rows) {
                const auto& item=snapshot.bindings[row];
                ImGui::Text("%s / %s",item.label.c_str(),item.source.c_str());
                ImGui::TextDisabled("%s / %s",item.trigger.c_str(),ChordName(item).c_str());
            }
            ImGui::EndTooltip();
        }
        ImGui::PopID();
    }
    void View::Mouse(const Snapshot& snapshot,const std::vector<std::size_t>& visible,ImVec2 origin,float scale) {
        const auto transform=[&](std::vector<ImVec2> points) { return Transform(std::move(points),origin,scale); };
        auto body=Bezier({{55,5},{24,5},{10,30},{10,65},{10,108},{8,147},{32,166},{45,177},{65,177},{78,166},
            {102,147},{100,108},{100,65},{100,30},{86,5},{55,5}});
        DrawShape(transform(std::move(body)),t::Input,t::Muted,1.5F);
        DiagramButton(snapshot,visible,256,transform(Bezier({{52,8},{26,8},{14,31},{14,64},{21,76},{38,80},{52,77},
            {52,55},{52,30},{52,8}})),"1");
        DiagramButton(snapshot,visible,257,transform(Bezier({{58,8},{84,8},{96,31},{96,64},{89,76},{72,80},{58,77},
            {58,55},{58,30},{58,8}})),"2");
        auto wheel=Bezier({{50,28},{50,21},{60,21},{60,28},{60,41},{60,44},{60,49},{60,56},{50,56},{50,49},
            {50,42},{50,35},{50,28}});
        DiagramButton(snapshot,visible,258,transform(std::move(wheel)),"3");
        DiagramButton(snapshot,visible,259,transform(Bezier({{9,83},{3,83},{3,107},{9,107},{14,107},{14,83},{9,83}})),"4");
        DiagramButton(snapshot,visible,260,transform(Bezier({{9,113},{3,113},{3,137},{9,137},{14,137},{14,113},{9,113}})),"5");
        DiagramButton(snapshot,visible,264,transform(Box(108,30,31,32)),"+");
        DiagramButton(snapshot,visible,265,transform(Box(108,66,31,32)),"-");
        const auto window=ImGui::GetWindowPos();
        ImGui::GetWindowDrawList()->AddText(t::CurrentFonts().body,12*scale,{window.x+origin.x+108*scale,window.y+origin.y+14*scale},t::Muted,"Wheel");
    }
    void View::Controller(const Snapshot& snapshot,const std::vector<std::size_t>& visible,float width,float height) {
        const float scale=std::min({t::Scale(),width*0.54F/trace::Width,(height-t::Px(76))/trace::Height});
        const ImVec2 origin{(width-trace::Width*scale)/2,t::Px(50)};
        const auto transform=[&](std::vector<ImVec2> points) { return Transform(std::move(points),origin,scale); };
        const auto contour=[&](const auto& shape) { return transform(std::vector<ImVec2>(std::begin(shape),std::end(shape))); };
        DrawShape(contour(trace::Shell),t::Surface,t::Muted,1.5F);
        DrawShape(contour(trace::Face),t::Input,t::Muted);
        const auto button=[&](int code,const auto& shape,const char* label,ImU32 ink=0) {
            DiagramButton(snapshot,visible,code,contour(shape),label,ink);
        };
        DiagramButton(snapshot,visible,280,transform(Box(170,-62,90,40)),"LT");
        DiagramButton(snapshot,visible,281,transform(Box(575,-62,90,40)),"RT");
        button(274,trace::LeftShoulder,"LB"); button(275,trace::RightShoulder,"RB");
        DrawShape(contour(trace::LeftStickOuter),t::Surface,t::Border);
        DrawShape(contour(trace::LeftStickMiddle),t::Input,t::Border);
        DrawShape(contour(trace::RightStickOuter),t::Surface,t::Border);
        DrawShape(contour(trace::RightStickMiddle),t::Input,t::Border);
        button(272,trace::LeftStickPress,"L3"); button(273,trace::RightStickPress,"R3");
        button(271,trace::View,""); button(270,trace::Menu,"");
        button(279,trace::Y,"Y",IM_COL32(244,211,94,255));
        button(278,trace::X,"X",IM_COL32(108,176,247,255));
        button(277,trace::B,"B",IM_COL32(246,125,125,255));
        button(276,trace::A,"A",IM_COL32(119,213,147,255));
        DrawShape(contour(trace::DpadOuter),t::Surface,t::Border);
        DrawShape(contour(trace::DpadCross),t::Input,t::Border);
        button(266,trace::DpadUp,"^"); button(267,trace::DpadDown,"v");
        button(268,trace::DpadLeft,"<"); button(269,trace::DpadRight,">");
        DrawShape(contour(trace::GuideBottom),t::Muted,t::Muted);
        DrawShape(contour(trace::GuideRight),t::Muted,t::Muted);
        DrawShape(contour(trace::GuideLeft),t::Muted,t::Muted);
        DrawShape(contour(trace::GuideTop),t::Muted,t::Muted);
        const auto window=ImGui::GetWindowPos();
        auto* draw=ImGui::GetWindowDrawList();
        const auto point=[&](float x,float y) { return ImVec2{window.x+origin.x+x*scale,window.y+origin.y+y*scale}; };
        draw->AddRect(point(345,163),point(360,175),t::Text,0,0,1.2F);
        draw->AddRect(point(353,169),point(368,181),t::Text,0,0,1.2F);
        for (int i=0; i<3; ++i) draw->AddLine(point(468,164+i*7.0F),point(489,164+i*7.0F),t::Text,1.2F);
        draw->AddLine(point(215,-22),point(180,0),t::Faint);
        draw->AddLine(point(620,-22),point(655,0),t::Faint);
        const int left[]{280,274,272,271,266,268,269,267}, right[]{281,275,279,278,270,277,276,273};
        const float side=std::max(t::Px(100),origin.x-t::Px(18)), rowHeight=(height-t::Px(24))/8;
        for (int column=0; column<2; ++column) for (int row=0; row<8; ++row) {
            const int code=column ? right[row] : left[row];
            std::string action="None in filter", activation; std::size_t count{};
            for (const auto i : visible) if (snapshot.bindings[i].key==code) {
                if (!count) { action=snapshot.bindings[i].label; activation=snapshot.bindings[i].trigger; }
                else if (activation!=snapshot.bindings[i].trigger) activation="mixed";
                ++count;
            }
            if (count>1) action+=" (+"+std::to_string(count-1)+")";
            const auto label=KeyName(code)+(activation.empty() ? "" : " / "+activation)+"\n"+action;
            const float x=column ? width-side-t::Px(8) : t::Px(8), y=t::Px(8)+row*rowHeight;
            ImGui::PushID(column+1);
            DiagramButton(snapshot,visible,code,Box(x,y,side,rowHeight-t::Px(4)),label.c_str());
            ImGui::PopID();
        }
        const char* note="Sticks: L3 / R3 press. Guide / Share are system buttons.";
        ImGui::PushFont(t::CurrentFonts().body,t::Px(11));
        const auto text=ImGui::CalcTextSize(note);
        draw->AddText({window.x+(width-text.x)/2,window.y+height-t::Px(12)},t::Faint,note);
        ImGui::PopFont();
    }
}
