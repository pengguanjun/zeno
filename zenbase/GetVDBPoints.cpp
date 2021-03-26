#include <zen/zen.h>
#include <zen/ParticlesObject.h>
#include <zen/VDBGrid.h>
#include <omp.h>

namespace zenbase {

struct GetVDBPoints : zen::INode {
  virtual void apply() override {
    auto grid = get_input("grid")->as<VDBPointsGrid>()->m_grid;

    std::vector<openvdb::points::PointDataTree::LeafNodeType*> pars;
    grid->tree().getNodes(pars);
    printf("GetVDBPoints: particle leaf nodes: %d\n", pars.size());

    auto ret = zen::IObject::make<ParticlesObject>();

    for (auto const &leaf: pars) {
      //attributes
      // Attribute reader
      // Extract the position attribute from the leaf by name (P is position).
      openvdb::points::AttributeArray& positionArray =
        leaf->attributeArray("P");
      // Extract the velocity attribute from the leaf by name (v is velocity).
      openvdb::points::AttributeArray& velocityArray =
        leaf->attributeArray("v");

      using PositionCodec = openvdb::points::FixedPointCodec</*one byte*/false>;
      using VelocityCodec = openvdb::points::TruncateCodec;
      // Create read handles for position and velocity
      openvdb::points::AttributeHandle<openvdb::Vec3f, PositionCodec> positionHandle(positionArray);
      openvdb::points::AttributeHandle<openvdb::Vec3f, VelocityCodec> velocityHandle(velocityArray);

      for (auto iter = leaf->beginIndexOn(); iter; ++iter) {
        openvdb::Vec3R p = iter.getCoord().asVec3d() + positionHandle.get(*iter);
        openvdb::Vec3R v = iter.getCoord().asVec3d() + velocityHandle.get(*iter);
        ret->pos.push_back(glm::vec3(p[0], p[1], p[2]));
        ret->vel.push_back(glm::vec3(v[0], v[1], v[2]));
      }
    }
    set_output("pars", ret);
  }
};

static int defGetVDBPoints = zen::defNodeClass<GetVDBPoints>("GetVDBPoints",
    { /* inputs: */ {
        "grid",
    }, /* outputs: */ {
        "pars",
    }, /* params: */ {
    }, /* category: */ {
      "openvdb",
    }});

}
