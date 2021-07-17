#include "scitbx/array_family/boost_python/flex_fwd.h"
//#include <cudatbx/cuda_base.cuh>
#include "simtbx/kokkos/simulation.h"
#include "simtbx/kokkos/simulation_kernels.h"
#include "simtbx/kokkos/kokkos_utils.h"
#include "scitbx/array_family/flex_types.h"

#define THREADS_PER_BLOCK_X 128
#define THREADS_PER_BLOCK_Y 1
#define THREADS_PER_BLOCK_TOTAL (THREADS_PER_BLOCK_X * THREADS_PER_BLOCK_Y)

namespace simtbx {
namespace Kokkos {

  namespace af = scitbx::af;
  //refactor later into helper file
/*  static cudaError_t cudaMemcpyVectorDoubleToDevice(CUDAREAL *dst, const double *src, size_t vector_items) {
        CUDAREAL * temp = new CUDAREAL[vector_items];
        for (size_t i = 0; i < vector_items; i++) {
                temp[i] = src[i];
        }
        cudaError_t ret = cudaMemcpy(dst, temp, sizeof(*dst) * vector_items, cudaMemcpyHostToDevice);
        delete temp;
        return ret;
  }*/

  // extract subview from [start_index * length; (start_index + 1) * length)
  template <typename T>
  view_1d_t<T> extract_subview(view_1d_t<T> A, int start_index, int length) {
    return ::Kokkos::subview(A, ::Kokkos::pair<int, int>(start_index * length, (start_index + 1) * length ));
  }

  // make a unit vector pointing in same direction and report magnitude (both args can be same vector)
  double cpu_unitize(const double * vector, double * new_unit_vector) {

        double v1 = vector[1];
        double v2 = vector[2];
        double v3 = vector[3];

        double mag = sqrt(v1 * v1 + v2 * v2 + v3 * v3);

        if (mag != 0.0) {
                // normalize it
                new_unit_vector[0] = mag;
                new_unit_vector[1] = v1 / mag;
                new_unit_vector[2] = v2 / mag;
                new_unit_vector[3] = v3 / mag;
        } else {
                // can't normalize, report zero vector 
                new_unit_vector[0] = 0.0;
                new_unit_vector[1] = 0.0;
                new_unit_vector[2] = 0.0;
                new_unit_vector[3] = 0.0;
        }
        return mag;
  }

  void
  exascale_api::show(){
    SCITBX_EXAMINE(SIM.roi_xmin);
    SCITBX_EXAMINE(SIM.roi_xmax);
    SCITBX_EXAMINE(SIM.roi_ymin);
    SCITBX_EXAMINE(SIM.roi_ymax);
    SCITBX_EXAMINE(SIM.oversample);
    SCITBX_EXAMINE(SIM.point_pixel);
    SCITBX_EXAMINE(SIM.pixel_size);
    SCITBX_EXAMINE(m_subpixel_size);
    SCITBX_EXAMINE(m_steps);
    SCITBX_EXAMINE(SIM.detector_thickstep);
    SCITBX_EXAMINE(SIM.detector_thicksteps);
    SCITBX_EXAMINE(SIM.detector_thick);
    SCITBX_EXAMINE(SIM.detector_attnlen);
    SCITBX_EXAMINE(SIM.curved_detector);
    SCITBX_EXAMINE(SIM.distance);
    SCITBX_EXAMINE(SIM.close_distance);
    SCITBX_EXAMINE(SIM.dmin);
    SCITBX_EXAMINE(SIM.phi0);
    SCITBX_EXAMINE(SIM.phistep);
    SCITBX_EXAMINE(SIM.phisteps);
    SCITBX_EXAMINE(SIM.sources);
    SCITBX_EXAMINE(SIM.mosaic_spread);
    SCITBX_EXAMINE(SIM.mosaic_domains);
    SCITBX_EXAMINE(SIM.Na);
    SCITBX_EXAMINE(SIM.Nb);
    SCITBX_EXAMINE(SIM.Nc);
    SCITBX_EXAMINE(SIM.fluence);
    SCITBX_EXAMINE(SIM.spot_scale);
    SCITBX_EXAMINE(SIM.integral_form);
    SCITBX_EXAMINE(SIM.default_F);
    SCITBX_EXAMINE(SIM.interpolate);
    SCITBX_EXAMINE(SIM.nopolar);
    SCITBX_EXAMINE(SIM.polarization);
    SCITBX_EXAMINE(SIM.fudge);
  }

