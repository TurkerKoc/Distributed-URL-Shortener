FROM ubuntu:22.10 as base

RUN apt-get update && apt-get install -y cmake g++ libasan6 libcurl4-openssl-dev
WORKDIR cbdp
ENV CBDP_PORT 4242
COPY . .
RUN mkdir -p cmake-build-debug
RUN apt-get update && apt-get install -y git libssl-dev pkg-config
RUN apt-get install -y uuid-dev
RUN cd cmake-build-debug && cmake ..
RUN cd cmake-build-debug && make

FROM base as coordinator

#COPY cmake-build-debug/coordinator_readFromBlob .
CMD exec ./cmake-build-debug/coordinator filelist.csv "$CBDP_PORT"

FROM base as worker

ENV CBDP_COORDINATOR coordinator
#COPY cmake-build-debug/worker_readFromBlob .
#CMD echo "worker $CBDP_COORDINATOR $CBDP_PORT" && exec ./cmake-build-debug/worker "$CBDP_COORDINATOR" "$CBDP_PORT"
CMD exec ./cmake-build-debug/worker "$CBDP_COORDINATOR" "$CBDP_PORT"
