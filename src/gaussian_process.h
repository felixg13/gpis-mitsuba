/*
 * SimpleGP_DrJit.h - Lightweight Gaussian Process for GPIS using DrJit/Mitsuba3
 *
 * A minimal, focused GP implementation using DrJit for device compatibility.
 * Designed specifically for Gaussian Process Implicit Surfaces.
 *
 * Properly integrates with Mitsuba's variant system using MI_VARIANT macros.
 */

#pragma once

#include <drjit/tensor.h>
#include <memory>
#include <mitsuba/core/random.h>
#include <mitsuba/mitsuba.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

namespace gpis {

/// Covariance kernel interface
template <typename Float, typename Spectrum> class Kernel {
public:
  MI_IMPORT_CORE_TYPES()
  using Scalar = dr::scalar_t<Float>;

  virtual ~Kernel() = default;

  /// Evaluate k(x, y) for scalar points
  virtual Float evaluate(const Point3f &x, const Point3f &y) const = 0;

  /// Get kernel parameters (for inspection/modification)
  virtual std::vector<Scalar> get_params() const = 0;
  virtual void set_params(const std::vector<Scalar> &params) = 0;
};

/// Squared Exponential (RBF) kernel: k(x,y) = σ² exp(-||x-y||²/(2ℓ²))
template <typename Float, typename Spectrum>
class SquaredExponentialKernel : public Kernel<Float, Spectrum> {
public:
  MI_IMPORT_CORE_TYPES()
  using Scalar = dr::scalar_t<Float>;
  using Base = Kernel<Float, Spectrum>;

  SquaredExponentialKernel(Scalar lengthscale = 1.0,
                           Scalar signal_variance = 1.0)
      : m_lengthscale(lengthscale), m_signal_variance(signal_variance) {}

  Float evaluate(const Point3f &x, const Point3f &y) const override {
    Float dist_sq = dr::squared_norm(x - y);
    Scalar l_sq = m_lengthscale * m_lengthscale;
    return m_signal_variance * dr::exp(-dist_sq / (2.f * l_sq));
  }

  // TensorXf evaluate_batch(const TensorXf &X, const TensorXf &Y) const
  // override {
  //   size_t N = X.shape(0);
  //   size_t M = Y.shape(0);

  //   TensorXf X2 = dr::square(X);

  //   size_t D = X.shape(1);

  //   TensorXf X_norm2_1d = TensorXf::zero_(N);
  //   for (size_t d = 0; d < D; ++d) {
  //     // take column d → shape (N)
  //     TensorXf col = drjit::take(X2, TensorXf::Index(d), /*axis=*/1);
  //     X_norm2_1d = X_norm2_1d + col;
  //   }

  //   TensorXf Y_norm2_1d = TensorXf::zero_(N);
  //   TensorXf Y2 = dr::square(X);
  //   for (size_t d = 0; d < D; ++d) {
  //     // take column d → shape (N)
  //     TensorXf col = drjit::take(Y2, TensorXf::Index(d), /*axis=*/1);
  //     Y_norm2_1d = Y_norm2_1d + col;
  //   }

  //   // Promote to (N, 1) and (1, M)
  //   size_t shape_X[2] = {N, 1};
  //   size_t shape_Y[2] = {1, M};

  //   TensorXf X_norm2(X_norm2_1d.array(), 2, shape_X);
  //   TensorXf Y_norm2(Y_norm2_1d.array(), 2, shape_Y);

  //   // (N, M)
  //   TensorXf XY = tensor_matmul(X, dr::transpose(Y));

  //   // ||x - y||² = ||x||² + ||y||² - 2 x·y
  //   TensorXf dist2 = X_norm2 + Y_norm2 - 2.f * XY;

  //   Scalar l_sq = m_lengthscale * m_lengthscale;

  //   return m_signal_variance * dr::exp(-dist2 / (2.f * l_sq));
  // }

  std::vector<Scalar> get_params() const override {
    return {m_lengthscale, m_signal_variance};
  }
  void set_params(const std::vector<Scalar> &params) override {
    if (params.size() >= 1)
      m_lengthscale = params[0];
    if (params.size() >= 2)
      m_signal_variance = params[1];
  }