  void
  exascale_api::add_energy_channel_from_kokkos_amplitudes_cuda(
    int const& ichannel,
    simtbx::Kokkos::kokkos_energy_channels & gec,
    simtbx::Kokkos::kokkos_detector & gdt
  ){/*
    cudaSafeCall(cudaSetDevice(SIM.device_Id));

    // transfer source_I, source_lambda
    // the int arguments are for sizes of the arrays
    int source_count = SIM.sources;
    transfer_double2kokkos(m_source_I, SIM.source_I, source_count);
    transfer_double2kokkos(m_source_lambda, SIM.source_lambda, source_count);
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_I, SIM.source_I, SIM.sources));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_lambda, SIM.source_lambda, SIM.sources));

    // magic happens here(?): take pointer from singleton, temporarily use it for add Bragg iteration:
    vector_cudareal_t current_channel_Fhkl = gec.d_channel_Fhkl[ichannel];

    cudaDeviceProp deviceProps = { 0 };
    cudaSafeCall(cudaGetDeviceProperties(&deviceProps, SIM.device_Id));
    int smCount = deviceProps.multiProcessorCount;
    dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
    dim3 numBlocks(smCount * 8, 1);

    std::size_t panel_size = gdt.m_slow_dim_size * gdt.m_fast_dim_size;

    // the for loop around panels.  Offsets given.
    for (std::size_t panel_id = 0; panel_id < gdt.m_panel_count; panel_id++){
      // loop thru panels and increment the array ptrs
      nanoBraggSpotsCUDAKernel<<<numBlocks, threadsPerBlock>>>(
      gdt.m_slow_dim_size, gdt.m_fast_dim_size, SIM.roi_xmin,
      SIM.roi_xmax, SIM.roi_ymin, SIM.roi_ymax, SIM.oversample, SIM.point_pixel,
      SIM.pixel_size, m_subpixel_size, m_steps, SIM.detector_thickstep, SIM.detector_thicksteps,
      SIM.detector_thick, SIM.detector_attnlen,
      &(gdt.m_sdet_vector[m_vector_length * panel_id]),
      &(gdt.m_fdet_vector[m_vector_length * panel_id]),
      &(gdt.m_odet_vector[m_vector_length * panel_id]),
      &(gdt.m_pix0_vector[m_vector_length * panel_id]),
      SIM.curved_detector, gdt.metrology.dists[panel_id], gdt.metrology.dists[panel_id], m_beam_vector,
      gdt.metrology.Xbeam[panel_id], gdt.metrology.Ybeam[panel_id],
      SIM.dmin, SIM.phi0, SIM.phistep, SIM.phisteps, m_spindle_vector,
      SIM.sources, m_source_X, m_source_Y, m_source_Z,
      m_source_I, m_source_lambda, m_a0, m_b0,
      m_c0, SIM.xtal_shape, SIM.mosaic_spread, SIM.mosaic_domains, m_mosaic_umats,
      SIM.Na, SIM.Nb, SIM.Nc, SIM.V_cell,
      m_water_size, m_water_F, m_water_MW, simtbx::nanoBragg::r_e_sqr, SIM.fluence,
      simtbx::nanoBragg::Avogadro, SIM.spot_scale, SIM.integral_form, SIM.default_F,
      SIM.interpolate, current_channel_Fhkl, gec.host_FhklParams, SIM.nopolar,
      m_polar_vector, SIM.polarization, SIM.fudge,
      // &(gdt.m_maskimage[panel_size * panel_id]),
      NULL, 
      // return arrays:
      &(gdt.m_floatimage[panel_size * panel_id]),
      &(gdt.m_omega_reduction[panel_size * panel_id]),
      &(gdt.m_max_I_x_reduction[panel_size * panel_id]),
      &(gdt.m_max_I_y_reduction[panel_size * panel_id]),
      &(gdt.m_rangemap[panel_size * panel_id]);

      cudaSafeCall(cudaPeekAtLastError());
    }
    cudaSafeCall(cudaDeviceSynchronize());

    //don't want to free the gec data when the nanoBragg goes out of scope, so switch the pointer
    // cu_current_channel_Fhkl = NULL;

    add_array(gdt.m_accumulate_floatimage, gdt.m_floatimage);
  */}

