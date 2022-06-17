#!/usr/bin/env bash
# @REAPER_GENERATED_BY_CMAKE_MSG@

# Redirect output to stderr.
exec 1>&2

################################################################################
# Verify clang-format config in git

# Config var used by clang-format's git integration
clang_format_style=$(git config --get hooks.clangformat.style)

# Warn the user for his configuration of clang-format
if [ "$clang_format_style" != "file" ]
then
    echo "Warning: clang-format is not configured properly."
    echo
    echo "To be able to use the 'git clang-format' command"
    echo "on your repo, please setup git using:"
    echo
    echo "    git config hooks.clangformat.style file"
    echo
fi

################################################################################
# Check for formatting issues

# Check for C++ source files using clang-format
files_to_check=$(git diff --cached --name-only | grep ".*\.\(cpp\|h\|inl\)$")
files_to_repair=()
format_diff=$(mktemp format_diff.XXXXXX)

for file in $files_to_check
do
    # Use mktemp to create temporary files.
    # They HAVE to be in the same dir as the source file so that clang-format
    # uses the right rules.
    temp_file=$(mktemp $file-staged.XXXXXX)
    temp_diff=$(mktemp $file-diff.XXXXXX)

    # Save content of the staged file
    git show :$file > $temp_file

    # Diff with the output of clang-format
    clang-format -style=file $temp_file | git diff --color --no-index $temp_file - > $temp_diff

    # Test if the diff contained changes
    if test $(cat $temp_diff | wc -c) != 0
    then
        files_to_repair+=($file)
        cat $temp_diff >> $format_diff
    fi

    # Remove temporary file
    rm $temp_file $temp_diff
done

if test ${#files_to_repair[@]} != 0
then
    cat $format_diff
    echo
    echo "Error: some files are not formatted correctly. To fix these issues:"
    echo
    echo "    clang-format -style=file -i ${files_to_repair[@]}"
    echo
    echo "Or:"
    echo
    echo "    git clang-format ${files_to_repair[@]}"
    echo
    rm $format_diff
    exit 1
fi

rm $format_diff

# Currently the 'git clang-format' command does not ignore unstaged changes
# making our job more difficult than it is. The following is kept for future
# reference when the tool gets patched

#   if test $(git clang-format --diff | wc -c) != 0
#   then
#       echo "Error: there were formatting errors."
#       echo
#       echo "To print these changes, execute:"
#       echo
#       echo "    git clang-format --diff"
#       echo
#       echo "To apply them:"
#       echo
#       echo "    git clang-format"
#       echo
#       exit 1
#   fi

################################################################################
# Check for non-ascii file additions

if git rev-parse --verify HEAD >/dev/null 2>&1
then
    against=HEAD
else
    # Initial commit: diff against an empty tree object
    against=4b825dc642cb6eb9a060e54bf8d69288fbee4904
fi

# Cross platform projects tend to avoid non-ascii filenames; prevent
# them from being added to the repository. We exploit the fact that the
# printable range starts at the space character and ends with tilde.
# Note that the use of brackets around a tr range is ok here, (it's
# even required, for portability to Solaris 10's /usr/bin/tr), since
# the square bracket bytes happen to fall in the designated range.
if test $(git diff --cached --name-only --diff-filter=A -z $against |
    LC_ALL=C tr -d '[ -~]\0' | wc -c) != 0
then
    echo "Error: Attempt to add a non-ascii file name."
    echo
    exit 1
fi

################################################################################
# Check for whitespace errors

# If there are whitespace errors, print the offending file names and fail.
exec git diff-index --check --cached $against --
