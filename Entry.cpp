#include <list>
#include <queue>
#include <fstream>
#include <glad/glad.h>
#include <SFML/Graphics.hpp>
#include <SFML/OpenGL.hpp>
#include <imgui.h>
#include <imgui-SFML.h>
#include "Wave.h"

namespace {
  auto srcVert{R"(
    #version 330 core
    uniform ivec2 nElems;
    flat out ivec2 posElem;
    smooth out vec2 factor;
    void main() {
      int idxElem = gl_VertexID / 6;
      posElem.y = idxElem / nElems.x;
      posElem.x = idxElem - (posElem.y * nElems.x);
      switch (gl_VertexID - idxElem * 6) {
        case 0:
          factor = vec2(0, 0); break;
        case 1: case 3:
          factor = vec2(0, 1); break;
        case 2: case 4:
          factor = vec2(1, 0); break;
        default:
          factor = vec2(1, 1); break;
      }
      vec2 viewPos = 2 * (posElem + factor) / nElems - vec2(1, 1);
      gl_Position = vec4(viewPos.x, -viewPos.y, 0, 1);
    }
  )"};

  auto srcFrag{R"(
    #version 330 core
    uniform sampler2D data;
    uniform ivec2 nElems;
    flat in ivec2 posElem;
    smooth in vec2 factor;
    out vec4 color;
    vec3 getColor(ivec2 pos) {
      if (pos.x < 0 || pos.y < 0 || pos.x >= nElems.x - 1 || pos.y >= nElems.y - 1)
        return vec3(0, 0, 0);
      float c = texelFetch(data, pos, 0).r;
      return c > 0 ? vec3(1, 0.5, 0.15) * c : vec3(0.6, 0.85, 0.92) * -c;
    }
    void main() {
      vec3 c00 = getColor(posElem - ivec2(1, 1));
      vec3 c10 = getColor(posElem - ivec2(0, 1));
      vec3 c01 = getColor(posElem - ivec2(1, 0));
      vec3 c11 = getColor(posElem);
      color = vec4(mix(mix(c00, c10, factor.x), mix(c01, c11, factor.x), factor.y), 1);
    }
  )"};

  struct Source {
    ImVec2 pos;
    float wave;
    Source(const ImVec2 &pos) :pos(pos) {}
  };

  constexpr auto tps(60), sr(44100);
}