  void
  exascale_api::add_energy_channel_mask_allpanel_cuda(
    int const& ichannel,
    simtbx::Kokkos::kokkos_energy_channels & gec,
    simtbx::Kokkos::kokkos_detector & gdt,
    af::shared<bool> all_panel_mask
  ){/*
        cudaSafeCall(cudaSetDevice(SIM.device_Id));

        // here or there, need to convert the all_panel_mask (3D map) into a 1D list of accepted pixels
        // coordinates for the active pixel list are absolute offsets into the detector array
        af::shared<int> active_pixel_list;
        const bool* jptr = all_panel_mask.begin();
        for (int j=0; j < all_panel_mask.size(); ++j){
          if (jptr[j]) {
            active_pixel_list.push_back(j);
          }
        }
        gdt.set_active_pixels_on_KOKKOS(active_pixel_list);

        // transfer source_I, source_lambda
        // the int arguments are for sizes of the arrays
        cudaSafeCall(cudaMemcpyVectorDoubleToDevice(m_source_I, SIM.source_I, SIM.sources));
        cudaSafeCall(cudaMemcpyVectorDoubleToDevice(m_source_lambda, SIM.source_lambda, SIM.sources));

        // magic happens here: take pointer from singleton, temporarily use it for add Bragg iteration:
        vector_cudareal_t current_channel_Fhkl = gec.d_channel_Fhkl[ichannel];

        cudaDeviceProp deviceProps = { 0 };
        cudaSafeCall(cudaGetDeviceProperties(&deviceProps, SIM.device_Id));
        int smCount = deviceProps.multiProcessorCount;
        dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
        dim3 numBlocks(smCount * 8, 1);

        // for call for all panels at the same time

          debranch_maskall_CUDAKernel<<<numBlocks, threadsPerBlock>>>(
          gdt.m_panel_count, gdt.m_slow_dim_size, gdt.m_fast_dim_size, active_pixel_list.size(),
          SIM.oversample, SIM.point_pixel,
          SIM.pixel_size, m_subpixel_size, m_steps,
          SIM.detector_thickstep, SIM.detector_thicksteps,
          SIM.detector_thick, SIM.detector_attnlen,
          m_vector_length,
          gdt.m_sdet_vector,
          gdt.m_fdet_vector,
          gdt.m_odet_vector,
          gdt.m_pix0_vector,
          gdt.m_distance, gdt.m_distance, m_beam_vector,
          gdt.m_Xbeam, gdt.m_Ybeam,
          SIM.dmin, SIM.phi0, SIM.phistep, SIM.phisteps, m_spindle_vector,
          SIM.sources, m_source_X, m_source_Y, m_source_Z,
          m_source_I, m_source_lambda, m_a0, m_b0,
          m_c0, SIM.xtal_shape, SIM.mosaic_domains, m_mosaic_umats,
          SIM.Na, SIM.Nb, SIM.Nc, SIM.V_cell,
          m_water_size, m_water_F, m_water_MW, simtbx::nanoBragg::r_e_sqr, SIM.fluence,
          simtbx::nanoBragg::Avogadro, SIM.spot_scale, SIM.integral_form, SIM.default_F,
          current_channel_Fhkl, gec.host_FhklParams, SIM.nopolar,
          m_polar_vector, SIM.polarization, SIM.fudge,
          gdt.m_active_pixel_list,
          // return arrays:
          gdt.m_floatimage,
          gdt.m_omega_reduction,
          gdt.m_max_I_x_reduction,
          gdt.m_max_I_y_reduction,
          gdt.m_rangemap);

          cudaSafeCall(cudaPeekAtLastError());
        cudaSafeCall(cudaDeviceSynchronize());

        //don't want to free the gec data when the nanoBragg goes out of scope, so switch the pointer
        // cu_current_channel_Fhkl = NULL;

        add_array(gdt.m_accumulate_floatimage, gdt.m_floatimage);
  */}


