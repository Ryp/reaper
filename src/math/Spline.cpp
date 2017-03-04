////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Spline.h"

const float Spline::DefaultWeight = 1.0f;

Spline::Spline(unsigned int degree)
:   _degree(degree)
{}

Spline::~Spline() {}

void Spline::addControlPoint(const glm::vec3& point, float weight)
{
    ControlPoint    ct;

    ct.pos = point;
    ct.weight = weight;
    _ctPoints.push_back(ct);
}

void Spline::build()
{
    _knotsVector = std::vector<float>(_ctPoints.size() + _degree + 1);
    std::size_t knots = _knotsVector.size();

    // Increase surface bounds order
    for (unsigned int i = 0; i <= _degree; ++i)
    {
        _knotsVector[i] = 0.0f;
        _knotsVector[knots - 1 - i] = 1.0f;
    }
    // Eval interior knots
    for (unsigned int i = 0; i < (knots - 2 * (_degree - 1) - 3); ++i)
        _knotsVector[_degree + i] = static_cast<float>(i) / static_cast<float>(knots - 2 * (_degree - 1) - 3);
}

glm::vec3 Spline::eval(float t) const
{
    glm::vec3   result;
    float       sum = 0.0f;
    float       coeff;

    for (std::size_t i = 0; i <_ctPoints.size(); ++i)
    {
        coeff = evalSplinePrimitive(static_cast<unsigned int>(i), _degree, t) * _ctPoints[i].weight;
        result += _ctPoints[i].pos * coeff;
        sum += coeff;
    }
    result *= (1.0f / sum);
    return (result);
}

float Spline::evalSplinePrimitive(unsigned int i, unsigned int degree, float t) const
{
    float   res = 0.0f;
    float   ratio1;
    float   ratio2;

    if (degree > 0)
    {
        if ((t == _knotsVector[i]) || (_knotsVector[i + degree] == _knotsVector[i]))
            ratio1 = 0.0f;
        else
            ratio1 = (t - _knotsVector[i]) / (_knotsVector[i + degree] - _knotsVector[i]);
        if ((t == _knotsVector[i + degree + 1]) || (_knotsVector[i + degree + 1] == _knotsVector[i + 1]))
            ratio2 = 0.0f;
        else
            ratio2 = (_knotsVector[i + degree + 1] - t) / (_knotsVector[i + degree + 1] - _knotsVector[i + 1]);
        if (ratio1 != 0.0f)
            res += ratio1 * evalSplinePrimitive(i, degree - 1, t);
        if (ratio2 != 0.0f)
            res += ratio2 * evalSplinePrimitive(i + 1, degree - 1, t);
    }
    else if (t >= _knotsVector[i] && t <= _knotsVector[i + 1])
        res = 1.0f;
    return (res);
}
