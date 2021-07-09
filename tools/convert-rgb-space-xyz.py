#!/usr/bin/env python
#
# This program allows to generate matrices to convert between RGB spaces and XYZ
# All hardcoded values are directly taken from the ITU-R documents
#
# NOTE: When trying to convert from one space to another, make sure the whitepoint is the same,
# otherwise math gets more complicated (see Bradford transform).
#
# See also:
# http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
# https://ninedegreesbelow.com/photography/xyz-rgb.html

import numpy

def xy_to_XYZ(xy):
    return [xy[0] / xy[1], 1.0, (1.0 - xy[0] - xy[1]) / xy[1]]

def compute_rgb_to_zyx_matrix(whitepoint_XYZ, R, G, B):
    Xr = R[0] / R[1]
    Yr = 1
    Zr = (1 - R[0] - R[1]) / R[1]

    Xg = G[0] / G[1]
    Yg = 1
    Zg = (1 - G[0] - G[1]) / G[1]

    Xb = B[0] / B[1]
    Yb = 1
    Zb = (1 - B[0] - B[1]) / B[1]

    m = numpy.array([
        [Xr, Xg, Xb],
        [Yr, Yg, Yb],
        [Zr, Zg, Zb]])

    m_inverse = numpy.linalg.inv(m)

    S = numpy.dot(m_inverse, whitepoint_XYZ)

    m = numpy.array([
        [S[0] * Xr, S[1] * Xg, S[2] * Xb],
        [S[0] * Yr, S[1] * Yg, S[2] * Yb],
        [S[0] * Zr, S[1] * Zg, S[2] * Zb]])

    return m

def d65_XYZ():
    d65_xy = [0.3127, 0.3290]

    return xy_to_XYZ(d65_xy)

def compute_srgb_to_xyz():
    R_xy = [0.640, 0.330]
    G_xy = [0.300, 0.600]
    B_xy = [0.150, 0.060]

    return compute_rgb_to_zyx_matrix(d65_XYZ(), R_xy, G_xy, B_xy)

def compute_rec709_rgb_to_xyz():
    R_xy = [0.640, 0.330]
    G_xy = [0.300, 0.600]
    B_xy = [0.150, 0.060]

    return compute_rgb_to_zyx_matrix(d65_XYZ(), R_xy, G_xy, B_xy)

def compute_rec2020_rgb_to_xyz():
    R_xy = [0.708, 0.292]
    G_xy = [0.170, 0.797]
    B_xy = [0.131, 0.046]

    return compute_rgb_to_zyx_matrix(d65_XYZ(), R_xy, G_xy, B_xy)

def compute_display_p3_rgb_to_xyz():
    R_xy = [0.680, 0.320]
    G_xy = [0.265, 0.690]
    B_xy = [0.150, 0.060]

    return compute_rgb_to_zyx_matrix(d65_XYZ(), R_xy, G_xy, B_xy)

def compute_full_transform(A_to_XYZ, B_to_XYZ):

    XYZ_to_B = numpy.linalg.inv(B_to_XYZ)

    A_to_B = numpy.matmul(XYZ_to_B, A_to_XYZ)

    print(f'M\n{A_to_B}')

    B_to_A = numpy.linalg.inv(A_to_B)

    print(f'M-1\n{B_to_A}')

if __name__ == '__main__':
    numpy.set_printoptions(precision = 10, suppress = True, floatmode = 'fixed')

    compute_full_transform(compute_rec709_rgb_to_xyz(), compute_rec2020_rgb_to_xyz())
