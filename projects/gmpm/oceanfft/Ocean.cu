#include <zeno/zeno.h>
#include <zeno/types/NumericObject.h>
//-------attached some cuda library for FFT-----------------


//--------------
#include <math.h>
#include <math_constants.h>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <cufft.h>
#define MAX_EPSILON 0.10f
#define THRESHOLD   0.15f
#define REFRESH_DELAY     10 //ms



namespace zeno {
//---------------
        //---------------

        int cuda_iDivUp(int a, int b)
        {
            return (a + (b - 1)) / b;
        }
        
        
        // complex math functions
        __device__
        float2 conjugate(float2 arg)
        {
            return make_float2(arg.x, -arg.y);
        }
        
        __device__
        float2 complex_exp(float arg)
        {
            return make_float2(cosf(arg), sinf(arg));
        }
        
        __device__
        float2 complex_add(float2 a, float2 b)
        {
            return make_float2(a.x + b.x, a.y + b.y);
        }
        
        __device__
        float2 complex_mult(float2 ab, float2 cd)
        {
            return make_float2(ab.x * cd.x - ab.y * cd.y, ab.x * cd.y + ab.y * cd.x);
        }
        
        __device__
        float2 normalize(float2 k)
        {       
                float2 v = k;
                float len = ::sqrt(v.x*v.x + v.y*v.y)+0.00001;
                v.x /= len;
                v.y /= len;
                return v;
        }
        
        
        // generate wave heightfield at time t based on initial heightfield and dispersion relationship
        __global__ void generateSpectrumKernel(float2 *h0,
                                               float2 *ht,
                                               float2 *Dx,
                                               float2 *Dz,
                                               unsigned int in_width,
                                               unsigned int out_width,
                                               unsigned int out_height,
                                               float t,
                                               float patchSize)
        {
            unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
            unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;
            unsigned int in_index = y*in_width+x;
            unsigned int in_mindex = (out_height - y)*in_width + (out_width - x); // mirrored
            unsigned int out_index = y*out_width+x;
        
            // calculate wave vector
            float2 k;
            k.x = (-(int)out_width / 2.0f + x) * (2.0f * CUDART_PI_F / patchSize);
            k.y = (-(int)out_width / 2.0f + y) * (2.0f * CUDART_PI_F / patchSize);
        
            // calculate dispersion w(k)
            float k_len = sqrtf(k.x*k.x + k.y*k.y);
            float w = sqrtf(9.81f * k_len);
        
            if ((x < out_width) && (y < out_height))
            {
                float2 h0_k = h0[in_index];
                float2 h0_mk = h0[in_mindex];
        
                // output frequency-space complex values
        
                ht[out_index] = complex_add(complex_mult(h0_k, complex_exp(w * t)), complex_mult(conjugate(h0_mk), complex_exp(-w * t)));
        
                float2 nk = normalize(k);
        
                Dx[out_index] = make_float2(ht[out_index].y * nk.x , -ht[out_index].x * nk.x);
                Dz[out_index] = make_float2(ht[out_index].y * nk.y , -ht[out_index].x * nk.y);
        
            }
        }
        
        __global__ void updateDxKernel(float2  *Dx, float2 *Dz, unsigned int width)
        {
            unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
            unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;
            unsigned int i = y*width+x;
        
            // cos(pi * (m1 + m2))
            float sign_correction = ((x + y) & 0x01) ? -1.0f : 1.0f;
        
            Dx[i].x = Dx[i].x * sign_correction;
            Dz[i].x = Dz[i].x * sign_correction;
        }
        
        
        // update height map values based on output of FFT
        __global__ void updateHeightmapKernel(float  *heightMap,
                                              float2 *ht,
                                              unsigned int width)
        {
            unsigned int x = blockIdx.x*blockDim.x + threadIdx.x;
            unsigned int y = blockIdx.y*blockDim.y + threadIdx.y;
            unsigned int i = y*width+x;
        
            // cos(pi * (m1 + m2))
            float sign_correction = ((x + y) & 0x01) ? -1.0f : 1.0f;
        
            heightMap[i] = ht[i].x * sign_correction;
        }




struct OceanFFT : zeno::IObject{
    OceanFFT() = default;
    //static constexpr unsigned int spectrumW = meshSize + 4;
    //static constexpr unsigned int spectrumH = meshSize + 1;
    //static constexpr int frameCompare = 4;  
    ~OceanFFT() {
        if (d_h0 != nullptr) {
            cudaFree(d_h0);
            cudaFree(h_h0);
            cudaFree(d_ht);
            cudaFree(d_slope);
            cudaFree(Dx);
            cudaFree(Dz);
            cudaFree(g_hDx);
            cudaFree(g_hDz);
            free(g_hhptr);
        }
    }

