////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#include "Spline.h"

namespace Reaper::Math
{
Spline ConstructSpline(u32 order, const std::vector<glm::vec4>& controlPoints)
{
    const u32 controlPointCount = static_cast<u32>(controlPoints.size()); // cast
    const u32 knotCount = order + controlPointCount + 1;

    Spline spline;
    spline.order = order;

    // Copy control points
    spline.controlPoints.resize(controlPointCount);
    for (u32 i = 0; i < controlPointCount; ++i)
        spline.controlPoints[i] = controlPoints[i];

    // Increase surface bounds order
    spline.knots.resize(knotCount);
    for (u32 i = 0; i <= order; ++i)
    {
        spline.knots[i] = 0.0f;
        spline.knots[knotCount - 1 - i] = 1.0f;
    }

    // Eval interior knots
    for (u32 i = 0; i < (knotCount - 2 * (order - 1) - 3); ++i)
        spline.knots[order + i] = static_cast<float>(i) / static_cast<float>(knotCount - 2 * (order - 1) - 3);

    return spline;
}

namespace
{
    float evalSplinePrimitive(const Spline& spline, u32 i, u32 order, float t)
    {
        float res = 0.0f;
        float ratio1;
        float ratio2;

        if (order > 0)
        {
            if ((t == spline.knots[i]) || (spline.knots[i + order] == spline.knots[i]))
                ratio1 = 0.0f;
            else
                ratio1 = (t - spline.knots[i]) / (spline.knots[i + order] - spline.knots[i]);

            if ((t == spline.knots[i + order + 1]) || (spline.knots[i + order + 1] == spline.knots[i + 1]))
                ratio2 = 0.0f;
            else
                ratio2 = (spline.knots[i + order + 1] - t) / (spline.knots[i + order + 1] - spline.knots[i + 1]);

            if (ratio1 != 0.0f)
                res += ratio1 * evalSplinePrimitive(spline, i, order - 1, t);

            if (ratio2 != 0.0f)
                res += ratio2 * evalSplinePrimitive(spline, i + 1, order - 1, t);
        }
        else if (t >= spline.knots[i] && t <= spline.knots[i + 1])
            res = 1.0f;
        return res;
    }
} // namespace

glm::vec3 EvalSpline(const Spline& spline, float t)
{
    glm::vec3 result;
    float     sum = 0.0f;
    float     coeff;
    const u32 controlPointCount = static_cast<u32>(spline.controlPoints.size()); // cast

    for (u32 i = 0; i < controlPointCount; i++)
    {
        coeff = evalSplinePrimitive(spline, static_cast<unsigned int>(i), spline.order, t) * spline.controlPoints[i].w;
        result += glm::vec3(spline.controlPoints[i]) * coeff;
        sum += coeff;
    }
    result *= (1.0f / sum);
    return result;
}
} // namespace Reaper::Math
