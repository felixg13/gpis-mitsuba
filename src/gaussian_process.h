#pragma once

#include <drjit/tensor.h>
#include <memory>
#include <mitsuba/core/random.h>
#include <mitsuba/mitsuba.h>
#include <vector>

NAMESPACE_BEGIN(mitsuba)

namespace gpis {

// Kernel now templated on Value type
template <typename Value> class Kernel {
public:
  using Scalar = dr::scalar_t<Value>;
  using Point3 = mitsuba::Point<Value, 3>;

  virtual ~Kernel() = default;

  virtual Value evaluate(const Point3 &x, const Point3 &y) const = 0;

  virtual std::vector<Scalar> get_params() const = 0;
  virtual void set_params(const std::vector<Scalar> &params) = 0;
};

template <typename Value>
class SquaredExponentialKernel : public Kernel<Value> {
public:
  using Scalar = dr::scalar_t<Value>;
  using Point3 = mitsuba::Point<Value, 3>;
  using Base = Kernel<Value>;

  SquaredExponentialKernel(Scalar lengthscale = 1.0,
                           Scalar signal_variance = 1.0)
      : m_lengthscale(lengthscale), m_signal_variance(signal_variance) {}

  Value evaluate(const Point3 &x, const Point3 &y) const override {
    Value dist_sq = dr::squared_norm(x - y);
    Scalar l_sq = m_lengthscale * m_lengthscale;
    return Value(m_signal_variance) * dr::exp(-dist_sq / Value(2.f * l_sq));
  }

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

// Main GP class templated on Value type
template <typename Value> class GaussianProcess {
public:
  using Scalar = dr::scalar_t<Value>;
  using Point3 = mitsuba::Point<Value, 3>;
  using UInt32 = dr::uint32_array_t<Value>;
  using PCG32 = dr::PCG32<UInt32>;

  GaussianProcess(std::shared_ptr<Kernel<Value>> kernel,
                  Scalar noise_variance = 1e-6, uint64_t seed = 0)
      : m_kernel(kernel), m_noise_variance(noise_variance), m_is_trained(false),
        m_rng(PCG32(seed)) {}

  void add_observation(const Point3 &x, Scalar y) {
    m_observations_x.push_back(x);
    m_observations_y.push_back(y);
    m_is_trained = false;
  }

  void add_observations(const std::vector<Point3> &X,
                        const std::vector<Scalar> &y) {
    for (size_t i = 0; i < X.size(); i++) {
      m_observations_x.push_back(X[i]);
      m_observations_y.push_back(y[i]);
    }
    m_is_trained = false;
  }

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

    m_K_flat.resize(n * n);
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < n; ++j) {
        m_K_flat[i * n + j] =
            m_kernel->evaluate(m_observations_x[i], m_observations_x[j]);
      }
    }

    for (size_t i = 0; i < n; ++i) {
      m_K_flat[i * n + i] = m_K_flat[i * n + i] + Value(m_noise_variance);
    }

    m_y_flat.resize(n);
    for (size_t i = 0; i < n; ++i) {
      m_y_flat[i] = Value(m_observations_y[i]);
    }

    m_L_flat = cholesky_decompose(m_K_flat, n);
    m_alpha_flat = cholesky_solve(m_L_flat, m_y_flat, n);
    m_is_trained = true;
  }

  Value predict_mean(const Point3 &x) const {
    if (!m_is_trained || m_observations_x.empty()) {
      return Value(0.f);
    }

    size_t n = m_observations_x.size();

    std::vector<Value> k_star(n);
    for (size_t i = 0; i < n; i++) {
      k_star[i] = m_kernel->evaluate(x, m_observations_x[i]);
    }

    Value result = Value(0.f);
    for (size_t i = 0; i < n; i++) {
      result = result + k_star[i] * m_alpha_flat[i];
    }
    return result;
  }

  Value predict_variance(const Point3 &x) const {
    if (!m_is_trained || m_observations_x.empty()) {
      return m_kernel->evaluate(x, x);
    }

    size_t n = m_observations_x.size();

    std::vector<Value> k_star(n);
    for (size_t i = 0; i < n; i++) {
      k_star[i] = m_kernel->evaluate(x, m_observations_x[i]);
    }

    Value k_star_star = m_kernel->evaluate(x, x);
    std::vector<Value> v = cholesky_solve(m_L_flat, k_star, n);

    Value dot_product = Value(0.f);
    for (size_t i = 0; i < n; i++) {
      dot_product = dot_product + k_star[i] * v[i];
    }

    return k_star_star - dot_product;
  }

  std::pair<std::vector<Value>, std::vector<Value>>
  predict_full(const std::vector<Point3> &X_test) const {
    size_t m = X_test.size();
    size_t n = m_observations_x.size();

    std::vector<Value> means(m);
    std::vector<Value> cov_flat(m * m);

    if (!m_is_trained || n == 0) {
      for (size_t i = 0; i < m; i++) {
        means[i] = Value(0.f);
        for (size_t j = 0; j < m; j++) {
          cov_flat[i * m + j] = m_kernel->evaluate(X_test[i], X_test[j]);
        }
      }
      return {means, cov_flat};
    }

    std::vector<Value> k_star(m * n);
    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < n; j++) {
        k_star[i * n + j] = m_kernel->evaluate(X_test[i], m_observations_x[j]);
      }
    }

    for (size_t i = 0; i < m; i++) {
      Value sum = Value(0.f);
      for (size_t j = 0; j < n; j++) {
        sum = sum + k_star[i * n + j] * m_alpha_flat[j];
      }
      means[i] = sum;
    }

    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < m; j++) {
        cov_flat[i * m + j] = m_kernel->evaluate(X_test[i], X_test[j]);
      }
    }

    std::vector<Value> v_flat = cholesky_solve_matrix(m_L_flat, k_star, n, m);

    for (size_t i = 0; i < m; i++) {
      for (size_t j = 0; j < m; j++) {
        Value dot = Value(0.f);
        for (size_t k = 0; k < n; k++) {
          dot = dot + k_star[i * n + k] * v_flat[k * m + j];
        }
        cov_flat[i * m + j] = cov_flat[i * m + j] - dot;
      }
    }

    return {means, cov_flat};
  }

  template <typename FloatP>
  FloatP sample(const mitsuba::Point<FloatP, 3> &X,
                dr::mask_t<FloatP> active = true) const {
    return dr::select(active, X.x(), FloatP(0));
  }

  const Kernel<Value> &kernel() const { return *m_kernel; }
  Kernel<Value> &kernel() { return *m_kernel; }

  size_t num_observations() const { return m_observations_x.size(); }
  bool is_trained() const { return m_is_trained; }

  Scalar noise_variance() const { return m_noise_variance; }
  void set_noise_variance(Scalar variance) {
    m_noise_variance = variance;
    m_is_trained = false;
  }

