// The D3DX math compat layer, exercised by running it.
//
// WHY THIS IS THE HIGHEST-VALUE RUNNING TEST IN THE TREE
//
// src/External/dxsdk/Include/d3dx9.h replaces D3DX, which was removed from the Windows
// SDK. Every other shim in this port fails LOUDLY -- Chilkat returned an error, FMOD
// made no sound, GameBlocks reported not-connected. This one returns numbers. A wrong
// sign in a rotation, a transposed multiply, an inverse computed against the wrong
// convention: all of it compiles, links, runs, and produces a world that is subtly
// wrong in a way no assertion in the engine will catch.
//
// The blast radius is the whole product. D3DXMATRIX appears 864 times;
// GameObject::UpdateTransform (GameObj.h:305-318) builds every object's world matrix
// through D3DXMatrixTranslation / D3DXMatrixScaling and operator*. If this file is
// wrong, everything downstream of it is wrong, and the renderer phase would spend weeks
// bisecting for a bug that was always here.
//
// THE CONVENTION UNDER TEST
//
// D3DX is row-major storage, row-vector convention (v' = v * M), left-handed. The
// compat layer's own header says so and says not to "fix" it. That means:
//
//   - translation lives in the FOURTH ROW (_41 _42 _43), not the fourth column
//   - D3DXMatrixMultiply(out, A, B) composes A THEN B, so the rightmost matrix is
//     applied last -- the opposite of the column-vector convention most maths texts use
//
// Getting that backwards is the single most likely way to reimplement D3DX wrongly, so
// several tests below check the ORDER of composition rather than just the arithmetic.
//
// Expected values are derived by hand from the documented semantics, not captured from
// a run of this implementation. A test whose expectations came out of the code under
// test proves only that the code is deterministic.

#include "warz_test.h"

#include "d3dx9.h"

namespace {

// Row-major element access, so a test can say "row 3, column 0" and mean it.
float at(const D3DXMATRIX& m, int row, int col) { return m.m[row][col]; }

void check_matrix_equals(const D3DXMATRIX& got, const float (&want)[16], double eps = 1e-5)
{
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR_EPS(at(got, r, c), want[r * 4 + c], eps);
}

} // namespace

// ---------------------------------------------------------------------------
// Identity and storage order
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, identity_is_identity)
{
    D3DXMATRIX m;
    D3DXMatrixIdentity(&m);

    const float want[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    check_matrix_equals(m, want);
}

WARZ_TEST(d3dx_matrix, named_fields_alias_the_array_row_major)
{
    // _11.._44 and m[r][c] are a union. If the two ever disagree -- a column-major
    // "fix" being the obvious way for that to happen -- half the engine writes through
    // one spelling and reads through the other.
    D3DXMATRIX m;
    D3DXMatrixIdentity(&m);
    m._12 = 7.0f;
    m._21 = 9.0f;

    CHECK_NEAR(at(m, 0, 1), 7.0f);   // _12 is row 0, column 1
    CHECK_NEAR(at(m, 1, 0), 9.0f);   // _21 is row 1, column 0
}

// ---------------------------------------------------------------------------
// Translation and scaling -- the two the object transform path depends on
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, translation_lands_in_the_fourth_row)
{
    D3DXMATRIX m;
    D3DXMatrixTranslation(&m, 10.0f, 20.0f, 30.0f);

    // Row-vector convention: translation in the fourth ROW. In the column-vector
    // convention it would be the fourth column, and this is the assertion that pins
    // which one this codebase is in.
    CHECK_NEAR(m._41, 10.0f);
    CHECK_NEAR(m._42, 20.0f);
    CHECK_NEAR(m._43, 30.0f);
    CHECK_NEAR(m._44, 1.0f);

    CHECK_NEAR(m._14, 0.0f);
    CHECK_NEAR(m._24, 0.0f);
    CHECK_NEAR(m._34, 0.0f);
}

WARZ_TEST(d3dx_matrix, scaling_is_diagonal)
{
    D3DXMATRIX m;
    D3DXMatrixScaling(&m, 2.0f, 3.0f, 4.0f);

    const float want[16] = {
        2, 0, 0, 0,
        0, 3, 0, 0,
        0, 0, 4, 0,
        0, 0, 0, 1,
    };
    check_matrix_equals(m, want);
}

