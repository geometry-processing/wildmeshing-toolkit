
#include "TetWild.h"

#include <wmtk/utils/AMIPS.h>
#include <limits>
#include <wmtk/utils/Logger.hpp>
#include <wmtk/utils/TetraQualityUtils.hpp>
#include <wmtk/utils/io.hpp>

#include <igl/predicates/predicates.h>
#include <spdlog/fmt/ostr.h>

#include <igl/winding_number.h>

using std::cout;
using std::endl;

void pausee()
{
    std::cout << "pausing..." << std::endl;
    char c;
    std::cin >> c;
    if (c == '0') exit(0);
}
void tetwild::TetWild::mesh_improvement(int max_its)
{
    ////preprocessing
    wmtk::logger().info("========it pre========");
    local_operations({{0, 1, 0, 0}}, false);

    ////operation loops
    bool is_hit_min_edge_length = false;
    const int M = 3;
    int m = 0;
    double pre_max_energy = 0., pre_avg_energy = 0.;
    for (int it = 0; it < max_its; it++) {
        ///ops
        wmtk::logger().info("========it {}========", it);
        auto [max_energy, avg_energy] = local_operations({{1, 2, 1, 1}});

        ///energy check
        std::cout<<max_energy <<" "<< m_params.stop_energy<<std::endl;
        if (max_energy < m_params.stop_energy) break;

        ///sizing field
        if (it > 0 && (max_energy > 1e3 || pre_max_energy - max_energy < 1e-1 && pre_avg_energy - avg_energy < 1e-2)) {
            m++;
            if (m == M) {
                wmtk::logger().info("adjust_sizing_field...");
                is_hit_min_edge_length = adjust_sizing_field(max_energy);
                m = 0;
            }
        } else
            m = 0;
        if(is_hit_min_edge_length){
            //todo: maybe to do sth
        }
        pre_max_energy = max_energy;
        pre_avg_energy = avg_energy;
    }

    const auto& vs = get_vertices();
    for (auto& v : vs) m_vertex_attribute[v.vid(*this)].m_scalar = 1;
    wmtk::logger().info("========it post========");
    local_operations({{0, 1, 0, 0}});

    ////winding number
    filter_outside();
}

#include <igl/Timer.h>
std::tuple<double, double> tetwild::TetWild::local_operations(const std::array<int, 4>& ops, bool collapse_limite_length)
{
    igl::Timer timer;

    std::tuple<double, double> energy;

    for (int i = 0; i < ops.size(); i++) {
        timer.start();
        if (i == 0) {
            for (int n = 0; n < ops[i]; n++) {
                wmtk::logger().info("==splitting {}==", n);
                split_all_edges();
            }
        } else if (i == 1) {
            for (int n = 0; n < ops[i]; n++) {
                wmtk::logger().info("==collapsing {}==", n);
                collapse_all_edges();
            }
        } else if (i == 2) {
            for (int n = 0; n < ops[i]; n++) {
                wmtk::logger().info("==swapping {}==", n);
                swap_all_edges();
                swap_all_faces();
            }
        } else if (i == 3) {
            for (int n = 0; n < ops[i]; n++) {
                wmtk::logger().info("==smoothing {}==", n);
                smooth_all_vertices();
            }
        }

        if (ops[i] > 0) {
            wmtk::logger().info("#t {}", tet_size());
            wmtk::logger().info("#v {}", vertex_size());
            energy = get_max_avg_energy();
            wmtk::logger().info("max energy = {}", std::get<0>(energy));
            wmtk::logger().info("avg energy = {}", std::get<1>(energy));
            wmtk::logger().info("time = {}", timer.getElapsedTime());
        }
    }

    check_attributes(); // fortest

    return energy;
}

