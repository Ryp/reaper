#!/usr/bin/env bash
#
# framegraph_fill_template.sh
#
# Fill these markers in:
#   - marker-res
#   - marker-renderpass
#   - marker-side-effect-pass
#   - marker-used-input
#   - marker-used-output
#   - marker-unused-input
#   - marker-unused-output

fill_template()
{
    local template_file=$1
    local output_file=$2

    # Insert each file after the corresponding marker
    sed -e '/marker-res/r./resource.txt'                                    \
        -e '/marker-renderpass/r./renderpass.txt'                           \
        -e '/marker-side-effect-renderpass/r./renderpass-sideeffect.txt'    \
        -e '/marker-used-input/r./input.txt'                                \
        -e '/marker-used-output/r./output.txt'                              \
        $template_file > $output_file
}

fill_template $1 $2
