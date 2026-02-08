// src/shapes/my_shape.cpp
#include <mitsuba/core/properties.h>
#include <mitsuba/core/warp.h>
#include <mitsuba/render/shape.h>

NAMESPACE_BEGIN(mitsuba)

template <typename Float, typename Spectrum>
class MyShape final : public Shape<Float, Spectrum> {
public:
  MI_IMPORT_BASE(Shape, m_to_world, m_to_object)
  MI_IMPORT_TYPES()

  using typename Base::ScalarIndex;
  using typename Base::ScalarSize;

  MyShape(const Properties &props) : Base(props) {
    // Get sphere radius from properties (default = 1.0)
    m_radius = props.get<ScalarFloat>("radius", 1.f);

    // Initialize transforms if not already set
    if (props.has_property("to_world"))
      set_children();

    // Mark as emitter if needed
    m_is_emitter = false;
  }

  ScalarBoundingBox3f bbox() const override {
    ScalarBoundingBox3f bbox;
    ScalarPoint3f center = m_to_world.scalar().translation();
    ScalarVector3f extent(m_radius);
    bbox.min = center - extent;
    bbox.max = center + extent;
    return bbox;
  }

  ScalarFloat surface_area() const override {
    return 4.f * dr::Pi<ScalarFloat> * m_radius * m_radius;
  }

  // Sample a point on the sphere surface
  PositionSample3f sample_position(Float time, const Point2f &sample,
                                   Mask active) const override {
    MI_MASK_ARGUMENT(active);

    PositionSample3f ps = dr::zeros<PositionSample3f>();

    // Sample uniformly on unit sphere
    Vector3f local_p = warp::square_to_uniform_sphere(sample);

    // Scale by radius
    local_p *= m_radius;

    // Transform to world space
    ps.p = m_to_world.value().transform_affine(local_p);
    ps.n = dr::normalize(m_to_world.value().transform_affine(local_p));
    ps.time = time;
    ps.delta = false;
    ps.pdf = 1.f / surface_area();

    return ps;
  }

  Float pdf_position(const PositionSample3f & /*ps*/,
                     Mask active) const override {
    MI_MASK_ARGUMENT(active);
    return 1.f / surface_area();
  }

  // Ray-sphere intersection
  PreliminaryIntersection3f
  ray_intersect_preliminary(const Ray3f &ray, Mask active) const override {
    MI_MASK_ARGUMENT(active);

    PreliminaryIntersection3f pi = dr::zeros<PreliminaryIntersection3f>();

    // Transform ray to object space
    Ray3f ray_o = m_to_object.value().transform_affine(ray);

    // Ray-sphere intersection (sphere at origin with radius m_radius)
    Float a = dr::squared_norm(ray_o.d);
    Float b = 2.f * dr::dot(ray_o.o, ray_o.d);
    Float c = dr::squared_norm(ray_o.o) - m_radius * m_radius;

    auto [solution_found, t0, t1] = math::solve_quadratic(a, b, c);

    // Select closer intersection that's positive
    Mask valid_t0 = solution_found && t0 > ray_o.mint && t0 < ray_o.maxt;
    Mask valid_t1 = solution_found && t1 > ray_o.mint && t1 < ray_o.maxt;

    Float t = dr::select(valid_t0, t0, dr::Infinity<Float>);
    t = dr::select(valid_t1 && (t1 < t), t1, t);

    active &= dr::isfinite(t);

    pi.t = dr::select(active, t, dr::Infinity<Float>);
    pi.shape = this;
    pi.prim_index = 0;

    return pi;
  }

  Mask ray_test(const Ray3f &ray, Mask active) const override {
    MI_MASK_ARGUMENT(active);

    // Transform ray to object space
    Ray3f ray_o = m_to_object.value().transform_affine(ray);

    // Ray-sphere intersection test
    Float a = dr::squared_norm(ray_o.d);
    Float b = 2.f * dr::dot(ray_o.o, ray_o.d);
    Float c = dr::squared_norm(ray_o.o) - m_radius * m_radius;

    auto [solution_found, t0, t1] = math::solve_quadratic(a, b, c);

    Mask hit = solution_found && ((t0 > ray_o.mint && t0 < ray_o.maxt) ||
                                  (t1 > ray_o.mint && t1 < ray_o.maxt));

    return active && hit;
  }

  SurfaceInteraction3f compute_surface_interaction(
      const Ray3f &ray, const PreliminaryIntersection3f &pi, uint32_t ray_flags,
      uint32_t recursion_depth, Mask active) const override {
    MI_MASK_ARGUMENT(active);
    constexpr bool IsDiff = dr::is_diff_v<Float>;

    SurfaceInteraction3f si = dr::zeros<SurfaceInteraction3f>();

    // Compute hit point
    si.t = dr::select(active, pi.t, dr::Infinity<Float>);
    si.p = ray(si.t);

    // Transform hit point to object space to compute normal
    Point3f p_local = m_to_object.value().transform_affine(si.p);

    // Normal is just the normalized position vector (sphere at origin)
    Vector3f n_local = dr::normalize(p_local);

    // Transform normal to world space
    si.n = dr::normalize(m_to_world.value().transform_affine(n_local));
    si.sh_frame.n = si.n;

    // Compute tangent frame
    auto [s, t] = coordinate_system(si.n);
    si.dp_du = s;
    si.dp_dv = t;

    // Compute UV coordinates (spherical mapping)
    Float theta = dr::safe_acos(n_local.z());
    Float phi = dr::atan2(n_local.y(), n_local.x());
    si.uv = Point2f(phi * dr::InvTwoPi<Float> + 0.5f, theta * dr::InvPi<Float>);

    si.time = ray.time;
    si.wavelengths = ray.wavelengths;
    si.shape = this;
    si.prim_index = 0;

    // Store color based on normal (map from [-1,1] to [0,1])
    if constexpr (dr::is_cuda_v<Float>) {
      // For GPU variant
      si.duv_dx = dr::zeros<Point2f>();
      si.duv_dy = dr::zeros<Point2f>();
    }

    return si;
  }

  std::string to_string() const override {
    std::ostringstream oss;
    oss << "MyShape[" << std::endl
        << "  radius = " << m_radius << "," << std::endl
        << "  surface_area = " << surface_area() << "," << std::endl
        << "  to_world = " << string::indent(m_to_world) << std::endl
        << "]";
    return oss.str();
  }

  MI_DECLARE_CLASS()

private:
  ScalarFloat m_radius;
};

MI_IMPLEMENT_CLASS_VARIANT(MyShape, Shape)
MI_EXPORT_PLUGIN(MyShape, "My custom sphere shape")

NAMESPACE_END(mitsuba)