private:
  std::shared_ptr<Kernel<Value>> m_kernel;
  Scalar m_noise_variance;
  PCG32 m_rng;

  std::vector<Point3> m_observations_x;
  std::vector<Scalar> m_observations_y;

  bool m_is_trained;
  std::vector<Value> m_K_flat;
  std::vector<Value> m_L_flat;
  std::vector<Value> m_y_flat;
  std::vector<Value> m_alpha_flat;

  static std::vector<Value> cholesky_decompose(const std::vector<Value> &A_flat,
                                               size_t n) {
    std::vector<Value> L_flat(n * n, Value(0.f));

    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j <= i; ++j) {
        Value sum = Value(0.f);

        for (size_t k = 0; k < j; ++k) {
          sum = sum + L_flat[i * n + k] * L_flat[j * n + k];
        }

        if (i == j) {
          Value A_ii = A_flat[i * n + i];
          L_flat[i * n + i] = dr::sqrt(dr::maximum(A_ii - sum, Value(1e-10f)));
        } else {
          Value A_ij = A_flat[i * n + j];
          Value L_jj = L_flat[j * n + j];
          L_flat[i * n + j] = (A_ij - sum) / dr::maximum(L_jj, Value(1e-10f));
        }
      }
    }

    return L_flat;
  }

  static std::vector<Value>
  forward_substitution(const std::vector<Value> &L_flat,
                       const std::vector<Value> &b_flat, size_t n) {
    std::vector<Value> x_flat(n);

    for (size_t i = 0; i < n; ++i) {
      Value sum = Value(0.f);
      for (size_t j = 0; j < i; ++j) {
        sum = sum + L_flat[i * n + j] * x_flat[j];
      }
      Value L_ii = L_flat[i * n + i];
      x_flat[i] = (b_flat[i] - sum) / dr::maximum(L_ii, Value(1e-10f));
    }

    return x_flat;
  }

  static std::vector<Value>
  backward_substitution(const std::vector<Value> &L_flat,
                        const std::vector<Value> &b_flat, size_t n) {
    std::vector<Value> x_flat(n);

    for (int i = n - 1; i >= 0; --i) {
      Value sum = Value(0.f);
      for (size_t j = i + 1; j < n; ++j) {
        sum = sum + L_flat[j * n + i] * x_flat[j];
      }
      Value L_ii = L_flat[i * n + i];
      x_flat[i] = (b_flat[i] - sum) / dr::maximum(L_ii, Value(1e-10f));
    }

    return x_flat;
  }

  static std::vector<Value> cholesky_solve(const std::vector<Value> &L_flat,
                                           const std::vector<Value> &b_flat,
                                           size_t n) {
    std::vector<Value> y_flat = forward_substitution(L_flat, b_flat, n);
    return backward_substitution(L_flat, y_flat, n);
  }

  static std::vector<Value>
  cholesky_solve_matrix(const std::vector<Value> &L_flat,
                        const std::vector<Value> &B_flat, size_t n, size_t m) {
    std::vector<Value> X_flat(n * m);

    for (size_t col = 0; col < m; ++col) {
      std::vector<Value> b_col(n);
      for (size_t row = 0; row < n; ++row) {
        b_col[row] = B_flat[row * m + col];
      }

      std::vector<Value> x_col = cholesky_solve(L_flat, b_col, n);

      for (size_t row = 0; row < n; ++row) {
        X_flat[row * m + col] = x_col[row];
      }
    }

    return X_flat;
  }
};

} // namespace gpis

NAMESPACE_END(mitsuba)
