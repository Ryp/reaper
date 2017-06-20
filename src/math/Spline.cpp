////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2017 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Spline.h"

Spline::Spline(unsigned int degree)
:   m_degree(degree)
{}

Spline::~Spline() {}

void Spline::addControlPoint(const glm::vec3& point, float weight)
{
    ControlPoint    ct;

    ct.pos = point;
    ct.weight = weight;
    m_ctPoints.push_back(ct);
}

void Spline::build()
{
    m_knots = std::vector<float>(m_ctPoints.size() + m_degree + 1);
    std::size_t knotCount = m_knots.size();

    // Increase surface bounds order
    for (unsigned int i = 0; i <= m_degree; ++i)
    {
        m_knots[i] = 0.0f;
        m_knots[knotCount - 1 - i] = 1.0f;
    }
    // Eval interior knots
    for (unsigned int i = 0; i < (knotCount - 2 * (m_degree - 1) - 3); ++i)
        m_knots[m_degree + i] = static_cast<float>(i) / static_cast<float>(knotCount - 2 * (m_degree - 1) - 3);
}

glm::vec3 Spline::eval(float t) const
{
    glm::vec3   result;
    float       sum = 0.0f;
    float       coeff;

    for (std::size_t i = 0; i <m_ctPoints.size(); ++i)
    {
        coeff = evalSplinePrimitive(static_cast<unsigned int>(i), m_degree, t) * m_ctPoints[i].weight;
        result += m_ctPoints[i].pos * coeff;
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
        if ((t == m_knots[i]) || (m_knots[i + degree] == m_knots[i]))
            ratio1 = 0.0f;
        else
            ratio1 = (t - m_knots[i]) / (m_knots[i + degree] - m_knots[i]);
        if ((t == m_knots[i + degree + 1]) || (m_knots[i + degree + 1] == m_knots[i + 1]))
            ratio2 = 0.0f;
        else
            ratio2 = (m_knots[i + degree + 1] - t) / (m_knots[i + degree + 1] - m_knots[i + 1]);
        if (ratio1 != 0.0f)
            res += ratio1 * evalSplinePrimitive(i, degree - 1, t);
        if (ratio2 != 0.0f)
            res += ratio2 * evalSplinePrimitive(i + 1, degree - 1, t);
    }
    else if (t >= m_knots[i] && t <= m_knots[i + 1])
        res = 1.0f;
    return (res);
}
