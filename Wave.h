#pragma once
#include <Eigen/Eigen>

struct Wave {
  int xElems, yElems;
  float timeStep;
  float waveSpeed;
  float dampConst;
  Eigen::VectorXf u0, u1, force;
  Eigen::SparseMatrix<float> mass, stiffness;
  Eigen::SimplicialLLT<Eigen::SparseMatrix<float>> inverseMass;
  
  Wave(int xElems, int yElems, float timeStep, float waveSpeed, float dampConst);
  int xNodes() const { return xElems - 1; }
  int yNodes() const { return yElems - 1; }
  int nNodes() const { return xNodes() * yNodes(); }
  int idxNode(int x, int y) const;
  void fill(std::vector<Eigen::Triplet<float>> &init, int x1, int y1, int x2, int y2, float value);
  void fillForce(int x, int y, float value);
  void populateMass();
  void populateStiffness();
  void clearForce() { force.setZero(); }
  void addForce(float xCenter, float yCenter, float depth, float width);
  void timeDifference(Eigen::VectorXf &du0, Eigen::VectorXf &du1, const Eigen::VectorXf &u0, const Eigen::VectorXf &u1);
  void simulate();
};
