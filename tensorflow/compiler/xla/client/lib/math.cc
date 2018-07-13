/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/client/lib/math.h"

#include "tensorflow/compiler/xla/client/lib/constants.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"

namespace xla {

XlaOp Sqrt(XlaOp operand) { return Pow(operand, ScalarLike(operand, 0.5)); }

XlaOp Rsqrt(XlaOp operand) { return Pow(operand, ScalarLike(operand, -0.5)); }

XlaOp Square(XlaOp operand) { return Pow(operand, ScalarLike(operand, 2.0)); }

XlaOp Reciprocal(XlaOp operand) {
  return Pow(operand, ScalarLike(operand, -1.0));
}

namespace {

// Polynomials for computing erf/erfc.  Originally from cephes.
// Note we use float for compatibility across devices, at the cost of some
// precision for 64 bit computations.
//
// Coefficients are in descending order.
std::array<float, 9> kErfcPCoefficient = {
    2.46196981473530512524E-10, 5.64189564831068821977E-1,
    7.46321056442269912687E0,   4.86371970985681366614E1,
    1.96520832956077098242E2,   5.26445194995477358631E2,
    9.34528527171957607540E2,   1.02755188689515710272E3,
    5.57535335369399327526E2};
std::array<float, 9> kErfcQCoefficient = {
    1.00000000000000000000E0, 1.32281951154744992508E1,
    8.67072140885989742329E1, 3.54937778887819891062E2,
    9.75708501743205489753E2, 1.82390916687909736289E3,
    2.24633760818710981792E3, 1.65666309194161350182E3,
    5.57535340817727675546E2};
std::array<float, 6> kErfcRCoefficient = {
    5.64189583547755073984E-1, 1.27536670759978104416E0,
    5.01905042251180477414E0,  6.16021097993053585195E0,
    7.40974269950448939160E0,  2.97886665372100240670E0};
std::array<float, 7> kErfcSCoefficient = {
    1.00000000000000000000E0, 2.26052863220117276590E0,
    9.39603524938001434673E0, 1.20489539808096656605E1,
    1.70814450747565897222E1, 9.60896809063285878198E0,
    3.36907645100081516050E0};
std::array<float, 5> kErfTCoefficient = {
    9.60497373987051638749E0, 9.00260197203842689217E1,
    2.23200534594684319226E3, 7.00332514112805075473E3,
    5.55923013010394962768E4};
std::array<float, 6> kErfUCoefficient = {
    1.00000000000000000000E0, 3.35617141647503099647E1,
    5.21357949780152679795E2, 4.59432382970980127987E3,
    2.26290000613890934246E4, 4.92673942608635921086E4};
}  // namespace

// Evaluate the polynomial given coefficients and `x`.
// N.B. Coefficients should be supplied in decreasing order.
XlaOp EvaluatePolynomial(XlaOp x,
                         tensorflow::gtl::ArraySlice<float> coefficients) {
  XlaOp poly = ScalarLike(x, 0.0);
  for (float c : coefficients) {
    poly = poly * x + ScalarLike(x, c);
  }
  return poly;
}

// Compute an approximation of the error function complement (1 - erf(x)).
XlaOp Erfc(XlaOp x) {
  XlaOp abs_x = Abs(x);
  XlaOp z = Exp(-x * x);

  XlaOp pp = EvaluatePolynomial(abs_x, kErfcPCoefficient);
  XlaOp pq = EvaluatePolynomial(abs_x, kErfcQCoefficient);
  XlaOp pr = EvaluatePolynomial(abs_x, kErfcRCoefficient);
  XlaOp ps = EvaluatePolynomial(abs_x, kErfcSCoefficient);

  XlaOp y = Select(Lt(abs_x, ScalarLike(x, 8.0)), z * pp / pq, z * pr / ps);

  return Select(Lt(x, ScalarLike(x, 0.0)), ScalarLike(x, 2.0) - y, y);
}

// Compute a polynomial approximation of the error function.
XlaOp Erf(XlaOp x) {
  XlaOp z = x * x;
  XlaOp pt = EvaluatePolynomial(z, kErfTCoefficient);
  XlaOp pu = EvaluatePolynomial(z, kErfUCoefficient);
  return x * pt / pu;
}

// Approximation for the inverse error function from
//   Giles, M., "Approximating the erfinv function".
// The approximation has the form:
//   w = -log((1 - x) * (1 + x))
//   if ( w < 5 ) {
//     w = w - 2.5
//     p = sum_{i=1}^n lq[i]*w^i
//   } else {
//     w = sqrt(w) - 3
//     p = sum_{i=1}^n gq[i]*w^i
//   }
//   return p*x
XlaOp ErfInv(XlaOp x) {
  XlaBuilder* b = x.builder();
  return b->ReportErrorOrReturn([&]() -> StatusOr<XlaOp> {
    TF_ASSIGN_OR_RETURN(Shape shape, b->GetShape(x));
    constexpr int kDegree = 9;
    constexpr std::array<float, 9> w_less_than_5_constants = {
        2.81022636e-08f,  3.43273939e-07f, -3.5233877e-06f,
        -4.39150654e-06f, 0.00021858087f,  -0.00125372503f,
        -0.00417768164f,  0.246640727f,    1.50140941f};
    constexpr std::array<float, 9> w_greater_than_5_constants = {
        -0.000200214257f, 0.000100950558f, 0.00134934322f,
        -0.00367342844f,  0.00573950773f,  -0.0076224613f,
        0.00943887047f,   1.00167406f,     2.83297682f};

    auto one = ScalarLike(x, 1.0);
    auto w = -Log((one - x) * (one + x));

    auto lt = Lt(w, ScalarLike(x, 5.0));
    auto coefficient = [&](int i) {
      return Select(lt,
                    Broadcast(ScalarLike(x, w_less_than_5_constants[i]),
                              AsInt64Slice(shape.dimensions())),
                    Broadcast(ScalarLike(x, w_greater_than_5_constants[i]),
                              AsInt64Slice(shape.dimensions())));
    };
    w = Select(lt, w - ScalarLike(x, 2.5), Sqrt(w) - ScalarLike(x, 3.0));
    auto p = coefficient(0);
    for (int i = 1; i < kDegree; ++i) {
      p = coefficient(i) + p * w;
    }
    return p * x;
  });
}

}  // namespace xla
