#include <zeno/zeno.h>
#include <zeno/logger.h>
#include <zeno/ListObject.h>
#include <zeno/NumericObject.h>
#include <zeno/PrimitiveObject.h>
#include <zeno/utils/UserData.h>
#include <zeno/StringObject.h>

#include <anisotropic_SNH.h>
#include <backward_euler_integrator.h>
#include <fstream>
#include <algorithm>

#include <matrix_helper.hpp>
#include<Eigen/SparseCholesky>
#include <iomanip>

#include <cmath>

namespace{

using namespace zeno;

struct FEMMesh : zeno::IObject{
    FEMMesh() = default;
    std::shared_ptr<PrimitiveObject> _mesh;
    std::vector<int> _bouDoFs;
    std::vector<int> _freeDoFs;
    std::vector<int> _DoF2FreeDoF;
    std::vector<int> _SpMatFreeDoFs;
    std::vector<Mat12x12i> _elmSpIndices;

    std::vector<double> _elmMass;
    std::vector<double> _elmVolume;
    std::vector<Mat9x12d> _elmdFdx;
    std::vector<Mat4x4d> _elmMinv;

    std::vector<double> _elmYoungModulus;
    std::vector<double> _elmPossonRatio;
    std::vector<double> _elmDensity;

    SpMat _connMatrix;
    SpMat _freeConnMatrix;

    std::vector<Mat3x3d> _elmAct;
    std::vector<Mat3x3d> _elmOrient;
    std::vector<Vec3d> _elmWeight;

    Eigen::Map<const SpMat> MapHMatrix(const FEM_Scaler* HValBuffer){
        size_t n = _mesh->size() * 3;
        return Eigen::Map<const SpMat>(n,n,_connMatrix.nonZeros(),
            _connMatrix.outerIndexPtr(),_connMatrix.innerIndexPtr(),HValBuffer);
    }

    Eigen::Map<const SpMat> MapHucMatrix(const FEM_Scaler* HucValBuffer) {
        size_t nuc = _freeDoFs.size();
        return Eigen::Map<const SpMat>(nuc,nuc,_freeConnMatrix.nonZeros(),
            _freeConnMatrix.outerIndexPtr(),_freeConnMatrix.innerIndexPtr(),HucValBuffer);
    }

    Eigen::Map<SpMat> MapHMatrixRef(FEM_Scaler* HValBuffer){
        size_t n = _mesh->size() * 3;
        return Eigen::Map<SpMat>(n,n,_connMatrix.nonZeros(),
            _connMatrix.outerIndexPtr(),_connMatrix.innerIndexPtr(),HValBuffer);
    }

    Eigen::Map<SpMat> MapHucMatrixRef(FEM_Scaler* HucValBuffer) {
        size_t nuc = _freeDoFs.size();
        return Eigen::Map<SpMat>(nuc,nuc,_freeConnMatrix.nonZeros(),
            _freeConnMatrix.outerIndexPtr(),_freeConnMatrix.innerIndexPtr(),HucValBuffer);
    }

