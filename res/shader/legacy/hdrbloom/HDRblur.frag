#version 450 core

uniform sampler2D textureUnit0;
uniform vec2 tc_offset[25];
uniform int lodLevel;

out vec4 outColor;

void main(void)
{
    vec4 hdrSample[25];
    for (int i = 0; i < 25; ++i)
    {
        // Perform 25 lookups around the current texel
        hdrSample[i] = textureLod (textureUnit0, vertOutTexCoords.st +
                tc_offset[i], lodLevel);
    }

    // 5x5 Gaussian kernel
    //   1  4  7  4 1
    //   4 16 26 16 4
    //   7 26 41 26 7 / 273
    //   4 16 26 16 4
    //   1  4  7  4 1

    // Calculate weighted color of a region
    vec4 color = hdrSample[12];
    outColor = (
            (1.0 * (hdrSample[0] + hdrSample[4] + hdrSample[20] +
                    hdrSample[24])) +
            (4.0 * (hdrSample[1] + hdrSample[3] + hdrSample[5] + hdrSample[9] +
                    hdrSample[15] + hdrSample[19] + hdrSample[21] +
                    hdrSample[23])) +
            (7.0 * (hdrSample[2] + hdrSample[10] + hdrSample[14] +
                    hdrSample[22])) +
            (16.0 * (hdrSample[6] + hdrSample[8] + hdrSample[16] +
                     hdrSample[18])) +
            (26.0 * (hdrSample[7] + hdrSample[11] + hdrSample[13] +
                     hdrSample[17])) +
            (41.0 * (hdrSample[12]))
            ) / 273.0;
}
