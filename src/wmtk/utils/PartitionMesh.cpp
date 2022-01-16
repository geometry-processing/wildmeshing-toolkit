#include <wmtk/utils/PartitionMesh.h>

namespace wmtk {

tbb::concurrent_vector<int> partition_TriMesh(wmtk::TriMesh m, int num_partition)
{
    auto f_tuples = m.get_faces();
    Eigen::MatrixXi F(f_tuples.size(), 3);
    for (int i = 0; i < f_tuples.size(); i++) {
        auto f_vertice = f_tuples[i].oriented_tri_vertices(m);
        F(i, 0) = f_vertice[0].vid();
        F(i, 1) = f_vertice[1].vid();
        F(i, 2) = f_vertice[2].vid();
    }
    auto partitioned_v = partition_mesh_vertices(F, num_partition);
    tbb::concurrent_vector<int> v_pid(partitioned_v.rows());
    for (int i = 0; i < partitioned_v.rows(); i++) {
        v_pid[i] = partitioned_v(i, 0);
    }
    return v_pid;
}
} // namespace wmtk