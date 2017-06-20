////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <list>
#include <vector>

#include <glm/vec3.hpp>

#include "MathExport.h"

class REAPER_MATH_API Spline
{
    struct ControlPoint {
        glm::vec3   pos;
        float       weight;
    };

    static constexpr float DefaultWeight = 1.f;

public:
    explicit Spline(unsigned int degree);
    ~Spline();

private:
    Spline(const Spline& other);
    Spline& operator=(const Spline& other);

public:
    /**
     * @brief adds NURBS control point
     * @param point
     * @param weight only positive values are covered for now
     */
    void        addControlPoint(const glm::vec3& point, float weight = DefaultWeight);
    void        build();
    glm::vec3   eval(float t) const; // NOTE t C [0;1]

private:
    float    evalSplinePrimitive(unsigned int i, unsigned int d, float t) const;

private:
    const unsigned int          m_degree;
    std::vector<ControlPoint>   m_ctPoints;
    std::vector<float>          m_knots;
};