    void DoPreComputation() {
        size_t nm_elms = _mesh->quads.size();
        for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
            auto elm = _mesh->quads[elm_id];
            Mat4x4d M;
            for(size_t i = 0;i < 4;++i){
                auto vert = _mesh->verts[elm[i]];
                M.block(0,i,3,1) << vert[0],vert[1],vert[2];
            }
            M.bottomRows(1).setConstant(1.0);
            _elmVolume[elm_id] = fabs(M.determinant()) / 6;
            _elmMass[elm_id] = _elmVolume[elm_id] * _elmDensity[elm_id];


            Mat3x3d Dm;
            for(size_t i = 1;i < 4;++i){
                auto vert = _mesh->verts[elm[i]];
                auto vert0 = _mesh->verts[elm[0]];
                Dm.col(i - 1) << vert[0]-vert0[0],vert[1]-vert0[1],vert[2]-vert0[2];
            }

            Mat3x3d DmInv = Dm.inverse();
            _elmMinv[elm_id] = M.inverse();


            double m = DmInv(0,0);
            double n = DmInv(0,1);
            double o = DmInv(0,2);
            double p = DmInv(1,0);
            double q = DmInv(1,1);
            double r = DmInv(1,2);
            double s = DmInv(2,0);
            double t = DmInv(2,1);
            double u = DmInv(2,2);

            double t1 = - m - p - s;
            double t2 = - n - q - t;
            double t3 = - o - r - u; 

            _elmdFdx[elm_id] << 
                t1, 0, 0, m, 0, 0, p, 0, 0, s, 0, 0, 
                 0,t1, 0, 0, m, 0, 0, p, 0, 0, s, 0,
                 0, 0,t1, 0, 0, m, 0, 0, p, 0, 0, s,
                t2, 0, 0, n, 0, 0, q, 0, 0, t, 0, 0,
                 0,t2, 0, 0, n, 0, 0, q, 0, 0, t, 0,
                 0, 0,t2, 0, 0, n, 0, 0, q, 0, 0, t,
                t3, 0, 0, o, 0, 0, r, 0, 0, u, 0, 0,
                 0,t3, 0, 0, o, 0, 0, r, 0, 0, u, 0,
                 0, 0,t3, 0, 0, o, 0, 0, r, 0, 0, u;
        }
    }
// load .node file
    void LoadVerticesFromFile(const std::string& filename) {
        size_t num_vertices,space_dimension,d1,d2;
        std::ifstream node_fin;
        try {
            node_fin.open(filename.c_str());
            if (!node_fin.is_open()) {
                std::cerr << "ERROR::NODE::FAILED::" << filename << std::endl;
            }
            node_fin >> num_vertices >> space_dimension >> d1 >> d2;
            auto &pos = _mesh->add_attr<vec3f>("pos");
            pos.resize(num_vertices);

            for(size_t vert_id = 0;vert_id < num_vertices;++vert_id) {
                node_fin >> d1;
                for (size_t i = 0; i < space_dimension; ++i)
                    node_fin >> pos[vert_id][i];
            }
            node_fin.close();
        }catch(std::exception &e){
            std::cerr << e.what() << std::endl;
        }
    }

// load .ele file
    void LoadElementsFromFile(const std::string& filename) {
        size_t nm_elms,elm_size,v_start_idx,elm_idx;
        std::ifstream ele_fin;
        try {
            ele_fin.open(filename.c_str());
            if (!ele_fin.is_open()) {
                std::cerr << "ERROR::TET::FAILED::" << filename << std::endl;
            }
            ele_fin >> nm_elms >> elm_size >> v_start_idx;

            auto& quads = _mesh->quads;
            quads.resize(nm_elms);

            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id) {
                ele_fin >> elm_idx;
                for (size_t i = 0; i < elm_size; ++i) {
                    ele_fin >> quads[elm_id][i];
                    quads[elm_id][i] -= v_start_idx;
                }
            }
            ele_fin.close();
        }catch(std::exception &e){
            std::cerr << e.what() << std::endl;
        }
    }

    void LoadBoundaryIndicesFromFile(const std::string& filename) {
        size_t nm_cons_dofs,start_idx;
        std::ifstream bou_fin;
        try{
            bou_fin.open(filename.c_str());
            if(!bou_fin.is_open()){
                std::cerr << "ERROR::BOU::FAILED::" << filename << std::endl;
            }
            bou_fin >> nm_cons_dofs >> start_idx;
            _bouDoFs.resize(nm_cons_dofs);
            for(size_t c_id = 0;c_id < nm_cons_dofs;++c_id){
                bou_fin >> _bouDoFs[c_id];
                _bouDoFs[c_id] -= start_idx;
            }
            bou_fin.close();
        }catch(std::exception &e){
            std::cerr << e.what() << std::endl;
        }
    }

    void UpdateDoFsMapping() {
        size_t nmDoFs = _mesh->verts.size() * 3;
        size_t nmBouDoFs = _bouDoFs.size();
        size_t nmFreeDoFs = nmDoFs - _bouDoFs.size();
        _freeDoFs.resize(nmFreeDoFs);

        for(size_t cdof_idx = 0,dof = 0,ucdof_count = 0;dof < nmDoFs;++dof){
            if(cdof_idx >= nmBouDoFs || dof != _bouDoFs[cdof_idx]){
                _freeDoFs[ucdof_count] = dof;
                ++ucdof_count;
            }
            else
                ++cdof_idx;
        }
// build uc mapping
        _DoF2FreeDoF.resize(nmDoFs,-1);
        for(size_t i = 0;i < nmFreeDoFs;++i){
            int ucdof = _freeDoFs[i];
            _DoF2FreeDoF[ucdof] = i;
        }
// Initialize connectivity matrices
        size_t nm_elms = _mesh->quads.size();
        std::set<Triplet,triplet_cmp> connTriplets;
        size_t nm_insertions = 0;
        for (size_t elm_id = 0; elm_id < nm_elms; ++elm_id) {
            const auto& elm = _mesh->quads[elm_id];
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = 0; j < 4; ++j)
                    for (size_t k = 0; k < 3; ++k)
                        for (size_t l = 0; l < 3; ++l) {
                            size_t row = elm[i] * 3 + k;
                            size_t col = elm[j] * 3 + l;
                            if(row > col)
                                continue;
                            if(row == col){
                                connTriplets.insert(Triplet(row, col, 1.0));
                                ++nm_insertions;
                            }else{
                                connTriplets.insert(Triplet(row, col, 1.0));
                                connTriplets.insert(Triplet(col, row, 1.0));
                                nm_insertions += 2;
                            }
                        }
        }
        _connMatrix = SpMat(nmDoFs,nmDoFs);
        _connMatrix.setFromTriplets(connTriplets.begin(),connTriplets.end());
        _connMatrix.makeCompressed();

        std::set<Triplet,triplet_cmp> freeConnTriplets;
        nm_insertions = 0;
        for(size_t elm_id = 0;elm_id < nm_elms;++elm_id) {
            const auto& elm = _mesh->quads[elm_id];
            for (size_t i = 0; i < 4; ++i)
                for (size_t j = 0; j < 4; ++j)
                    for (size_t k = 0; k < 3; ++k)
                        for (size_t l = 0; l < 3; ++l) {
                            size_t row = _DoF2FreeDoF[elm[i] * 3 + k];
                            size_t col = _DoF2FreeDoF[elm[j] * 3 + l];
                            if(row == -1 || col == -1 || row > col)
                                continue;
                            if(row == col){
                                freeConnTriplets.insert(Triplet(row,col,1.0));
                                nm_insertions++;
                            }else{
                                freeConnTriplets.insert(Triplet(row,col,1.0));
                                freeConnTriplets.insert(Triplet(col,row,1.0));
                                nm_insertions += 2;
                            }
                        }
        }
        _freeConnMatrix = SpMat(nmFreeDoFs,nmFreeDoFs);
        _freeConnMatrix.setFromTriplets(freeConnTriplets.begin(),freeConnTriplets.end());
        _freeConnMatrix.makeCompressed();

        _SpMatFreeDoFs.resize(_freeConnMatrix.nonZeros());
        size_t uc_idx = 0;
        size_t idx = 0;
        for(size_t k = 0;k < size_t(_connMatrix.outerSize());++k)
            for(SpMat::InnerIterator it(_connMatrix,k);it;++it){
                size_t row = it.row();
                size_t col = it.col();
                if(_DoF2FreeDoF[row] == -1 || _DoF2FreeDoF[col] == -1){
                    idx++;
                    continue;
                }
                _SpMatFreeDoFs[uc_idx] = idx;
                ++uc_idx;
                ++idx;
            }
    }
};

