#ifndef RENDERPASS_HPP_INCLUDED
#define RENDERPASS_HPP_INCLUDED

#include "context.hpp"
#include "resources.hpp"

namespace drv {

  struct SinglePassDesc {
    SinglePassDesc &add_color_attachment();
    SinglePassDesc &add_depth_attachment();

  private:
    std::vector<vk::AttachmentDescription> attachments;
  };

}


#endif