// ---------------------------------------------------------------------------
// Multiplication order
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, multiply_applies_left_operand_first)
{
    // Scale by 2, then translate by 10. In D3DX's row-vector convention that is
    // S * T, and a point at x=1 must land at 1*2 + 10 = 12.
    //
    // With the operands composed the other way round the point would land at
    // (1 + 10) * 2 = 22. Both are plausible-looking matrices; only one is D3DX.
    D3DXMATRIX s, t, st;
    D3DXMatrixScaling(&s, 2.0f, 2.0f, 2.0f);
    D3DXMatrixTranslation(&t, 10.0f, 0.0f, 0.0f);
    D3DXMatrixMultiply(&st, &s, &t);

    D3DXVECTOR3 p(1.0f, 0.0f, 0.0f);
    D3DXVECTOR3 out;
    D3DXVec3TransformCoord(&out, &p, &st);

    CHECK_NEAR(out.x, 12.0f);
    CHECK_NEAR(out.y, 0.0f);
    CHECK_NEAR(out.z, 0.0f);
}

WARZ_TEST(d3dx_matrix, multiply_by_identity_is_a_no_op)
{
    D3DXMATRIX a, id, out;
    D3DXMatrixRotationYawPitchRoll(&a, 0.3f, -0.7f, 1.1f);
    D3DXMatrixIdentity(&id);

    D3DXMatrixMultiply(&out, &a, &id);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR(at(out, r, c), at(a, r, c));

    D3DXMatrixMultiply(&out, &id, &a);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR(at(out, r, c), at(a, r, c));
}

WARZ_TEST(d3dx_matrix, multiply_aliases_safely)
{
    // The engine writes m = m * other in place. An implementation that accumulates into
    // the output as it goes -- rather than into a temporary -- reads back values it has
    // already overwritten, and the corruption depends on element order, which is the
    // kind of bug that survives casual testing.
    D3DXMATRIX a, b, want;
    D3DXMatrixRotationZ(&a, 0.5f);
    D3DXMatrixTranslation(&b, 1.0f, 2.0f, 3.0f);
    D3DXMatrixMultiply(&want, &a, &b);

    D3DXMATRIX inplace = a;
    D3DXMatrixMultiply(&inplace, &inplace, &b);
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR(at(inplace, r, c), at(want, r, c));
}

// ---------------------------------------------------------------------------
// Rotations
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, rotation_z_turns_x_towards_y)
{
    // Left-handed, +90 degrees about Z: the X axis goes to +Y.
    D3DXMATRIX m;
    D3DXMatrixRotationZ(&m, D3DX_PI / 2.0f);

    D3DXVECTOR3 x(1.0f, 0.0f, 0.0f), out;
    D3DXVec3TransformCoord(&out, &x, &m);

    CHECK_NEAR_EPS(out.x, 0.0f, 1e-6);
    CHECK_NEAR(out.y, 1.0f);
    CHECK_NEAR_EPS(out.z, 0.0f, 1e-6);
}

WARZ_TEST(d3dx_matrix, rotation_x_turns_y_towards_z)
{
    D3DXMATRIX m;
    D3DXMatrixRotationX(&m, D3DX_PI / 2.0f);

    D3DXVECTOR3 y(0.0f, 1.0f, 0.0f), out;
    D3DXVec3TransformCoord(&out, &y, &m);

    CHECK_NEAR_EPS(out.x, 0.0f, 1e-6);
    CHECK_NEAR_EPS(out.y, 0.0f, 1e-6);
    CHECK_NEAR(out.z, 1.0f);
}

WARZ_TEST(d3dx_matrix, rotation_y_turns_z_towards_x)
{
    D3DXMATRIX m;
    D3DXMatrixRotationY(&m, D3DX_PI / 2.0f);

    D3DXVECTOR3 z(0.0f, 0.0f, 1.0f), out;
    D3DXVec3TransformCoord(&out, &z, &m);

    CHECK_NEAR(out.x, 1.0f);
    CHECK_NEAR_EPS(out.y, 0.0f, 1e-6);
    CHECK_NEAR_EPS(out.z, 0.0f, 1e-6);
}

