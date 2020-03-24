docker run \
    --privileged \
    -i -t  \
    --rm \
    --name ndn_test \
    --volume ~/ice-ar:/ice-ar \
    xrt_env:latest