  Scalar lengthscale() const { return m_lengthscale; }
  Scalar signal_variance() const { return m_signal_variance; }

private:
  Scalar m_lengthscale;
  Scalar m_signal_variance;
};

/// Simple Gaussian Process for GPIS using DrJit
template <typename Float, typename Spectrum> class GaussianProcess {
public:
  MI_IMPORT_CORE_TYPES()
  using Scalar = dr::scalar_t<Float>;
  using PCG32 = dr::PCG32<UInt32>;

  GaussianProcess(std::shared_ptr<Kernel<Float, Spectrum>> kernel,
                  Scalar noise_variance = 1e-6)
      : m_kernel(kernel), m_noise_variance(noise_variance),
        m_is_trained(false) {}

  /// Add a conditioning observation: f(x) = y
  void add_observation(const Point3f &x, Scalar y) {
    m_observations_x.push_back(x);
    m_observations_y.push_back(y);
    m_is_trained = false;
  }

  /// Add multiple observations at once
  void add_observations(const std::vector<Point3f> &X,
                        const std::vector<Scalar> &y) {
    for (size_t i = 0; i < X.size(); i++) {
      m_observations_x.push_back(X[i]);
      m_observations_y.push_back(y[i]);
    }
    m_is_trained = false;
  }

  /// Clear all observations
  void clear_observations() {
    m_observations_x.clear();
    m_observations_y.clear();
    m_is_trained = false;
  }

  void train() {
    size_t n = m_observations_x.size();
    if (n == 0) {
      m_is_trained = false;
      return;
    }

    // Build covariance matrix K
    m_K_flat.resize(n * n);
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < n; ++j) {
        m_K_flat[i * n + j] =
            m_kernel->evaluate(m_observations_x[i], m_observations_x[j]);
      }
    }

    // Add noise to diagonal for numerical stability
    for (size_t i = 0; i < n; ++i) {
      m_K_flat[i * n + i] = m_K_flat[i * n + i] + m_noise_variance;
    }

    // Build observation vector y
    m_y_flat.resize(n);
    for (size_t i = 0; i < n; ++i) {
      m_y_flat[i] = Float(m_observations_y[i]);
    }

    // Compute Cholesky decomposition: K = L L^T
    m_L_flat = cholesky_decompose(m_K_flat, n);

    // Precompute alpha = K^{-1} y for predictions
    m_alpha_flat = cholesky_solve(m_L_flat, m_y_flat, n);