WARZ_TEST(d3dx_matrix, rotation_about_an_axis_matches_the_axis_specific_form)
{
    // D3DXMatrixRotationAxis about (0,0,1) must agree with D3DXMatrixRotationZ. Two
    // independent implementations of the same rotation; agreement is real evidence.
    D3DXMATRIX axis, z;
    D3DXVECTOR3 zaxis(0.0f, 0.0f, 1.0f);
    D3DXMatrixRotationAxis(&axis, &zaxis, 0.7f);
    D3DXMatrixRotationZ(&z, 0.7f);

    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            CHECK_NEAR(at(axis, r, c), at(z, r, c));
}

WARZ_TEST(d3dx_matrix, rotations_are_orthonormal)
{
    // A rotation's upper 3x3 must have unit-length, mutually perpendicular rows. This
    // catches a scale that leaked into a rotation, which is otherwise invisible until
    // meshes start growing every frame a transform is reapplied.
    D3DXMATRIX m;
    D3DXMatrixRotationYawPitchRoll(&m, 0.4f, 0.9f, -1.3f);

    for (int r = 0; r < 3; ++r) {
        const double len2 = double(at(m, r, 0)) * at(m, r, 0)
                          + double(at(m, r, 1)) * at(m, r, 1)
                          + double(at(m, r, 2)) * at(m, r, 2);
        CHECK_NEAR(len2, 1.0);
    }

    for (int a = 0; a < 3; ++a) {
        for (int b = a + 1; b < 3; ++b) {
            const double dot = double(at(m, a, 0)) * at(m, b, 0)
                             + double(at(m, a, 1)) * at(m, b, 1)
                             + double(at(m, a, 2)) * at(m, b, 2);
            CHECK_NEAR_EPS(dot, 0.0, 1e-6);
        }
    }
}

// ---------------------------------------------------------------------------
// Inverse
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, inverse_times_original_is_identity)
{
    D3DXMATRIX m, inv, prod;
    D3DXMatrixRotationYawPitchRoll(&m, 0.3f, 0.6f, -0.2f);
    m._41 = 5.0f;  m._42 = -3.0f;  m._43 = 11.0f;   // a real world matrix has translation

    float det = 0.0f;
    CHECK(D3DXMatrixInverse(&inv, &det, &m) != nullptr);
    CHECK_NEAR_EPS(det, 1.0f, 1e-4);   // rotation + translation preserves volume

    D3DXMatrixMultiply(&prod, &m, &inv);
    const float want[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1,
    };
    check_matrix_equals(prod, want, 1e-4);
}

WARZ_TEST(d3dx_matrix, inverse_of_a_scale_is_the_reciprocal_scale)
{
    D3DXMATRIX m, inv;
    D3DXMatrixScaling(&m, 2.0f, 4.0f, 8.0f);

    CHECK(D3DXMatrixInverse(&inv, nullptr, &m) != nullptr);
    CHECK_NEAR(inv._11, 0.5f);
    CHECK_NEAR(inv._22, 0.25f);
    CHECK_NEAR(inv._33, 0.125f);
}

WARZ_TEST(d3dx_matrix, singular_matrix_inverse_fails_rather_than_returning_garbage)
{
    // A zero scale is degenerate. D3DX returns NULL; an implementation that divides by
    // a zero determinant instead hands back infinities that propagate silently.
    D3DXMATRIX m, inv;
    D3DXMatrixScaling(&m, 1.0f, 0.0f, 1.0f);

    CHECK(D3DXMatrixInverse(&inv, nullptr, &m) == nullptr);
}

WARZ_TEST(d3dx_matrix, transpose_swaps_rows_and_columns)
{
    D3DXMATRIX m, t;
    D3DXMatrixIdentity(&m);
    m._12 = 2.0f;  m._13 = 3.0f;  m._41 = 4.0f;

    D3DXMatrixTranspose(&t, &m);
    CHECK_NEAR(t._21, 2.0f);
    CHECK_NEAR(t._31, 3.0f);
    CHECK_NEAR(t._14, 4.0f);
}

