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

class Spline
{
    typedef struct {
        glm::vec3   pos;
        float       weight;
    } ControlPoint;
    static const float  DefaultWeight;

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
    const unsigned int          _degree;
    std::vector<ControlPoint>   _ctPoints;
    std::vector<float>          _knotsVector;
};
