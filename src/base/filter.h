//
// Created by Mike on 2021/12/8.
//

#pragma once

#include <base/scene_node.h>

namespace luisa::render {

class Filter : public SceneNode {

public:
    Filter() noexcept : SceneNode{SceneNode::Tag::FILTER} {}

};

}