struct FEMMeshToPrimitive : zeno::INode{
    virtual void apply() override {
        auto res = get_input<FEMMesh>("femmesh");
        set_output("primitive", res->_mesh);
    }
};


ZENDEFNODE(FEMMeshToPrimitive, {
    {"femmesh"},
    {"primitive"},
    {},
    {"FEM"},
});


struct AddMuscleFibers : zeno::INode {
    virtual void apply() override {
        auto fibers = get_input<zeno::PrimitiveObject>("fibers");
        auto femmesh = get_input<FEMMesh>("femmesh");
        auto fposs = fibers->attr<zeno::vec3f>("pos");
        auto fdirs = fibers->attr<zeno::vec3f>("vel");

        const auto &mpos = femmesh->_mesh->attr<zeno::vec3f>("pos");
        const auto &tets = femmesh->_mesh->quads;

        std::vector<zeno::vec3f> tet_dirs;
        std::vector<zeno::vec3f> tet_pos;
        tet_dirs.resize(tets.size(),zeno::vec3f(0));
        tet_pos.resize(tets.size());

        // Retrieve the center of the tets
        for(size_t elm_id = 0;elm_id < tets.size();++elm_id){
            auto tet = tets[elm_id];
            tet_pos[elm_id] = zeno::vec3f(0);

            for(size_t i = 0;i < 4;++i){
                tet_pos[elm_id] += mpos[tet[i]];
            }
            tet_pos[elm_id] /= 4;
        }

        float sigma = 2;

        for(size_t i = 0;i < fibers->size();++i){
            auto fpos = fposs[i];
            auto fdir = fdirs[i];
            fdir /= zeno::length(fdir);

            for(size_t elm_id = 0;elm_id < tets.size();++elm_id){
                float dissqrt = zeno::lengthSquared(fpos - tet_pos[elm_id]);
                float weight = exp(-dissqrt / pow(sigma,2));
                tet_dirs[elm_id] += weight * fdir;
            }
        }

        for(size_t elm_id = 0;elm_id < tets.size();++elm_id){
            tet_dirs[elm_id] /= zeno::length(tet_dirs[elm_id]);
        }

        for(size_t elm_id = 0;elm_id < tets.size();++elm_id){
            auto dir0 = tet_dirs[elm_id] / zeno::length(tet_dirs[elm_id]);
            auto ref_dir = dir0;
            ref_dir[0] += 1;
            auto dir1 = zeno::cross(dir0,ref_dir);
            dir1 /= zeno::length(dir1);
            auto dir2 = zeno::cross(dir0,dir1);
            dir2 /= zeno::length(dir2);

            Mat3x3d orient;
            orient.col(0) << dir0[0],dir0[1],dir0[2];
            orient.col(1) << dir1[0],dir1[1],dir1[2];
            orient.col(2) << dir2[0],dir2[1],dir2[2];

            femmesh->_elmOrient[elm_id] = orient;
        }

        set_output("outMesh",femmesh);

        std::cout << "output fiber geo" << std::endl;

        auto fgeo = std::make_shared<zeno::PrimitiveObject>();
        fgeo->resize(tets.size() * 2);
        float length = 400;
        if(has_input("length")){
            length = get_input<NumericObject>("length")->get<float>();
        }

        auto& pos = fgeo->attr<zeno::vec3f>("pos");
        auto& lines = fgeo->lines;
        lines.resize(tets.size());
        float dt = 0.01;

        std::cout << "build geo" << std::endl;

        for(size_t elm_id = 0;elm_id < tets.size();++elm_id){
            pos[elm_id] = tet_pos[elm_id];
            auto pend = tet_pos[elm_id] + dt * tet_dirs[elm_id] * length;
            pos[elm_id + tets.size()] = pend;
            lines[elm_id] = zeno::vec2i(elm_id,elm_id + tets.size());
        }


        set_output("fiberGeo",fgeo);
    }
};

ZENDEFNODE(AddMuscleFibers, {
    {"fibers","femmesh"},
    {"outMesh","fiberGeo"},
    {},
    {"FEM"},
});

struct PrimitieveOut : zeno::INode {
    virtual void apply() override {
        auto prim = get_input<zeno::PrimitiveObject>("pin");
        set_output("pout",prim);
    }
};

