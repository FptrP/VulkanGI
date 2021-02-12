#ifndef RENDER_OCT_HPP_INCLUDED
#define RENDER_OCT_HPP_INCLUDED

#include "postprocessing.hpp"


struct OctahedralRenderer {
  OctahedralRenderer(){}

  void init(DriverState &ds) {
    ds.pipelines.load_shader(ds.ctx, "cubemap_to_oct_fs", "src/shaders/cubemap_to_oct_frag.spv", vk::ShaderStageFlagBits::eFragment);
    renderer.init_attachment(vk::Format::eR32Sfloat);
    renderer.init(ds, 1, "cubemap_to_oct_fs");
  }

  void release(DriverState &ds) {
    renderer.release(ds);
  }

  void transformt_to_oct(DriverState &ds, drv::ImageViewID cubemap, vk::Sampler smp, drv::ImageViewID out_oct) {
    const auto &inf = out_oct->get_base_img()->get_info();  
    

    renderer.set_attachment(0, out_oct);
    renderer.set_image_sampler(0, cubemap, smp);
    renderer.set_render_area(inf.extent.width, inf.extent.height);
    renderer.render_and_wait(ds);
  }

private:

  PostProcessingPass<Nil, Nil> renderer;
};

#endif
