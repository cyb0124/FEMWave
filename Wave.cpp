#include "Wave.h"

Wave::Wave(int xElems, int yElems, float timeStep, float waveSpeed, float dampConst)
  :xElems(xElems), yElems(yElems), timeStep(timeStep), waveSpeed(waveSpeed), dampConst(dampConst),
  u0(nNodes()), u1(nNodes()), force(nNodes()), mass(nNodes(), nNodes()), stiffness(nNodes(), nNodes()) {
  u0.setZero();
  u1.setZero();
  populateMass();
  populateStiffness();
}

int Wave::idxNode(int x, int y) const {
  if (x < 0 || x >= xNodes() || y < 0 || y >= yNodes())
    return -1;
  return y * xNodes() + x;
}

void Wave::fill(std::vector<Eigen::Triplet<float>> &init, int x1, int y1, int x2, int y2, float value) {
  auto idx1{idxNode(x1, y1)};
  auto idx2{idxNode(x2, y2)};
  if (idx1 < 0 || idx2 < 0)
    return;
  init.emplace_back(idx1, idx2, value);
  if (idx1 != idx2)
    init.emplace_back(idx2, idx1, value);
}

void Wave::fillForce(int x, int y, float value) {
  auto idx{idxNode(x, y)};
  if (idx < 0)
    return;
  force(idx) += value;
}

void Wave::populateMass() {
  std::vector<Eigen::Triplet<float>> init;
  for (int y{}; y < yElems; ++y) {
    for (int x{}; x < xElems; ++x) {
      fill(init, x - 1, y - 1, x - 1, y - 1, 1.f / 9 );
      fill(init, x,     y - 1, x - 1, y - 1, 1.f / 18 );
      fill(init, x,     y - 1, x    , y - 1, 1.f / 9 );
      fill(init, x - 1, y    , x - 1, y - 1, 1.f / 18);
      fill(init, x - 1, y    , x    , y - 1, 1.f / 36);
      fill(init, x - 1, y    , x - 1, y    , 1.f / 9 );
      fill(init, x,     y    , x - 1, y - 1, 1.f / 36);
      fill(init, x,     y    , x    , y - 1, 1.f / 18 );
      fill(init, x,     y    , x - 1, y    , 1.f / 18 );
      fill(init, x,     y    , x    , y    , 1.f / 9 );
    }
  }
  mass.setFromTriplets(init.begin(), init.end());
  inverseMass.analyzePattern(mass);
  inverseMass.factorize(mass);
}

void Wave::populateStiffness() {
  std::vector<Eigen::Triplet<float>> init;
  for (int y{}; y < yElems; ++y) {
    for (int x{}; x < xElems; ++x) {
      fill(init, x - 1, y - 1, x - 1, y - 1,  2.f / 3);
      fill(init, x    , y - 1, x - 1, y - 1, -1.f / 6);
      fill(init, x    , y - 1, x    , y - 1,  2.f / 3);
      fill(init, x - 1, y    , x - 1, y - 1, -1.f / 6);
      fill(init, x - 1, y    , x    , y - 1, -1.f / 3);
      fill(init, x - 1, y    , x - 1, y    ,  2.f / 3);
      fill(init, x    , y    , x - 1, y - 1, -1.f / 3);
      fill(init, x    , y    , x    , y - 1, -1.f / 6);
      fill(init, x    , y    , x - 1, y    , -1.f / 6);
      fill(init, x    , y    , x    , y    ,  2.f / 3);
    }
  }
  stiffness.setFromTriplets(init.begin(), init.end());
}

void Wave::addForce(float xCenter, float yCenter, float depth, float width) {
  for (int y{}; y < yElems; ++y) {
    for (int x{}; x < xElems; ++x) {
      auto xOffset{x + 0.5f - xCenter};
      auto yOffset{y + 0.5f - yCenter};
      auto fValue{depth * std::exp(-(xOffset * xOffset + yOffset * yOffset) / (width * width))};
      fillForce(x - 1, y - 1, fValue / 4);
      fillForce(x - 1, y, fValue / 4);
      fillForce(x, y - 1, fValue / 4);
      fillForce(x, y, fValue / 4);
    }
  }
}

void Wave::timeDifference(Eigen::VectorXf &du0, Eigen::VectorXf &du1, const Eigen::VectorXf &u0, const Eigen::VectorXf &u1) {
  du0 = (u1 - dampConst * u0) * timeStep;
  du1 = inverseMass.solve(force - (waveSpeed * waveSpeed) * (stiffness * u0)) * timeStep;
}

void Wave::simulate() {
  Eigen::VectorXf k10, k11, k20, k21, k30, k31, k40, k41;
  timeDifference(k10, k11, u0          , u1          );
  timeDifference(k20, k21, u0 + k10 / 2, u1 + k11 / 2);
  timeDifference(k30, k31, u0 + k20 / 2, u1 + k21 / 2);
  timeDifference(k40, k41, u0 + k30    , u1 + k31    );
  u0 += (k10 + 2 * k20 + 2 * k30 + k40) / 6;
  u1 += (k11 + 2 * k21 + 2 * k31 + k41) / 6;
}