ZENDEFNODE(PrimitieveOut, {
    {"pin"},
    {"pout"},
    {},
    {"FEM"},
});


struct MakeFEMMeshFromFile : zeno::INode{
    virtual void apply() override {
        auto node_file = get_input<zeno::StringObject>("NodeFile")->get();
        auto ele_file = get_input<zeno::StringObject>("EleFile")->get();    
        auto bou_file = get_input<zeno::StringObject>("BouFile")->get();
        float density = get_input<zeno::NumericObject>("density")->get<float>();
        float E = get_input<zeno::NumericObject>("YoungModulus")->get<float>();
        float nu = get_input<zeno::NumericObject>("PossonRatio")->get<float>();

        auto res = std::make_shared<FEMMesh>();
        res->_mesh = std::make_shared<PrimitiveObject>();

        res->LoadVerticesFromFile(node_file);
        res->LoadElementsFromFile(ele_file);
        res->LoadBoundaryIndicesFromFile(bou_file);

        for(size_t i = 0;i < res->_mesh->quads.size();++i){
            auto tet = res->_mesh->quads[i];
            res->_mesh->tris.emplace_back(tet[0],tet[1],tet[2]);
            res->_mesh->tris.emplace_back(tet[1],tet[3],tet[2]);
            res->_mesh->tris.emplace_back(tet[0],tet[2],tet[3]);
            res->_mesh->tris.emplace_back(tet[0],tet[3],tet[1]);
        }
// allocate memory
        size_t nm_elms = res->_mesh->quads.size();
        res->_elmAct.resize(nm_elms,Mat3x3d::Identity());
        res->_elmOrient.resize(nm_elms,Mat3x3d::Identity());
        res->_elmWeight.resize(nm_elms,Vec3d(1.0,0.5,0.5));
        res->_elmYoungModulus.resize(nm_elms,E);
        res->_elmPossonRatio.resize(nm_elms,nu);
        res->_elmDensity.resize(nm_elms,density);

        res->_elmVolume.resize(nm_elms);
        res->_elmdFdx.resize(nm_elms);
        res->_elmMass.resize(nm_elms);
        res->_elmMinv.resize(nm_elms);

        res->UpdateDoFsMapping();
        res->DoPreComputation();
// rendering mesh
        auto resGeo = std::make_shared<PrimitiveObject>();
        auto &pos = resGeo->add_attr<zeno::vec3f>("pos");
        // std::cout << "OUTPUT FRAME " << cur_frame.norm() << std::endl;
        for(size_t i = 0;i < res->_mesh->size();++i){
            auto vert = res->_mesh->verts[i];
            pos.emplace_back(vert[0],vert[1],vert[2]);
        }

        for(int i=0;i < res->_mesh->quads.size();++i){
            auto tet = res->_mesh->quads[i];
            resGeo->tris.emplace_back(tet[0],tet[1],tet[2]);
            resGeo->tris.emplace_back(tet[1],tet[3],tet[2]);
            resGeo->tris.emplace_back(tet[0],tet[2],tet[3]);
            resGeo->tris.emplace_back(tet[0],tet[3],tet[1]);
        }
        pos.resize(res->_mesh->size());

        set_output("FEMMesh",res);
    }
};

ZENDEFNODE(MakeFEMMeshFromFile, {
    {{"readpath","NodeFile"},{"readpath", "EleFile"},{"readpath","BouFile"},
        {"density"},{"YoungModulus"},{"PossonRatio"}},
    {"FEMMesh"},
    {},
    {"FEM"},
});

struct SetUniformMuscleAnisotropicWeight : zeno::INode {
    virtual void apply() override {
        auto mesh = get_input<FEMMesh>("inputMesh");
        auto uni_weight = get_input<zeno::NumericObject>("weight")->get<zeno::vec3f>();
        for(size_t i = 0;i < mesh->_mesh->quads.size();++i){
            mesh->_elmWeight[i] << uni_weight[0],uni_weight[1],uni_weight[2];
        }
        set_output("aniMesh",mesh);
    }
};

ZENDEFNODE(SetUniformMuscleAnisotropicWeight, {
    {{"inputMesh"},{"weight"}},
    {"aniMesh"},
    {},
    {"FEM"},
});


struct SetUniformActivation : zeno::INode {
    virtual void apply() override {
        auto mesh = get_input<FEMMesh>("inputMesh");
        auto uniform_Act = get_input<zeno::NumericObject>("uniform_act")->get<zeno::vec3f>();

        for(size_t i = 0;i < mesh->_mesh->quads.size();++i){
            Mat3x3d fdir = mesh->_elmOrient[i];
            Vec3d act_vec;
            act_vec << uniform_Act[0],uniform_Act[1],uniform_Act[2]; 
            mesh->_elmAct[i] << fdir * act_vec.asDiagonal() * fdir.transpose();
        }

        set_output("actMesh",mesh);
    }
};

ZENDEFNODE(SetUniformActivation, {
    {{"inputMesh"},{"uniform_act"}},
    {"actMesh"},
    {},
    {"FEM"},
});