  void
  exascale_api::add_background_cuda(simtbx::Kokkos::kokkos_detector & gdt) {
        // cudaSafeCall(cudaSetDevice(SIM.device_Id));

        // transfer source_I, source_lambda
        // the int arguments are for sizes of the arrays
        int sources_count = SIM.sources;
        transfer_double2kokkos(m_source_I, SIM.source_I, sources_count);
        transfer_double2kokkos(m_source_lambda, SIM.source_lambda, sources_count);
        // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(m_source_I, SIM.source_I, SIM.sources));
        // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(m_source_lambda, SIM.source_lambda, SIM.sources));

        vector_cudareal_t stol_of("stol_of", SIM.stols);
        transfer_X2kokkos(stol_of, SIM.stol_of, SIM.stols);
        // CUDAREAL * cu_stol_of;
        // cudaSafeCall(cudaMalloc((void ** )&cu_stol_of, sizeof(*cu_stol_of) * SIM.stols));
        // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_stol_of, SIM.stol_of, SIM.stols));

        vector_cudareal_t Fbg_of("Fbg_of", SIM.stols);
        transfer_X2kokkos(Fbg_of, SIM.Fbg_of, SIM.stols);
        // CUDAREAL * cu_Fbg_of;
        // cudaSafeCall(cudaMalloc((void ** )&cu_Fbg_of, sizeof(*cu_Fbg_of) * SIM.stols));
        // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_Fbg_of, SIM.Fbg_of, SIM.stols));

        // cudaDeviceProp deviceProps = { 0 };
        // cudaSafeCall(cudaGetDeviceProperties(&deviceProps, SIM.device_Id));
        // int smCount = deviceProps.multiProcessorCount;
        // dim3 threadsPerBlock(THREADS_PER_BLOCK_X, THREADS_PER_BLOCK_Y);
        // dim3 numBlocks(smCount * 8, 1);

        //  initialize the device memory within a kernel.
        //  modify the arguments to initialize multipanel detector.
        ::Kokkos::parallel_for("nanoBraggSpotsInit", gdt.m_panel_count * gdt.m_slow_dim_size * gdt.m_fast_dim_size, KOKKOS_LAMBDA (const int& j) {
          gdt.m_floatimage(j) = 0;
          gdt.m_omega_reduction(j) = 0;
          gdt.m_max_I_x_reduction(j) = 0;
          gdt.m_max_I_y_reduction(j) = 0;
          gdt.m_rangemap(j) = false;
        });       
        // nanoBraggSpotsInitCUDAKernel<<<numBlocks, threadsPerBlock>>>(
        //   gdt.m_panel_count * gdt.m_slow_dim_size, gdt.m_fast_dim_size,
        //   gdt.cu_floatimage, gdt.cu_omega_reduction,
        //   gdt.cu_max_I_x_reduction, gdt.cu_max_I_y_reduction,
        //   gdt.cu_rangemap);
        // cudaSafeCall(cudaPeekAtLastError());
        // cudaSafeCall(cudaDeviceSynchronize());

        std::size_t panel_size = gdt.m_slow_dim_size * gdt.m_fast_dim_size;

        // the for loop around panels.  Offsets given.
        for (std::size_t panel_id = 0; panel_id < gdt.m_panel_count; panel_id++) {
          add_background(SIM.sources,
          SIM.oversample,
          SIM.pixel_size, gdt.m_slow_dim_size, gdt.m_fast_dim_size, SIM.detector_thicksteps,
          SIM.detector_thickstep, SIM.detector_attnlen,
          extract_subview(gdt.m_sdet_vector, panel_id, m_vector_length),
          extract_subview(gdt.m_fdet_vector, panel_id, m_vector_length),
          extract_subview(gdt.m_odet_vector, panel_id, m_vector_length),
          extract_subview(gdt.m_pix0_vector, panel_id, m_vector_length),
          gdt.metrology.dists[panel_id], SIM.point_pixel, SIM.detector_thick,
          m_source_X, m_source_Y, m_source_Z,
          m_source_lambda, m_source_I,
          SIM.stols, stol_of, Fbg_of,
          SIM.nopolar, SIM.polarization, m_polar_vector,
          simtbx::nanoBragg::r_e_sqr, SIM.fluence, SIM.amorphous_molecules,
          // returns:
          extract_subview(gdt.m_floatimage, panel_id, panel_size));

          // cudaSafeCall(cudaPeekAtLastError());
        }
        // cudaSafeCall(cudaDeviceSynchronize());
        ::Kokkos::fence();
        add_array(gdt.m_accumulate_floatimage, gdt.m_floatimage);

        // cudaSafeCall(cudaFree(cu_stol_of));
        // cudaSafeCall(cudaFree(cu_Fbg_of));
  }