    // FFT data
    cufftHandle fftPlan;
    float2 *d_h0{nullptr};   // heightfield at time 0
    float2 *h_h0{nullptr};
    float2 *d_ht{nullptr};   // heightfield at time t
    float2 *d_slope{nullptr};
    float2 *Dx{nullptr};
    float2 *Dz{nullptr};
    float2 *g_hDx{nullptr};
    float2 *g_hDz{nullptr};
    // pointers to device object
    float *g_hptr{nullptr};
    float *g_hhptr{nullptr};
    float2 *g_sptr{nullptr};


    // simulation parameters
    float g{9.81f};              // gravitational constant
    float A{1e-7f};              // wave scale factor
    float patchSize{100};        // patch size
    float windSpeed{100.0f};
    float windDir{CUDART_PI_F/3.0f};
    float dir_depend{0.07f};
    float Kx;
    float Ky;
    float Vdir;
    float V;

    float in_width;
    float out_width;
    float out_height;

    float animTime{0.0f};
    float prevTime{0.0f};
    float animationRate{-0.001f};
 
    float *d_heightMap{nullptr};
    unsigned int width;
    unsigned int height;
    bool autoTest;

    float choppyness;
    int WaveExponent = 8;
    int meshSize;
    int spectrumW;
    int spectrumH;
    int frameCompare;
    int spectrumSize = spectrumW*spectrumH*sizeof(float2);
};


void GenerateSpectrumKernel(float2 *d_h0,
        float2 *d_ht,
        float2 *Dx,
        float2 *Dz,
        unsigned int in_width,
        unsigned int out_width,
        unsigned int out_height,
        float animTime,
        float patchSize)
{
    dim3 block(8, 8, 1);
    dim3 grid(cuda_iDivUp(out_width, block.x), cuda_iDivUp(out_height, block.y), 1);
    generateSpectrumKernel<<<grid, block>>>(d_h0, d_ht, Dx, Dz, in_width, out_width, out_height, animTime, patchSize);
    cudaDeviceSynchronize();
}
    



void UpdateDxKernel(float2  *Dx,float2 *Dz, unsigned int width, unsigned int height)
{
    dim3 block(8, 8, 1);
    dim3 grid(cuda_iDivUp(width, block.x), cuda_iDivUp(height, block.y), 1);
    
    updateDxKernel<<<grid, block>>>(Dx, Dz, width);
    cudaDeviceSynchronize();
}


    
    

void UpdateHeightmapKernel(float  *d_heightMap,
    float2 *d_ht,
    unsigned int width,
    unsigned int height)
{
    dim3 block(8, 8, 1);
    dim3 grid(cuda_iDivUp(width, block.x), cuda_iDivUp(height, block.y), 1);

    updateHeightmapKernel<<<grid, block>>>(d_heightMap, d_ht, width);


    cudaDeviceSynchronize();
}
//----------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------
//put gaussian into zeno node

float urand()
{
    return rand() / (float)RAND_MAX;
}

// Generates Gaussian random number with mean 0 and standard deviation 1.
float gauss()
{
    float u1 = urand();
    float u2 = urand();

    if (u1 < 1e-6f)
    {
        u1 = 1e-6f;
    }

    return sqrtf(-2 * logf(u1)) * cosf(2*CUDART_PI_F * u2);
}

//free function
float phillips(float Kx, float Ky, float Vdir, float V, float A, float dir_depend, float g)
//float phillips(std::shared_ptr<OceanFFT> cuOceanObj)
{
            float k_squared = Kx * Kx + Ky * Ky;

            if (k_squared == 0.0f)
            {
                return 0.0f;
            }

            // largest possible wave from constant wind of velocity v
            float L = V * V / g;

            float k_x = Kx / sqrtf(k_squared);
            float k_y = Ky / sqrtf(k_squared);
            float w_dot_k = k_x * cosf(Vdir) + k_y * sinf(Vdir);

            float phil= A * expf(-1.0f / (k_squared * L * L)) / (k_squared * k_squared) * w_dot_k * w_dot_k;

            // filter out waves moving opposite to wind
            if (w_dot_k < 0.0f)
            {
                phil *= dir_depend;
            }

            // damp out waves with very small length w << l
            float w = L / 10000;
            phil *= expf(-k_squared * w * w);

            return phil;
}

void generate_h0(std::shared_ptr<OceanFFT> OceanObj)
{
    for (unsigned int y = 0; y <= OceanObj->meshSize; y++)
    {
        for (unsigned int x = 0; x<=OceanObj->meshSize; x++)
        {
            float kx = (-(int)OceanObj->meshSize / 2.0f + x) * (2.0f * CUDART_PI_F / OceanObj->patchSize);
            float ky = (-(int)OceanObj->meshSize / 2.0f + y) * (2.0f * CUDART_PI_F / OceanObj->patchSize);

            float P = sqrtf(phillips(kx, ky, OceanObj->windDir, OceanObj->windSpeed, OceanObj->A, OceanObj->dir_depend,OceanObj->g));

            if (kx == 0.0f && ky == 0.0f)
            {
                P = 0.0f;
            }

            //float Er = urand()*2.0f-1.0f;
            //float Ei = urand()*2.0f-1.0f;
            float Er = zeno::gauss();
            float Ei = zeno::gauss();

            float h0_re = Er * P * CUDART_SQRT_HALF_F;
            float h0_im = Ei * P * CUDART_SQRT_HALF_F;

            int i = y*OceanObj->spectrumW+x;
            OceanObj->h_h0[i].x = h0_re;
            OceanObj->h_h0[i].y = h0_im;
        }
    }
}

//here begins zeno stuff
 
struct UpdateDx : zeno::INode {
    virtual void apply() override {
        printf("UpdateDxKernel::apply() called!\n");
        
        auto real_ocean = get_input<OceanFFT>("real_ocean");
        //have some questions need to ask for details.
        UpdateDxKernel(real_ocean->Dx, real_ocean->Dz, real_ocean->width, real_ocean->height);
    }
};
ZENDEFNODE(UpdateDx,
    { /* inputs:  */ { "real_ocean" }, 
      /* outputs: */ { }, 
      /* params: */  { }, 
      /* category: */ {"Ocean",}});


struct GenerateSpectrum : zeno::INode {
    virtual void apply() override {
        printf("GenerateSpectrumKernel::apply() called!\n");

        auto real_ocean = get_input<OceanFFT>("real_ocean");


        GenerateSpectrumKernel(real_ocean->d_h0, real_ocean->d_ht, real_ocean->Dx, 
        real_ocean->Dz, real_ocean->in_width, real_ocean->out_width, 
        real_ocean->out_height, real_ocean->animTime, real_ocean->patchSize);
    }
};
ZENDEFNODE(GenerateSpectrum,
    { /* inputs: */ { "real_ocean" }, 
    /* outputs: */ {},
    /* params: */  {}, 
    /* category: */ { "Ocean",}});


struct UpdateHeightmap : zeno::INode {
    virtual void apply() override {
        printf("UpdateHeightmapKernel::apply() called!\n");

        auto real_ocean = get_input<OceanFFT>("real_ocean");

        UpdateHeightmapKernel(real_ocean->d_heightMap, real_ocean->d_ht, real_ocean->width, real_ocean->height);
    }
};

ZENDEFNODE(UpdateHeightmap,
        { /* inputs: */ { "real_ocean"}, 
        /* outputs: */  { }, 
        /* params: */ { }, 
        /* category: */ {"Ocean",}});



struct MakeCuOcean : zeno::INode {
    void apply() override
    {

        auto cuOceanObj = std::make_shared<OceanFFT>();
        auto o2 = std::make_shared<OceanFFT>();

        cuOceanObj->meshSize = get_input<zeno::NumericObject>("meshSize")->get<int>();
        // create FFT plan
        //checkCudaErrors(cufftPlan2d(&cuOceanObj->fftPlan, cuOceanObj->meshSize, cuOceanObj->meshSize, CUFFT_C2C));
        cufftPlan2d(&cuOceanObj->fftPlan, cuOceanObj->meshSize, cuOceanObj->meshSize, CUFFT_C2C);

        //phillip spectrum
        phillips(cuOceanObj->Kx, cuOceanObj->Ky, cuOceanObj->Vdir, cuOceanObj->V, cuOceanObj->A, cuOceanObj->dir_depend, cuOceanObj->g);

        //other parameters
        cuOceanObj->WaveExponent = get_input<zeno::NumericObject>("WaveExponent")->get<int>();
        cuOceanObj->choppyness   = get_input<zeno::NumericObject>("choppyness")->get<float>();
        cuOceanObj->spectrumW    = get_input<zeno::NumericObject>("spectrumW")->get<int>();        
        cuOceanObj->spectrumH    = get_input<zeno::NumericObject>("spectrumH")->get<int>();
        cuOceanObj->frameCompare = get_input<zeno::NumericObject>("frameCompare")->get<int>();
        cuOceanObj->animationRate= get_input<zeno::NumericObject>("animationRate")->get<float>();
        cuOceanObj->prevTime = get_input<zeno::NumericObject>("prevTime")->get<float>();
        cuOceanObj->animTime = get_input<zeno::NumericObject>("animTime")->get<float>();        

        cudaMalloc((void**)&(cuOceanObj->d_h0), sizeof(float2)*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->d_ht), sizeof(float2)*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->d_slope),sizeof(float2)*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->g_hptr), sizeof(float)*cuOceanObj->meshSize*cuOceanObj->meshSize);
        cuOceanObj->g_hhptr = (float*)malloc(sizeof(float) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->g_sptr), sizeof(float2)*cuOceanObj->meshSize*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->Dx), sizeof(float2) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        cudaMalloc((void**)&(cuOceanObj->Dz), sizeof(float2) *cuOceanObj->meshSize*cuOceanObj->meshSize);
      

        cuOceanObj->g_hDz = (float2*)malloc(sizeof(float) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        cuOceanObj->g_hDx = (float2*)malloc(sizeof(float) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        // cudaMalloc((void**)&(cuOceanObj->g_hDz), sizeof(float2) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        // cudaMalloc((void**)&(cuOceanObj->g_hDx), sizeof(float2) *cuOceanObj->meshSize*cuOceanObj->meshSize);
        cuOceanObj->h_h0 = (float2*)malloc(sizeof(float2) *cuOceanObj->meshSize*cuOceanObj->meshSize);

        generate_h0(cuOceanObj);
        //cpu to gpu
        cudaMemcpy(cuOceanObj->d_h0, cuOceanObj->h_h0, cuOceanObj->spectrumSize, cudaMemcpyHostToDevice);
 
        set_output("gpuOcean", cuOceanObj);
    }
};