    m_is_trained = true;
  }

  /// Predict posterior mean at point x
  Float predict_mean(const Point3f &x) const {
    if (!m_is_trained || m_observations_x.empty()) {
      return Float(0.f); // Prior mean is zero
    }

    size_t n = m_observations_x.size();

    // Compute k* = [k(x, x_1), ..., k(x, x_n)]
    std::vector<Float> k_star(n);
    for (size_t i = 0; i < n; i++) {
      k_star[i] = m_kernel->evaluate(x, m_observations_x[i]);
    }

    // Mean: μ(x) = k* K^{-1} y = k* α
    Float result = 0.f;
    for (size_t i = 0; i < n; i++) {
      result += k_star[i] * m_alpha_flat[i];
    }
    return result;
  }

  /// Predict posterior variance at point x
  Float predict_variance(const Point3f &x) const {
    if (!m_is_trained || m_observations_x.empty()) {
      return m_kernel->evaluate(x, x); // Prior variance
    }

    size_t n = m_observations_x.size();

    // k* = [k(x, x_1), ..., k(x, x_n)]
    std::vector<Float> k_star(n);
    for (size_t i = 0; i < n; i++) {
      k_star[i] = m_kernel->evaluate(x, m_observations_x[i]);
    }

    // k** = k(x, x)
    Float k_star_star = m_kernel->evaluate(x, x);

    // Variance: σ²(x) = k** - k* K^{-1} k*
    std::vector<Float> v = cholesky_solve(m_L_flat, k_star, n);

    Float dot_product = 0.f;
    for (size_t i = 0; i < n; i++) {
      dot_product += k_star[i] * v[i];
    }

    return k_star_star - dot_product;
  }

  /// Predict posterior mean and covariance at multiple points
  std::pair<std::vector<Float>, std::vector<Float>> 
  predict_full(const std::vector<Point3f> &X_test) const {
    size_t m = X_test.size();
    size_t n = m_observations_x.size();

    std::vector<Float> means(m);
    std::vector<Float> cov_flat(m * m);

    if (!m_is_trained || n == 0) {
      // Return prior
      for (size_t i = 0; i < m; i++) {
        means[i] = Float(0.f);
        for (size_t j = 0; j < m; j++) {
          cov_flat[i * m + j] = m_kernel->evaluate(X_test[i], X_test[j]);
        }
      }
      return {means, cov_flat};
    }

    // Compute k* matrix (m x n): k*[i,j] = k(x_test[i], x_train[j])
    std::vector<Float> k_star(m * n);
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < n; j++) {
        k_star[i * n + j] = m_kernel->evaluate(X_test[i], m_observations_x[j]);
      }
    }

    // Compute means: μ = k* α
    for (size_t i = 0; i < m; i++) {
      Float sum = 0.f;
      for (size_t j = 0; j < n; j++) {
        sum += k_star[i * n + j] * m_alpha_flat[j];
      }
      means[i] = sum;
    }

    // Compute k** (m x m): k**[i,j] = k(x_test[i], x_test[j])
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < m; j++) {
        cov_flat[i * m + j] = m_kernel->evaluate(X_test[i], X_test[j]);
      }
    }

    // Compute v = K^{-1} k*^T (n x m matrix)
    std::vector<Float> v_flat = cholesky_solve_matrix(m_L_flat, k_star, n, m);

    // Compute covariance: Σ = k** - k* K^{-1} k*^T = k** - k* v
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < m; j++) {
        Float dot = 0.f;
        for (size_t k = 0; k < n; k++) {
          dot += k_star[i * n + k] * v_flat[k * m + j];
        }
        cov_flat[i * m + j] = cov_flat[i * m + j] - dot;
      }
    }

    return {means, cov_flat};
  }

  /// Sample from posterior distribution at multiple points
  std::vector<Float> sample(const std::vector<Point3f> &X_test, PCG32 &rng) const {
    auto [mean, cov] = predict_full(X_test);
    size_t m = X_test.size();

    // Add small regularization for numerical stability
    for (size_t i = 0; i < m; ++i) {
      cov[i * m + i] = cov[i * m + i] + Float(1e-8f);
    }

    // Cholesky decomposition: Σ = L L^T
    std::vector<Float> L;
    try {
      L = cholesky_decompose(cov, m);
    } catch (...) {
      // If Cholesky fails, return mean (no randomness)
      return mean;
    }

    // Generate standard normal samples using DrJit RNG
    std::vector<Float> z(m);
    for (size_t i = 0; i < m; i++) {
      Float u1 = rng.next_float32();
      Float u2 = rng.next_float32();
      // Box-Muller transform for standard normal
      z[i] = dr::sqrt(-2.f * dr::log(u1)) * dr::cos(2.f * dr::Pi<Scalar> * u2);
    }

    // Transform: f = mean + L * z
    std::vector<Float> result(m);
    for (size_t i = 0; i < m; i++) {
      Float sum = mean[i];
      for (size_t j = 0; j < m; j++) {
        sum += L[i * m + j] * z[j];
      }
      result[i] = sum;
    }

    return result;
  }

  /// Get kernel
  const Kernel<Float, Spectrum> &kernel() const { return *m_kernel; }
  Kernel<Float, Spectrum> &kernel() { return *m_kernel; }

  /// Get number of observations
  size_t num_observations() const { return m_observations_x.size(); }

  /// Check if trained
  bool is_trained() const { return m_is_trained; }

  /// Get noise variance
  Scalar noise_variance() const { return m_noise_variance; }
  void set_noise_variance(Scalar variance) {
    m_noise_variance = variance;
    m_is_trained = false;
  }