// ---------------------------------------------------------------------------
// Projection
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_matrix, perspective_fov_lh_maps_the_depth_range_to_zero_one)
{
    // D3D clip space is z in [0,1], unlike OpenGL's [-1,1]. A point on the near plane
    // must map to 0 and one on the far plane to 1. Getting this wrong inverts or
    // compresses the depth buffer, which looks like z-fighting rather than like a bug
    // in a matrix.
    const float znear = 1.0f, zfar = 100.0f;
    D3DXMATRIX proj;
    D3DXMatrixPerspectiveFovLH(&proj, D3DX_PI / 4.0f, 16.0f / 9.0f, znear, zfar);

    // Row-vector convention: the w that divides comes from the third row's _34.
    CHECK_NEAR(proj._34, 1.0f);
    CHECK_NEAR(proj._44, 0.0f);

    // A point at z = znear: clip z = z*_33 + _43, w = z. Expect z/w == 0.
    {
        const float z = znear;
        const float clip_z = z * proj._33 + proj._43;
        CHECK_NEAR_EPS(clip_z / z, 0.0f, 1e-5);
    }
    // A point at z = zfar: expect z/w == 1.
    {
        const float z = zfar;
        const float clip_z = z * proj._33 + proj._43;
        CHECK_NEAR_EPS(clip_z / z, 1.0f, 1e-5);
    }
}

WARZ_TEST(d3dx_matrix, ortho_lh_maps_the_depth_range_to_zero_one)
{
    const float znear = 2.0f, zfar = 50.0f;
    D3DXMATRIX m;
    D3DXMatrixOrthoLH(&m, 20.0f, 10.0f, znear, zfar);

    CHECK_NEAR(m._11, 2.0f / 20.0f);
    CHECK_NEAR(m._22, 2.0f / 10.0f);

    D3DXVECTOR3 pnear(0.0f, 0.0f, znear), pfar(0.0f, 0.0f, zfar), out;
    D3DXVec3TransformCoord(&out, &pnear, &m);
    CHECK_NEAR_EPS(out.z, 0.0f, 1e-5);
    D3DXVec3TransformCoord(&out, &pfar, &m);
    CHECK_NEAR_EPS(out.z, 1.0f, 1e-5);
}

WARZ_TEST(d3dx_matrix, look_at_lh_puts_the_eye_at_the_origin_looking_down_plus_z)
{
    D3DXVECTOR3 eye(0.0f, 0.0f, -10.0f), target(0.0f, 0.0f, 0.0f), up(0.0f, 1.0f, 0.0f);
    D3DXMATRIX view;
    D3DXMatrixLookAtLH(&view, &eye, &target, &up);

    // The eye maps to the origin.
    D3DXVECTOR3 out;
    D3DXVec3TransformCoord(&out, &eye, &view);
    CHECK_NEAR_EPS(out.x, 0.0f, 1e-5);
    CHECK_NEAR_EPS(out.y, 0.0f, 1e-5);
    CHECK_NEAR_EPS(out.z, 0.0f, 1e-5);

    // The target sits 10 units down +Z -- left-handed, so forward is +Z, not -Z.
    D3DXVec3TransformCoord(&out, &target, &view);
    CHECK_NEAR(out.z, 10.0f);
}

// ---------------------------------------------------------------------------
// Vectors
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_vector, transform_coord_divides_by_w_and_transform_normal_ignores_translation)
{
    // The distinction that matters: a position is affected by translation, a direction
    // is not. Using one where the other belongs is a classic source of lighting that
    // moves with the camera.
    D3DXMATRIX t;
    D3DXMatrixTranslation(&t, 5.0f, 0.0f, 0.0f);

    D3DXVECTOR3 v(1.0f, 0.0f, 0.0f), out;

    D3DXVec3TransformCoord(&out, &v, &t);
    CHECK_NEAR(out.x, 6.0f);

    D3DXVec3TransformNormal(&out, &v, &t);
    CHECK_NEAR(out.x, 1.0f);
}

WARZ_TEST(d3dx_vector, cross_product_is_right_handed_in_component_terms)
{
    // x cross y == z. This is the component-wise definition D3DX uses regardless of
    // the handedness of the coordinate system.
    D3DXVECTOR3 x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f), out;
    D3DXVec3Cross(&out, &x, &y);

    CHECK_NEAR(out.x, 0.0f);
    CHECK_NEAR(out.y, 0.0f);
    CHECK_NEAR(out.z, 1.0f);

    // Anti-commutative.
    D3DXVec3Cross(&out, &y, &x);
    CHECK_NEAR(out.z, -1.0f);
}

