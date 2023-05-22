#!/usr/bin/env bash

docker container ls --quiet --all --filter "name=^makepy-cicd-sc-openocd_sources.*" | while read image_id ; do
  echo === $image_id ===
  docker stop $image_id
  docker rm $image_id
done
