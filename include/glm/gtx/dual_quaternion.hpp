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
/// @ref gtx_dual_quaternion
/// @file glm/gtx/dual_quaternion.hpp
/// @date 2013-02-10 / 2013-02-20
/// @author Maksim Vorobiev (msomeone@gmail.com)
///
/// @see core (dependence)
/// @see gtc_half_float (dependence)
/// @see gtc_constants (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtc_dual_quaternion GLM_GTX_dual_quaternion
/// @ingroup gtc
///
/// @brief Defines a templated dual-quaternion type and several dual-quaternion operations.
///
/// <glm/gtx/dual_quaternion.hpp> need to be included to use these functionalities.
///////////////////////////////////////////////////////////////////////////////////

#ifndef GLM_GTX_dual_quaternion
#define GLM_GTX_dual_quaternion GLM_VERSION

// Dependency:
#include "../glm.hpp"
#include "../gtc/half_float.hpp"
#include "../gtc/constants.hpp"
#include "../gtc/quaternion.hpp"

#if(defined(GLM_MESSAGES) && !defined(glm_ext))
#	pragma message("GLM: GLM_GTX_dual_quaternion extension included")
#endif

namespace glm{
namespace detail
{
	template <typename T, precision P>
	struct tdualquat// : public genType<T, tquat>
	{
		enum ctor{null};
		
		typedef T value_type;
		typedef glm::detail::tquat<T, P> part_type;
		typedef std::size_t size_type;
		
	public:
		glm::detail::tquat<T, P> real, dual;
		
		GLM_FUNC_DECL GLM_CONSTEXPR size_type length() const;
		
		// Constructors
		tdualquat();
		explicit tdualquat(tquat<T, P> const & real);
		tdualquat(tquat<T, P> const & real,tquat<T, P> const & dual);
		tdualquat(tquat<T, P> const & orientation,tvec3<T, P> const& translation);
		
		//////////////////////////////////////////////////////////////
		// tdualquat conversions
		explicit tdualquat(tmat2x4<T, P> const & holder_mat);
		explicit tdualquat(tmat3x4<T, P> const & aug_mat);
		
		// Accesses
		part_type & operator[](int i);
		part_type const & operator[](int i) const;
		
		// Operators
		tdualquat<T, P> & operator*=(value_type const & s);
		tdualquat<T, P> & operator/=(value_type const & s);
	};
	
	template <typename T, precision P>
	detail::tquat<T, P> operator- (
		detail::tquat<T, P> const & q);
	
	template <typename T, precision P>
	detail::tdualquat<T, P> operator+ (
		detail::tdualquat<T, P> const & q,
		detail::tdualquat<T, P> const & p);
	
	template <typename T, precision P>
	detail::tdualquat<T, P> operator* (
		detail::tdualquat<T, P> const & q,
		detail::tdualquat<T, P> const & p);
	
	template <typename T, precision P>
	detail::tvec3<T, P> operator* (
		detail::tquat<T, P> const & q,
		detail::tvec3<T, P> const & v);
	
	template <typename T, precision P>
	detail::tvec3<T, P> operator* (
		detail::tvec3<T, P> const & v,
		detail::tquat<T, P> const & q);
	
	template <typename T, precision P>
	detail::tvec4<T, P> operator* (
		detail::tquat<T, P> const & q,
		detail::tvec4<T, P> const & v);
	
	template <typename T, precision P>
	detail::tvec4<T, P> operator* (
		detail::tvec4<T, P> const & v,
		detail::tquat<T, P> const & q);
	
	template <typename T, precision P>
	detail::tdualquat<T, P> operator* (
		detail::tdualquat<T, P> const & q,
		typename detail::tdualquat<T, P>::value_type const & s);
	
	template <typename T, precision P>
	detail::tdualquat<T, P> operator* (
		typename detail::tdualquat<T, P>::value_type const & s,
		detail::tdualquat<T, P> const & q);
	
	template <typename T, precision P>
	detail::tdualquat<T, P> operator/ (
		detail::tdualquat<T, P> const & q,
		typename detail::tdualquat<T, P>::value_type const & s);
} //namespace detail
	
	/// @addtogroup gtc_dual_quaternion
	/// @{

