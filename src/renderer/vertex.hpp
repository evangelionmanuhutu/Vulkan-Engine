// Copyright 2025, Evangelion Manuhutu

#ifndef VERTEX_HPP
#define VERTEX_HPP

#include <vulkan/vulkan.h>
#include <array>

#include <glm/glm.hpp>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 color;
    glm::vec2 texCoord;
};

#endif