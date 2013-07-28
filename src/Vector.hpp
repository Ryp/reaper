#ifndef VECTOR_HPP
#define VECTOR_HPP

#include <vector>
#include <cmath>

template <unsigned int size, class type>
class BaseVector
{
  typedef BaseVector<size, type> Vec;

public:
  BaseVector() : _vector(size) {}
  BaseVector(type val) : _vector(size, val) {}
  BaseVector(const Vec& other) : _vector(other._vector) {}
  virtual ~BaseVector() {}

public:
  Vec& operator=(const Vec& other)
  {
    if (this != &other)
      _vector = other._vector;
    return (*this);
  }

  bool	operator==(const Vec& other) const
  {
    for (unsigned int i = 0; i < size; ++i)
      if ((*this)[i] != other[i])
	return (false);
    return (true);
  }

  bool	operator!=(const Vec& other) const
  {
    return (!(*this == other));
  }

  Vec operator+(const Vec& other) const
  {
    Vec tmp(*this);

    for (unsigned i = 0; i < size; ++i)
      tmp[i] += other._vector[i];
    return (tmp);
  }

  Vec operator+=(const Vec& other)
  {
    for (unsigned i = 0; i < size; ++i)
      _vector[i] += other._vector[i];
    return (*this);
  }

  Vec  operator-(const Vec & other) const
  {
    Vec tmp(*this);

    for (unsigned i = 0; i < size; ++i)
      tmp[i] -= other._vector[i];
    return (tmp);
  }

  Vec  operator-() const
  {
    Vec tmp(*this);

    for (unsigned i = 0; i < size; ++i)
      tmp[i] = -_vector[i];
    return (tmp);
  }

  type operator*(const Vec& other) const
  {
    type tmp = 0;

    for (unsigned i = 0; i < size; ++i)
      tmp += _vector[i] * other._vector[i];
    return (tmp);
  }

  Vec operator*(type coeff) const
  {
    Vec tmp(*this);

    tmp.scale(coeff);
    return (tmp);
  }

  Vec operator*=(type coeff)
  {
    scale(coeff);
    return (*this);
  }

  Vec operator/(type coeff) const
  {
    Vec tmp(*this);

    tmp.scale(1 / coeff);
    return (tmp);
  }

  void scale(type coeff)
  {
    for (unsigned i = 0; i < size; ++i)
      _vector[i] *= coeff;
  }

  void scale(const Vec& other)
  {
    for (unsigned i = 0; i < size; ++i)
      _vector[i] *= other[i];
  }

  double length()
  {
    double l = 0;

    for (unsigned i = 0; i < size; ++i)
      l += static_cast<double>(_vector[i] * _vector[i]);
    l = sqrt(static_cast<double>(l));
    return (l);
  }

  void normalize()
  {
    scale(1.0f / length());
  }

  template<class type2>
  BaseVector<size, type2> convert() const
  {
    BaseVector<size, type2> vec;

    for (unsigned i = 0; i < size; ++i)
      vec[i] = static_cast<type2>(round(static_cast<type>(_vector[i])));
    return (vec);
  }

public:
  type &	operator[](int idx)
  {
    return (_vector[idx]);
  }

  const type &	operator[](int idx) const
  {
    return (_vector[idx]);
  }

protected:
  std::vector<type>	_vector;
};

template<class type>
class Vector4 : public BaseVector<4, type>
{
public:
  Vector4(type x, type y, type z, type w)
  {
    (*this)[0] = x;
    (*this)[1] = y;
    (*this)[2] = z;
    (*this)[3] = w;
  }

  Vector4(BaseVector<4, type> const & vec) : BaseVector<4, type>(vec) {}
  Vector4(type val = 0) : BaseVector<4, type>(val) {}
};

template<class type>
class Vector3 : public BaseVector<3, type>
{
public:
  Vector3(type x, type y, type z)
  {
    (*this)[0] = x;
    (*this)[1] = y;
    (*this)[2] = z;
  }

  Vector3(BaseVector<3, type> const & vec) : BaseVector<3, type>(vec) {}
  Vector3(type val = 0) : BaseVector<3, type>(val) {}

  type operator^(const Vector3& other) const
  {
    Vector3 tmp;

    for (unsigned i = 0; i < 3; ++i)
      tmp[i] = Vector3::_vector[(i + 1) % 3] * other._vector[(i + 2) % 3] - Vector3::_vector[(i + 2) % 3] * other._vector[(i + 1) % 3];
    return (tmp);
  }
};

template<class type>
class Vector2 : public BaseVector<2, type>
{
public:
  Vector2(type x, type y)
  {
    (*this)[0] = x;
    (*this)[1] = y;
  }

  double ratio() const
  {
    return (static_cast<double>(Vector2::_vector[0]) / static_cast<double>(Vector2::_vector[1]));
  }

  Vector2(BaseVector<2, type> const & vec) : BaseVector<2, type>(vec) {}
  Vector2(type val = 0) : BaseVector<2, type>(val) {}
};

typedef Vector4<float> Vect4f;
typedef Vector3<int> Vect3i;
typedef Vector3<float> Vect3f;
typedef Vector2<unsigned int> Vect2u;
typedef Vector2<int> Vect2i;
typedef Vector2<float> Vect2f;

#endif // VECTOR_HPP
