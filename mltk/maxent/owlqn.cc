// Copyright (c) 2013 MLTK Project.
// Author: Lifeng Wang (ofandywang@gmail.com)
//
// Implementations of OWLQN algorithm.
//
// Pls refer to 'Galen Andrew and Jianfeng Gao, Scalable training of
// L1-regularized log-linear models, in ICML 2007.'

#include "mltk/maxent/maxent.h"

#include <math.h>
#include <iostream>
#include <vector>

#include "mltk/common/double_vector.h"

namespace mltk {
namespace maxent {

using mltk::common::DoubleVector;

const static int32_t OWLQN_M = 10;
const static double LINE_SEARCH_ALPHA = 0.1;
const static double LINE_SEARCH_BETA = 0.5;

// stopping criteria
const static int32_t OWLQN_MAX_ITER = 300;
const static double MIN_GRAD_NORM = 0.0001;

inline int32_t Sign(double x) {
  if (x > 0) { return 1; }
  if (x < 0) { return -1; }
  return 0;
};

static DoubleVector PseudoGradient(const DoubleVector& x,
                                   const DoubleVector& grad0,
                                   const double C) {
  DoubleVector grad = grad0;
  for (size_t i = 0; i < x.Size(); i++) {
    if (x[i] != 0) {
      grad[i] += C * Sign(x[i]);
      continue;
    }
    const double gm = grad0[i] - C;
    if (gm > 0) {
      grad[i] = gm;
      continue;
    }
    const double gp = grad0[i] + C;
    if (gp < 0) {
      grad[i] = gp;
      continue;
    }
    grad[i] = 0;
  }

  return grad;
}

DoubleVector ApproximateHg(const int32_t iter,
                           const DoubleVector& grad,
                           const DoubleVector s[],
                           const DoubleVector y[],
                           const double z[]);

std::vector<double> MaxEnt::PerformOWLQN(const std::vector<double>& x0,
                                         const double C) {
  const size_t dim = x0.size();
  DoubleVector x(x0);

  DoubleVector grad(dim), dx(dim);
  double f = RegularizedFuncGrad(C, x, grad);

  DoubleVector s[OWLQN_M];
  DoubleVector y[OWLQN_M];
  double z[OWLQN_M];  // rho

  for (int32_t iter = 0; iter < OWLQN_MAX_ITER; ++iter) {  // stopping criteria 1
    DoubleVector pg = PseudoGradient(x, grad, C);

    std::cerr << "iter = " << iter + 1
        << ", obj(err) = " << f
        << ", accuracy = " << train_accuracy_ << std::endl;
    if (heldout_.size() > 0) {
      const double heldout_logl = CalcHeldoutLikelihood();
      std::cerr << "\theldout_logl(err) = " << -1 * heldout_logl
          << ", accuracy = " << heldout_accuracy_ << std::endl;
    }

    // stopping criteria 2
    if (sqrt(DotProduct(pg, pg)) < MIN_GRAD_NORM) { break; }

    dx = -1 * ApproximateHg(iter, pg, s, y, z);
    if (DotProduct(dx, pg) >= 0) { dx.Project(-1 * pg); }

    DoubleVector x1(dim), grad1(dim);
    f = ConstrainedLineSearch(C, x, pg, f, dx, x1, grad1);

    s[iter % OWLQN_M] = x1 - x;
    y[iter % OWLQN_M] = grad1 - grad;
    z[iter % OWLQN_M] = 1.0 / DotProduct(y[iter % OWLQN_M], s[iter % OWLQN_M]);

    x = x1;
    grad = grad1;
  }

  return x.STLVector();
}

double MaxEnt::RegularizedFuncGrad(const double C,
                                   const DoubleVector& x,
                                   DoubleVector& grad) {
  double f = FunctionGradient(x.STLVector(), &(grad.STLVector()));
  for (size_t i = 0; i < x.Size(); i++) {
    f += C * fabs(x[i]);
  }
  return f;
}

double MaxEnt::ConstrainedLineSearch(double C,
                                     const DoubleVector& x0,
                                     const DoubleVector& grad0,
                                     const double f0,
                                     const DoubleVector& dx,
                                     DoubleVector& x,
                                     DoubleVector& grad1) {
  // compute the orthant to explore
  DoubleVector orthant = x0;
  for (size_t i = 0; i < orthant.Size(); i++) {
    if (orthant[i] == 0) orthant[i] = -grad0[i];
  }

  double t = 1.0 / LINE_SEARCH_BETA;

  double f;
  do {
    t *= LINE_SEARCH_BETA;
    x = x0 + t * dx;
    x.Project(orthant);
    f = RegularizedFuncGrad(C, x, grad1);
  } while (f > f0 + LINE_SEARCH_ALPHA * DotProduct(x - x0, grad0));

  return f;
}

}  // namespace maxent
}  // namespace mltk