int main() {
  sf::ContextSettings contextSettings;
  contextSettings.antialiasingLevel = 8;
  contextSettings.majorVersion = 3;
  contextSettings.minorVersion = 3;
  sf::RenderWindow wnd(sf::VideoMode(800, 800), "FEMWave", sf::Style::Titlebar | sf::Style::Close, contextSettings);
  ImGui::SFML::Init(wnd);
  ImGui::GetIO().IniFilename = nullptr;
  if (!gladLoadGL())
    return EXIT_FAILURE;
  GLuint glTex;
  glGenTextures(1, &glTex);
  glBindTexture(GL_TEXTURE_2D, glTex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glBindTexture(GL_TEXTURE_2D, 0);
  sf::Shader glProg;
  sf::Texture x;
  glProg.loadFromMemory(srcVert, srcFrag);
  Wave wave{128, 128, 1.f / tps, 16, 0.1f};
  glProg.setUniform("nElems", sf::Glsl::Ivec2{wave.xElems, wave.yElems});
  glProg.setUniform("data", 0);
  float force{1024}, range{1}, sourceFreq{1}, sourcePhase{};
  int mode{};
  std::list<Source> sources;
  sf::CircleShape circle{8};
  circle.setOrigin(circle.getRadius(), circle.getRadius());
  std::vector<std::pair<float, float>> soundOut;
  std::queue<std::pair<float, float>> soundIn;
  sf::Clock wallClock;
  while (true) {
    sf::Event ev;
    while (wnd.pollEvent(ev)) {
      ImGui::SFML::ProcessEvent(ev);
      if (ev.type == sf::Event::Closed)
        goto stop;
    }
    wave.clearForce();
    ImGui::SFML::Update(wnd, wallClock.restart());
    ImGui::Begin("FEMWave");
    ImGui::SliderFloat("waveSpeed", &wave.waveSpeed, 0, 16);
    ImGui::SliderFloat("dampConst", &wave.dampConst, 0, 4, "%.3f", 4);
    ImGui::SliderFloat("force", &force, 1, 1024, "%.3f", 4);
    ImGui::SliderFloat("range", &range, 1, 16, "%.3f", 2);
    ImGui::SliderFloat("sourceFreq", &sourceFreq, 1.f / 8, 8, "%.3f", 2);
    ImGui::RadioButton("addForce", &mode, 0); ImGui::SameLine();
    ImGui::RadioButton("sources", &mode, 1);
    soundOut.emplace_back(
      wave.u0(wave.idxNode(wave.xNodes() / 3 * 1, wave.yNodes() / 2)),
      wave.u0(wave.idxNode(wave.xNodes() / 3 * 2, wave.yNodes() / 2))
    );
    ImGui::Value("soundTime", soundOut.size() / static_cast<float>(sr));
    ImGui::Value("inAvail", soundIn.size() / static_cast<float>(sr));
    if (ImGui::Button("loadSound")) {
      std::ifstream ifs{"input.dat", std::ios::in | std::ios::binary};
      float data[2];
      while (ifs.read(reinterpret_cast<char*>(data), sizeof data))
        soundIn.emplace(data[0], data[1]);
    }
    ImGui::SameLine();
    if (ImGui::Button("saveSound")) {
      float max{};
      for (auto &i : soundOut)
        max = std::max({max, std::abs(i.first), std::abs(i.second)});
      std::ofstream ofs{"sound.dat", std::ios::out | std::ios::binary};
      for (auto &i : soundOut) {
        float data[2] { i.first / max, i.second / max };
        ofs.write(reinterpret_cast<char*>(data), sizeof data);
      }
    }
    ImGui::End();
    if (!ImGui::GetIO().WantCaptureMouse) {
      if (mode) {
        if (ImGui::IsMouseClicked(0)) {
          auto selectedSrc{sources.end()};
          for (auto i{sources.begin()}; i != sources.end(); ++i) {
            if (std::hypot(ImGui::GetMousePos().x - i->pos.x, ImGui::GetMousePos().y - i->pos.y) < circle.getRadius()) {
              selectedSrc = i;
              break;
            }
          }
          if (selectedSrc == sources.end()) {
            sources.emplace_back(ImGui::GetMousePos());
          } else {
            sources.erase(selectedSrc);
          }
        }
      } else if (ImGui::IsAnyMouseDown()) {
        wave.addForce(
          ImGui::GetMousePos().x * wave.xElems / wnd.getSize().x,
          ImGui::GetMousePos().y * wave.yElems / wnd.getSize().y,
          ImGui::IsMouseDown(1) ? -force : force, range
        );
      }
    }
    if (soundIn.empty()) {
      sourcePhase = std::fmod(sourcePhase + sourceFreq / tps, 1.f);
      float wave{std::sin(2 * std::acos(-1.f) * sourcePhase)};
      for (auto &i : sources)
        i.wave = wave;
    } else {
      sourcePhase = 0;
      if (sources.size() == 2) {
        std::tie(sources.front().wave, sources.back().wave) = soundIn.front();
        soundIn.pop();
      } else {
        for (auto &i : sources)
          i.wave = 0;
      }
    }
    for (auto &i : sources) {
      wave.addForce(
        i.pos.x * wave.xElems / wnd.getSize().x,
        i.pos.y * wave.yElems / wnd.getSize().y,
        i.wave * force, range
      );
    }
    wave.simulate();
    sf::Shader::bind(&glProg);
    glBindTexture(GL_TEXTURE_2D, glTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, wave.xNodes(), wave.yNodes(), 0, GL_RED, GL_FLOAT, wave.u0.data());
    glDrawArrays(GL_TRIANGLES, 0, wave.xElems * wave.yElems * 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    sf::Shader::bind(nullptr);
    for (auto &i : sources) {
      circle.setPosition(i.pos.x, i.pos.y);
      circle.setFillColor(i.wave > 0
        ? sf::Color{0, static_cast<uint8_t>(i.wave * 255), 0}
        : sf::Color{static_cast<uint8_t>(i.wave * -255), 0, 0});
      wnd.draw(circle);
    }
    ImGui::SFML::Render(wnd);
    wnd.display();
  }
stop:
  glDeleteTextures(1, &glTex);
  ImGui::SFML::Shutdown();
  wnd.close();
}