WARZ_TEST(d3dx_vector, dot_and_length)
{
    D3DXVECTOR3 a(3.0f, 4.0f, 0.0f), b(1.0f, 0.0f, 0.0f);
    CHECK_NEAR(D3DXVec3Dot(&a, &b), 3.0f);
    CHECK_NEAR(D3DXVec3Length(&a), 5.0f);
}

WARZ_TEST(d3dx_vector, normalize_yields_unit_length)
{
    D3DXVECTOR3 v(3.0f, 4.0f, 0.0f), out;
    D3DXVec3Normalize(&out, &v);

    CHECK_NEAR(D3DXVec3Length(&out), 1.0f);
    CHECK_NEAR(out.x, 0.6f);
    CHECK_NEAR(out.y, 0.8f);
}

WARZ_TEST(d3dx_vector, normalizing_a_zero_vector_does_not_produce_nan)
{
    // Zero-length normals reach this from degenerate geometry. D3DX returns zero
    // rather than dividing; a NaN here would spread through every subsequent
    // computation and turn a single bad triangle into an invisible mesh.
    D3DXVECTOR3 zero(0.0f, 0.0f, 0.0f), out(9.0f, 9.0f, 9.0f);
    D3DXVec3Normalize(&out, &zero);

    CHECK(!std::isnan(out.x));
    CHECK(!std::isnan(out.y));
    CHECK(!std::isnan(out.z));
}

WARZ_TEST(d3dx_vector, minimize_and_maximize_are_componentwise)
{
    // These build bounding boxes. A swapped pair silently inverts every AABB in the
    // level, and an inverted AABB culls its own contents.
    D3DXVECTOR3 a(1.0f, 5.0f, -2.0f), b(3.0f, 2.0f, 7.0f), out;

    D3DXVec3Minimize(&out, &a, &b);
    CHECK_NEAR(out.x, 1.0f);
    CHECK_NEAR(out.y, 2.0f);
    CHECK_NEAR(out.z, -2.0f);

    D3DXVec3Maximize(&out, &a, &b);
    CHECK_NEAR(out.x, 3.0f);
    CHECK_NEAR(out.y, 5.0f);
    CHECK_NEAR(out.z, 7.0f);
}

// ---------------------------------------------------------------------------
// Quaternions
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_quaternion, rotation_matrix_round_trips)
{
    // Matrix -> quaternion -> matrix must return the original. The matrix-to-quaternion
    // conversion picks one of four branches depending on which diagonal element is
    // largest, and a bug in a rarely-taken branch survives casual testing -- so this
    // runs a spread of angles that lands in different branches.
    const float angles[][3] = {
        { 0.0f,  0.0f,  0.0f},
        { 0.3f,  0.6f, -0.2f},
        { 3.0f,  0.1f,  0.1f},   // large yaw: a different diagonal dominates
        { 0.1f,  3.0f,  0.1f},
        { 0.1f,  0.1f,  3.0f},
        {-2.5f, -2.5f,  2.5f},
    };

    for (const auto& a : angles) {
        D3DXMATRIX m, back;
        D3DXMatrixRotationYawPitchRoll(&m, a[0], a[1], a[2]);

        D3DXQUATERNION q;
        D3DXQuaternionRotationMatrix(&q, &m);
        D3DXMatrixRotationQuaternion(&back, &q);

        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                CHECK_NEAR_EPS(at(back, r, c), at(m, r, c), 1e-4);
    }
}

WARZ_TEST(d3dx_quaternion, slerp_endpoints_are_exact)
{
    D3DXQUATERNION a, b, out;
    D3DXQuaternionRotationYawPitchRoll(&a, 0.0f, 0.0f, 0.0f);
    D3DXQuaternionRotationYawPitchRoll(&b, 1.2f, 0.0f, 0.0f);

    D3DXQuaternionSlerp(&out, &a, &b, 0.0f);
    CHECK_NEAR_EPS(out.x, a.x, 1e-5);
    CHECK_NEAR_EPS(out.y, a.y, 1e-5);
    CHECK_NEAR_EPS(out.z, a.z, 1e-5);
    CHECK_NEAR_EPS(out.w, a.w, 1e-5);

    D3DXQuaternionSlerp(&out, &a, &b, 1.0f);
    CHECK_NEAR_EPS(out.x, b.x, 1e-5);
    CHECK_NEAR_EPS(out.w, b.w, 1e-5);
}

