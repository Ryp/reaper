///////////////////////////////////////////////////////////////////////////////////
/// OpenGL Mathematics (glm.g-truc.net)
///
/// Copyright (c) 2005 - 2013 G-Truc Creation (www.g-truc.net)
/// Permission is hereby granted, free of charge, to any person obtaining a copy
/// of this software and associated documentation files (the "Software"), to deal
/// in the Software without restriction, including without limitation the rights
/// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
/// copies of the Software, and to permit persons to whom the Software is
/// furnished to do so, subject to the following conditions:
/// 
/// The above copyright notice and this permission notice shall be included in
/// all copies or substantial portions of the Software.
/// 
/// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
/// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
/// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
/// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
/// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
/// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
/// THE SOFTWARE.
///
/// @ref core
/// @file glm/core/type_mat3x3.hpp
/// @date 2005-01-27 / 2011-06-15
/// @author Christophe Riccio
///////////////////////////////////////////////////////////////////////////////////

#ifndef glm_core_type_mat3x3
#define glm_core_type_mat3x3

#include "../fwd.hpp"
#include "type_mat.hpp"

namespace glm{
namespace detail
{
	template <typename T, precision P>
	struct tmat3x3
	{
		enum ctor{_null};
		typedef T value_type;
		typedef std::size_t size_type;
		typedef tvec3<T, P> col_type;
		typedef tvec3<T, P> row_type;
		typedef tmat3x3<T, P> type;
		typedef tmat3x3<T, P> transpose_type;

		static GLM_FUNC_DECL size_type col_size();
		static GLM_FUNC_DECL size_type row_size();

		GLM_FUNC_DECL GLM_CONSTEXPR size_type length() const;

	public:
		/// Implementation detail
		/// @cond DETAIL
		GLM_FUNC_DECL tmat3x3<T, P> _inverse() const;
		/// @endcond

	private:
		// Data
		col_type value[3];

	public:
		// Constructors
		GLM_FUNC_DECL tmat3x3();
		GLM_FUNC_DECL tmat3x3(tmat3x3<T, P> const & m);

		GLM_FUNC_DECL explicit tmat3x3(
			ctor Null);
		GLM_FUNC_DECL explicit tmat3x3(
			value_type const & s);
		GLM_FUNC_DECL explicit tmat3x3(
			value_type const & x0, value_type const & y0, value_type const & z0,
			value_type const & x1, value_type const & y1, value_type const & z1,
			value_type const & x2, value_type const & y2, value_type const & z2);
		GLM_FUNC_DECL explicit tmat3x3(
			col_type const & v0,
			col_type const & v1,
			col_type const & v2);

		//////////////////////////////////////
		// Conversions
		template <typename U>
		GLM_FUNC_DECL explicit tmat3x3(
			U const & x);
			
		template<
			typename X1, typename Y1, typename Z1,
			typename X2, typename Y2, typename Z2,
			typename X3, typename Y3, typename Z3>
		GLM_FUNC_DECL explicit tmat3x3(
			X1 const & x1, Y1 const & y1, Z1 const & z1,
			X2 const & x2, Y2 const & y2, Z2 const & z2,
			X3 const & x3, Y3 const & y3, Z3 const & z3);
			
		template <typename V1, typename V2, typename V3>
		GLM_FUNC_DECL explicit tmat3x3(
			tvec3<V1, P> const & v1,
			tvec3<V2, P> const & v2,
			tvec3<V3, P> const & v3);

		// Matrix conversions
		template <typename U, precision Q>
		GLM_FUNC_DECL explicit tmat3x3(tmat3x3<U, Q> const & m);

		GLM_FUNC_DECL explicit tmat3x3(tmat2x2<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat4x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat2x3<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat3x2<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat2x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat4x2<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat3x4<T, P> const & x);
		GLM_FUNC_DECL explicit tmat3x3(tmat4x3<T, P> const & x);

		// Accesses
		GLM_FUNC_DECL col_type & operator[](size_type i);
		GLM_FUNC_DECL col_type const & operator[](size_type i) const;

		// Unary updatable operators
		GLM_FUNC_DECL tmat3x3<T, P>& operator=  (tmat3x3<T, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator=  (tmat3x3<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator+= (U const & s);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator+= (tmat3x3<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator-= (U const & s);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator-= (tmat3x3<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator*= (U const & s);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator*= (tmat3x3<U, P> const & m);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator/= (U const & s);
		template <typename U>
		GLM_FUNC_DECL tmat3x3<T, P>& operator/= (tmat3x3<U, P> const & m);

		//////////////////////////////////////
		// Increment and decrement operators

		GLM_FUNC_DECL tmat3x3<T, P> & operator++ ();
		GLM_FUNC_DECL tmat3x3<T, P> & operator-- ();
		GLM_FUNC_DECL tmat3x3<T, P> operator++(int);
		GLM_FUNC_DECL tmat3x3<T, P> operator--(int);
	};

	// Binary operators
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator+ (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator+ (
		typename tmat3x3<T, P>::value_type const & s,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator+ (
		tmat3x3<T, P> const & m1,
		tmat3x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator- (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator- (
		typename tmat3x3<T, P>::value_type const & s,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator- (
		tmat3x3<T, P> const & m1,
		tmat3x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator* (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator* (
		typename tmat3x3<T, P>::value_type const & s,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x3<T, P>::col_type operator* (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x3<T, P>::row_type operator* (
		typename tmat3x3<T, P>::col_type const & v,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator* (
		tmat3x3<T, P> const & m1,
		tmat3x3<T, P> const & m2);
		
	template <typename T, precision P>
	GLM_FUNC_DECL tmat2x3<T, P> operator* (
		tmat3x3<T, P> const & m1,
		tmat2x3<T, P> const & m2);
		
	template <typename T, precision P>
	GLM_FUNC_DECL tmat4x3<T, P> operator* (
		tmat3x3<T, P> const & m1,
		tmat4x3<T, P> const & m2);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator/ (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::value_type const & s);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator/ (
		typename tmat3x3<T, P>::value_type const & s,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x3<T, P>::col_type operator/ (
		tmat3x3<T, P> const & m,
		typename tmat3x3<T, P>::row_type const & v);

	template <typename T, precision P>
	GLM_FUNC_DECL typename tmat3x3<T, P>::row_type operator/ (
		typename tmat3x3<T, P>::col_type const & v,
		tmat3x3<T, P> const & m);

	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> operator/ (
		tmat3x3<T, P> const & m1,
		tmat3x3<T, P> const & m2);

	// Unary constant operators
	template <typename T, precision P>
	GLM_FUNC_DECL tmat3x3<T, P> const operator-(
		tmat3x3<T, P> const & m);

}//namespace detail
}//namespace glm

#ifndef GLM_EXTERNAL_TEMPLATE
#include "type_mat3x3.inl"
#endif

#endif //glm_core_type_mat3x3
