////////////////////////////////////////////////////////////////////////////////
/// Reaper
///
/// Copyright (c) 2015-2021 Thibault Schueller
/// This file is distributed under the MIT License
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/matrix_major_storage.hpp>

#include <array>
#include <type_traits>

template <typename T>
struct alignas(8) hlsl_vector2
{
    T x;
    T y;

    hlsl_vector2() = default;
    hlsl_vector2(const glm::tvec2<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
    }

    operator glm::tvec2<T>() const { return glm::tvec2<T>(x, y); }
};

// NOTE: This type has no 'alignas()' specifier.
// I could not find a way to force alignment to 16 bytes while keeping the size to 12 bytes.
template <typename T>
struct hlsl_vector3
{
    T x;
    T y;
    T z;

    hlsl_vector3() = default;
    hlsl_vector3(const glm::tvec3<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
    }

    operator glm::tvec3<T>() const { return glm::tvec3<T>(x, y, z); }
};

// This type is used in 3x3 matrices to add the necessary padding
// This allows us then to use a plain array and get the element[n] syntax.
template <typename T>
struct alignas(16) hlsl_vector3_with_padding
{
    T x;
    T y;
    T z;
    T _pad;

    hlsl_vector3_with_padding() = default;
    hlsl_vector3_with_padding(const glm::tvec3<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
    }

    operator glm::tvec3<T>() const { return glm::tvec3<T>(x, y, z); }
};

template <typename T>
struct alignas(16) hlsl_vector4
{
    T x;
    T y;
    T z;
    T w;

    hlsl_vector4() = default;
    hlsl_vector4(const glm::tvec4<T>& other)
    {
        this->x = other.x;
        this->y = other.y;
        this->z = other.z;
        this->w = other.w;
    }

    operator glm::tvec4<T>() const { return glm::tvec4<T>(x, y, z, w); }
};

enum MatrixStorage
{
    ColumnMajor,
    RowMajor
};

template <MatrixStorage Storage, typename T>
struct alignas(16) hlsl_matrix3x3
{
    hlsl_vector3_with_padding<T> element[3];

    hlsl_matrix3x3() = default;
    hlsl_matrix3x3(const glm::tmat3x3<T>& other)
    {
        if constexpr (Storage == RowMajor)
        {
            this->element[0] = glm::row(other, 0);
            this->element[1] = glm::row(other, 1);
            this->element[2] = glm::row(other, 2);
        }
        else
        {
            this->element[0] = glm::column(other, 0);
            this->element[1] = glm::column(other, 1);
            this->element[2] = glm::column(other, 2);
        }
    }

    operator glm::tmat3x3<T>() const
    {
        if constexpr (Storage == RowMajor)
            return glm::rowMajor3(glm::tvec3<T>(element[0]), glm::tvec3<T>(element[1]), glm::tvec3<T>(element[2]));
        else
            return glm::colMajor3(glm::tvec3<T>(element[0]), glm::tvec3<T>(element[1]), glm::tvec3<T>(element[2]));
    }
};

template <MatrixStorage Storage, typename T>
struct alignas(16) hlsl_matrix3x4
{
    // Template madness because row major and column major don't have the same storage
    // but we still want to specify storage using a template.
    using Matrix3x4RowMajorStorage = std::array<hlsl_vector4<T>, 3>;
    using Matrix3x4ColMajorStorage = std::array<hlsl_vector3_with_padding<T>, 4>;
    using Matrix3x4Storage =
        std::conditional_t<Storage == RowMajor, Matrix3x4RowMajorStorage, Matrix3x4ColMajorStorage>;

    Matrix3x4Storage element;

    hlsl_matrix3x4() = default;
    hlsl_matrix3x4(const glm::tmat4x3<T>& other)
    {
        if constexpr (Storage == RowMajor)
        {
            this->element[0] = glm::row(other, 0);
            this->element[1] = glm::row(other, 1);
            this->element[2] = glm::row(other, 2);
        }
        else
        {
            this->element[0] = glm::column(other, 0);
            this->element[1] = glm::column(other, 1);
            this->element[2] = glm::column(other, 2);
            this->element[3] = glm::column(other, 3);
        }
    }

    operator glm::tmat4x3<T>() const
    {
        if constexpr (Storage == RowMajor)
        {
            return glm::tmat4x3<T>(glm::tvec3<T>(element[0].x, element[1].x, element[2].x),
                                   glm::tvec3<T>(element[0].y, element[1].y, element[2].y),
                                   glm::tvec3<T>(element[0].z, element[1].z, element[2].z),
                                   glm::tvec3<T>(element[0].w, element[1].w, element[2].w));
        }
        else
        {
            return glm::tmat4x3<T>(glm::tvec3<T>(element[0]), glm::tvec3<T>(element[1]), glm::tvec3<T>(element[2]),
                                   glm::tvec3<T>(element[3]));
        }
    }
};

template <MatrixStorage Storage, typename T>
struct alignas(16) hlsl_matrix4x4
{
    hlsl_vector4<T> element[4];

    hlsl_matrix4x4() = default;
    hlsl_matrix4x4(const glm::tmat4x4<T>& other)
    {
        if constexpr (Storage == RowMajor)
        {
            this->element[0] = glm::row(other, 0);
            this->element[1] = glm::row(other, 1);
            this->element[2] = glm::row(other, 2);
            this->element[3] = glm::row(other, 3);
        }
        else
        {
            this->element[0] = glm::column(other, 0);
            this->element[1] = glm::column(other, 1);
            this->element[2] = glm::column(other, 2);
            this->element[3] = glm::column(other, 3);
        }
    }

    operator glm::tmat4x4<T>() const
    {
        if constexpr (Storage == RowMajor)
        {
            return glm::rowMajor4(glm::tvec4<T>(element[0]), glm::tvec4<T>(element[1]), glm::tvec4<T>(element[2]),
                                  glm::tvec4<T>(element[3]));
        }
        else
        {
            return glm::colMajor4(glm::tvec4<T>(element[0]), glm::tvec4<T>(element[1]), glm::tvec4<T>(element[2]),
                                  glm::tvec4<T>(element[3]));
        }
    }
};