WARZ_TEST(d3dx_quaternion, slerp_stays_on_the_unit_sphere)
{
    // Interpolated rotations must stay normalised. A slerp that drifts off the unit
    // sphere scales the mesh it is applied to, a little more at every keyframe -- the
    // characteristic "growing character" animation bug.
    D3DXQUATERNION a, b, out;
    D3DXQuaternionRotationYawPitchRoll(&a, 0.2f, -0.4f, 0.9f);
    D3DXQuaternionRotationYawPitchRoll(&b, 2.1f,  0.8f, -1.4f);

    for (int i = 0; i <= 10; ++i) {
        const float t = float(i) / 10.0f;
        D3DXQuaternionSlerp(&out, &a, &b, t);
        const double len2 = double(out.x) * out.x + double(out.y) * out.y
                          + double(out.z) * out.z + double(out.w) * out.w;
        CHECK_NEAR_EPS(len2, 1.0, 1e-4);
    }
}

WARZ_TEST(d3dx_quaternion, inverse_of_a_unit_quaternion_is_its_conjugate)
{
    D3DXQUATERNION q, inv;
    D3DXQuaternionRotationYawPitchRoll(&q, 0.5f, 0.25f, -0.75f);
    D3DXQuaternionInverse(&inv, &q);

    CHECK_NEAR_EPS(inv.x, -q.x, 1e-5);
    CHECK_NEAR_EPS(inv.y, -q.y, 1e-5);
    CHECK_NEAR_EPS(inv.z, -q.z, 1e-5);
    CHECK_NEAR_EPS(inv.w,  q.w, 1e-5);
}

// ---------------------------------------------------------------------------
// Planes -- used by frustum culling, so a sign error here culls the wrong half
// ---------------------------------------------------------------------------

WARZ_TEST(d3dx_plane, from_point_normal_gives_signed_distance)
{
    // Plane through (0,5,0) with normal +Y. Points above are positive, below negative.
    D3DXPLANE p;
    D3DXVECTOR3 point(0.0f, 5.0f, 0.0f), normal(0.0f, 1.0f, 0.0f);
    D3DXPlaneFromPointNormal(&p, &point, &normal);

    D3DXVECTOR3 above(0.0f, 8.0f, 0.0f), below(0.0f, 1.0f, 0.0f), on(3.0f, 5.0f, -2.0f);
    CHECK_NEAR(D3DXPlaneDotCoord(&p, &above),  3.0f);
    CHECK_NEAR(D3DXPlaneDotCoord(&p, &below), -4.0f);
    CHECK_NEAR_EPS(D3DXPlaneDotCoord(&p, &on), 0.0f, 1e-5);
}

WARZ_TEST(d3dx_plane, from_three_points_agrees_with_point_normal)
{
    // Two independent constructions of the same plane, z = 0 with normal +Z.
    D3DXVECTOR3 a(0.0f, 0.0f, 0.0f), b(1.0f, 0.0f, 0.0f), c(0.0f, 1.0f, 0.0f);
    D3DXPLANE from_points;
    D3DXPlaneFromPoints(&from_points, &a, &b, &c);

    D3DXVECTOR3 probe(0.0f, 0.0f, 4.0f);
    const float d = D3DXPlaneDotCoord(&from_points, &probe);

    // Magnitude 4 either way; the sign depends on the winding convention, and pinning
    // magnitude alone keeps this test about the construction rather than the winding.
    CHECK_NEAR(std::fabs(d), 4.0f);

    // The plane must be normalised: |normal| == 1, or every distance it reports is
    // scaled and frustum culling silently gains or loses margin.
    const double len = std::sqrt(double(from_points.a) * from_points.a
                              + double(from_points.b) * from_points.b
                              + double(from_points.c) * from_points.c);
    CHECK_NEAR(len, 1.0);
}