struct TransformFEMMesh : zeno::INode {
    virtual void apply() override {
        auto mesh = get_input<FEMMesh>("inputMesh");
        zeno::vec3f translate = zeno::vec3f(0.0);
        if(has_input("translate")){
            translate = get_input<zeno::NumericObject>("translate")->get<zeno::vec3f>();
        }
        zeno::vec3f scale = zeno::vec3f(1.0);
        if(has_input("scale")){
            scale = get_input<zeno::NumericObject>("scale")->get<zeno::vec3f>();
        }

        for(size_t i = 0;i < mesh->_mesh->size();++i){
            for(size_t j = 0;j < 3;++j){
                mesh->_mesh->verts[i][j] += translate[j];
                mesh->_mesh->verts[i][j] *= scale[j];
            }
        }
        mesh->DoPreComputation();

        set_output("outMesh",mesh);
    }
};

ZENDEFNODE(TransformFEMMesh,{
    {{"inputMesh"},{"translate"},{"scale"}},
    {"outMesh"},
    {},
    {"FEM"}
});


struct MuscleForceModel : zeno::IObject {
    MuscleForceModel() = default;
    std::shared_ptr<AnisotropicSNHModel> _anisotropicForce;
};

struct MakeMuscleForceModel : zeno::INode {
    virtual void apply() override {
        auto res = std::make_shared<MuscleForceModel>();
        res->_anisotropicForce = std::make_shared<AnisotropicSNHModel>();
        set_output("MuscleForceModel",res);
    }
};

ZENDEFNODE(MakeMuscleForceModel, {
    {},
    {"MuscleForceModel"},
    {},
    {"FEM"},
});

struct FEMIntegrator : zeno::IObject {
    FEMIntegrator() = default;
    std::shared_ptr<BackEulerIntegrator> _intPtr;
    std::vector<VecXd> _traj;
    size_t _stepID;
};

struct MakeFEMIntegrator : zeno::INode {
    virtual void apply() override {
        auto mesh = get_input<FEMMesh>("Mesh");
        auto gravity = get_input<zeno::NumericObject>("gravity")->get<zeno::vec3f>();
        auto dt = get_input<zeno::NumericObject>("dt")->get<float>();

        auto res = std::make_shared<FEMIntegrator>();
        res->_intPtr = std::make_shared<BackEulerIntegrator>();
        res->_intPtr->SetGravity(Vec3d(gravity[0],gravity[1],gravity[2]));
        res->_intPtr->SetTimeStep(dt);
        res->_traj.resize(res->_intPtr->GetCouplingLength(),
                    VecXd::Zero(mesh->_mesh->size() * 3));
        for(size_t i = 0;i < res->_intPtr->GetCouplingLength();++i)
            for(size_t j = 0;j < mesh->_mesh->size();++j){
                auto& vert = mesh->_mesh->verts[j];
                res->_traj[i][j*3 + 0] = vert[0];
                res->_traj[i][j*3 + 1] = vert[1];
                res->_traj[i][j*3 + 2] = vert[2];
            }

        res->_stepID = 0;

        set_output("FEMIntegrator",res);
    }
};

ZENDEFNODE(MakeFEMIntegrator,{
    {{"Mesh"},{"gravity"},{"dt"}},
    {"FEMIntegrator"},
    {},
    {"FEM"},
});

struct SetInitialDeformation : zeno::INode {
    virtual void apply() override {
        auto integrator = get_input<FEMIntegrator>("integrator");
        auto deformation = get_input<zeno::NumericObject>("deform")->get<zeno::vec3f>();
        for(size_t i = 0;i < integrator->_intPtr->GetCouplingLength();++i){
            for(size_t j = 0;j < integrator->_traj[0].size()/3;++j){
                integrator->_traj[i][j*3 + 0] *= deformation[0];
                integrator->_traj[i][j*3 + 0] *= deformation[1];
                integrator->_traj[i][j*3 + 0] *= deformation[2];
            }
        }

        integrator->_stepID = 0;
        set_output("intOut",integrator);
    }
};

ZENDEFNODE(SetInitialDeformation,{
    {{"integrator"},{"deform"}},
    {"intOut"},
    {},
    {"FEM"},
});


