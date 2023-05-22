#!/usr/bin/env bash
package_info="$1"

prefix_string=$(echo -n "$package_info" | awk -F# '{print $1}' | awk -F\' '{print $2}')
sources_hash=$(echo -n "$package_info" | awk -F# '{print $2}' | awk -F: '{ print $1}')
config_hash=$(echo -n "$package_info" | awk -F# '{print $2}' | awk -F: '{ print $2}')
binary_hash=$(echo -n "$package_info" | tr -d \' | awk -F# '{print $3}')

echo "src: $sources_hash"
echo "cfg: $config_hash"
echo "bin: $binary_hash"
echo "prefix_string: $prefix_string"

URL_BASE="http://artifactory.dev.syntacore.com:8082/artifactory/tools-conan"

product=$(echo -n "$prefix_string" | awk -F/ '{ print $1}')
semver=$(echo -n "$prefix_string" | awk -F/ '{ print $2}' | awk -F@ '{print $1}')
branch=$(echo -n "$prefix_string" | awk -F@ '{ print $2}')
branch_start=$(echo -n "$branch" | awk -F/ '{ print $1 }')
branch_tail=${branch#"$branch_start/"}

echo "product: $product"
echo "semver: $semver"
echo "branch: $branch"
echo "branch_start: $branch_start"
echo "branch_tail: $branch_tail"

PART1="$URL_BASE/$branch_start/$product/$semver/$branch_tail"
PART2="$sources_hash/package/$config_hash/$binary_hash/conan_package.tgz"
echo "The Magick Link: $PART1/$PART2"