ZENDEFNODE(MakeCuOcean,
        { /* inputs:  */ { "WaveExponent", "choppyness", "spectrumW", "spectrumH", "frameCompare", "animationRate","prevTime", "animTime", "meshSize" }, 
          /* outputs: */ { "gpuOcean", }, 
          /* params: */  {  }, 
          /* category: */ {"Ocean",}});


//--------------------------------------------------------------------------------
//--------------------------------------------------------------------------------



struct OceanCompute : zeno::INode {
    void apply() override{

    // move to ocean 
    //CUDA Implementation
    // generate wave spectrum in frequency domain
    // execute inverse FFT to convert to spatial domain
    //CUDA TEST generate wave spectrum in frequency domain
    // execute inverse FFT to convert to spatial domain
    // update heightmap values
    //-----------------------------------------------------------------------------------
    
    auto t = get_input<zeno::NumericObject>("time")->get<float>();

    auto CalOcean = get_input<OceanFFT>("ocean_FFT");
    //  size_t num_bytes;

    // checkCudaErrors(cufftExecC2C(CalOcean->fftPlan, CalOcean->d_ht, CalOcean->d_ht, CUFFT_INVERSE));
    // checkCudaErrors(cufftExecC2C(CalOcean->fftPlan, CalOcean->Dx, CalOcean->Dx, CUFFT_INVERSE));    
    // checkCudaErrors(cufftExecC2C(CalOcean->fftPlan, CalOcean->Dz, CalOcean->Dz, CUFFT_INVERSE));
    
    //--------------------------------------------------------------------------------------------------
    GenerateSpectrumKernel(CalOcean->d_h0, CalOcean->d_ht, CalOcean->Dx, CalOcean->Dz, CalOcean->spectrumW, CalOcean->meshSize, CalOcean->meshSize, CalOcean->animTime, CalOcean->patchSize);
    cufftExecC2C(CalOcean->fftPlan, CalOcean->d_ht, CalOcean->d_ht, CUFFT_INVERSE);
    cufftExecC2C(CalOcean->fftPlan, CalOcean->Dx, CalOcean->Dx, CUFFT_INVERSE);    
    cufftExecC2C(CalOcean->fftPlan, CalOcean->Dz, CalOcean->Dz, CUFFT_INVERSE);
   

    UpdateHeightmapKernel(CalOcean->g_hptr, CalOcean->d_ht, CalOcean->meshSize, CalOcean->meshSize);
    GenerateSpectrumKernel(CalOcean->d_h0, CalOcean->d_ht,CalOcean->Dx, CalOcean->Dz, CalOcean->spectrumW, CalOcean->meshSize, CalOcean->meshSize, t, CalOcean->patchSize);
    cufftExecC2C(CalOcean->fftPlan, CalOcean->d_ht, CalOcean->d_ht, CUFFT_INVERSE);

    //generate vertex positions
    UpdateDxKernel(CalOcean->Dx, CalOcean->Dz, CalOcean->meshSize, CalOcean->meshSize);


    cudaMemcpy(CalOcean->d_h0, CalOcean->h_h0, CalOcean->spectrumSize, cudaMemcpyHostToDevice);

    set_output("OceanMesh", CalOcean);
    }
};


ZENDEFNODE(OceanCompute,
        { /* inputs:  */ {"time","ocean_FFT", "Grid",}, 
          /* outputs: */ { "OceanMesh", }, 
          /* params: */  {  }, 
          /* category: */ {"Ocean",}});






}