struct DoTimeStep : zeno::INode {
    virtual void apply() override {
        auto mesh = get_input<FEMMesh>("mesh");
        auto force_model = get_input<MuscleForceModel>("muscleForce");
        auto integrator = get_input<FEMIntegrator>("integrator");
        auto epsilon = get_input<zeno::NumericObject>("epsilon")->get<float>();

        size_t clen = integrator->_intPtr->GetCouplingLength();
        size_t curID = (integrator->_stepID + clen) % clen;
        size_t preID = (integrator->_stepID + clen - 1) % clen;

        // set initial guess
        integrator->_traj[curID] = integrator->_traj[preID];

        size_t iter_idx = 0;

        VecXd deriv(mesh->_mesh->size() * 3);
        VecXd ruc(mesh->_freeDoFs.size()),dpuc(mesh->_freeDoFs.size()),dp(mesh->_mesh->size() * 3);
    
        _HValueBuffer.resize(mesh->_connMatrix.nonZeros());
        _HucValueBuffer.resize(mesh->_freeConnMatrix.nonZeros());

        const size_t max_iters = 20;
        const size_t max_linesearch = 20;
        _wolfeBuffer.resize(max_linesearch);

        size_t search_idx = 0;

        do{
            FEM_Scaler e0,e1,eg0;
            e0 = EvalObjDerivHessian(mesh,force_model,integrator,deriv,_HValueBuffer);

            // {
            //     std::cout << "check derivative" << std::endl;
            //     size_t nm_dofs = integrator->_traj[curID].size();
            //     MatXd H_fd = MatXd(nm_dofs,nm_dofs);
            //     VecXd deriv_fd = deriv,deriv_tmp = deriv;
            //     FEM_Scaler e0_tmp;
            //     VecXd cur_frame_copy = integrator->_traj[curID];

            //     for(size_t i = 0;i < nm_dofs;++i){
            //         integrator->_traj[curID] = cur_frame_copy;
            //         FEM_Scaler step = cur_frame_copy[i] * 1e-8;
            //         step = fabs(step) < 1e-8 ? 1e-8 : step;

            //         integrator->_traj[curID][i] += step;
            //         e0_tmp = EvalObjDeriv(mesh,force_model,integrator,deriv_tmp);

            //         deriv_fd[i] = (e0_tmp - e0) / step;
            //         H_fd.col(i) = (deriv_tmp - deriv) / step;


            //     }
            //     integrator->_traj[curID] = cur_frame_copy;

            //     FEM_Scaler deriv_error = (deriv_fd - deriv).norm();
            //     FEM_Scaler H_error = (mesh->MapHMatrix(_HValueBuffer.data()).toDense() - H_fd).norm();

            //     if(deriv_error > 1e-4){
            //         std::cout << "D_ERROR : " << deriv_error << std::endl;
            //         std::cout << "deriv_norm : " << deriv_fd.norm() << "\t" << deriv.norm() << std::endl;
            //         // for(size_t i = 0;i < deriv_fd.size();++i){
            //         //     std::cout << "idx : " << i << "\t" << deriv_fd[i] << "\t" << deriv[i] << std::endl;
            //         // }
            //         // for(size_t i = 0;i < nm_dofs;++i)
            //         //     std::cout << "idx : " << i << "\t" << deriv[i] << "\t" << deriv_fd[i] << std::endl;
            //         // throw std::runtime_error("DERROR");
            //     }

            //     if(H_error > 1e-3){
            //         std::cout << "H_error : " << H_error << std::endl;
            //         // std::cout << std::setprecision(6) << "H_cmp : " << std::endl << mesh->MapHMatrix(_HValueBuffer.data()).toDense() << std::endl;
            //         // std::cout << std::setprecision(6) << "H_fd : " << std::endl << H_fd << std::endl;

            //         throw std::runtime_error("HERROR");
            //     }
            // }

            // throw std::runtime_error("INT_ERROR");

            MatHelper::RetrieveDoFs(deriv.data(),ruc.data(),mesh->_freeDoFs.size(),mesh->_freeDoFs.data());
            MatHelper::RetrieveDoFs(_HValueBuffer.data(),_HucValueBuffer.data(),mesh->_SpMatFreeDoFs.size(),mesh->_SpMatFreeDoFs.data());

            if(ruc.norm() < epsilon){
                std::cout << "[" << iter_idx << "]break with ruc = " << ruc.norm() << "\t < \t" << epsilon << std::endl;
                break;
            }
            ruc *= -1;

            _LUSolver.compute(mesh->MapHucMatrix(_HucValueBuffer.data()));
            dpuc = _LUSolver.solve(ruc);


            eg0 = -dpuc.dot(ruc);

            if(eg0 > 0){
                std::cerr << "non-negative descent direction detected " << eg0 << std::endl;
                throw std::runtime_error("non-negative descent direction");
            }
            dp.setZero();
            MatHelper::UpdateDoFs(dpuc.data(),dp.data(),mesh->_freeDoFs.size(),mesh->_freeDoFs.data());

            search_idx = 0;

            // if(ruc.norm() < epsilon || \
            //         fabs(eg0) < epsilon * epsilon){

            //     std::cout << "break with ruc = " << ruc.norm() << "\t <  \t" << epsilon  << "\t or \t" << eg0 << "\t < \t" << epsilon * epsilon << std::endl;
            //     break;
            // }
            FEM_Scaler alpha = 2.0f;
            FEM_Scaler beta = 0.5f;
            FEM_Scaler c1 = 0.01f;

            // get_state_ref(0) += dp;
            double armijo_condition;

            do{
                if(search_idx != 0)
                    integrator->_traj[curID] -= alpha * dp;
                alpha *= beta;
                integrator->_traj[curID] += alpha * dp;
                e1 = EvalObj(mesh,force_model,integrator);
                ++search_idx;
                _wolfeBuffer[search_idx-1](0) = (e1 - e0)/alpha;
                _wolfeBuffer[search_idx-1](1) = eg0;

                armijo_condition = double(e1) - double(e0) - double(c1)*double(alpha)*double(eg0);
            }while(/*(e1 > e0 + c1*alpha*eg0)*/ armijo_condition > 0.0f /* || (fabs(eg1) > c2*fabs(eg0))*/ && (search_idx < max_linesearch));

            if(search_idx == max_linesearch){
                std::cout << "LINESEARCH EXCEED" << std::endl;
                for(size_t i = 0;i < max_linesearch;++i)
                    std::cout << "idx:" << i << "\t" << _wolfeBuffer[i].transpose() << std::endl;
                throw std::runtime_error("LINESEARCH");
            }

            ++iter_idx;
        }while(iter_idx < max_iters);


        if(iter_idx == max_iters){
            std::cout << "MAX NEWTON ITERS EXCEED" << std::endl;
        }


        integrator->_stepID++;

        // std::cout << "nm_iter : " << iter_idx << "\t" << "nm_search : " << search_idx << std::endl;
        // std::cout << "frame : " << integrator->_traj[curID].norm() << std::endl;

        // output the current frame

        const VecXd& cur_frame = integrator->_traj[curID];
        auto resGeo = std::make_shared<PrimitiveObject>();
        auto &pos = resGeo->add_attr<zeno::vec3f>("pos");
        // std::cout << "OUTPUT FRAME " << cur_frame.norm() << std::endl;
        for(size_t i = 0;i < mesh->_mesh->size();++i){
            auto vert = cur_frame.segment(i*3,3);
            pos.emplace_back(vert[0],vert[1],vert[2]);
        }

        for(int i=0;i < mesh->_mesh->quads.size();++i){
            auto tet = mesh->_mesh->quads[i];
            resGeo->tris.emplace_back(tet[0],tet[1],tet[2]);
            resGeo->tris.emplace_back(tet[1],tet[3],tet[2]);
            resGeo->tris.emplace_back(tet[0],tet[2],tet[3]);
            resGeo->tris.emplace_back(tet[0],tet[3],tet[1]);
        }
        pos.resize(mesh->_mesh->size());
        set_output("curentFrame", resGeo);
    }

