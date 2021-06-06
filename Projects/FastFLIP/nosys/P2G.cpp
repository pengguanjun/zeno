#include <zen/zen.h>
#include <zen/MeshObject.h>
#include <zen/VDBGrid.h>
#include <omp.h>
#include "FLIP_vdb.h"
/*FLIP_vdb::particle_to_grid_collect_style(
	openvdb::points::PointDataGrid::Ptr particles,
	openvdb::Vec3fGrid::Ptr velocity,
	openvdb::Vec3fGrid::Ptr velocity_after_p2g,
	openvdb::Vec3fGrid::Ptr velocity_weights,
	openvdb::FloatGrid::Ptr liquid_sdf,
	openvdb::FloatGrid::Ptr pushed_out_liquid_sdf,
	float dx);
 */
namespace zenbase{
    
    struct FLIP_P2G : zen::INode{
        virtual void apply() override {
            auto dx = std::get<float>(get_param("dx"));
            auto Particles = get_input("Particles")->as<VDBPointsGrid>();
            auto VelGrid = get_input("Velocity")->as<VDBFloat3Grid>();
            auto PostP2GVelGrid = get_input("PostP2GVelocity")->as<VDBFloat3Grid>();
            auto VelWeightGrid = get_input("VelocityWeights")->as<VDBFloat3Grid>();
            auto LiquidSDFGrid = get_input("LiquidSDF")->as<VDBFloatGrid>();
            auto ExtractedLiquidSDFGrid = get_input("ExtractedLiquidSDF")->as<VDBFloatGrid>();
            FLIP_vdb::particle_to_grid_collect_style(Particles->m_grid, 
            VelGrid->m_grid, PostP2GVelGrid->m_grid, 
            VelWeightGrid->m_grid, LiquidSDFGrid->m_grid, 
            ExtractedLiquidSDFGrid->m_grid, dx);
        }
    };

static int defFLIP_P2G = zen::defNodeClass<FLIP_P2G>("FLIP_P2G",
    { /* inputs: */ {
        "Particles", "Velocity", "PostP2GVelocity", "VelocityWeights", "LiquidSDF", "ExtractedLiquidSDF",
    }, 
    /* outputs: */ {
    }, 
    /* params: */ {
      {"float", "dx", "0.01 0.0"},
    }, 
    
    /* category: */ {
    "FLIPSolver",
    }});

}