private:
  std::shared_ptr<Kernel<Float, Spectrum>> m_kernel;
  Scalar m_noise_variance;

  // Observations
  std::vector<Point3f> m_observations_x;
  std::vector<Scalar> m_observations_y;

  // Cached for predictions
  bool m_is_trained;
  std::vector<Float> m_K_flat;      // Covariance matrix
  std::vector<Float> m_L_flat;      // Cholesky factor
  std::vector<Float> m_y_flat;      // Observation values
  std::vector<Float> m_alpha_flat;  // K^{-1} y

  /// Cholesky decomposition: A = L L^T (returns lower triangular L)
  static std::vector<Float> cholesky_decompose(const std::vector<Float> &A_flat, size_t n) {
    std::vector<Float> L_flat(n * n, Float(0.f));
    
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j <= i; ++j) {
        Float sum = 0.f;
        
        for (size_t k = 0; k < j; ++k) {
          sum += L_flat[i * n + k] * L_flat[j * n + k];
        }
        
        if (i == j) {
          Float A_ii = A_flat[i * n + i];
          L_flat[i * n + i] = dr::sqrt(dr::maximum(A_ii - sum, Float(1e-10f)));
        } else {
          Float A_ij = A_flat[i * n + j];
          Float L_jj = L_flat[j * n + j];
          L_flat[i * n + j] = (A_ij - sum) / dr::maximum(L_jj, Float(1e-10f));
        }
      }
    }
    
    return L_flat;
  }

  /// Forward substitution: solve L x = b
  static std::vector<Float> forward_substitution(const std::vector<Float> &L_flat, 
                                                   const std::vector<Float> &b_flat, 
                                                   size_t n) {
    std::vector<Float> x_flat(n);
    
    for (size_t i = 0; i < n; ++i) {
      Float sum = 0.f;
      for (size_t j = 0; j < i; ++j) {
        sum += L_flat[i * n + j] * x_flat[j];
      }
      Float L_ii = L_flat[i * n + i];
      x_flat[i] = (b_flat[i] - sum) / dr::maximum(L_ii, Float(1e-10f));
    }
    
    return x_flat;
  }

  /// Backward substitution: solve L^T x = b
  static std::vector<Float> backward_substitution(const std::vector<Float> &L_flat, 
                                                    const std::vector<Float> &b_flat, 
                                                    size_t n) {
    std::vector<Float> x_flat(n);
    
    for (int i = n - 1; i >= 0; --i) {
      Float sum = 0.f;
      for (size_t j = i + 1; j < n; ++j) {
        sum += L_flat[j * n + i] * x_flat[j];
      }
      Float L_ii = L_flat[i * n + i];
      x_flat[i] = (b_flat[i] - sum) / dr::maximum(L_ii, Float(1e-10f));
    }
    
    return x_flat;
  }

  /// Solve A x = b using Cholesky factor L
  static std::vector<Float> cholesky_solve(const std::vector<Float> &L_flat, 
                                            const std::vector<Float> &b_flat, 
                                            size_t n) {
    std::vector<Float> y_flat = forward_substitution(L_flat, b_flat, n);
    return backward_substitution(L_flat, y_flat, n);
  }

  /// Solve A X = B for matrix B (where B is n x m, returns n x m)
  static std::vector<Float> cholesky_solve_matrix(const std::vector<Float> &L_flat,
                                                    const std::vector<Float> &B_flat,
                                                    size_t n, size_t m) {
    std::vector<Float> X_flat(n * m);

    for (size_t col = 0; col < m; ++col) {
      // Extract column
      std::vector<Float> b_col(n);
      for (size_t row = 0; row < n; ++row) {
        b_col[row] = B_flat[row * m + col];
      }

      // Solve for this column
      std::vector<Float> x_col = cholesky_solve(L_flat, b_col, n);

      // Store result
      for (size_t row = 0; row < n; ++row) {
        X_flat[row * m + col] = x_col[row];
      }
    }

    return X_flat;
  }
};

} // namespace gpis

NAMESPACE_END(mitsuba)