bool tetwild::TetWild::adjust_sizing_field(double max_energy)
{
    const auto& vertices = get_vertices();
    const auto& tets = get_tets(); // todo: avoid copy!!!

    static const Scalar stop_filter_energy = m_params.stop_energy * 0.8;
    Scalar filter_energy =
        max_energy / 100 > stop_filter_energy ? max_energy / 100 : stop_filter_energy;
    if (filter_energy > 100) filter_energy = 100;

    Scalar recover_scalar = 1.5;
//    std::vector<Scalar> scale_multipliers(vertices.size(), recover_scalar);
    std::vector<Scalar> scale_multipliers(m_vertex_attribute.size(), recover_scalar);
    Scalar refine_scalar = 0.5;
    Scalar min_refine_scalar = m_params.l_min / m_params.l;

    const double R = m_params.l * 2;
    for (size_t i = 0; i < tets.size(); i++) {
        int tid = tets[i].tid(*this);
        if (m_tet_attribute[tid].m_quality < filter_energy) continue;

        std::map<size_t, double> new_scalars;
        //
        std::queue<size_t> v_queue;
        auto vs = oriented_tet_vertices(tets[i]);
        Vector3d c(0, 0, 0);
        for (int j = 0; j < 4; j++) {
            v_queue.push(vs[j].vid(*this));
            c += m_vertex_attribute[vs[j].vid(*this)].m_posf;
        }
        c /= 4;
        //
        while (!v_queue.empty()) {
            size_t vid = v_queue.front();
            v_queue.pop();

            bool is_close = false;
            double dist = (m_vertex_attribute[vid].m_posf - c).norm();
            if (dist > R) {
                new_scalars[vid] = 0;
            } else {
                new_scalars[vid] = (1 + dist / R) * refine_scalar;//linear interpolate
                is_close = true;
            }

            if (!is_close) continue;

            auto vids = get_one_ring_vids_for_vertex(vid);
            for (size_t n_vid : vids) {
                if (new_scalars.count(n_vid)) continue;
                v_queue.push(n_vid);
            }
        }

        for (auto& info : new_scalars) {
            if (info.second == 0) continue;

            size_t vid = info.first;
            double scalar = info.second;
            if (scalar < scale_multipliers[vid]) scale_multipliers[vid] = scalar;
        }
    }

    bool is_hit_min_edge_length = false;
    for (size_t i = 0; i < vertices.size(); i++) {
        size_t vid = vertices[i].vid(*this);
        auto& v_attr = m_vertex_attribute[vid];

        Scalar new_scale = v_attr.m_sizing_scalar * scale_multipliers[vid];
        if (new_scale > 1)
            v_attr.m_sizing_scalar = 1;
        else if (new_scale < min_refine_scalar) {
            is_hit_min_edge_length = true;
            v_attr.m_sizing_scalar = min_refine_scalar;
        } else
            v_attr.m_sizing_scalar = new_scale;
    }

    return is_hit_min_edge_length;
}

void tetwild::TetWild::filter_outside(bool remove_ouside)
{
    Eigen::MatrixXd V(triangle_insertion_cache.input_surface.vertices.size(), 3);
    Eigen::MatrixXi F(triangle_insertion_cache.input_surface.faces.size(), 3);

    for (int i = 0; i < V.rows(); i++) {
        V.row(i) = triangle_insertion_cache.input_surface.vertices[i];
    }
    for (int i = 0; i < F.rows(); i++) {
        F.row(i) << triangle_insertion_cache.input_surface.faces[i][0],
            triangle_insertion_cache.input_surface.faces[i][1],
            triangle_insertion_cache.input_surface.faces[i][2];
    }

    const auto& tets = get_tets(); // todo: avoid copy!!!
    Eigen::MatrixXd C(tets.size(), 3);
    for (size_t i = 0; i < tets.size(); i++) {
        C.row(i) << 0, 0, 0;
        auto vs = oriented_tet_vertices(tets[i]);
        for (auto& v : vs) C.row(i) += m_vertex_attribute[v.vid(*this)].m_posf;
        C.row(i) /= 4;
    }

    Eigen::VectorXd W;
    igl::winding_number(V, F, C, W);

    std::vector<size_t> rm_tids;
    for (int i = 0; i < W.rows(); i++) {
        if (W(i) <= 0.5){
            m_tet_attribute[tets[i].tid(*this)].m_is_outside = true;
            if(remove_ouside)
                rm_tids.push_back(tets[i].tid(*this));
        }
    }

    if(remove_ouside)
        remove_tets_by_ids(rm_tids);
}

/////////////////////////////////////////////////////////////////////

void tetwild::TetWild::output_mesh(std::string file)
{
    consolidate_mesh();

    wmtk::MshData msh;

    const auto& vtx = get_vertices();
    msh.add_tet_vertices(vtx.size(), [&](size_t k) {
        auto i = vtx[k].vid(*this);
        return m_vertex_attribute[i].m_posf;
    });

    const auto& tets = get_tets();
    msh.add_tets(tets.size(), [&](size_t k) {
        auto i = tets[k].tid(*this);
        auto vs = oriented_tet_vertices(tets[k]);
        std::array<size_t, 4> data;
        for (int j = 0; j < 4; j++) {
            data[j] = vs[j].vid(*this);
            assert(data[j] < vtx.size());
        }
        return data;
    });

    msh.add_tet_vertex_attribute<1>("tv index", [&](size_t i) {
        return m_vertex_attribute[i].m_sizing_scalar;
    });
    msh.add_tet_attribute<1>("t energy", [&](size_t i) { return m_tet_attribute[i].m_quality; });

    msh.save(file, true);
}