  void
  exascale_api::allocate_cuda() {
    //cudaSafeCall(cudaSetDevice(SIM.device_Id));

    // water_size not defined in class, CLI argument, defaults to 0
    double water_size = 0.0;
    // missing constants
    double water_F = 2.57;
    double water_MW = 18.0;

    // make sure we are normalizing with the right number of sub-steps
    int nb_steps = SIM.phisteps * SIM.mosaic_domains * SIM.oversample * SIM.oversample;
    double nb_subpixel_size = SIM.pixel_size / SIM.oversample;

    //create transfer arguments to device space
    m_subpixel_size = nb_subpixel_size; //check for conflict?
    m_steps = nb_steps; //check for conflict?

    // presumably thickness and attenuation can be migrated to the gpu detector class XXX FIXME
    //cu_detector_thick = SIM.detector_thick;
    //cu_detector_mu = SIM.detector_attnlen; // synonyms
    //cu_distance = SIM.distance; // distance and close distance, detector properties? XXX FIXME
    //cu_close_distance = SIM.close_distance;

    m_water_size = water_size;
    m_water_F = water_F;
    m_water_MW = water_MW;

    //const int vector_length = 4;

    transfer_double2kokkos(m_beam_vector, SIM.beam_vector, m_vector_length);
    transfer_double2kokkos(m_spindle_vector, SIM.spindle_vector, m_vector_length);
    transfer_double2kokkos(m_a0, SIM.a0, m_vector_length);
    transfer_double2kokkos(m_b0, SIM.b0, m_vector_length);
    transfer_double2kokkos(m_c0, SIM.c0, m_vector_length);

    // cudaSafeCall(cudaMalloc((void ** )&cu_beam_vector, sizeof(*cu_beam_vector) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_beam_vector, SIM.beam_vector, vector_length));

    // cudaSafeCall(cudaMalloc((void ** )&cu_spindle_vector, sizeof(*cu_spindle_vector) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_spindle_vector, SIM.spindle_vector, vector_length));

    // cudaSafeCall(cudaMalloc((void ** )&cu_a0, sizeof(*cu_a0) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_a0, SIM.a0, vector_length));

    // cudaSafeCall(cudaMalloc((void ** )&cu_b0, sizeof(*cu_b0) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_b0, SIM.b0, vector_length));

    // cudaSafeCall(cudaMalloc((void ** )&cu_c0, sizeof(*cu_c0) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_c0, SIM.c0, vector_length));

    // Unitize polar vector before sending it to the GPU.
    // Optimization do it only once here rather than multiple time per pixel in the GPU.
    double polar_vector_unitized[4];
    cpu_unitize(SIM.polar_vector, polar_vector_unitized);
    transfer_double2kokkos(m_polar_vector, polar_vector_unitized, m_vector_length);

    int sources_count = SIM.sources;
    transfer_double2kokkos(m_source_X, SIM.source_X, sources_count);
    transfer_double2kokkos(m_source_Y, SIM.source_Y, sources_count);
    transfer_double2kokkos(m_source_Z, SIM.source_Z, sources_count);
    transfer_double2kokkos(m_source_I, SIM.source_I, sources_count);
    transfer_double2kokkos(m_source_lambda, SIM.source_lambda, sources_count);

    int mosaic_domains_count = SIM.mosaic_domains;
    transfer_double2kokkos(m_mosaic_umats, SIM.mosaic_umats, mosaic_domains_count * 9);
    
    // cudaSafeCall(cudaMalloc((void ** )&cu_polar_vector, sizeof(*cu_polar_vector) * vector_length));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_polar_vector, polar_vector_unitized, vector_length));

    // cudaSafeCall(cudaMalloc((void ** )&cu_source_X, sizeof(*cu_source_X) * sources_count));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_X, SIM.source_X, sources_count));

    // cudaSafeCall(cudaMalloc((void ** )&cu_source_Y, sizeof(*cu_source_Y) * sources_count));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_Y, SIM.source_Y, sources_count));

    // cudaSafeCall(cudaMalloc((void ** )&cu_source_Z, sizeof(*cu_source_Z) * sources_count));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_Z, SIM.source_Z, sources_count));

    // cudaSafeCall(cudaMalloc((void ** )&cu_source_I, sizeof(*cu_source_I) * sources_count));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_I, SIM.source_I, sources_count));

    // cudaSafeCall(cudaMalloc((void ** )&cu_source_lambda, sizeof(*cu_source_lambda) * sources_count));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_source_lambda, SIM.source_lambda, sources_count));

    // cudaSafeCall(cudaMalloc((void ** )&cu_mosaic_umats, sizeof(*cu_mosaic_umats) * mosaic_domains_count * 9));
    // cudaSafeCall(cudaMemcpyVectorDoubleToDevice(cu_mosaic_umats, SIM.mosaic_umats, mosaic_domains_count * 9));
  };

/*  exascale_api::~exascale_api(){
    cudaSafeCall(cudaSetDevice(SIM.device_Id));

        cudaSafeCall(cudaFree(cu_beam_vector));
        cudaSafeCall(cudaFree(cu_spindle_vector));
        cudaSafeCall(cudaFree(cu_source_X));
        cudaSafeCall(cudaFree(cu_source_Y));
        cudaSafeCall(cudaFree(cu_source_Z));
        cudaSafeCall(cudaFree(cu_source_I));
        cudaSafeCall(cudaFree(cu_source_lambda));
        cudaSafeCall(cudaFree(cu_a0));
        cudaSafeCall(cudaFree(cu_b0));
        cudaSafeCall(cudaFree(cu_c0));
        cudaSafeCall(cudaFree(cu_mosaic_umats));
        cudaSafeCall(cudaFree(cu_polar_vector));
  }
*/

} // Kokkos
} // simtbx