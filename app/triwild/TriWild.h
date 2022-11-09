#pragma once

#include <igl/Timer.h>
#include <igl/doublearea.h>
#include <lagrange/SurfaceMesh.h>

#include <lagrange/bvh/EdgeAABBTree.h>
#include <wmtk/TriMesh.h>
#include <sec/envelope/SampleEnvelope.hpp>
#include "Parameters.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

namespace triwild {
class VertexAttributes
{
public:
    Eigen::Vector2d pos;

    size_t partition_id = 0; // TODO this should not be here

    // Vertices marked as fixed cannot be modified by any local operation
    bool fixed = false;
};


class FaceAttributes
{
public:
    double area = 0;
};

class TriWild : public wmtk::TriMesh
{
public:
    json js_log;
    // default envelop use_exact = true
    sample_envelope::SampleEnvelope m_envelope;

    bool m_has_envelope = false;
    // Energy Assigned to undefined energy
    // TODO: why not the max double?
    const double MAX_ENERGY = 1e50;
    double m_target_l = -1.; // targeted edge length
    double m_target_lr = 5e-2; // targeted relative edge length
    double m_eps = 0.0; // envelope size default to 0.0
    bool m_bnd_freeze = false; // freeze boundary default to false
    double m_max_energy = -1;
    double m_stop_energy = 5;
    std::function<Eigen::RowVector2d(const Eigen::RowVector2d&)> m_get_closest_point;

    TriWild(){};

    virtual ~TriWild(){};

    // Store the per-vertex attributes
    wmtk::AttributeCollection<VertexAttributes> vertex_attrs;
    struct InfoCache
    {
        size_t v1;
        size_t v2;
        double max_energy;
        int partition_id;
    };
    tbb::enumerable_thread_specific<InfoCache> cache;
    // Store the per-face attribute
    wmtk::AttributeCollection<FaceAttributes> face_attrs;

    bool invariants(const std::vector<Tuple>& new_tris);

    // Initializes the mesh
    // default with strict boundary freeze
    /**
     * @brief Create a mesh object
     *
     * @param V igl format vertices
     * @param F igl format faces
     * @param eps absolute envelop size
     * @param bnd_freeze boundary not moving
     */
    void create_mesh(
        const Eigen::MatrixXd& V,
        const Eigen::MatrixXi& F,
        double eps = 0.0,
        bool bnd_freeze = true);

    Eigen::Matrix<uint64_t, Eigen::Dynamic, 2, Eigen::RowMajor> get_bnd_edge_matrix();

    // Exports V and F of the stored mesh
    void export_mesh(Eigen::MatrixXd& V, Eigen::MatrixXi& F);

    // Writes a triangle mesh in OBJ format
    void write_obj(const std::string& path);

    // Computes the quality of a triangle
    double get_quality(const Tuple& loc) const;

    // Computes the average quality of a mesh
    Eigen::VectorXd get_quality_all_triangles();

    // Check if a triangle is inverted
    bool is_inverted(const Tuple& loc) const;

    std::vector<TriMesh::Tuple> new_edges_after(const std::vector<TriMesh::Tuple>& tris) const;

    // Smoothing
    void smooth_all_vertices();
    bool smooth_before(const Tuple& t) override;
    bool smooth_after(const Tuple& t) override;
    bool smooth_after_without_index(const Tuple& t);

    // Collapse
    void collapse_all_edges();
    bool collapse_edge_before(const Tuple& t) override;
    bool collapse_edge_after(const Tuple& t) override;
    // Split
    void split_all_edges();
    bool split_edge_before(const Tuple& t) override;
    bool split_edge_after(const Tuple& t) override;
    // Swap
    void swap_all_edges();
    bool swap_edge_before(const Tuple& t) override;
    bool swap_edge_after(const Tuple& t) override;

    void mesh_improvement(int max_its);
    double get_length2(const Tuple& t) const;
};

} // namespace triwild