double tetwild::TetWild::get_length2(const wmtk::TetMesh::Tuple& l) const
{
    auto& m = *this;
    auto& v1 = l;
    auto v2 = l.switch_vertex(m);
    double length =
        (m.m_vertex_attribute[v1.vid(m)].m_posf - m.m_vertex_attribute[v2.vid(m)].m_posf)
            .squaredNorm();
    return length;
}

std::tuple<double, double> tetwild::TetWild::get_max_avg_energy()
{
    double max_energy;
    double avg_energy = 0;

    const auto& tets = get_tets(); // todo: avoid copy!!!
    for (size_t i = 0; i < tets.size(); i++) {
        if (i == 0)
            max_energy = m_tet_attribute[tets[i].tid(*this)].m_quality;
        else {
            if (m_tet_attribute[tets[i].tid(*this)].m_quality > max_energy)
                max_energy = m_tet_attribute[tets[i].tid(*this)].m_quality;
        }

        avg_energy += m_tet_attribute[tets[i].tid(*this)].m_quality;
    }

    avg_energy /= tets.size();

    return std::make_tuple(max_energy, avg_energy);
}


bool tetwild::TetWild::is_inverted(const Tuple& loc) const
{
    // Return a positive value if the point pd lies below the
    // plane passing through pa, pb, and pc; "below" is defined so
    // that pa, pb, and pc appear in counterclockwise order when
    // viewed from above the plane.

    auto vs = oriented_tet_vertices(loc);

    //
    if (m_vertex_attribute[vs[0].vid(*this)].m_is_rounded &&
        m_vertex_attribute[vs[1].vid(*this)].m_is_rounded &&
        m_vertex_attribute[vs[2].vid(*this)].m_is_rounded &&
        m_vertex_attribute[vs[3].vid(*this)].m_is_rounded) {
        igl::predicates::exactinit();
        auto res = igl::predicates::orient3d(
            m_vertex_attribute[vs[0].vid(*this)].m_posf,
            m_vertex_attribute[vs[1].vid(*this)].m_posf,
            m_vertex_attribute[vs[2].vid(*this)].m_posf,
            m_vertex_attribute[vs[3].vid(*this)].m_posf);
        int result;
        if (res == igl::predicates::Orientation::POSITIVE)
            result = 1;
        else if (res == igl::predicates::Orientation::NEGATIVE)
            result = -1;
        else
            result = 0;

        if (result < 0) // neg result == pos tet (tet origin from geogram delaunay)
            return false;
        return true;
    } else {
        Vector3 n = ((m_vertex_attribute[vs[1].vid(*this)].m_pos) -
                     m_vertex_attribute[vs[0].vid(*this)].m_pos)
                        .cross(
                            (m_vertex_attribute[vs[2].vid(*this)].m_pos) -
                            m_vertex_attribute[vs[0].vid(*this)].m_pos);
        Vector3 d = (m_vertex_attribute[vs[3].vid(*this)].m_pos) -
                    m_vertex_attribute[vs[0].vid(*this)].m_pos;
        auto res = n.dot(d);
        if (res > 0) // predicates returns pos value: non-inverted
            return false;
        else
            return true;
    }
}

bool tetwild::TetWild::round(const Tuple& v)
{
    size_t i = v.vid(*this);

    auto old_pos = m_vertex_attribute[i].m_pos;
    m_vertex_attribute[i].m_pos << m_vertex_attribute[i].m_posf[0], m_vertex_attribute[i].m_posf[1],
        m_vertex_attribute[i].m_posf[2];
    auto conn_tets = get_one_ring_tets_for_vertex(v);
    m_vertex_attribute[i].m_is_rounded = true;
    for (auto& tet : conn_tets) {
        if (is_inverted(tet)) {
            m_vertex_attribute[i].m_is_rounded = false;
            m_vertex_attribute[i].m_pos = old_pos;
            return false;
        }
    }

    return true;
}

double tetwild::TetWild::get_quality(const Tuple& loc) const
{
    std::array<Vector3d, 4> ps;
    auto its = oriented_tet_vertices(loc);
    for (int j = 0; j < 4; j++) {
        ps[j] = m_vertex_attribute[its[j].vid(*this)].m_posf;
    }

    std::array<double, 12> T;
    for (int j = 0; j < 3; j++) {
        T[0 * 3 + j] = ps[0][j];
        T[1 * 3 + j] = ps[1][j];
        T[2 * 3 + j] = ps[2][j];
        T[3 * 3 + j] = ps[3][j];
    }

    double energy = wmtk::AMIPS_energy(T);
    if (std::isinf(energy) || std::isnan(energy) || energy < 3 - 1e-3) return MAX_ENERGY;
    return energy;
}


