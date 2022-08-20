#include <optix.h>
#include <cuda/random.h>
#include <sutil/vec_math.h>
#include <cuda/helpers.h>
#include "optixPathTracer.h"
#include "TraceStuff.h"
#include "DisneyBSDF.h"

extern "C" {
__constant__ Params params;
}
//------------------------------------------------------------------------------
//
//
//
//------------------------------------------------------------------------------
static __inline__ __device__
vec3 RRTAndODTFit(vec3 v)
{
    vec3 a = v * (v + 0.0245786f) - 0.000090537f;
    vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
    return a / b;
}
static __inline__ __device__
vec3 ACESFitted(vec3 color, float gamma)
{
//    const mat3x3 ACESInputMat = mat3x3
//        (
//            0.59719, 0.35458, 0.04823,
//            0.07600, 0.90834, 0.01566,
//            0.02840, 0.13383, 0.83777
//        );
//    mat3x3 ACESOutputMat = mat3x3
//    (
//        1.60475, -0.53108, -0.07367,
//        -0.10208,  1.10813, -0.00605,
//        -0.00327, -0.07276,  1.07602
//    );
    vec3 v1 = vec3(0.59719, 0.35458, 0.04823);
    vec3 v2 = vec3(0.07600, 0.90834, 0.01566);
    vec3 v3 = vec3(0.02840, 0.13383, 0.83777);
    color = vec3(dot(color, v1), dot(color, v2), dot(color, v3));

    // Apply RRT and ODT
    color = RRTAndODTFit(color);

    v1 = vec3(1.60475, -0.53108, -0.07367);
    v2 = vec3(-0.10208,  1.10813, -0.00605);
    v3 = vec3(-0.00327, -0.07276,  1.07602);
    color = vec3(dot(color, v1), dot(color, v2), dot(color, v3));

    // Clamp to [0, 1]
    color = clamp(color, 0.0, 1.0);

    color = pow(color, vec3(1. / gamma));

    return color;
}
extern "C" __global__ void __raygen__rg()
{
    const int    w   = params.width;
    const int    h   = params.height;
    //const float3 eye = params.eye;
    const uint3  idx = optixGetLaunchIndex();
    const int    subframe_index = params.subframe_index;
    const CameraInfo cam = params.cam;

    unsigned int seed = tea<4>( idx.y*w + idx.x, subframe_index );

    float3 result = make_float3( 0.0f );
    int i = params.samples_per_launch;
    do
    {
        // The center of each pixel is at fraction (0.5,0.5)
        float2 subpixel_jitter = make_float2( rnd( seed ), rnd( seed ) );

        float2 d = 2.0f * make_float2(
                ( static_cast<float>( idx.x ) + subpixel_jitter.x ) / static_cast<float>( w ),
                ( static_cast<float>( idx.y ) + subpixel_jitter.y ) / static_cast<float>( h )
                ) - 1.0f;
        float3 ray_direction = normalize(cam.right * d.x + cam.up * d.y + cam.front);
        float3 ray_origin    = cam.eye;

        RadiancePRD prd;
        prd.emitted      = make_float3(0.f);
        prd.radiance     = make_float3(0.f);
        prd.attenuation  = make_float3(1.f);
        prd.attenuation2 = make_float3(1.f);
        prd.prob         = 1.0f;
        prd.prob2        = 1.0f;
        prd.countEmitted = true;
        prd.done         = false;
        prd.seed         = seed;
        prd.opacity      = 0;
        prd.flags        = 0;
        prd.is_inside    = false;
        prd.maxDistance  = 1e16f;
        prd.medium       = DisneyBSDF::PhaseFunctions::vacuum;

        prd.depth = 0;
        for( ;; )
        {
            traceRadiance(
                    params.handle,
                    ray_origin,
                    ray_direction,
                    1e-5f,  // tmin       // TODO: smarter offset
                    prd.maxDistance,  // tmax
                    &prd );

            //result += prd.emitted;
            if(prd.countEmitted==false || prd.depth>0)
                result += prd.radiance * prd.attenuation2/(prd.prob2);
            if(prd.countEmitted==true && prd.depth>0){
                prd.done = true;
            }
            if( prd.done ){
                
                break;
            }
            if(prd.depth>4){
               //float RRprob = clamp(length(prd.attenuation)/1.732f,0.01f,0.9f); 
                float RRprob = 0.9;
                if(rnd(prd.seed) > RRprob || prd.depth>8){
                    prd.done=true;

                }
                prd.attenuation = prd.attenuation / RRprob;
            }
            if(prd.countEmitted == true)
                prd.passed = true;
            ray_origin    = prd.origin;
            ray_direction = prd.direction;
            // if(prd.passed == false)
            //     ++depth;        
            //}else{
                //prd.passed = false;
            //}
        }
    }
    while( --i );

    const uint3    launch_index = optixGetLaunchIndex();
    const unsigned int image_index  = launch_index.y * params.width + launch_index.x;
    float3         accum_color  = result / static_cast<float>( params.samples_per_launch );

    if( subframe_index > 0 )
    {
        const float                 a = 1.0f / static_cast<float>( subframe_index+1 );
        const float3 accum_color_prev = make_float3( params.accum_buffer[ image_index ]);
        accum_color = lerp( accum_color_prev, accum_color, a );
    }
    /*if (launch_index.x == 0) {*/
        /*printf("%p\n", params.accum_buffer);*/
        /*printf("%p\n", params.frame_buffer);*/
    /*}*/
    params.accum_buffer[ image_index ] = make_float4( accum_color, 1.0f);
    vec3 aecs_fitted = ACESFitted(vec3(accum_color), 2.2);
    float3 out_color = aecs_fitted;
    params.frame_buffer[ image_index ] = make_color ( out_color );
}


extern "C" __global__ void __miss__radiance()
{
    MissData* rt_data  = reinterpret_cast<MissData*>( optixGetSbtDataPointer() );
    RadiancePRD* prd = getPRD();

    prd->passed = false;
    prd->countEmitted = false;

    if(prd->medium != DisneyBSDF::isotropic){
        prd->radiance = make_float3( rt_data->bg_color );
        prd->done      = true;
    }
    prd->attenuation *= DisneyBSDF::Transmission(prd->extinction,optixGetRayTmax());
    prd->origin += prd->direction * optixGetRayTmax();
    prd->direction = DisneyBSDF::SampleScatterDirection(prd->seed);
    float tmpPDF;
    prd->maxDistance = DisneyBSDF::SampleDistance(prd->seed,prd->extinction,tmpPDF);
    prd->scatterPDF= tmpPDF;
    prd->depth++;

    if(length(prd->attenuation)<1e-7f){
        prd->done = true;
    }

}
