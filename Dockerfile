FROM ubuntu:22.10 as base

RUN apt-get update && apt-get install -y cmake g++ libasan6 libcurl4-openssl-dev git libssl-dev pkg-config uuid-dev

WORKDIR cbdp

# Copy code
COPY ./protos/* .
COPY ./src/* .
COPY ./.clang* .
COPY ./CMakeLists.txt .

# Build
RUN mkdir -p cmake-build-debug
RUN cd cmake-build-debug && cmake ..
RUN cd cmake-build-debug && make

FROM base as loadBalancer

WORKDIR cbdp
COPY --from=base /cbdp/cmake-build-debug/src/loadBalancer .
CMD exec ./loadBalancer

FROM base as raft

WORKDIR cbdp
COPY --from=base /cbdp/cmake-build-debug/src/raft .
CMD exec ./raft $RAFT_NODE_NUMBER

FROM base as client

WORKDIR cbdp
COPY --from=base /cbdp/cmake-build-debug/src/client .