    FEM_Scaler EvalObj(const std::shared_ptr<FEMMesh>& mesh,
        const std::shared_ptr<MuscleForceModel>& muscle,
        const std::shared_ptr<FEMIntegrator>& integrator) {
            FEM_Scaler obj = 0;

            size_t clen = integrator->_intPtr->GetCouplingLength();
            size_t nm_elms = mesh->_mesh->quads.size();

            _objBuffer.resize(nm_elms);
            
            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                TetAttributes attrbs;
                AssignElmAttribs(elm_id,attrbs,mesh);
                std::vector<Vec12d> elm_traj(clen);
                auto tet = mesh->_mesh->quads[elm_id];
                for(size_t i = 0;i < clen;++i){
                    size_t frameID = (integrator->_stepID + clen - i) % clen;
                    RetrieveElmVector(tet,elm_traj[clen - i - 1],integrator->_traj[frameID]);
                }

                FEM_Scaler elm_obj = 0;
                integrator->_intPtr->EvalElmObj(attrbs,muscle->_anisotropicForce,elm_traj,&_objBuffer[elm_id]);
            }


            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                obj += _objBuffer[elm_id];
            }

            return obj;
    }

    FEM_Scaler EvalObjDeriv(const std::shared_ptr<FEMMesh>& mesh,
        const std::shared_ptr<MuscleForceModel>& muscle,
        const std::shared_ptr<FEMIntegrator>& integrator,
        VecXd& deriv) {
            FEM_Scaler obj = 0;

            size_t clen = integrator->_intPtr->GetCouplingLength();
            size_t nm_elms = mesh->_mesh->quads.size();

            _objBuffer.resize(nm_elms);
            _derivBuffer.resize(nm_elms);
            
            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                TetAttributes attrbs;
                AssignElmAttribs(elm_id,attrbs,mesh);
                std::vector<Vec12d> elm_traj(clen);
                auto tet = mesh->_mesh->quads[elm_id];
                for(size_t i = 0;i < clen;++i){
                    size_t frameID = (integrator->_stepID + clen - i) % clen;
                    RetrieveElmVector(tet,elm_traj[clen - i - 1],integrator->_traj[frameID]);
                }

                FEM_Scaler elm_obj = 0;
                integrator->_intPtr->EvalElmObjDeriv(attrbs,muscle->_anisotropicForce,elm_traj,&_objBuffer[elm_id],_derivBuffer[elm_id]);
            }


            deriv.setZero();
            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                auto tet = mesh->_mesh->quads[elm_id];
                obj += _objBuffer[elm_id];
                AssembleElmVector(tet,_derivBuffer[elm_id],deriv);
            }

            return obj;
    }

    FEM_Scaler EvalObjDerivHessian(const std::shared_ptr<FEMMesh>& mesh,
        const std::shared_ptr<MuscleForceModel>& muscle,
        const std::shared_ptr<FEMIntegrator>& integrator,
        VecXd& deriv,
        VecXd& HValBuffer) {
            FEM_Scaler obj = 0;
            size_t clen = integrator->_intPtr->GetCouplingLength();
            size_t nm_elms = mesh->_mesh->quads.size();

            _objBuffer.resize(nm_elms);
            _derivBuffer.resize(nm_elms);
            _HBuffer.resize(nm_elms);
            
            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                TetAttributes attrbs;
                AssignElmAttribs(elm_id,attrbs,mesh);
                // std::cout << "attrbs.elm_Act" << std::endl << attrbs._activation << std::endl;
                // throw std::runtime_error("CHECK ATTRS");
                std::vector<Vec12d> elm_traj(clen);
                auto tet = mesh->_mesh->quads[elm_id];
                for(size_t i = 0;i < clen;++i){
                    size_t frameID = (integrator->_stepID + clen - i) % clen;
                    RetrieveElmVector(tet,elm_traj[clen - i - 1],integrator->_traj[frameID]);
                }

                integrator->_intPtr->EvalElmObjDerivJacobi(attrbs,muscle->_anisotropicForce,elm_traj,
                    &_objBuffer[elm_id],_derivBuffer[elm_id],_HBuffer[elm_id],true);


            //     {
            //         FEM_Scaler obj_tmp;
            //         Vec12d deriv_tmp,deriv_fd;
            //         Mat12x12d H_fd;
            //         Vec12d frame_copy = elm_traj[clen - 1];

            //         for(size_t i = 0;i < 12;++i){
            //             elm_traj[clen - 1] = frame_copy;
            //             FEM_Scaler step = frame_copy[i] * 1e-8;
            //             step = fabs(step) < 1e-8 ? 1e-8 : step;
            //             elm_traj[clen - 1][i] += step;

            //             integrator->_intPtr->EvalElmObjDeriv(attrbs,muscle->_anisotropicForce,elm_traj,&obj_tmp,deriv_tmp);
            //             deriv_fd[i] = (obj_tmp - _objBuffer[elm_id])  / step;
            //             H_fd.col(i) = (deriv_tmp - _derivBuffer[elm_id]) / step;
            //         }

            //         elm_traj[clen - 1] = frame_copy;

            //         FEM_Scaler D_error = (deriv_fd - _derivBuffer[elm_id]).norm() / deriv_fd.norm();
            //         FEM_Scaler H_error = (H_fd - _HBuffer[elm_id]).norm()/H_fd.norm();

            //         if(D_error > 1e-3){
            //             std::cout << "ELM_ID : " << elm_id << std::endl;
            //             std::cout << "INT_ELM_D_error : " << D_error << std::endl;
            //             for(size_t i = 0;i < 12;++i)
            //                 std::cout << "idx : " << i << "\t" << deriv_fd[i] << "\t" << _derivBuffer[elm_id][i] << std::endl;
            //             throw std::runtime_error("INT_ELM_D_ERROR");
            //         }

            //         if(H_error > 1e-3){
            //             std::cout << "ELM_ID : " << elm_id << std::endl;
            //             std::cout << "D_Error : " << D_error << std::endl;
            //             std::cout << "INT_ELM_H_Error : " << H_error << std::endl;
            //             std::cout << "H_cmp : " << std::endl << _HBuffer[elm_id] << std::endl;
            //             std::cout << "H_fd : " << std::endl << H_fd << std::endl;
            //             throw std::runtime_error("INT_ELM_H_ERROR");
            //         }
            //         throw std::runtime_error("INT_ERROR_CHECK");
            //     }


            }

            deriv.setZero();
            HValBuffer.setZero();
            for(size_t elm_id = 0;elm_id < nm_elms;++elm_id){
                auto tet = mesh->_mesh->quads[elm_id];
                obj += _objBuffer[elm_id];
                AssembleElmVector(tet,_derivBuffer[elm_id],deriv);
                AssembleElmMatrixAdd(tet,_HBuffer[elm_id],mesh->MapHMatrixRef(HValBuffer.data()));

            }
            return obj;
    }

    void AssignElmAttribs(size_t elm_id,TetAttributes& attrbs,const std::shared_ptr<FEMMesh>& mesh) const {
        attrbs._elmID = elm_id;
        attrbs._Minv = mesh->_elmMinv[elm_id];
        attrbs._dFdX = mesh->_elmdFdx[elm_id];
        attrbs._fiberOrient = mesh->_elmOrient[elm_id];
        attrbs._fiberWeight = mesh->_elmWeight[elm_id];
        attrbs._activation = mesh->_elmAct[elm_id];
        attrbs._E = mesh->_elmYoungModulus[elm_id];
        attrbs._nu = mesh->_elmPossonRatio[elm_id];
        attrbs._volume = mesh->_elmVolume[elm_id];
        attrbs._density = mesh->_elmDensity[elm_id];
    }

    void RetrieveElmVector(const zeno::vec4i& elm,Vec12d& elm_vec,const VecXd& global_vec) const{  
        for(size_t i = 0;i < 4;++i)
            elm_vec.segment(i*3,3) = global_vec.segment(elm[i]*3,3);
    } 

    void AssembleElmVector(const zeno::vec4i& elm,const Vec12d& elm_vec,VecXd& global_vec) const{
        for(size_t i = 0;i < 4;++i)
            global_vec.segment(elm[i]*3,3) += elm_vec.segment(i*3,3);
    }

    void AssembleElmMatrixAdd(const zeno::vec4i& elm,const Mat12x12d& elm_H,Eigen::Map<SpMat> H) const{
        for(size_t i = 0;i < 4;++i) {
            for(size_t j = 0;j < 4;++j)
                for (size_t r = 0; r < 3; ++r)
                    for (size_t c = 0; c < 3; ++c)
                        H.coeffRef(elm[i] * 3 + r, elm[j] * 3 + c) += elm_H(i * 3 + r, j * 3 + c);
        } 
    }

    VecXd _HValueBuffer;
    VecXd _HucValueBuffer;
    VecXd _FreeJacobiBuffer;
    std::vector<FEM_Scaler> _objBuffer;
    std::vector<Vec12d> _derivBuffer;
    std::vector<Mat12x12d> _HBuffer;

    Eigen::SparseLU<SpMat> _LUSolver;

    std::vector<Vec2d> _wolfeBuffer;
};

ZENDEFNODE(DoTimeStep,{
    {{"mesh"},{"muscleForce"},{"integrator"},{"epsilon"}},
    {"curentFrame"},
    {},
    {"FEM"},
});

};