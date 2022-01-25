#include <wmtk/utils/TupleUtils.hpp>

#include <Tracy.hpp>

#include <algorithm>

namespace wmtk {
void unique_edge_tuples(const TetMesh& m, std::vector<TetMesh::Tuple>& edges)
{
    ZoneScoped;
    auto edge_ids = std::vector<size_t>();
    for (auto& e : edges) edge_ids.push_back(e.eid(m));
    std::sort(edge_ids.begin(), edge_ids.end());
    edge_ids.erase(std::unique(edge_ids.begin(), edge_ids.end()), edge_ids.end());
    edges.clear();
    for (auto i:edge_ids) {
        edges.emplace_back(m.tuple_from_edge(i/6, i%6));
    }
}

void unique_face_tuples(const TetMesh& m, std::vector<TetMesh::Tuple>& faces)
{
    ZoneScoped;
    std::stable_sort(
        faces.begin(),
        faces.end(),
        [&](const TetMesh::Tuple& a, const TetMesh::Tuple& b) {
            return a.fid(m) < b.fid(m);
        }); // todo: use unique global id here would be very slow!

    faces.erase(
        std::unique(
            faces.begin(),
            faces.end(),
            [&](const TetMesh::Tuple& a, const TetMesh::Tuple& b) { return a.fid(m) == b.fid(m); }),
        faces.end());
}

void unique_directed_edge_tuples(const TetMesh& m, std::vector<TetMesh::Tuple>& edges)
{
    ZoneScoped;
    std::stable_sort(
        edges.begin(),
        edges.end(),
        [&](const TetMesh::Tuple& a, const TetMesh::Tuple& b) {
            throw "check me!";
            const int aeid = a.eid(m), beid = b.eid(m);
            if (aeid == beid) return a.vid(m) < b.vid(m);
            return aeid < beid;
        });

    edges.erase(
        std::unique(
            edges.begin(),
            edges.end(),
            [&](const TetMesh::Tuple& a, const TetMesh::Tuple& b) {
                throw "check me!";
                return a.eid(m) == b.eid(m) && a.vid(m) == b.vid(m);
            }),
        edges.end());
}
} // namespace wmtk
