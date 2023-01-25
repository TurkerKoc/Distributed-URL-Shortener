FROM ubuntu:22.10 as base

RUN apt-get update && apt-get install -y cmake g++ libasan6 libcurl4-openssl-dev git libssl-dev pkg-config uuid-dev
RUN apt-get update && apt-get install -y build-essential autoconf libtool pkg-config curl unzip zip

WORKDIR /
# install protobuf first, then grpc,
ENV PROTO_HOME="/proto_home"
RUN mkdir -p $PROTO_HOME

WORKDIR $HOME

ENV MY_INSTALL_DIR="${HOME}/.local"
RUN mkdir -p $MY_INSTALL_DIR

WORKDIR $MY_INSTALL_DIR


ENV OLD_PATH "$PATH"
ENV NEW_PATH "$MY_INSTALL_DIR/bin:$OLD_PATH"
RUN echo "export PATH=$MY_INSTALL_DIR/bin:$OLD_PATH" >> ~/.bashrc
RUN echo "export PATH=$/bin:$MY_INSTALL_DIR/bin:$OLD_PATH" >> /etc/environment
RUN echo $NEW_PATH

RUN git clone --recurse-submodules -b v1.50.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc

WORKDIR $MY_INSTALL_DIR/grpc

#RUN dpkg-reconfigure dash --no-reload
RUN apt-get update && apt-get install -y bash
SHELL ["/bin/bash", "-c"]

RUN ls -l

RUN mkdir -p cmake/build  \
    && pushd cmake/build \
    && cmake -DgRPC_INSTALL=ON \
          -DgRPC_BUILD_TESTS=OFF \
          -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
          ../.. \
    && make -j 4 \
    && make install \
    && popd


WORKDIR /cbdp

# Copy code
COPY . .
RUN rm -rf cmake-build-debug

RUN apt-get install -y sqlite3 libsqlite3-dev

# Build
RUN mkdir -p cmake-build-debug
RUN cd cmake-build-debug && cmake -DCMAKE_PREFIX_PATH=$MY_INSTALL_DIR ..
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