	/// Returns the normalized quaternion.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tdualquat<T, P> normalize(
		detail::tdualquat<T, P> const & q);

	/// Returns the linear interpolation of two dual quaternion.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tdualquat<T, P> lerp(
		detail::tdualquat<T, P> const & x,
		detail::tdualquat<T, P> const & y,
		typename detail::tdualquat<T, P>::value_type const & a);

	/// Returns the q inverse.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tdualquat<T, P> inverse(
		detail::tdualquat<T, P> const & q);

	/*
	/// Extracts a rotation part from dual-quaternion to a 3 * 3 matrix.
	/// TODO
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tmat3x3<T, P> mat3_cast(
		detail::tdualquat<T, P> const & x);
	*/
	
	/// Converts a quaternion to a 2 * 4 matrix.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tmat2x4<T, P> mat2x4_cast(
		detail::tdualquat<T, P> const & x);

	/// Converts a quaternion to a 3 * 4 matrix.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tmat3x4<T, P> mat3x4_cast(
		detail::tdualquat<T, P> const & x);

	/// Converts a 2 * 4 matrix (matrix which holds real and dual parts) to a quaternion.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tdualquat<T, P> dualquat_cast(
		detail::tmat2x4<T, P> const & x);

	/// Converts a 3 * 4 matrix (augmented matrix rotation + translation) to a quaternion.
	///
	/// @see gtc_dual_quaternion
	template <typename T, precision P>
	detail::tdualquat<T, P> dualquat_cast(
		detail::tmat3x4<T, P> const & x);


	/// Dual-quaternion of low half-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<half, lowp>		lowp_hdualquat;
	
	/// Dual-quaternion of medium half-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<half, mediump>	mediump_hdualquat;
	
	/// Dual-quaternion of high half-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<half, highp>		highp_hdualquat;
	
	
	/// Dual-quaternion of low single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, lowp>		lowp_dualquat;
	
	/// Dual-quaternion of medium single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, mediump>	mediump_dualquat;
	
	/// Dual-quaternion of high single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, highp>		highp_dualquat;


	/// Dual-quaternion of low single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, lowp>		lowp_fdualquat;
	
	/// Dual-quaternion of medium single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, mediump>	mediump_fdualquat;
	
	/// Dual-quaternion of high single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<float, highp>		highp_fdualquat;
	
	
	/// Dual-quaternion of low double-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<double, lowp>		lowp_ddualquat;
	
	/// Dual-quaternion of medium double-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<double, mediump>	mediump_ddualquat;
	
	/// Dual-quaternion of high double-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef detail::tdualquat<double, highp>	highp_ddualquat;

	
#if(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	/// Dual-quaternion of floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef highp_fdualquat			dualquat;
	
	/// Dual-quaternion of single-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef highp_fdualquat			fdualquat;
#elif(defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef highp_fdualquat			dualquat;
	typedef highp_fdualquat			fdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && defined(GLM_PRECISION_MEDIUMP_FLOAT) && !defined(GLM_PRECISION_LOWP_FLOAT))
	typedef mediump_fdualquat		dualquat;
	typedef mediump_fdualquat		fdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_FLOAT) && !defined(GLM_PRECISION_MEDIUMP_FLOAT) && defined(GLM_PRECISION_LOWP_FLOAT))
	typedef lowp_fdualquat			dualquat;
	typedef lowp_fdualquat			fdualquat;
#else
#	error "GLM error: multiple default precision requested for single-precision floating-point types"
#endif
	
	
#if(!defined(GLM_PRECISION_HIGHP_HALF) && !defined(GLM_PRECISION_MEDIUMP_HALF) && !defined(GLM_PRECISION_LOWP_HALF))
	/// Dual-quaternion of default half-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef highp_hdualquat			hdualquat;
#elif(defined(GLM_PRECISION_HIGHP_HALF) && !defined(GLM_PRECISION_MEDIUMP_HALF) && !defined(GLM_PRECISION_LOWP_HALF))
	typedef highp_hdualquat			hdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_HALF) && defined(GLM_PRECISION_MEDIUMP_HALF) && !defined(GLM_PRECISION_LOWP_HALF))
	typedef mediump_hdualquat		hdualquat;
#elif(!defined(GLM_PRECISION_HIGHP_HALF) && !defined(GLM_PRECISION_MEDIUMP_HALF) && defined(GLM_PRECISION_LOWP_HALF))
	typedef lowp_hdualquat			hdualquat;
#else
#	error "GLM error: Multiple default precision requested for half-precision floating-point types"
#endif


#if(!defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	/// Dual-quaternion of default double-precision floating-point numbers.
	///
	/// @see gtc_dual_quaternion
	typedef highp_ddualquat			ddualquat;
#elif(defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef highp_ddualquat			ddualquat;
#elif(!defined(GLM_PRECISION_HIGHP_DOUBLE) && defined(GLM_PRECISION_MEDIUMP_DOUBLE) && !defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef mediump_ddualquat		ddualquat;
#elif(!defined(GLM_PRECISION_HIGHP_DOUBLE) && !defined(GLM_PRECISION_MEDIUMP_DOUBLE) && defined(GLM_PRECISION_LOWP_DOUBLE))
	typedef lowp_ddualquat			ddualquat;
#else
#	error "GLM error: Multiple default precision requested for double-precision floating-point types"
#endif

	/// @}
} //namespace glm

#include "dual_quaternion.inl"

#endif//GLM_GTX_dual_quaternion