bool tetwild::TetWild::invariants(const std::vector<Tuple>& tets)
{
    // check inversion
    for (auto& t : tets)
        if (is_inverted(t)) return false;
    for (auto& t : tets) {
        for (auto j = 0; j < 4; j++) {
            auto f_t = tuple_from_face(t.tid(*this), j);
            auto fid = f_t.fid(*this);
            if (m_face_attribute[fid].m_is_surface_fs) {
                auto vs = get_face_vertices(f_t);
                if (m_envelope.is_outside(
                        {{m_vertex_attribute[vs[0].vid(*this)].m_posf,
                          m_vertex_attribute[vs[1].vid(*this)].m_posf,
                          m_vertex_attribute[vs[2].vid(*this)].m_posf}}))
                    return false;
            }
        }
    }
    return true;
}

std::vector<std::array<size_t, 3>> tetwild::TetWild::get_faces_by_condition(
    std::function<bool(const FaceAttributes&)> cond)
{
    auto res = std::vector<std::array<size_t, 3>>();
    for (auto f : get_faces()) {
        auto fid = f.fid(*this);
        if (cond(m_face_attribute[fid])) {
            auto tid = fid / 4, lid = fid % 4;
            auto verts = get_face_vertices(f);
            res.emplace_back(std::array<size_t, 3>{
                {verts[0].vid(*this), verts[1].vid(*this), verts[2].vid(*this)}});
        }
    }
    return res;
}

bool tetwild::TetWild::is_edge_on_surface(const Tuple& loc)
{
    size_t v1_id = loc.vid(*this);
    auto loc1 = loc.switch_vertex(*this);
    size_t v2_id = loc1.vid(*this);
    if (!m_vertex_attribute[v1_id].m_is_on_surface || !m_vertex_attribute[v2_id].m_is_on_surface)
        return false;

    auto tets = get_incident_tets_for_edge(loc);
    std::vector<size_t> n_vids;
    for (auto& t : tets) {
        auto vs = oriented_tet_vertices(t);
        for (int j = 0; j < 4; j++) {
            if (vs[j].vid(*this) != v1_id && vs[j].vid(*this) != v2_id)
                n_vids.push_back(vs[j].vid(*this));
        }
    }
    wmtk::vector_unique(n_vids);

    for (size_t vid : n_vids) {
        auto [_, fid] = tuple_from_face({{v1_id, v2_id, vid}});
        if (m_face_attribute[fid].m_is_surface_fs) return true;
    }

    return false;
}

void tetwild::TetWild::check_attributes()
{
    using std::cout;
    using std::endl;

    for (auto& f : get_faces()) {
        auto fid = f.fid(*this);
        auto vs = get_face_vertices(f);

        if (m_face_attribute[fid].m_is_surface_fs) {
            if (!(m_vertex_attribute[vs[0].vid(*this)].m_is_on_surface &&
                  m_vertex_attribute[vs[1].vid(*this)].m_is_on_surface &&
                  m_vertex_attribute[vs[2].vid(*this)].m_is_on_surface))
                wmtk::logger().critical("surface track wrong");
            bool is_out = m_envelope.is_outside(
                {{m_vertex_attribute[vs[0].vid(*this)].m_posf,
                  m_vertex_attribute[vs[1].vid(*this)].m_posf,
                  m_vertex_attribute[vs[2].vid(*this)].m_posf}});
            if (is_out)
                wmtk::logger().critical(
                    "is_out f {} {} {}",
                    vs[0].vid(*this),
                    vs[1].vid(*this),
                    vs[2].vid(*this));
        }
        if (m_face_attribute[fid].m_is_bbox_fs >= 0) {
            if (!(!m_vertex_attribute[vs[0].vid(*this)].on_bbox_faces.empty() &&
                  !m_vertex_attribute[vs[1].vid(*this)].on_bbox_faces.empty() &&
                  !m_vertex_attribute[vs[2].vid(*this)].on_bbox_faces.empty()))
                wmtk::logger().critical("bbox track wrong {}", fid);
        }
    }

    for (auto& v : get_vertices()) {
        size_t i = v.vid(*this);
        if (m_vertex_attribute[i].m_is_on_surface) {
            bool is_out = m_envelope.is_outside(m_vertex_attribute[i].m_posf);
            if (is_out) wmtk::logger().critical("is_out v");
        }

        if (m_vertex_attribute[i].m_is_rounded) {
            if (m_vertex_attribute[i].m_pos[0] != m_vertex_attribute[i].m_posf[0] ||
                m_vertex_attribute[i].m_pos[1] != m_vertex_attribute[i].m_posf[1] ||
                m_vertex_attribute[i].m_pos[2] != m_vertex_attribute[i].m_posf[2])
                wmtk::logger().critical("rounding error {}", i);
        }
    }
}